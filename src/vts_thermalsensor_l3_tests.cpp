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
 * UT control plane.  A single test covers both scenarios:
 *
 *   ThermalStateTransition — driven by transition_states in the L3 YAML.
 *     If suppressionTimeoutMs == 0: all transitions expect an event (TC1 path).
 *     If suppressionTimeoutMs  > 0: transitions after CRITICAL_SHUTDOWN_IMMINENT
 *       expect NO event within suppressionTimeoutMs (TC2 path).
 *
 * **Pre-Conditions:** HAL service must be running with control-plane enabled @n
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
            // Service may have restarted after CRITICAL_SHUTDOWN_IMMINENT — ignore unregister errors
            auto status = gTHERMALSENSORHandle->unregisterEventListener(mListener, &_r);
            if (!status.isOk())
            {
                UT_LOG_INFO("[TearDown] unregisterEventListener failed (service restarting, code=%d)",
                            status.exceptionCode());
            }
            mListener.clear();
        }
    }
};


/**
 * @brief Verify thermal state transitions driven by transition_states in the L3 YAML.
 *
 * Behaviour is controlled entirely by the YAML written by the Python host:
 *
 *   suppressionTimeoutMs == 0  (TC1 path — exceed/recovery lifecycle):
 *     Every consecutive (from→to) pair must produce an event within
 *     eventTimeoutMs.  Monotonic timestamps and getCurrentThermalState()
 *     consistency are verified at each step.
 *
 *   suppressionTimeoutMs  > 0  (TC2 path — critical shutdown with suppression):
 *     Steps up to and including CRITICAL_SHUTDOWN_IMMINENT behave as TC1.
 *     Steps after SHUTDOWN must produce NO event within suppressionTimeoutMs,
 *     and getCurrentThermalState() must remain CRITICAL_SHUTDOWN_IMMINENT.
 *
 * **Test Group ID:** Basic: 03@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** HAL service must be running with control-plane enabled@n
 * **Dependencies:** IThermalSensor, UT control-plane@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |@n
 * | :----: | --------- | ---------- |-------------- | ----- |@n
 * | 01 | Assert initial state is NORMAL via getCurrentThermalState(). | input: none | State == NORMAL | Should Pass |@n
 * | 02 | Drain any stale events from the listener queue. | timeout: 200 ms | No event received | Should Pass |@n
 * | 03 | For each consecutive (from→to) state pair where to != post-SHUTDOWN: assert event arrives within eventTimeoutMs, event.state matches expected, timestamp is monotonically increasing, and getCurrentThermalState() reflects the new state. | transition_states from YAML; eventTimeoutMs | Event received with correct state and monotonic timestamp | Should Pass |@n
 * | 04 | (TC2 only, suppressionTimeoutMs > 0) For each state after CRITICAL_SHUTDOWN_IMMINENT: assert NO event arrives within suppressionTimeoutMs and getCurrentThermalState() remains CRITICAL_SHUTDOWN_IMMINENT. | transition_states after SHUTDOWN; suppressionTimeoutMs | No event received; state remains CRITICAL_SHUTDOWN_IMMINENT | Should Pass |@n
 */
UT_ADD_TEST(VTS_L3_THERMALSENSOR, ThermalStateTransition)
{
    const char *testName = "ThermalStateTransition";
    thermalSensorL3Config_t cfg;

    UT_ASSERT_EQUAL_FATAL(
        vts_thermalsensor_parse_l3_config(
            VTS_THERMALSENSOR_TEST_CONFIG_PATH, gModuleName, gTestGroup, testName, cfg),
        UT_STATUS_OK);

    UT_ASSERT_TRUE(cfg.transitionStates.size() >= 2);

    auto toState = [](const std::string& s) -> State
    {
        if (s == "CRITICAL_TEMPERATURE_EXCEEDED")
            return State::CRITICAL_TEMPERATURE_EXCEEDED;
        if (s == "CRITICAL_TEMPERATURE_RECOVERED")
            return State::CRITICAL_TEMPERATURE_RECOVERED;
        if (s == "CRITICAL_SHUTDOWN_IMMINENT")
            return State::CRITICAL_SHUTDOWN_IMMINENT;
        if (s == "NORMAL")
            return State::NORMAL;
        UT_LOG_ERROR("[L3 ThermalStateTransition] Unknown state string in config: '%s'", s.c_str());
        UT_ASSERT_TRUE(false);
        return State::NORMAL; /* unreachable; silences compiler warning */
    };

    const bool suppressionEnabled = (cfg.suppressionTimeoutMs > 0);

    UT_LOG_INFO("[L3 ThermalStateTransition] %zu state(s), timeoutMs=%d suppressionMs=%d (%s)",
                cfg.transitionStates.size(), cfg.eventTimeoutMs, cfg.suppressionTimeoutMs,
                suppressionEnabled ? "TC2-shutdown" : "TC1-lifecycle");

    mListener->resetEventQueue();

    /* Step 02: drain stale events */
    {
        ActionEvent stale;
        UT_ASSERT_FALSE(mListener->getNextEvent(stale, chrono::milliseconds(200)));
    }

    /* Step 01: initial state must be NORMAL */
    {
        State initialState;
        THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, initialState);
        UT_ASSERT_EQUAL(initialState, State::NORMAL);
        UT_LOG_INFO("[L3 ThermalStateTransition] Initial state confirmed: NORMAL");
    }

    auto waitMs      = chrono::milliseconds(cfg.eventTimeoutMs);
    auto suppressMs  = chrono::milliseconds(cfg.suppressionTimeoutMs);
    int64_t lastTs   = 0;
    bool inShutdown  = false;

    for (size_t step = 1; step < cfg.transitionStates.size(); ++step)
    {
        const std::string& fromName = cfg.transitionStates[step - 1];
        const std::string& toName   = cfg.transitionStates[step];
        State expectedState = toState(toName);

        if (!inShutdown)
        {
            /* Step 03: expect event */
            UT_LOG_INFO("[L3 ThermalStateTransition] step %zu: %s -> %s (expect event, timeout=%dms)",
                        step, fromName.c_str(), toName.c_str(), cfg.eventTimeoutMs);

            ActionEvent ev;
            UT_ASSERT_TRUE(mListener->getNextEvent(ev, waitMs));
            UT_ASSERT_EQUAL(ev.state, expectedState);
            UT_ASSERT_TRUE(ev.timestampMonotonicMs > lastTs);
            lastTs = ev.timestampMonotonicMs;

            UT_LOG_INFO("[L3 ThermalStateTransition] step %zu OK: state=%s ts=%lld",
                        step, toName.c_str(), (long long)lastTs);

            if (suppressionEnabled && toName == "CRITICAL_SHUTDOWN_IMMINENT")
            {
                inShutdown = true;
                mListener->resetEventQueue();

                State cur;
                auto stateStatus = gTHERMALSENSORHandle->getCurrentThermalState(&cur);
                if (stateStatus.isOk())
                {
                    UT_ASSERT_EQUAL(cur, expectedState);
                    UT_LOG_INFO("[L3 ThermalStateTransition] SHUTDOWN reached — state verified before restart");
                }
                else
                {
                    UT_LOG_INFO("[L3 ThermalStateTransition] SHUTDOWN reached — service restarting, state check skipped (code=%d)",
                                stateStatus.exceptionCode());
                }
                continue;
            }

            {
                State cur;
                THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, cur);
                UT_ASSERT_EQUAL(cur, expectedState);
            }
        }
        else
        {
            /* Step 04 (TC2): expect suppression */
            UT_LOG_INFO("[L3 ThermalStateTransition] step %zu: %s -> %s (expect SUPPRESSED, window=%dms)",
                        step, fromName.c_str(), toName.c_str(), cfg.suppressionTimeoutMs);

            ActionEvent ev;
            UT_ASSERT_FALSE(mListener->getNextEvent(ev, suppressMs));

            State cur;
            auto stateStatus = gTHERMALSENSORHandle->getCurrentThermalState(&cur);
            if (stateStatus.isOk())
            {
                UT_ASSERT_EQUAL(cur, State::CRITICAL_SHUTDOWN_IMMINENT);
                UT_LOG_INFO("[L3 ThermalStateTransition] step %zu OK: %s suppressed, state remains SHUTDOWN_IMMINENT",
                            step, toName.c_str());
            }
            else
            {
                UT_LOG_INFO("[L3 ThermalStateTransition] step %zu OK: %s suppressed (service restarting, code=%d)",
                            step, toName.c_str(), stateStatus.exceptionCode());
            }
        }
    }
    /* mListener is unregistered by TearDown() */
}

/** @} */ // End of THERMAL_SENSOR_HALTEST_L3
