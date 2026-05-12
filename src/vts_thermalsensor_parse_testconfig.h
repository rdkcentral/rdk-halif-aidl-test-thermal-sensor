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
#include <vector>
#include <ut.h>

#define VTS_THERMALSENSOR_TEST_CONFIG_PATH "./test_configuration/vts_l3_ThermalSensor.yaml"

/**
 * @brief Shared L3 config for all test cases.
 *
 * The ordered transition_states list drives verification in the C++ test.
 * Each consecutive pair (states[n-1], states[n]) represents one expected
 * state-change event.  States after SHUTDOWN are suppressed — the C++ binary
 * waits suppressionTimeoutMs and asserts NO event arrives.
 *
 * Example TC1: [NORMAL, EXCEEDED, RECOVERED, NORMAL]
 * Example TC2: [NORMAL, EXCEEDED, SHUTDOWN, RECOVERED, NORMAL]
 */
typedef struct thermalSensorL3Config
{
    int32_t                  eventTimeoutMs;      /**< Max ms to wait for each expected event */
    int32_t                  suppressionTimeoutMs;/**< ms to confirm NO event after SHUTDOWN (0 = unused) */
    std::vector<std::string> transitionStates;    /**< Ordered list of states from the YAML */
} thermalSensorL3Config_t;

/**
 * @brief Parse L3 test config (shared by all test cases).
 */
UT_status_t vts_thermalsensor_parse_l3_config(
    const char *configFile,
    const char *module,
    const char *testGroup,
    const char *testName,
    thermalSensorL3Config_t& config);

#endif // __VTS_THERMALSENSOR_PARSE_TESTCONFIG_H__
