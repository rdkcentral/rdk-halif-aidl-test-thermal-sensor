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
 * @defgroup THERMAL_SENSOR_HALTEST_MAIN Thermal Sensor HAL Tests Main File
 * @{
 * @parblock
 *
 * ### Tests for Thermal Sensor HAL :
 *
 * This is to ensure that the API meets the operational requirements of the
 * module across all vendors.
 *
 * **Pre-Conditions:** Thermal Sensor HAL service must be running @n
 * **Dependencies:** None @n
 *
 * @endparblock
 */

/**
 * @file main.cpp
 */

#include <ut.h>
#include <ut_log.h>
#include "vts_thermalsensor_init.h"

int main(int argc, char **argv)
{
    UT_status_t result;

    UT_init(argc, argv);

    UT_LOG_INFO("==========================================================================");
    UT_LOG_INFO("\t BUILD_TIMESTAMP: \033[0;32m%s\033[0m", BUILD_TIMESTAMP);
    UT_LOG_INFO("\t UNIFIED VTS Version: \033[0;32m%s\033[0m", HALIF_TEST_PROJECT_VERSION);
    UT_LOG_INFO("\t AIDL HALIF Version: \033[0;32m%s\033[0m", RDK_HALIF_AIDL_VERSION);
    UT_LOG_INFO("\t VTS THERMAL SENSOR HALIF Test Version: \033[0;32m%s\033[0m", THERMALSENSOR_VERSION);
    UT_LOG_INFO("==========================================================================");

    result = VTS_THERMALSENSOR_init();
    if (result != UT_STATUS_OK)
    {
        UT_LOG_ERROR("VTS_THERMALSENSOR_init failed. Aborting test execution.");
        return result;
    }

    result = UT_run_tests();

    VTS_THERMALSENSOR_destroy();
    return result;
}

/** @} */ // End of THERMAL_SENSOR_HALTEST_MAIN
