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
 * @file vts_thermalsensor_parse_hfp.h
 */

#ifndef __VTS_THERMALSENSOR_PARSE_HFP_H__
#define __VTS_THERMALSENSOR_PARSE_HFP_H__

#include <stdint.h>
#include <stdbool.h>
#include <string>
#include <vector>

#include "com/rdk/hal/sensor/thermal/State.h"
#include <ut_control_plane.h>
#include "vts_thermalsensor_helper_functions.h"

using namespace com::rdk::hal::sensor::thermal;

/**
 * @brief Per-sensor entry parsed from the HFP profile.
 *
 * Mirrors one element of the sensor.thermal list in
 * hfp-thermalsensor-vcomponent.yaml and covers all policy sections:
 * sensor characteristics, operational range, trigger thresholds, and vendor.
 */
typedef struct thermalSensorHFPSensor {
    std::string id;                      ///< Sensor identifier (e.g. "soc_die")
    std::string sensorName;              ///< Human-readable name (e.g. "SoC Die")
    std::string location;                ///< Domain hint (e.g. "CPU", "Board")
    // sensor_reading_range_celsius
    double sensorReadingMinCelsius;      ///< Absolute measurable minimum
    double sensorReadingMaxCelsius;      ///< Absolute measurable maximum
    // operational_temperature_celsius
    double operationalMinCelsius;        ///< Safe-operating minimum
    double operationalMaxCelsius;        ///< Safe-operating maximum
    // triggers (policy thresholds — recovered < exceeded < shutdown)
    double triggerRecoveredCelsius;      ///< Below this = CRITICAL_TEMPERATURE_RECOVERED
    double triggerExceededCelsius;       ///< Above this = CRITICAL_TEMPERATURE_EXCEEDED
    double triggerShutdownCelsius;       ///< Above this (from EXCEEDED) = CRITICAL_SHUTDOWN_IMMINENT
    // vendor
    int    vendorCode;                   ///< Vendor diagnostic code
    std::string vendorInfo;              ///< Vendor info string
} thermalSensorHFPSensor_t;

/**
 * @brief Top-level HFP structure for the thermal sensor.
 *
 * sensor.thermal in the profile is a list; each element is one sensor.
 */
typedef struct thermalSensorHFP {
    std::vector<thermalSensorHFPSensor_t> sensors;  ///< All sensors described in the profile
} thermalSensorHFP_t;

/**
 * @brief Parsed HFP global (populated by vts_thermalsensor_parse_hfp).
 */
extern thermalSensorHFP_t gThermalSensorHFP;

/**
 * @brief Parse the hardware feature profile YAML for the thermal sensor.
 *
 * Populates gThermalSensorHFP from the profile loaded into the global
 * ut_kvp_profile instance.
 */
void vts_thermalsensor_parse_hfp(void);

#endif // __VTS_THERMALSENSOR_PARSE_HFP_H__
