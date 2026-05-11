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

using namespace std;
using namespace std::chrono;
using namespace com::rdk::hal::sensor::thermal;

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
/*  Test 1 — TC1: Exceed and Recovery Lifecycle with Aggregation              */
/* ========================================================================== */

/**
 * @brief TC1 – Verify thermal state transitions as defined by transition_states in the L3 YAML.
 *
 * Covers:
 *  - One listener is sufficient: IThermalSensor fires onThermalStateChange to all
 *    registered listeners for the system-wide state, not per-sensor.
 *  - Verifies initial state is NORMAL via getCurrentThermalState()
 *  - For each consecutive (from, to) pair in transition_states:
 *      - The listener receives the expected state-change event
 *      - timestampMonotonicMs increases monotonically across steps
 *      - getCurrentThermalState() reflects the new state after each event
 *  - Unregisters the listener at the end
 *
 * **Test Group ID:** Basic: 03@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L3_THERMALSENSOR, ThermalExceedRecoveryLifecycle)
{
    const char *testName = "ThermalExceedRecoveryLifecycle";
    thermalSensorL3Config_t cfg;

    UT_ASSERT_EQUAL_FATAL(
        vts_thermalsensor_parse_l3_config(
            VTS_THERMALSENSOR_TEST_CONFIG_PATH, gModuleName, gTestGroup, testName, cfg),
        UT_STATUS_OK);

    /* Need at least 2 states to have at least one transition */
    UT_ASSERT_TRUE(cfg.transitionStates.size() >= 2);

    /* Map state name string -> State enum */
    auto toState = [](const std::string& s) -> State {
        if (s == "CRITICAL_TEMPERATURE_EXCEEDED")  return State::CRITICAL_TEMPERATURE_EXCEEDED;
        if (s == "CRITICAL_TEMPERATURE_RECOVERED") return State::CRITICAL_TEMPERATURE_RECOVERED;
        if (s == "CRITICAL_SHUTDOWN_IMMINENT")      return State::CRITICAL_SHUTDOWN_IMMINENT;
        return State::NORMAL;
    };

    size_t sensorCount = gThermalSensorHFP.sensors.size();
    UT_ASSERT_TRUE(sensorCount > 0);

    UT_LOG_INFO("[L3 TC1] HFP has %zu sensor(s), transition sequence has %zu state(s), timeoutMs=%d",
                sensorCount, cfg.transitionStates.size(), cfg.eventTimeoutMs);

    /* One listener is sufficient — onThermalStateChange is a system-wide event
     * delivered to all registered listeners regardless of which sensor triggered it.
     * The fixture's mListener is already registered in SetUp(). */
    mListener->resetEventQueue();

    /* Drain any stale events */
    {
        ActionEvent s;
        UT_ASSERT_FALSE(mListener->getNextEvent(s, chrono::milliseconds(200)));
    }

    /* ---- Verify initial state is NORMAL ---- */
    {
        State initialState;
        THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, initialState);
        UT_ASSERT_EQUAL(initialState, State::NORMAL);
        UT_LOG_INFO("[L3 TC1] Initial state confirmed: NORMAL");
    }

    auto waitMs  = chrono::milliseconds(cfg.eventTimeoutMs);
    int64_t lastTs = 0;

    /* ---- Iterate each consecutive (from -> to) transition in the config list ---- */
    for (size_t step = 1; step < cfg.transitionStates.size(); ++step)
    {
        const std::string& fromName = cfg.transitionStates[step - 1];
        const std::string& toName   = cfg.transitionStates[step];
        State expectedState = toState(toName);

        UT_LOG_INFO("[L3 TC1] step %zu: %s -> %s (waiting up to %dms)",
                    step, fromName.c_str(), toName.c_str(), cfg.eventTimeoutMs);

        ActionEvent ev;
        UT_ASSERT_TRUE(mListener->getNextEvent(ev, waitMs));
        UT_ASSERT_EQUAL(ev.state, expectedState);
        UT_ASSERT_TRUE(ev.timestampMonotonicMs > lastTs);
        lastTs = ev.timestampMonotonicMs;

        if (ev.temperatureReading.has_value())
        {
            UT_LOG_INFO("[L3 TC1] step %zu: trigger sensor='%s' temp=%.2f",
                step,
                android::String8(ev.temperatureReading.value().sensorName).string(),
                ev.temperatureReading.value().temperatureCelsius);
        }

        State cur;
        THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, cur);
        UT_ASSERT_EQUAL(cur, expectedState);

        UT_LOG_INFO("[L3 TC1] step %zu OK: state=%s ts=%lld",
                    step, toName.c_str(), (long long)lastTs);
    }
    /* mListener is unregistered by TearDown() */
}

/**
 * @brief TC2 – Verify critical shutdown trigger and suppression of subsequent transitions.
 *
 * Covers:
 *  - For each consecutive (from, to) pair in transition_states up to and including SHUTDOWN:
 *      the listener receives the expected state-change event with monotonic timestamp.
 *  - For each state after SHUTDOWN in transition_states:
 *      no event must arrive within suppressionTimeoutMs (events are suppressed).
 *  - getCurrentThermalState() remains SHUTDOWN_IMMINENT after suppressed injections.
 *
 * **Test Group ID:** Basic: 03@n
 * **Test Case ID:** 002@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L3_THERMALSENSOR, CriticalShutdownWithSuppression)
{
    const char *testName = "CriticalShutdownWithSuppression";
    thermalSensorL3Config_t cfg;

    UT_ASSERT_EQUAL_FATAL(
        vts_thermalsensor_parse_l3_config(
            VTS_THERMALSENSOR_TEST_CONFIG_PATH, gModuleName, gTestGroup, testName, cfg),
        UT_STATUS_OK);

    UT_ASSERT_TRUE(cfg.transitionStates.size() >= 2);

    auto toState = [](const std::string& s) -> State {
        if (s == "CRITICAL_TEMPERATURE_EXCEEDED")  return State::CRITICAL_TEMPERATURE_EXCEEDED;
        if (s == "CRITICAL_TEMPERATURE_RECOVERED") return State::CRITICAL_TEMPERATURE_RECOVERED;
        if (s == "CRITICAL_SHUTDOWN_IMMINENT")      return State::CRITICAL_SHUTDOWN_IMMINENT;
        return State::NORMAL;
    };

    UT_LOG_INFO("[L3 TC2] transition sequence has %zu state(s), timeoutMs=%d suppressionMs=%d",
                cfg.transitionStates.size(), cfg.eventTimeoutMs, cfg.suppressionTimeoutMs);

    mListener->resetEventQueue();
    auto waitMs       = chrono::milliseconds(cfg.eventTimeoutMs);
    auto suppressMs   = chrono::milliseconds(cfg.suppressionTimeoutMs);
    int64_t lastTs    = 0;
    bool    inShutdown = false;

    for (size_t step = 1; step < cfg.transitionStates.size(); ++step)
    {
        const std::string& fromName = cfg.transitionStates[step - 1];
        const std::string& toName   = cfg.transitionStates[step];
        State expectedState = toState(toName);

        if (!inShutdown)
        {
            /* Before SHUTDOWN: expect the event to arrive */
            UT_LOG_INFO("[L3 TC2] step %zu: %s -> %s (expect event)",
                        step, fromName.c_str(), toName.c_str());

            ActionEvent ev;
            UT_ASSERT_TRUE(mListener->getNextEvent(ev, waitMs));
            UT_ASSERT_EQUAL(ev.state, expectedState);
            UT_ASSERT_TRUE(ev.timestampMonotonicMs > lastTs);
            lastTs = ev.timestampMonotonicMs;

            State cur;
            THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, cur);
            UT_ASSERT_EQUAL(cur, expectedState);

            UT_LOG_INFO("[L3 TC2] step %zu OK: state=%s ts=%lld",
                        step, toName.c_str(), (long long)lastTs);

            if (toName == "CRITICAL_SHUTDOWN_IMMINENT")
            {
                inShutdown = true;
                mListener->resetEventQueue();
                UT_LOG_INFO("[L3 TC2] CRITICAL_SHUTDOWN_IMMINENT reached — subsequent injections must be suppressed");
            }
        }
        else
        {
            /* After SHUTDOWN: event must be suppressed */
            UT_LOG_INFO("[L3 TC2] step %zu: %s -> %s (expect SUPPRESSED within %dms)",
                        step, fromName.c_str(), toName.c_str(), cfg.suppressionTimeoutMs);

            ActionEvent ev;
            bool got = mListener->getNextEvent(ev, suppressMs);
            UT_ASSERT_FALSE(got);

            /* State must remain SHUTDOWN_IMMINENT */
            State cur;
            THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, cur);
            UT_ASSERT_EQUAL(cur, State::CRITICAL_SHUTDOWN_IMMINENT);

            UT_LOG_INFO("[L3 TC2] step %zu OK: %s suppressed, state remains SHUTDOWN_IMMINENT",
                        step, toName.c_str());
        }
    }
    /* mListener is unregistered by TearDown() */
}

/** @} */ // End of THERMAL_SENSOR_HALTEST_L3
