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
 * @file vts_thermalsensor_parse_hfp.cpp
 */

#include "vts_thermalsensor_parse_hfp.h"
#include "vts_thermalsensor_helper_functions.h"
#include <ut_kvp_profile.h>
#include <ut_log.h>
#include <string>

/* Global HFP structure */
thermalSensorHFP_t gThermalSensorHFP;

void vts_thermalsensor_parse_hfp(void)
{
    char subValue[UT_KVP_MAX_ELEMENT_SIZE];

    gThermalSensorHFP = thermalSensorHFP_t{};

    /* sensor.thermal is a YAML list — each element is one sensor */
    int numSensors = UT_KVP_PROFILE_GET_LIST_COUNT("sensor.thermal");
    UT_LOG_INFO("vts_thermalsensor_parse_hfp: found %d sensor(s)", numSensors);

    for (int i = 0; i < numSensors; ++i)
    {
        thermalSensorHFPSensor_t s{};
        const std::string base = "sensor.thermal." + std::to_string(i);

        UT_KVP_PROFILE_GET_STRING((base + ".id").c_str(), subValue);
        s.id = subValue;

        UT_KVP_PROFILE_GET_STRING((base + ".sensorName").c_str(), subValue);
        s.sensorName = subValue;

        UT_KVP_PROFILE_GET_STRING((base + ".location").c_str(), subValue);
        s.location = subValue;

        /* sensor_reading_range_celsius */
        s.sensorReadingMinCelsius = ut_kvp_getDoubleField(
            ut_kvp_profile_getInstance(),
            (base + ".sensor_reading_range_celsius.min").c_str());
        s.sensorReadingMaxCelsius = ut_kvp_getDoubleField(
            ut_kvp_profile_getInstance(),
            (base + ".sensor_reading_range_celsius.max").c_str());

        /* operational_temperature_celsius */
        s.operationalMinCelsius = ut_kvp_getDoubleField(
            ut_kvp_profile_getInstance(),
            (base + ".operational_temperature_celsius.min").c_str());
        s.operationalMaxCelsius = ut_kvp_getDoubleField(
            ut_kvp_profile_getInstance(),
            (base + ".operational_temperature_celsius.max").c_str());

        /* triggers */
        s.triggerRecoveredCelsius = ut_kvp_getDoubleField(
            ut_kvp_profile_getInstance(),
            (base + ".triggers.critical_temperature_recovered_celsius").c_str());
        s.triggerExceededCelsius = ut_kvp_getDoubleField(
            ut_kvp_profile_getInstance(),
            (base + ".triggers.critical_temperature_exceeded_celsius").c_str());
        s.triggerShutdownCelsius = ut_kvp_getDoubleField(
            ut_kvp_profile_getInstance(),
            (base + ".triggers.entering_critical_shutdown_celsius").c_str());

        /* vendor */
        s.vendorCode = static_cast<int>(ut_kvp_getUInt32Field(
            ut_kvp_profile_getInstance(),
            (base + ".vendor.vendorCode").c_str()));
        UT_KVP_PROFILE_GET_STRING((base + ".vendor.vendorInfo").c_str(), subValue);
        s.vendorInfo = subValue;

        gThermalSensorHFP.sensors.push_back(s);
        UT_LOG_INFO("vts_thermalsensor_parse_hfp: sensor[%d] id=%s name=%s loc=%s "
                    "exceeded=%.1f recovered=%.1f shutdown=%.1f vendorCode=%d",
                    i, s.id.c_str(), s.sensorName.c_str(), s.location.c_str(),
                    s.triggerExceededCelsius, s.triggerRecoveredCelsius,
                    s.triggerShutdownCelsius, s.vendorCode);
    }
}
