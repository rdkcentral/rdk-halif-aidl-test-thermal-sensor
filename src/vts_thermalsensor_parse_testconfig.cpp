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

UT_status_t vts_thermalsensor_parse_l3_config(
    const char *configFile,
    const char *module,
    const char *testGroup,
    const char *testName,
    thermalSensorL3Config_t& config)
{
    ut_kvp_instance_t* kvp = (ut_kvp_instance_t*)kvp_create_instance(configFile);
    if (!kvp)
        return UT_STATUS_FAILURE;

    /* Shared config lives at module.testGroup (e.g. thermalsensor.L3).
     * testName is kept only for log messages. */
    const std::string node = std::string(module) + "." + testGroup;

    config.eventTimeoutMs      = (int32_t)ut_kvp_getUInt32Field(kvp, (node + ".event_timeout_ms").c_str());
    config.suppressionTimeoutMs = (int32_t)ut_kvp_getUInt32Field(kvp, (node + ".suppression_timeout_ms").c_str());
    if (config.suppressionTimeoutMs <= 0) config.suppressionTimeoutMs = 5000;

    /* Read the ordered transition_states list */
    const std::string listKey = node + ".transition_states";
    uint32_t count = ut_kvp_getListCount(kvp, listKey.c_str());
    config.transitionStates.clear();
    config.transitionStates.reserve(count);

    char stateBuf[64];
    for (uint32_t i = 0; i < count; ++i)
    {
        const std::string itemKey = listKey + "." + std::to_string(i);
        stateBuf[0] = '\0';
        ut_kvp_getStringField(kvp, itemKey.c_str(), stateBuf, sizeof(stateBuf));
        config.transitionStates.emplace_back(stateBuf);
    }

    UT_LOG_INFO("[THERMALSENSOR_TESTCONFIG] %s: timeoutMs=%d suppressionMs=%d transitions=%u [%s]",
                testName, config.eventTimeoutMs, config.suppressionTimeoutMs, count,
                [&]() -> std::string {
                    std::string s;
                    for (const auto& st : config.transitionStates)
                        s += (s.empty() ? "" : "->") + st;
                    return s;
                }().c_str());

    kvp_destroy_instance(kvp);
    return UT_STATUS_OK;
}
