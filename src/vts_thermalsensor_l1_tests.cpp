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
#include <inttypes.h>

using namespace std;
using namespace com::rdk::hal::sensor::thermal;

extern sp<IThermalSensor> gTHERMALSENSORHandle;

UT_ADD_TEST_TO_GROUP(VTS_L1_THERMALSENSOR, UT_TESTS_L1)

class VTS_L1_THERMALSENSOR : public UTCore
{
public:
    VTS_L1_THERMALSENSOR() : UTCore() 
    {

    }
    ~VTS_L1_THERMALSENSOR() override = default;
    void SetUp() override 
    { 
        android::ProcessState::self()->startThreadPool(); 
    }
    void TearDown() override 
    {

    }
};

/* -------------------------------------------------------------------------
 * Helper: look up an HFP sensor by sensorName.
 * Returns a pointer into gThermalSensorHFP.sensors, or nullptr if not found.
 * ------------------------------------------------------------------------- */
static const thermalSensorHFPSensor_t* getHFPSensorByName(const std::string& name)
{
    for (const auto& s : gThermalSensorHFP.sensors)
    {
        if (s.sensorName == name)
            return &s;
    }
    return nullptr;
}

/**
 * @brief Validate the positive path for registerEventListener() and unregisterEventListener(),
 *        including multiple-listener registration and re-registration after unregister.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** Thermal Sensor HAL service must be running@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |@n
 * | :----: | --------- | ---------- |-------------- | ----- |@n
 * | 01 | Create 10 distinct IThermalEventListener instances. | 10 listener objects | 10 listener objects created successfully | Should Pass |@n
 * | 02 | Register all 10 listeners in forward order via registerEventListener(). | listeners: 10 valid IThermalEventListener instances | Each call returns status OK and _aidl_return true | Should Pass |@n
 * | 03 | Unregister all 10 listeners in reverse order via unregisterEventListener(). | listeners: 10 registered IThermalEventListener instances | Each call returns status OK and _aidl_return true | Should Pass |@n
 * | 04 | Register a single valid listener via registerEventListener(). | listener: valid IThermalEventListener | Returns status OK and _aidl_return true | Should Pass |@n
 * | 05 | Unregister the registered listener via unregisterEventListener(). | listener: the registered IThermalEventListener | Returns status OK and _aidl_return true | Should Pass |@n
 * | 06 | Re-register the same listener via registerEventListener() after unregistering. | listener: previously unregistered IThermalEventListener | Returns status OK and _aidl_return true | Should Pass |@n
 * | 07 | Unregister the re-registered listener via unregisterEventListener(). | listener: re-registered IThermalEventListener | Returns status OK and _aidl_return true | Should Pass |@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, registerEventListener_pos)
{
    constexpr int NUM_LISTENERS = 10;
    std::vector<sp<IThermalEventListener>> listeners;
    listeners.reserve(NUM_LISTENERS);

    //Step 1: Create 10 distinct IThermalEventListener instances
    for (int i = 0; i < NUM_LISTENERS; ++i)
        listeners.push_back(sp<ThermalSensorEventListener>::make());

    // Step 2: register all listeners in forward order — each must succeed
    for (auto& l : listeners)
        THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l);

    // Step 3: unregister all listeners in reverse order — each must succeed
    for (auto it = listeners.rbegin(); it != listeners.rend(); ++it)
    {
        auto& l = *it;
        THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l);
    }

    // Step 4: register a single valid listener — must succeed
    sp<IThermalEventListener> validListener = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

    // Step 5: unregister the valid listener — must succeed
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

    // Step 6: re-register the same listener — must succeed after unregister
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

    // Step 7: final unregister — must succeed
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);
}

/**
 * @brief Validate the negative paths for registerEventListener().
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 002@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** Thermal Sensor HAL service must be running@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |@n
 * | :----: | --------- | ---------- |-------------- | ----- |@n
 * | 01 | Call registerEventListener() with a null listener. | listener: nullptr | Status returns EX_NULL_POINTER | Should Pass |@n
 * | 02 | Call registerEventListener() with a valid listener. | listener: valid IThermalEventListener | Returns status OK and _aidl_return true | Should Pass |@n
 * | 03 | Call registerEventListener() with the same listener again. | listener: already-registered IThermalEventListener | Returns status OK and _aidl_return false (already registered) | Should Pass |@n
 * | 04 | Unregister the valid listener via unregisterEventListener(). | listener: registered IThermalEventListener | Returns status OK and _aidl_return true | Should Pass |@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, registerEventListener_neg)
{
    sp<IThermalEventListener> nullListener = nullptr;

    // Step 1: null listener must be rejected by binder with EX_NULL_POINTER
    THERMALSENSOR_REGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, nullListener, Status::EX_NULL_POINTER);

    // Step 2: valid listener must register successfully
    sp<IThermalEventListener> validListener = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

    // Step 3: registering the same listener again must return false, status ok
    THERMALSENSOR_REGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, validListener, Status::EX_NONE);

    // Step 4: unregister the valid listener — must succeed
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

}

/**
 * @brief Validate the positive path for unregisterEventListener().
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 003@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |@n
 * | :----: | --------- | ---------- |-------------- | ----- |@n
 * | 01 | Register a valid listener via registerEventListener(). | listener: valid IThermalEventListener | Returns status OK and _aidl_return true | Should Pass |@n
 * | 02 | Unregister the registered listener via unregisterEventListener(). | listener: registered IThermalEventListener | Returns status OK and _aidl_return true | Should Pass |@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, unregisterEventListener_pos)
{
    // Step 1: valid listener must register successfully
    sp<IThermalEventListener> validListener = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

    // Step 2: unregister the valid listener — must succeed
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);
}

/**
 * @brief Validate the negative paths for unregisterEventListener().
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 004@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** None@n
 * **Dependencies:** None@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |@n
 * | :----: | --------- | ---------- |-------------- | ----- |@n
 * | 01 | Register a valid listener via registerEventListener(). | listener: valid IThermalEventListener | Returns status OK and _aidl_return true | Should Pass |@n
 * | 02 | Unregister the registered listener via unregisterEventListener(). | listener: registered IThermalEventListener | Returns status OK and _aidl_return true | Should Pass |@n
 * | 03 | Call unregisterEventListener() again with the same listener (already unregistered). | listener: previously unregistered IThermalEventListener | Returns status OK and _aidl_return false | Should Pass |@n
 * | 04 | Call unregisterEventListener() with a listener that was never registered. | listener: a different unregistered IThermalEventListener | Returns status OK and _aidl_return false | Should Pass |@n
 * | 05 | Call unregisterEventListener() with a null listener. | listener: nullptr | Status returns EX_NULL_POINTER | Should Pass |@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, unregisterEventListener_neg)
{
    sp<IThermalEventListener> nullListener = nullptr;

    // Step 1: valid listener must register successfully
    sp<IThermalEventListener> validListener = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

    // Step 2: unregister the valid listener — must succeed
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

    // Step 3: unregister the valid listener again — already unregistered, must fail
    THERMALSENSOR_UNREGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, validListener, Status::EX_NONE);

    // Step 4: unregistering a never-registered listener must return false, status ok
    sp<IThermalEventListener> unregisteredListener = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_UNREGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, unregisteredListener, Status::EX_NONE);

    // Step 5: null listener unregister must be rejected by binder with EX_NULL_POINTER
    THERMALSENSOR_UNREGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, nullListener, Status::EX_NULL_POINTER);
}

/**
 * @brief Validate getCurrentThermalState() returns the default initial state NORMAL.
 *
 * The HAL service initialises the thermal state to NORMAL on startup (per the
 * IThermalSensor HAL definition).  This test verifies that the API is callable
 * and returns the expected default value before any simulated thermal events.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 005@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** Thermal Sensor HAL service must be running with no prior thermal events@n
 * **Dependencies:** IThermalSensor@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |@n
 * | :----: | --------- | ---------- |-------------- | ----- |@n
 * | 01 | Call getCurrentThermalState() and capture the returned State and status. | input: none | Returns status OK | Should Pass |@n
 * | 02 | Assert the returned State equals NORMAL, the HAL-defined default initial state. | state enum value | State == NORMAL | Should Pass |@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, getCurrentThermalState_pos)
{
    /* Step 1: call the API — status must be OK */
    State state;
    THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, state);

    /* Step 2: default initial state must be NORMAL per HAL definition */
    UT_LOG_INFO("[L1 GetCurrentThermalState] state=%s",
                UT_Control_GetMapString(state_mapTable_thermalsensor, (int32_t)state));
    UT_ASSERT_EQUAL(state, State::NORMAL);
}

/**
 * @brief Validate getCurrentTemperatures() returns a non-empty vector of structurally valid readings.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 006@n
 * **Priority:** High@n
 *
 * **Pre-Conditions:** Thermal Sensor HAL service must be running@n
 * **Dependencies:** IThermalSensor@n
 * **User Interaction:** None@n
 *
 * **Test Procedure:**@n
 * | Variation / Step | Description | Test Data | Expected Result | Notes |@n
 * | :----: | --------- | ---------- |-------------- | ----- |@n
 * | 01 | Call getCurrentTemperatures() and capture the returned readings vector and status. | input: none | Returns status OK and a non-empty readings vector | Should Pass |@n
 * | 02 | For each TemperatureReading assert sensorName is not empty. | each reading.sensorName | sensorName length > 0 | Should Pass |@n
 * | 03 | For each TemperatureReading assert temperatureCelsius is above absolute zero (-273.15 °C). | each reading.temperatureCelsius | temperatureCelsius >= -273.15 | Should Pass |@n
 * | 04 | For each TemperatureReading assert timestampMonotonicMs is >= 0. | each reading.timestampMonotonicMs | timestampMonotonicMs >= 0 | Should Pass |@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, getCurrentTemperatures_pos)
{
    std::vector<TemperatureReading> readings;
    THERMALSENSOR_GET_TEMPERATURES(gTHERMALSENSORHandle, readings);

    /* Step 1: readings vector must not be empty */
    UT_ASSERT_TRUE(readings.size() > 0);

    for (size_t i = 0; i < readings.size(); ++i)
    {
        const TemperatureReading& r = readings[i];

        /* Step 2: sensorName must not be empty */
        UT_ASSERT_TRUE(r.sensorName.size() > 0);

        /* Step 3: temperatureCelsius must be above absolute zero */
        UT_ASSERT_TRUE(r.temperatureCelsius >= -273.15);

        /* Step 4: timestampMonotonicMs must be >= 0 */
        UT_ASSERT_TRUE(r.timestampMonotonicMs >= 0);

        UT_LOG_INFO("[L1 GetCurrentTemperatures] [%zu] sensor='%s' temp=%.2f timestampMs=%" PRId64,
                    i, android::String8(r.sensorName).string(),
                    r.temperatureCelsius, r.timestampMonotonicMs);
    }
}


/** @} */ // End of THERMAL_SENSOR_HALTEST_L1
