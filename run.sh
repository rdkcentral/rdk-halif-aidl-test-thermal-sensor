#!/bin/bash
#* ******************************************************************************
#*  If not stated otherwise in this file or this component's LICENSE
#*  file the following copyright and licenses apply:
#*
#*  Copyright 2026 RDK Management
#*
#*  Licensed under the Apache License, Version 2.0 (the License);
#*  you may not use this file except in compliance with the License.
#*  You may obtain a copy of the License at
#*
#*  http://www.apache.org/licenses/LICENSE-2.0
#*
#*  Unless required by applicable law or agreed to in writing, software
#*  distributed under the License is distributed on an AS IS BASIS,
#*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#*  See the License for the specific language governing permissions and
#*  limitations under the License.
#*
#* ******************************************************************************

RUN_DIR=$(realpath "$(dirname "$0")")
SERVICE_NAME=Vendor-ThermalSensor

export LD_LIBRARY_PATH=${RUN_DIR}:${LD_LIBRARY_PATH}

# Function to check if a process is running
check_process()
{
    local process_name=$1
    pgrep -f "$process_name" > /dev/null
    return $?
}

start_vcomponent_service()
{
    ${RUN_DIR}/$SERVICE_NAME &
    vcomponentservice_pid=$!
    # Wait for service to be ready
    sleep 1

    echo "Started vComponent ${SERVICE_NAME} (PID: $vcomponentservice_pid)"
}

if check_process $SERVICE_NAME; then
    echo "Continuing with existing ${SERVICE_NAME} (PID: $(pgrep -f $SERVICE_NAME))"
else
    start_vcomponent_service
fi

echo "Executing [vts_thermalsensor_hal_test]"
"${RUN_DIR}/vts_thermalsensor_hal_test" "$@"
