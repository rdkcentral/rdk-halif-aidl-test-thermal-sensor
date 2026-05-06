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
 * @file vts_thermalsensor_helper_functions.h
 */

#ifndef __VTS_THERMALSENSOR_HELPER_FUNCTIONS_H__
#define __VTS_THERMALSENSOR_HELPER_FUNCTIONS_H__

#include <string>
#include <vector>
#include <ut_control_plane.h>

/** Exception code map table for thermal sensor status */
extern ut_control_keyStringMapping_t statusThermalSensorException_mapTable[];

/** Boolean string map table */
extern ut_control_keyStringMapping_t bool_mapTable_thermalsensor[];

/** State string map table */
extern const ut_control_keyStringMapping_t state_mapTable_thermalsensor[];

#endif // __VTS_THERMALSENSOR_HELPER_FUNCTIONS_H__
