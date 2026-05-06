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
 * @file vts_thermalsensor_parse_testconfig.cpp
 */

#include "vts_thermalsensor_parse_testconfig.h"
#include <ut_kvp_profile.h>
#include <ut_log.h>
#include <string>
#include <cstring>

static void* kvp_create_instance(const char *fileName)
{
    ut_kvp_instance_t* instance = ut_kvp_createInstance();
    if (!instance)
    {
        UT_LOG_ERROR("[THERMALSENSOR_TESTCONFIG] Failed to create kvp instance");
        return nullptr;
    }
    if (ut_kvp_open(instance, const_cast<char*>(fileName)) != UT_KVP_STATUS_SUCCESS)
    {
        UT_LOG_ERROR("[THERMALSENSOR_TESTCONFIG] Failed to open config file: %s", fileName);
        ut_kvp_destroyInstance(instance);
        return nullptr;
    }
    return instance;
}

static void kvp_destroy_instance(void *instance)
{
    ut_kvp_destroyInstance((ut_kvp_instance_t*)instance);
}

UT_status_t vts_thermalsensor_parse_exceedrecovery_config(
    const char *configFile,
    const char *module,
    const char *testGroup,
    const char *testName,
    thermalSensorL3ExceedRecoveryConfig_t& config)
{
    ut_kvp_instance_t* kvp = (ut_kvp_instance_t*)kvp_create_instance(configFile);
    if (!kvp)
        return UT_STATUS_FAILURE;

    const std::string node = std::string(module) + "." + testGroup + "." + testName;

    config.eventTimeoutMs            = (int32_t)ut_kvp_getUInt32Field(kvp, (node + ".event_timeout_ms").c_str());
    config.criticalExceededCelsius   = ut_kvp_getDoubleField(kvp, (node + ".critical_temperature_exceeded_celsius").c_str());
    config.criticalRecoveredCelsius  = ut_kvp_getDoubleField(kvp, (node + ".critical_temperature_recovered_celsius").c_str());
    config.overTempCelsius           = ut_kvp_getDoubleField(kvp, (node + ".over_temp_celsius").c_str());
    config.recoveryTempCelsius       = ut_kvp_getDoubleField(kvp, (node + ".recovery_temp_celsius").c_str());
    config.normalTempCelsius         = ut_kvp_getDoubleField(kvp, (node + ".normal_temp_celsius").c_str());
    ut_kvp_getStringField(kvp, (node + ".primary_sensor").c_str(),
                          config.primarySensor, sizeof(config.primarySensor));
    ut_kvp_getStringField(kvp, (node + ".secondary_sensor").c_str(),
                          config.secondarySensor, sizeof(config.secondarySensor));

    UT_LOG_INFO("[THERMALSENSOR_TESTCONFIG] %s: exceeded=%.1f recovered=%.1f "
                "overTemp=%.1f recoveryTemp=%.1f normalTemp=%.1f timeoutMs=%d "
                "primarySensor=%s secondarySensor=%s",
                testName, config.criticalExceededCelsius, config.criticalRecoveredCelsius,
                config.overTempCelsius, config.recoveryTempCelsius, config.normalTempCelsius,
                config.eventTimeoutMs, config.primarySensor, config.secondarySensor);

    kvp_destroy_instance(kvp);
    return UT_STATUS_OK;
}

UT_status_t vts_thermalsensor_parse_shutdown_config(
    const char *configFile,
    const char *module,
    const char *testGroup,
    const char *testName,
    thermalSensorL3ShutdownConfig_t& config)
{
    ut_kvp_instance_t* kvp = (ut_kvp_instance_t*)kvp_create_instance(configFile);
    if (!kvp)
        return UT_STATUS_FAILURE;

    const std::string node = std::string(module) + "." + testGroup + "." + testName;

    config.eventTimeoutMs                   = (int32_t)ut_kvp_getUInt32Field(kvp, (node + ".event_timeout_ms").c_str());
    config.suppressionTimeoutMs             = (int32_t)ut_kvp_getUInt32Field(kvp, (node + ".suppression_timeout_ms").c_str());
    config.criticalExceededCelsius          = ut_kvp_getDoubleField(kvp, (node + ".critical_temperature_exceeded_celsius").c_str());
    config.enteringCriticalShutdownCelsius  = ut_kvp_getDoubleField(kvp, (node + ".entering_critical_shutdown_celsius").c_str());
    config.overTempCelsius                  = ut_kvp_getDoubleField(kvp, (node + ".over_temp_celsius").c_str());
    config.shutdownTempCelsius              = ut_kvp_getDoubleField(kvp, (node + ".shutdown_temp_celsius").c_str());
    config.recoveryTempCelsius              = ut_kvp_getDoubleField(kvp, (node + ".recovery_temp_celsius").c_str());
    if (config.suppressionTimeoutMs <= 0) config.suppressionTimeoutMs = 5000;
    ut_kvp_getStringField(kvp, (node + ".sensorName").c_str(),
                          config.sensorName, sizeof(config.sensorName));

    UT_LOG_INFO("[THERMALSENSOR_TESTCONFIG] %s: exceeded=%.1f shutdown=%.1f "
                "overTemp=%.1f shutdownTemp=%.1f recoveryTemp=%.1f "
                "timeoutMs=%d suppressionMs=%d sensor=%s",
                testName, config.criticalExceededCelsius,
                config.enteringCriticalShutdownCelsius,
                config.overTempCelsius, config.shutdownTempCelsius,
                config.recoveryTempCelsius, config.eventTimeoutMs,
                config.suppressionTimeoutMs, config.sensorName);

    kvp_destroy_instance(kvp);
    return UT_STATUS_OK;
}
