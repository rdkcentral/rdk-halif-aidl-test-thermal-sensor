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
 * @defgroup THERMAL_SENSOR_HALTEST_L2 Thermal Sensor HAL Tests L2 File
 * @{
 * @parblock
 *
 * ### L2 Test Cases for Thermal Sensor HAL :
 *
 * Level 2 functional tests covering state consistency and listener lifecycle.
 *
 * **Pre-Conditions:** Thermal Sensor HAL service must be running @n
 * **Dependencies:** IThermalSensor @n
 *
 * @endparblock
 */

/**
 * @file vts_thermalsensor_l2_tests.cpp
 */

#include "vts_thermalsensor.h"
#include "vts_thermalsensor_parse_hfp.h"

extern sp<IThermalSensor> gTHERMALSENSORHandle;

UT_ADD_TEST_TO_GROUP(VTS_L2_THERMALSENSOR, UT_TESTS_L2)

class VTS_L2_THERMALSENSOR : public UTCore
{
public:
    VTS_L2_THERMALSENSOR() : UTCore() {}
    ~VTS_L2_THERMALSENSOR() override = default;
    void SetUp() override { android::ProcessState::self()->startThreadPool(); }
    void TearDown() override {}
};

/**
 * @brief Validate that the current thermal state is a known enum value from the HFP profile.
 *
 * The HFP profile declares which states the platform supports.
 * The runtime state returned by getCurrentThermalState() must be one of those.
 *
 * **Test Group ID:** Basic: 02@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L2_THERMALSENSOR, ThermalStateConsistency)
{
    State state;
    THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, state);

    UT_LOG_INFO("[L2 ThermalStateConsistency] current state=%s",
                UT_Control_GetMapString(state_mapTable_thermalsensor, (int32_t)state));

    /* Validate state is a known enum member */
    bool valid = (state == State::NORMAL ||
                  state == State::CRITICAL_TEMPERATURE_EXCEEDED ||
                  state == State::CRITICAL_TEMPERATURE_RECOVERED ||
                  state == State::CRITICAL_SHUTDOWN_IMMINENT);
    UT_ASSERT_TRUE(valid);
}

/**
 * @brief Validate that register + unregister + re-register listener lifecycle works.
 *
 * Ensures the service correctly tracks listener registration across a full
 * register → unregister → re-register cycle without leaking or corrupting state.
 *
 * **Test Group ID:** Basic: 02@n
 * **Test Case ID:** 002@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L2_THERMALSENSOR, ListenerLifecycle)
{
    sp<IThermalEventListener> listener = sp<ThermalSensorEventListener>::make();

    // Register
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, listener);

    // Re-register same listener must fail (already registered)
    THERMALSENSOR_REGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, listener, Status::EX_NONE);

    // Unregister
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, listener);

    // After unregistration, unregistering again must fail
    THERMALSENSOR_UNREGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, listener, Status::EX_NONE);

    // Re-register must succeed after unregister
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, listener);

    // Final cleanup
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, listener);
}

/**
 * @brief Validate that multiple independent listeners can be registered simultaneously.
 *
 * **Test Group ID:** Basic: 02@n
 * **Test Case ID:** 003@n
 * **Priority:** Medium@n
 */
UT_ADD_TEST(VTS_L2_THERMALSENSOR, MultipleListeners)
{
    sp<IThermalEventListener> l1 = sp<ThermalSensorEventListener>::make();
    sp<IThermalEventListener> l2 = sp<ThermalSensorEventListener>::make();
    sp<IThermalEventListener> l3 = sp<ThermalSensorEventListener>::make();

    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l1);
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l2);
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l3);

    // Unregister in reverse order
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l3);
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l2);
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l1);
}

/** @} */ // End of THERMAL_SENSOR_HALTEST_L2
