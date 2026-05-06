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
 * @file vts_thermalsensor_parse_testconfig.h
 */

#ifndef __VTS_THERMALSENSOR_PARSE_TESTCONFIG_H__
#define __VTS_THERMALSENSOR_PARSE_TESTCONFIG_H__

#include <stdint.h>
#include <stdbool.h>
#include <string>
#include <ut.h>

#define VTS_THERMALSENSOR_TEST_CONFIG_PATH "./test_configuration/vts_l3_ThermalSensor.yaml"

/**
 * @brief L3 config for Test Case 1: Exceed-and-recovery lifecycle.
 *
 * NORMAL → EXCEEDED → RECOVERED → NORMAL
 * Validates: event delivery, monotonic timestamps, multi-sensor aggregation,
 * getCurrentThermalState() consistency, callbacks only on state changes.
 */
typedef struct thermalSensorL3ExceedRecoveryConfig
{
    int32_t  eventTimeoutMs;                /**< Max ms to wait for each expected event */
    double   criticalExceededCelsius;       /**< Threshold above which EXCEEDED fires */
    double   criticalRecoveredCelsius;      /**< Threshold below which RECOVERED fires (hysteresis) */
    double   overTempCelsius;               /**< Injection temp to trigger EXCEEDED (> criticalExceeded) */
    double   recoveryTempCelsius;           /**< Injection temp to trigger RECOVERED (<= criticalRecovered) */
    double   normalTempCelsius;             /**< Injection temp to confirm NORMAL after recovery */
    char     primarySensor[128];            /**< Primary sensor name to inject temperature on */
    char     secondarySensor[128];          /**< Secondary sensor name (must stay in safe range) */
} thermalSensorL3ExceedRecoveryConfig_t;

/**
 * @brief L3 config for Test Case 2: Critical shutdown with recovery suppression.
 *
 * NORMAL → EXCEEDED → CRITICAL_SHUTDOWN_IMMINENT; no recovery or normal after shutdown.
 */
typedef struct thermalSensorL3ShutdownConfig
{
    int32_t  eventTimeoutMs;                /**< Max ms to wait for each expected event */
    double   criticalExceededCelsius;       /**< Threshold for EXCEEDED */
    double   enteringCriticalShutdownCelsius; /**< Threshold for SHUTDOWN_IMMINENT */
    double   overTempCelsius;               /**< Injection temp to trigger EXCEEDED */
    double   shutdownTempCelsius;           /**< Injection temp to trigger SHUTDOWN_IMMINENT */
    double   recoveryTempCelsius;           /**< Injection temp that must be suppressed after shutdown */
    int32_t  suppressionTimeoutMs;          /**< ms to wait confirming no recovery event arrives */
    char     sensorName[128];               /**< Sensor name */
} thermalSensorL3ShutdownConfig_t;

/**
 * @brief Parse exceed-and-recovery lifecycle config (Test Case 1).
 */
UT_status_t vts_thermalsensor_parse_exceedrecovery_config(
    const char *configFile,
    const char *module,
    const char *testGroup,
    const char *testName,
    thermalSensorL3ExceedRecoveryConfig_t& config);

/**
 * @brief Parse critical-shutdown config (Test Case 2).
 */
UT_status_t vts_thermalsensor_parse_shutdown_config(
    const char *configFile,
    const char *module,
    const char *testGroup,
    const char *testName,
    thermalSensorL3ShutdownConfig_t& config);

#endif // __VTS_THERMALSENSOR_PARSE_TESTCONFIG_H__
