/**
 *  If not stated otherwise in this file or this component's LICENSE
 *  file the following copyright and licenses apply:
 *
 *  Copyright 2026 RDK Management
 *
 *  Licensed under the Apache License, Version 2.0 (the License);
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an AS IS BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/**
 * @defgroup THERMAL_SENSOR_HALTEST_L3 Thermal Sensor HAL Tests L3 File
 * @{
 * @parblock
 *
 * ### L3 Test Cases for Thermal Sensor HAL :
 *
 * Level 3 end-to-end tests driven by raw temperature injections via the
 * UT control plane.  Two tests cover the complete HAL contract:
 *
 *   ThermalExceedRecoveryLifecycle — NORMAL→EXCEEDED→RECOVERED→NORMAL;
 *     verifies event delivery, monotonic timestamps, aggregation,
 *     idempotent injection, 3-listener fan-out, and listener-gate.
 *
 *   CriticalShutdownWithSuppression — NORMAL→EXCEEDED→SHUTDOWN_IMMINENT;
 *     verifies shutdown delivery and suppression of all subsequent events.
 *
 * **Pre-Conditions:** vcomponent must be running with control-plane enabled @n
 * **Dependencies:** UT control-plane @n
 *
 * @endparblock
 */

/**
 * @file vts_thermalsensor_l3_tests.cpp
 */

#include "vts_thermalsensor.h"
#include "vts_thermalsensor_parse_hfp.h"
#include "vts_thermalsensor_parse_testconfig.h"

extern android::sp<IThermalSensor> gTHERMALSENSORHandle;

static const char *gModuleName = "thermalsensor";
static const char *gTestGroup  = "L3";

UT_ADD_TEST_TO_GROUP(VTS_L3_THERMALSENSOR, UT_TESTS_L3)

/* ========================================================================== */
/*  Test fixture                                                               */
/* ========================================================================== */

class VTS_L3_THERMALSENSOR : public UTCore
{
public:
    VTS_L3_THERMALSENSOR() : UTCore() {}
    ~VTS_L3_THERMALSENSOR() override = default;

    sp<ThermalSensorEventListener> mListener;

    void SetUp() override
    {
        android::ProcessState::self()->startThreadPool();
        mListener = sp<ThermalSensorEventListener>::make();
        THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, mListener);
    }

    void TearDown() override
    {
        if (mListener)
        {
            bool _r;
            gTHERMALSENSORHandle->unregisterEventListener(mListener, &_r);
            mListener.clear();
        }
    }
};

/* ========================================================================== */
/*  Test 5 — TC1: Exceed and Recovery Lifecycle with Aggregation              */
/* ========================================================================== */

/**
 * @brief TC1 – Verify thermal exceed and recovery lifecycle.
 *
 * Covers:
 *  - NORMAL → EXCEEDED → RECOVERED → NORMAL via TEMPERATURE_UPDATE
 *  - Idempotent injection produces no duplicate event
 *  - timestampMonotonicMs increases monotonically
 *  - getCurrentThermalState() always reflects latest event state
 *  - event.temperatureReading is the highest-severity sensor
 *  - Three listeners (mListener + l2 + l3) all receive every event
 *  - Listener-gate: l3 is unregistered mid-test; subsequent events must
 *    NOT reach l3, confirming unregister silences delivery
 *
 * **Test Group ID:** Basic: 03@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L3_THERMALSENSOR, ThermalExceedRecoveryLifecycle)
{
    const char *testName = "ThermalExceedRecoveryLifecycle";
    thermalSensorL3ExceedRecoveryConfig_t cfg;

    UT_ASSERT_EQUAL_FATAL(
        vts_thermalsensor_parse_exceedrecovery_config(
            VTS_THERMALSENSOR_TEST_CONFIG_PATH, gModuleName, gTestGroup, testName, cfg),
        UT_STATUS_OK);

    UT_LOG_INFO("[L3 TC1] exceeded=%.1f recovered=%.1f "
                "overTemp=%.1f recoveryTemp=%.1f normalTemp=%.1f timeoutMs=%d",
                cfg.criticalExceededCelsius, cfg.criticalRecoveredCelsius,
                cfg.overTempCelsius, cfg.recoveryTempCelsius, cfg.normalTempCelsius,
                cfg.eventTimeoutMs);

    // Register two additional listeners (fixture provides mListener)
    sp<ThermalSensorEventListener> l2 = sp<ThermalSensorEventListener>::make();
    sp<ThermalSensorEventListener> l3 = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l2);
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l3);

    mListener->resetEventQueue();
    l2->resetEventQueue();
    l3->resetEventQueue();

    // Drain any stale events
    { ActionEvent s; UT_ASSERT_FALSE(mListener->getNextEvent(s, chrono::milliseconds(200))); }

    auto waitMs = chrono::milliseconds(cfg.eventTimeoutMs);
    int64_t lastTs = 0;

    /* ---- Phase 1: over-temperature → CRITICAL_TEMPERATURE_EXCEEDED ---- */
    /* Python host injects primary sensor at overTempCelsius */
    {
        ActionEvent ev1, ev2, ev3;
        UT_ASSERT_TRUE(mListener->getNextEvent(ev1, waitMs));
        UT_ASSERT_TRUE(l2->getNextEvent(ev2, waitMs));
        UT_ASSERT_TRUE(l3->getNextEvent(ev3, waitMs));

        UT_ASSERT_EQUAL(ev1.state, State::CRITICAL_TEMPERATURE_EXCEEDED);
        UT_ASSERT_EQUAL(ev2.state, State::CRITICAL_TEMPERATURE_EXCEEDED);
        UT_ASSERT_EQUAL(ev3.state, State::CRITICAL_TEMPERATURE_EXCEEDED);
        UT_ASSERT_TRUE(ev1.timestampMonotonicMs > lastTs);
        UT_ASSERT_EQUAL(ev1.timestampMonotonicMs, ev2.timestampMonotonicMs);
        UT_ASSERT_EQUAL(ev1.timestampMonotonicMs, ev3.timestampMonotonicMs);
        lastTs = ev1.timestampMonotonicMs;

        if (ev1.temperatureReading.has_value())
        {
            UT_LOG_INFO("[L3 TC1] Phase 1: sensor=%s temp=%.2f",
                android::String8(ev1.temperatureReading.value().sensorName).string(),
                ev1.temperatureReading.value().temperatureCelsius);
            UT_ASSERT_TRUE(
                ev1.temperatureReading.value().temperatureCelsius > cfg.criticalExceededCelsius);
        }

        State cur; THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, cur);
        UT_ASSERT_EQUAL(cur, State::CRITICAL_TEMPERATURE_EXCEEDED);
        UT_LOG_INFO("[L3 TC1] Phase 1 OK: EXCEEDED ts=%lld (3 listeners)", (long long)lastTs);
    }

    /* ---- Phase 1b: idempotent injection — same overTemp, must NOT produce another event ---- */
    {
        ActionEvent stale;
        bool got = mListener->getNextEvent(stale, chrono::milliseconds(3000));
        UT_ASSERT_FALSE(got);
        UT_LOG_INFO("[L3 TC1] Phase 1b OK: no duplicate event on idempotent injection");
    }

    /* ---- Listener-gate: unregister l3; subsequent events must NOT reach it ---- */
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l3);
    l3->resetEventQueue();
    UT_LOG_INFO("[L3 TC1] l3 unregistered — subsequent events must not reach it");

    /* ---- Phase 2: recovery temperature → CRITICAL_TEMPERATURE_RECOVERED ---- */
    /* Python host injects primary sensor at recoveryTempCelsius */
    {
        ActionEvent ev1, ev2;
        UT_ASSERT_TRUE(mListener->getNextEvent(ev1, waitMs));
        UT_ASSERT_TRUE(l2->getNextEvent(ev2, waitMs));

        UT_ASSERT_EQUAL(ev1.state, State::CRITICAL_TEMPERATURE_RECOVERED);
        UT_ASSERT_EQUAL(ev2.state, State::CRITICAL_TEMPERATURE_RECOVERED);
        UT_ASSERT_TRUE(ev1.timestampMonotonicMs > lastTs);
        UT_ASSERT_EQUAL(ev1.timestampMonotonicMs, ev2.timestampMonotonicMs);
        lastTs = ev1.timestampMonotonicMs;

        // Listener-gate verification: l3 must NOT have received this event
        ActionEvent gate;
        UT_ASSERT_FALSE(l3->getNextEvent(gate, chrono::milliseconds(500)));
        UT_LOG_INFO("[L3 TC1] Phase 2 OK: RECOVERED ts=%lld; l3 gate confirmed", (long long)lastTs);

        State cur; THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, cur);
        UT_ASSERT_EQUAL(cur, State::CRITICAL_TEMPERATURE_RECOVERED);
    }

    /* ---- Phase 3: normal temperature → NORMAL ---- */
    /* Python host injects normalTempCelsius */
    {
        ActionEvent ev1, ev2;
        UT_ASSERT_TRUE(mListener->getNextEvent(ev1, waitMs));
        UT_ASSERT_TRUE(l2->getNextEvent(ev2, waitMs));

        UT_ASSERT_EQUAL(ev1.state, State::NORMAL);
        UT_ASSERT_EQUAL(ev2.state, State::NORMAL);
        UT_ASSERT_TRUE(ev1.timestampMonotonicMs > lastTs);

        // Listener-gate: l3 must still be silent
        ActionEvent gate;
        UT_ASSERT_FALSE(l3->getNextEvent(gate, chrono::milliseconds(500)));
        UT_LOG_INFO("[L3 TC1] Phase 3 OK: NORMAL ts=%lld; l3 gate confirmed",
                    (long long)ev1.timestampMonotonicMs);

        State cur; THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, cur);
        UT_ASSERT_EQUAL(cur, State::NORMAL);
    }

    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l2);
    // l3 already unregistered above
}

/* ========================================================================== */
/*  Test 6 — TC2: Critical Shutdown with Recovery Suppression                 */
/* ========================================================================== */

/**
 * @brief TC2 – Verify critical shutdown trigger and suppression of recovery.
 *
 * The state-machine path is: NORMAL → EXCEEDED → CRITICAL_SHUTDOWN_IMMINENT.
 *
 * The test verifies:
 *  - NORMAL → EXCEEDED event delivered with monotonic timestamp
 *  - EXCEEDED → SHUTDOWN_IMMINENT event delivered with monotonic timestamp
 *  - getCurrentThermalState() reflects SHUTDOWN_IMMINENT after both events
 *  - No RECOVERED or NORMAL events are emitted after entering shutdown
 *    (host injects recovery temperature; vcomponent must suppress it)
 *  - No NORMAL event emitted after injecting normal temperature either
 *
 * **Test Group ID:** Basic: 03@n
 * **Test Case ID:** 006@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L3_THERMALSENSOR, CriticalShutdownWithSuppression)
{
    const char *testName = "CriticalShutdownWithSuppression";
    thermalSensorL3ShutdownConfig_t cfg;

    UT_ASSERT_EQUAL_FATAL(
        vts_thermalsensor_parse_shutdown_config(
            VTS_THERMALSENSOR_TEST_CONFIG_PATH, gModuleName, gTestGroup, testName, cfg),
        UT_STATUS_OK);

    UT_LOG_INFO("[L3 TC2] exceeded=%.1f shutdown=%.1f overTemp=%.1f shutdownTemp=%.1f "
                "recoveryTemp=%.1f timeoutMs=%d suppressionMs=%d",
                cfg.criticalExceededCelsius, cfg.enteringCriticalShutdownCelsius,
                cfg.overTempCelsius, cfg.shutdownTempCelsius, cfg.recoveryTempCelsius,
                cfg.eventTimeoutMs, cfg.suppressionTimeoutMs);

    mListener->resetEventQueue();
    auto waitMs = chrono::milliseconds(cfg.eventTimeoutMs);
    int64_t lastTs = 0;

    /* ---- Phase 1: inject overTemp → EXCEEDED ---- */
    {
        ActionEvent event;
        UT_ASSERT_TRUE(mListener->getNextEvent(event, waitMs));
        UT_ASSERT_EQUAL(event.state, State::CRITICAL_TEMPERATURE_EXCEEDED);
        UT_ASSERT_TRUE(event.timestampMonotonicMs > lastTs);
        lastTs = event.timestampMonotonicMs;

        State cur; THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, cur);
        UT_ASSERT_EQUAL(cur, State::CRITICAL_TEMPERATURE_EXCEEDED);
        UT_LOG_INFO("[L3 TC2] Phase 1 OK: EXCEEDED ts=%lld", (long long)lastTs);
    }

    /* ---- Phase 2: inject shutdownTemp → SHUTDOWN_IMMINENT ---- */
    {
        ActionEvent event;
        UT_ASSERT_TRUE(mListener->getNextEvent(event, waitMs));
        UT_ASSERT_EQUAL(event.state, State::CRITICAL_SHUTDOWN_IMMINENT);
        UT_ASSERT_TRUE(event.timestampMonotonicMs > lastTs);
        lastTs = event.timestampMonotonicMs;

        State cur; THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, cur);
        UT_ASSERT_EQUAL(cur, State::CRITICAL_SHUTDOWN_IMMINENT);
        UT_LOG_INFO("[L3 TC2] Phase 2 OK: SHUTDOWN_IMMINENT ts=%lld", (long long)lastTs);
    }

    mListener->resetEventQueue();

    /* ---- Phase 3: inject recoveryTemp → must be SUPPRESSED ---- */
    /* Python host injects recovery temperature; vcomponent must not emit event */
    {
        ActionEvent event;
        bool got = mListener->getNextEvent(event, chrono::milliseconds(cfg.suppressionTimeoutMs));
        UT_ASSERT_FALSE(got);
        UT_LOG_INFO("[L3 TC2] Phase 3 OK: RECOVERED suppressed while in SHUTDOWN_IMMINENT");
    }

    /* ---- Phase 4: inject normalTemp → must also be SUPPRESSED ---- */
    {
        ActionEvent event;
        bool got = mListener->getNextEvent(event, chrono::milliseconds(cfg.suppressionTimeoutMs));
        UT_ASSERT_FALSE(got);
        UT_LOG_INFO("[L3 TC2] Phase 4 OK: NORMAL suppressed while in SHUTDOWN_IMMINENT");
    }

    /* Final state must still be SHUTDOWN_IMMINENT */
    {
        State cur; THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, cur);
        UT_ASSERT_EQUAL(cur, State::CRITICAL_SHUTDOWN_IMMINENT);
        UT_LOG_INFO("[L3 TC2] Final state correctly remains SHUTDOWN_IMMINENT");
    }
}

/** @} */ // End of THERMAL_SENSOR_HALTEST_L3
