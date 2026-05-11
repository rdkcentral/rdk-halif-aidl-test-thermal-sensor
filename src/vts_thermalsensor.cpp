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
 * @file vts_thermalsensor.cpp
 */

#include "vts_thermalsensor.h"
#include "vts_thermalsensor_helper_functions.h"

using namespace std;
using namespace std::chrono;
using namespace com::rdk::hal::sensor::thermal;

/* ========================================================================== */
/*  ThermalSensorEventListener Implementations                                */
/* ========================================================================== */

android::binder::Status ThermalSensorEventListener::onThermalStateChange(
    const ::com::rdk::hal::sensor::thermal::ActionEvent& event)
{
    UT_LOG_INFO("ThermalSensorEventListener::onThermalStateChange called with state:[%s]",
                UT_Control_GetMapString(state_mapTable_thermalsensor, (int32_t)event.state));
    {
        lock_guard<mutex> lock(mEventMutex);
        mEventQueue.push(event);
    }
    mEventCv.notify_all();
    return android::binder::Status::ok();
}

bool ThermalSensorEventListener::getNextEvent(ActionEvent& outEvent)
{
    UT_LOG_INFO("ThermalSensorEventListener::getNextEvent (no-wait) called");
    lock_guard<mutex> lock(mEventMutex);
    if (mEventQueue.empty())
    {
        UT_LOG_INFO("ThermalSensorEventListener::getNextEvent - queue empty, returning false");
        return false;
    }
    outEvent = mEventQueue.front();
    mEventQueue.pop();
    UT_LOG_INFO("ThermalSensorEventListener::getNextEvent - returned state:[%s]",
                UT_Control_GetMapString(state_mapTable_thermalsensor, (int32_t)outEvent.state));
    return true;
}

bool ThermalSensorEventListener::getNextEvent(ActionEvent& outEvent, chrono::milliseconds timeout)
{
    UT_LOG_INFO("ThermalSensorEventListener::getNextEvent (timeout=%lldms) called",
                static_cast<long long>(timeout.count()));
    unique_lock<mutex> lock(mEventMutex);
    bool got = mEventCv.wait_for(lock, timeout, [this]{ return !mEventQueue.empty(); });
    if (!got)
    {
        UT_LOG_INFO("ThermalSensorEventListener::getNextEvent - timed out after %lldms",
                    static_cast<long long>(timeout.count()));
        return false;
    }
    outEvent = mEventQueue.front();
    mEventQueue.pop();
    UT_LOG_INFO("ThermalSensorEventListener::getNextEvent - returned state:[%s]",
                UT_Control_GetMapString(state_mapTable_thermalsensor, (int32_t)outEvent.state));
    return true;
}

bool ThermalSensorEventListener::hasEvent()
{
    lock_guard<mutex> lock(mEventMutex);
    UT_LOG_INFO("ThermalSensorEventListener::hasEvent returning [%s]",
                mEventQueue.empty() ? "false" : "true");
    return !mEventQueue.empty();
}

void ThermalSensorEventListener::resetEventQueue()
{
    UT_LOG_INFO("ThermalSensorEventListener::resetEventQueue called");
    lock_guard<mutex> lock(mEventMutex);
    queue<ActionEvent> empty;
    swap(mEventQueue, empty);
}
