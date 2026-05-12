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
 * @file vts_thermalsensor_helper_functions.cpp
 */

#include "vts_thermalsensor_helper_functions.h"
#include <binder/Status.h>
#include "com/rdk/hal/sensor/thermal/State.h"

using android::binder::Status;

/* State map table */
const ut_control_keyStringMapping_t state_mapTable_thermalsensor[] = {
    { "NORMAL",                         (int32_t)com::rdk::hal::sensor::thermal::State::NORMAL },
    { "CRITICAL_TEMPERATURE_EXCEEDED",  (int32_t)com::rdk::hal::sensor::thermal::State::CRITICAL_TEMPERATURE_EXCEEDED },
    { "CRITICAL_TEMPERATURE_RECOVERED", (int32_t)com::rdk::hal::sensor::thermal::State::CRITICAL_TEMPERATURE_RECOVERED },
    { "CRITICAL_SHUTDOWN_IMMINENT",     (int32_t)com::rdk::hal::sensor::thermal::State::CRITICAL_SHUTDOWN_IMMINENT },
    { nullptr, -1 }
};

/* Exception code map table */
ut_control_keyStringMapping_t statusThermalSensorException_mapTable[] = {
    { "EX_NONE",                Status::EX_NONE },
    { "EX_SECURITY",            Status::EX_SECURITY },
    { "EX_BAD_PARCELABLE",      Status::EX_BAD_PARCELABLE },
    { "EX_ILLEGAL_ARGUMENT",    Status::EX_ILLEGAL_ARGUMENT },
    { "EX_NULL_POINTER",        Status::EX_NULL_POINTER },
    { "EX_ILLEGAL_STATE",       Status::EX_ILLEGAL_STATE },
    { "EX_NETWORK_MAIN_THREAD", Status::EX_NETWORK_MAIN_THREAD },
    { "EX_UNSUPPORTED_OPERATION", Status::EX_UNSUPPORTED_OPERATION },
    { "EX_SERVICE_SPECIFIC",    Status::EX_SERVICE_SPECIFIC },
    { "EX_PARCELABLE",          Status::EX_PARCELABLE },
    { nullptr, -1 }
};

/* Boolean map table */
ut_control_keyStringMapping_t bool_mapTable_thermalsensor[] = {
    { "false", 0 },
    { "true",  1 },
    { nullptr, -1 }
};
