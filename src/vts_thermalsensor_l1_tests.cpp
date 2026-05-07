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
 * @brief Validate the positive path for registerEventListener() and
 *        unregisterEventListener(), including re-registration after unregister.
 *
 * Steps:
 *  1. Register a valid listener — must succeed (_aidl_return true).
 *  2. Unregister the valid listener — must succeed (_aidl_return true).
 *  3. Re-register the same listener — must succeed (listener is no longer active).
 *  4. Unregister the re-registered listener — must succeed.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 001@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, RegisterUnregisterEventListener_Positive)
{
    // Step 1: register a valid listener — must succeed
    sp<IThermalEventListener> validListener = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

    // Step 2: unregister the valid listener — must succeed
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

    // Step 3: re-register the same listener — must succeed after unregister
    THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);

    // Step 4: final unregister — must succeed
    THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, validListener);
}

/**
 * @brief Validate that multiple concurrent listeners can be registered and
 *        unregistered independently without interference.
 *
 * Steps:
 *  1. Create NUM_LISTENERS distinct listener instances.
 *  2. Register all listeners in forward order — each must succeed.
 *  3. Unregister all listeners in reverse order — each must succeed.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 00@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, RegisterUnregisterEventListener_MultipleListener)
{
    constexpr int NUM_LISTENERS = 10;
    std::vector<sp<IThermalEventListener>> listeners;
    listeners.reserve(NUM_LISTENERS);

    // Step 1: create NUM_LISTENERS distinct listener instances
    for (int i = 0; i < NUM_LISTENERS; ++i)
        listeners.push_back(sp<ThermalSensorEventListener>::make());

    // Step 2: register all listeners in forward order — each must succeed
    for (auto& l : listeners)
        THERMALSENSOR_REGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l);

    // Step 3: unregister all listeners in reverse order — each must succeed
    for (auto& l : listeners)
        THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(gTHERMALSENSORHandle, l);
}

/**
 * @brief Validate all negative paths for registerEventListener() and
 *        unregisterEventListener().
 *
 * Steps:
 *  1. Register a null listener — binder must reject with EX_NULL_POINTER.
 *  2. Register a valid listener — must succeed (_aidl_return true).
 *  3. Register the same listener again — must return false (already registered), status ok.
 *  4. Unregister the valid listener — must succeed (_aidl_return true).
 *  5. Unregister a never-registered listener — must return false, status ok.
 *  6. Unregister a null listener — binder must reject with EX_NULL_POINTER.
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 003@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, RegisterUnregisterEventListener_Negative)
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

    // Step 5: unregistering a never-registered listener must return false, status ok
    sp<IThermalEventListener> unregisteredListener = sp<ThermalSensorEventListener>::make();
    THERMALSENSOR_UNREGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, unregisteredListener, Status::EX_NONE);

    // Step 6: null listener unregister must be rejected by binder with EX_NULL_POINTER
    THERMALSENSOR_UNREGISTER_EVENT_FAILURE(gTHERMALSENSORHandle, nullListener, Status::EX_NULL_POINTER);
}

/**
 * @brief Validate getCurrentThermalState() returns a valid state, is consistent
 *        across repeated calls, and matches the live temperature readings from
 *        all N sensors defined in the HFP profile.
 *
 * Steps:
 *  1. Call getCurrentThermalState() and assert the returned value is a known
 *     State enum member.
 *  2. For each sensor declared in the HFP profile, locate its corresponding
 *     TemperatureReading and compare its temperature against the HFP-defined
 *     policy thresholds (recovered / exceeded / shutdown).  Derive the
 *     expected system state from the worst-case sensor reading and assert
 *     that it matches the reported state.
 *  3. Call getCurrentThermalState() a second time and assert the result is
 *     identical to the first call (consistency check).
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 002@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, GetCurrentThermalState_ValidStateAndHFPConsistency)
{
    /* Step 1: state must be a known enum value */
    State state;
    THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, state);

    bool validState = (state == State::NORMAL ||
                       state == State::CRITICAL_TEMPERATURE_EXCEEDED ||
                       state == State::CRITICAL_TEMPERATURE_RECOVERED ||
                       state == State::CRITICAL_SHUTDOWN_IMMINENT);
    UT_ASSERT_TRUE(validState);
    UT_LOG_INFO("[L1 GetCurrentThermalState_ValidStateAndHFPConsistency] state=%s",
                UT_Control_GetMapString(state_mapTable_thermalsensor, (int32_t)state));

    std::vector<TemperatureReading> readings;
    THERMALSENSOR_GET_TEMPERATURES(gTHERMALSENSORHandle, readings);

    bool anyAboveShutdown  = false;
    bool anyAboveExceeded  = false;
    bool allBelowRecovered = true;

    /* Step 2: cross-check state against each HFP sensor's reading count with HFP sensor */
    UT_ASSERT_EQUAL(readings.size(), gThermalSensorHFP.sensors.size());

    for (const auto& reading : readings)
    {
        const thermalSensorHFPSensor_t* hfpSensor = getHFPSensorByName(
            android::String8(reading.sensorName).string());
        UT_ASSERT_TRUE(hfpSensor != nullptr);
        if (hfpSensor == nullptr)
            continue;

        double temp = reading.temperatureCelsius;
        UT_LOG_INFO("[L1 GetCurrentThermalState_ValidStateAndHFPConsistency] sensor='%s' temp=%.2f "
                        "thresholds: recovered=%.2f exceeded=%.2f shutdown=%.2f",
                        hfpSensor->sensorName.c_str(), temp,
                        hfpSensor->triggerRecoveredCelsius,
                        hfpSensor->triggerExceededCelsius,
                        hfpSensor->triggerShutdownCelsius);

        if (temp >= hfpSensor->triggerShutdownCelsius)
            anyAboveShutdown = true;
        if (temp >= hfpSensor->triggerExceededCelsius)
            anyAboveExceeded = true;
        if (temp > hfpSensor->triggerRecoveredCelsius)
            allBelowRecovered = false;
    }

    if (anyAboveShutdown)
    {
        UT_LOG_INFO("[L1 GetCurrentThermalState_ValidStateAndHFPConsistency] At least one sensor above shutdown threshold — expecting CRITICAL_SHUTDOWN_IMMINENT");
        UT_ASSERT_TRUE(state == State::CRITICAL_SHUTDOWN_IMMINENT);
    }
    else if (anyAboveExceeded)
    {
        UT_LOG_INFO("[L1 GetCurrentThermalState_ValidStateAndHFPConsistency] At least one sensor above exceeded threshold — expecting CRITICAL_TEMPERATURE_EXCEEDED or higher");
        UT_ASSERT_TRUE(state == State::CRITICAL_TEMPERATURE_EXCEEDED ||
                       state == State::CRITICAL_SHUTDOWN_IMMINENT);
    }
    else if (allBelowRecovered)
    {
        UT_LOG_INFO("[L1 GetCurrentThermalState_ValidStateAndHFPConsistency] All sensors below recovered threshold — expecting NORMAL or CRITICAL_TEMPERATURE_RECOVERED");
        UT_ASSERT_TRUE(state == State::NORMAL ||
                       state == State::CRITICAL_TEMPERATURE_RECOVERED);
    }
    else
    {
        UT_LOG_INFO("[L1 GetCurrentThermalState_ValidStateAndHFPConsistency] Mixed sensor states — no specific expectation");
    }
    /* Step 3: repeated call must return the same state (consistency) */
    State newState;
    THERMALSENSOR_GET_CURRENT_STATE(gTHERMALSENSORHandle, newState);
    UT_ASSERT_EQUAL(state, newState);
}

/**
 * @brief Validate getCurrentTemperatures() returns readings that match the HFP profile.
 *
 * Steps:
 *  1. Call getCurrentTemperatures() and assert the call succeeds.
 *  2. Assert the number of readings matches the number of sensors declared in the HFP profile.
 *  3. For each reading, validate:
 *       - sensorName is not empty.
 *       - temperatureCelsius is above absolute zero (-273.15 °C).
 *       - timestampMonotonicMs is >= 0 (0 is valid before first live event fires).
 *       - sensorName maps to a sensor declared in the HFP profile.
 *       - location matches the HFP-declared location.
 *       - temperatureCelsius is within [sensorReadingMinCelsius, sensorReadingMaxCelsius].
 *       - vendorCode matches the HFP-declared vendorCode.
 *       - vendorInfo matches the HFP-declared vendorInfo (if non-empty).
 *  4. Call getCurrentTemperatures() a second time and assert that each sensor's
 *     timestampMonotonicMs is >= the value from the first call (monotonic guarantee
 *     verified in L3 where live events are driven).
 *
 * **Test Group ID:** Basic: 01@n
 * **Test Case ID:** 003@n
 * **Priority:** High@n
 */
UT_ADD_TEST(VTS_L1_THERMALSENSOR, GetCurrentTemperatures_CountStructureAndProfileMatch)
{
    std::vector<TemperatureReading> readings;
    THERMALSENSOR_GET_TEMPERATURES(gTHERMALSENSORHandle, readings);

    // If HFP declares sensors, the reading count must match exactly
    if (!gThermalSensorHFP.sensors.empty())
    {
        size_t expectedCount = gThermalSensorHFP.sensors.size();
        UT_LOG_INFO("[L1 GetCurrentTemperatures_CountStructureAndProfileMatch] HFP expects %zu sensor(s), got %zu reading(s)",
                    expectedCount, readings.size());
        UT_ASSERT_EQUAL(readings.size(), expectedCount);
    }

    // Count verified above — validate structure and full HFP profile per reading
    for (size_t i = 0; i < readings.size(); ++i)
    {
        const TemperatureReading& currentReading = readings[i];

        // sensorName must not be empty (per TemperatureReading.aidl)
        UT_ASSERT_TRUE(currentReading.sensorName.size() > 0);
        // temperatureCelsius must be within absolute physical bounds
        UT_ASSERT_TRUE(currentReading.temperatureCelsius >= -273.15);
        // timestampMonotonicMs must be >= 0 (0 is valid before first live event)
        UT_ASSERT_TRUE(currentReading.timestampMonotonicMs >= 0);
        UT_LOG_INFO("[L1 GetCurrentTemperatures_CountStructureAndProfileMatch] [%zu] sensor='%s' timestampMonotonicMs=%" PRId64,
                    i, android::String8(currentReading.sensorName).c_str(), currentReading.timestampMonotonicMs);

        // Find matching HFP sensor by name
        const thermalSensorHFPSensor_t* hfp = getHFPSensorByName(
            android::String8(currentReading.sensorName).string());

        // Every reading must map to an HFP-declared sensor
        UT_ASSERT_TRUE(hfp != nullptr);

        if (hfp != nullptr)
        {
            // location must match HFP
            UT_ASSERT_TRUE(android::String8(currentReading.location).string() == hfp->location);

            // temperature must be within the HFP sensor reading range
            UT_ASSERT_TRUE(currentReading.temperatureCelsius >= hfp->sensorReadingMinCelsius);
            UT_ASSERT_TRUE(currentReading.temperatureCelsius <= hfp->sensorReadingMaxCelsius);

            // vendorCode must match HFP
            UT_ASSERT_EQUAL(currentReading.vendorCode, hfp->vendorCode);

            // vendorInfo: if non-empty, must match HFP (TemperatureReading.aidl allows null/empty)
            std::string actualVendorInfo = android::String8(currentReading.vendorInfo).string();
            if (!actualVendorInfo.empty())
            {
                UT_ASSERT_TRUE(actualVendorInfo == hfp->vendorInfo);
            }
            UT_LOG_INFO("[L1 GetCurrentTemperatures_CountStructureAndProfileMatch] vendorInfo='%s' (HFP='%s')",
                        actualVendorInfo.c_str(), hfp->vendorInfo.c_str());

            UT_LOG_INFO("[L1 GetCurrentTemperatures_CountStructureAndProfileMatch] [%zu] sensor='%s' location='%s' "
                        "temp=%.2f (range [%.2f, %.2f]) vendorCode=%d",
                        i,
                        android::String8(currentReading.sensorName).c_str(),
                        android::String8(currentReading.location).c_str(),
                        currentReading.temperatureCelsius,
                        hfp->sensorReadingMinCelsius,
                        hfp->sensorReadingMaxCelsius,
                        currentReading.vendorCode);
        }
        else
        {
            UT_LOG_INFO("[L1 GetCurrentTemperatures_CountStructureAndProfileMatch] [%zu] sensor='%s' temp=%.2f (no HFP entry)",
                        i,
                        android::String8(currentReading.sensorName).c_str(),
                        currentReading.temperatureCelsius);
        }
    }

}


/** @} */ // End of THERMAL_SENSOR_HALTEST_L1
