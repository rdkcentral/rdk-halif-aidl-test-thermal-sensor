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
 * @defgroup THERMAL_SENSOR_HALTEST_L1 Thermal Sensor HAL Tests L1 File
 * @{
 * @parblock
 *
 * ### L1 Test Cases for Thermal Sensor HAL :
 *
 * Level 1 functional tests (positive and negative).
 * Tests verify that each IThermalSensor API meets the operational requirements.
 *
 * **Pre-Conditions:** Thermal Sensor HAL service must be running @n
 * **Dependencies:** IThermalSensor @n
 *
 * @endparblock
 */

/**
 * @file vts_thermalsensor_l1_tests.cpp
 */

#include "vts_thermalsensor_parse_hfp.h"
#include "vts_thermalsensor.h"

extern sp<IThermalSensor> gTHERMALSENSORHandle;

UT_ADD_TEST_TO_GROUP(VTS_L1_THERMALSENSOR, UT_TESTS_L1)

class VTS_L1_THERMALSENSOR : public UTCore
{
public:
    VTS_L1_THERMALSENSOR() : UTCore() {}
    ~VTS_L1_THERMALSENSOR() override = default;
    void SetUp() override { android::ProcessState::self()->startThreadPool(); }
    void TearDown() override {}
};

/* ========================================================================== */
/*  registerEventListener                                                      */
/* ========================================================================== */

// 1. registerEventListener - Positive
UT_ADD_TEST(VTS_L1_THERMALSENSOR, RegisterEventListener_Positive)
{
    sp<IThermalEventListener> listener = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, listener);
    // Cleanup
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, listener);
}

// 2. registerEventListener - Negative: null listener — binder rejects at transport with EX_NULL_POINTER
UT_ADD_TEST(VTS_L1_THERMALSENSOR, RegisterEventListener_Negative_NullListener)
{
    sp<IThermalEventListener> nullListener = nullptr;
    THERMALSENSOR_REGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, nullListener, Status::EX_NULL_POINTER);
}

// 3. registerEventListener - Negative: registering same listener twice must return false
UT_ADD_TEST(VTS_L1_THERMALSENSOR, RegisterEventListener_Negative_AlreadyRegistered)
{
    sp<IThermalEventListener> listener = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, listener);
    // Second registration must return false, status ok
    THERMALSENSOR_REGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, listener, Status::EX_NONE);
    // Cleanup
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, listener);
}

/* ========================================================================== */
/*  unregisterEventListener                                                    */
/* ========================================================================== */

// 4. unregisterEventListener - Positive
UT_ADD_TEST(VTS_L1_THERMALSENSOR, UnregisterEventListener_Positive)
{
    sp<IThermalEventListener> listener = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, listener);
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, listener);
}

// 5. unregisterEventListener - Negative: listener not registered must return false
UT_ADD_TEST(VTS_L1_THERMALSENSOR, UnregisterEventListener_Negative_NotRegistered)
{
    sp<IThermalEventListener> listener = sp<ThermalSensorEventListener>::make();
    // Never registered — must return false, status ok
    THERMALSENSOR_UNREGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, listener, Status::EX_NONE);
}

// 6. unregisterEventListener - Negative: null listener — binder rejects at transport with EX_NULL_POINTER
UT_ADD_TEST(VTS_L1_THERMALSENSOR, UnregisterEventListener_Negative_Null)
{
    sp<IThermalEventListener> nullListener = nullptr;
    THERMALSENSOR_UNREGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, nullListener, Status::EX_NULL_POINTER);
}

/* ========================================================================== */
/*  getCurrentThermalState                                                     */
/* ========================================================================== */

// 7. getCurrentThermalState - Positive: must return ok with a valid State value
UT_ADD_TEST(VTS_L1_THERMALSENSOR, GetCurrentThermalState_Positive)
{
    State state;
    THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, state);
    // State must be one of the defined enum values
    bool validState = (state == State::NORMAL ||
                       state == State::CRITICAL_TEMPERATURE_EXCEEDED ||
                       state == State::CRITICAL_TEMPERATURE_RECOVERED ||
                       state == State::CRITICAL_SHUTDOWN_IMMINENT);
    UT_ASSERT_TRUE(validState);
    UT_LOG_INFO("[L1 GetCurrentThermalState_Positive] state=%s",
                UT_Control_GetMapString(state_mapTable_thermalsensor, (int32_t)state));
}

// 8. getCurrentThermalState - Positive: multiple calls must all succeed
UT_ADD_TEST(VTS_L1_THERMALSENSOR, GetCurrentThermalState_MultipleCallsConsistent)
{
    State state1, state2;
    THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, state1);
    THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, state2);
    // Without an injected change between the two calls the state must be identical
    UT_ASSERT_EQUAL(state1, state2);
}

/* ========================================================================== */
/*  getCurrentTemperatures                                                     */
/* ========================================================================== */

// 9. getCurrentTemperatures - Positive: must return ok (may be empty per spec)
UT_ADD_TEST(VTS_L1_THERMALSENSOR, GetCurrentTemperatures_Positive)
{
    std::vector<TemperatureReading> readings;
    THERMALSENSOR_GET_TEMPERATURES(gTHERMALSENSORHandle, readings);
    UT_LOG_INFO("[L1 GetCurrentTemperatures_Positive] returned %zu reading(s)", readings.size());
}

// 10. getCurrentTemperatures - Positive: check structure of returned readings
UT_ADD_TEST(VTS_L1_THERMALSENSOR, GetCurrentTemperatures_StructureValid)
{
    std::vector<TemperatureReading> readings;
    THERMALSENSOR_GET_TEMPERATURES(gTHERMALSENSORHandle, readings);

    for (size_t i = 0; i < readings.size(); ++i)
    {
        // sensorName must not be empty (per TemperatureReading.aidl)
        UT_ASSERT_TRUE(readings[i].sensorName.size() > 0);
        // temperatureCelsius must be within a plausible physical range
        UT_ASSERT_TRUE(readings[i].temperatureCelsius >= -273.15);
        UT_LOG_INFO("[L1 GetCurrentTemperatures_StructureValid] [%zu] sensor=%s temp=%.2f",
                    i,
                    android::String8(readings[i].sensorName).c_str(),
                    readings[i].temperatureCelsius);
    }
}

// 11. getCurrentTemperatures - Profile match: compare against HFP expected sensors
UT_ADD_TEST(VTS_L1_THERMALSENSOR, GetCurrentTemperatures_MatchProfile)
{
    if (gThermalSensorHFP.sensors.empty())
    {
        UT_LOG_INFO("[L1 GetCurrentTemperatures_MatchProfile] No sensors in HFP — skipping");
        return;
    }

    std::vector<TemperatureReading> readings;
    THERMALSENSOR_GET_TEMPERATURES(gTHERMALSENSORHandle, readings);

    for (const auto& expected : gThermalSensorHFP.sensors)
    {
        bool found = false;
        for (const auto& actual : readings)
        {
            if (android::String8(actual.sensorName).string() == expected.sensorName)
            {
                found = true;
                UT_LOG_INFO("[L1 GetCurrentTemperatures_MatchProfile] Found expected sensor '%s'",
                            expected.sensorName.c_str());
                break;
            }
        }
        UT_ASSERT_TRUE(found);
    }
}

/** @} */ // End of THERMAL_SENSOR_HALTEST_L1
