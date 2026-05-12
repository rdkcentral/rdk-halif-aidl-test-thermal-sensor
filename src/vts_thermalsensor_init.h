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
 * @file vts_thermalsensor_init.h
 */

#ifndef __VTS_THERMALSENSOR_INIT_H__
#define __VTS_THERMALSENSOR_INIT_H__

/**
 * @brief Initialize the Thermal Sensor VTS test suite.
 *
 * Binds to the IThermalSensor binder service and parses the HFP profile.
 */
void VTS_THERMALSENSOR_init(void);

/**
 * @brief Destroy/cleanup the Thermal Sensor VTS test suite.
 *
 * Releases the IThermalSensor handle.
 */
void VTS_THERMALSENSOR_destroy(void);

#endif // __VTS_THERMALSENSOR_INIT_H__
