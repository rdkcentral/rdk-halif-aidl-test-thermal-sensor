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
 * @file vts_thermalsensor_init.cpp
 */

#include "vts_thermalsensor_init.h"
#include "vts_thermalsensor.h"
#include "vts_thermalsensor_parse_hfp.h"

#include <ut_log.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;
using android::ProcessState;
using android::IPCThreadState;

using namespace com::rdk::hal::sensor::thermal;

/** Global handle to IThermalSensor singleton service */
sp<IThermalSensor> gTHERMALSENSORHandle = nullptr;

static UT_status_t vts_thermalsensor_getService()
{
    gTHERMALSENSORHandle = android::interface_cast<IThermalSensor>(
        defaultServiceManager()->getService(String16(IThermalSensor::serviceName().c_str())));

    if (gTHERMALSENSORHandle == nullptr)
    {
        UT_LOG_ERROR("ThermalSensor service not available");
        return UT_STATUS_FAILURE;
    }
    return UT_STATUS_OK;
}

UT_status_t VTS_THERMALSENSOR_init(void)
{
    UT_LOG_INFO("VTS_THERMALSENSOR_init: Binding to IThermalSensor service");

    if (vts_thermalsensor_getService() != UT_STATUS_OK)
    {
        UT_LOG_ERROR("Failed to get ThermalSensor service");
        return UT_STATUS_FAILURE;
    }

    UT_LOG_INFO("VTS_THERMALSENSOR_init: Successfully connected to IThermalSensor");

    /* Parse the HFP profile for device capabilities */
    vts_thermalsensor_parse_hfp();

    UT_LOG_INFO("VTS_THERMALSENSOR_init: Initialization complete");
    return UT_STATUS_OK;
}

void VTS_THERMALSENSOR_destroy(void)
{
    UT_LOG_INFO("VTS_THERMALSENSOR_destroy: Releasing IThermalSensor handle");
    gTHERMALSENSORHandle = nullptr;
    UT_LOG_INFO("VTS_THERMALSENSOR_destroy: Cleanup complete");
}
