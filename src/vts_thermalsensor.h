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
 * @addtogroup HPK Hardware Porting Kit
 * @{
 */

/**
 * @addtogroup THERMAL_SENSOR Thermal Sensor Module
 * @{
 */

/**
 * @addtogroup THERMAL_SENSOR_HALTEST Thermal Sensor HAL Tests
 * @{
 */

/**
 * @defgroup THERMAL_SENSOR_HALTEST_HEADER Thermal Sensor HAL Tests Header File
 * @{
 * @parblock
 *
 * ### Common functions/macros required for the thermal sensor :
 *
 * **Pre-Conditions:**  None @n
 * **Dependencies:** None @n
 *
 * @endparblock
 */

/**
 * @file vts_thermalsensor.h
 */

#ifndef __VTS_THERMALSENSOR_H__
#define __VTS_THERMALSENSOR_H__

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>

#include <ut.h>
#include <ut_log.h>
#include <ut_kvp_profile.h>
#include <ut_control_plane.h>

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>
#include <binder/Status.h>
#include <utils/String8.h>
#include <utils/String16.h>

#include "com/rdk/hal/sensor/thermal/BnThermalEventListener.h"
#include "com/rdk/hal/sensor/thermal/IThermalSensor.h"
#include "com/rdk/hal/sensor/thermal/IThermalEventListener.h"
#include "com/rdk/hal/sensor/thermal/State.h"
#include "com/rdk/hal/sensor/thermal/ActionEvent.h"
#include "com/rdk/hal/sensor/thermal/TemperatureReading.h"

#include "vts_thermalsensor_helper_functions.h"

using android::sp;
using android::String16;
using android::defaultServiceManager;
using android::binder::Status;

#define MAX_EVENT_WAIT_MS   15000

#ifndef ENABLE_UT_INFO_LOGGING
#ifdef UT_LOG_DEBUG
#undef UT_LOG_DEBUG
#define UT_LOG_DEBUG(...)
#endif
#ifdef UT_LOG_INFO
#undef UT_LOG_INFO
#define UT_LOG_INFO(...)
#endif
#endif

/* ========================================================================== */
/*  IThermalSensor Macros                                                      */
/* ========================================================================== */

#define THERMALSENSOR_REGISTER_EVENT_SUCCESS(sensorHandle, listener) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_DEBUG("Calling registerEventListener(IN:listener[%p], OUT:_aidl_return[])", listener.get()); \
        status = (sensorHandle)->registerEventListener(listener, &_aidl_return); \
        UT_LOG_DEBUG("Result registerEventListener(IN:listener[%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            listener.get(), \
            UT_Control_GetMapString(bool_mapTable_thermalsensor, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable_thermalsensor, status.isOk()), \
            UT_Control_GetMapString(statusThermalSensorException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
        UT_ASSERT_TRUE(_aidl_return); \
    } while (0)

#define THERMALSENSOR_REGISTER_EVENT_FAILURE(sensorHandle, listener, expectedException) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_DEBUG("Calling registerEventListener(IN:listener[%p], OUT:_aidl_return[])", \
            (listener ? listener.get() : nullptr)); \
        status = (sensorHandle)->registerEventListener(listener, &_aidl_return); \
        UT_LOG_DEBUG("Result registerEventListener(IN:listener[%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            (listener ? listener.get() : nullptr), \
            UT_Control_GetMapString(bool_mapTable_thermalsensor, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable_thermalsensor, status.isOk()), \
            UT_Control_GetMapString(statusThermalSensorException_mapTable, status.exceptionCode())); \
        if (expectedException == Status::EX_NONE) \
            UT_ASSERT_TRUE(status.isOk()); \
        else \
            UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), expectedException); \
        UT_ASSERT_FALSE(_aidl_return); \
    } while (0)

#define THERMALSENSOR_UNREGISTER_EVENT_SUCCESS(sensorHandle, listener) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_DEBUG("Calling unregisterEventListener(IN:listener[%p], OUT:_aidl_return[])", listener.get()); \
        status = (sensorHandle)->unregisterEventListener(listener, &_aidl_return); \
        UT_LOG_DEBUG("Result unregisterEventListener(IN:listener[%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            listener.get(), \
            UT_Control_GetMapString(bool_mapTable_thermalsensor, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable_thermalsensor, status.isOk()), \
            UT_Control_GetMapString(statusThermalSensorException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_TRUE(_aidl_return); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define THERMALSENSOR_UNREGISTER_EVENT_FAILURE(sensorHandle, listener, expectedException) \
    do { \
        ::android::binder::Status status; \
        bool _aidl_return = false; \
        UT_LOG_DEBUG("Calling unregisterEventListener(IN:listener[%p], OUT:_aidl_return[])", \
            (listener ? listener.get() : nullptr)); \
        status = (sensorHandle)->unregisterEventListener(listener, &_aidl_return); \
        UT_LOG_DEBUG("Result unregisterEventListener(IN:listener[%p], OUT:_aidl_return[%s]), status:[isOK:%s, exception:%s]", \
            (listener ? listener.get() : nullptr), \
            UT_Control_GetMapString(bool_mapTable_thermalsensor, _aidl_return), \
            UT_Control_GetMapString(bool_mapTable_thermalsensor, status.isOk()), \
            UT_Control_GetMapString(statusThermalSensorException_mapTable, status.exceptionCode())); \
        if (expectedException == Status::EX_NONE) \
            UT_ASSERT_TRUE(status.isOk()); \
        else \
            UT_ASSERT_FALSE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), expectedException); \
        UT_ASSERT_FALSE(_aidl_return); \
    } while (0)

#define THERMALSENSOR_GET_CURRENT_STATE(sensorHandle, state) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getCurrentThermalState(OUT:state[])"); \
        status = (sensorHandle)->getCurrentThermalState(&state); \
        UT_LOG_DEBUG("Result getCurrentThermalState(OUT:state[%s]), status:[isOK:%s, exception:%s]", \
            UT_Control_GetMapString(state_mapTable_thermalsensor, (int32_t)state), \
            UT_Control_GetMapString(bool_mapTable_thermalsensor, status.isOk()), \
            UT_Control_GetMapString(statusThermalSensorException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

#define THERMALSENSOR_GET_TEMPERATURES(sensorHandle, readings) \
    do { \
        ::android::binder::Status status; \
        UT_LOG_DEBUG("Calling getCurrentTemperatures(OUT:readings[])"); \
        status = (sensorHandle)->getCurrentTemperatures(&readings); \
        UT_LOG_DEBUG("Result getCurrentTemperatures(OUT:readings[count:%zu]), status:[isOK:%s, exception:%s]", \
            readings.size(), \
            UT_Control_GetMapString(bool_mapTable_thermalsensor, status.isOk()), \
            UT_Control_GetMapString(statusThermalSensorException_mapTable, status.exceptionCode())); \
        UT_ASSERT_TRUE(status.isOk()); \
        UT_ASSERT_EQUAL(status.exceptionCode(), Status::EX_NONE); \
    } while (0)

/* ========================================================================== */
/*  Listener Class                                                             */
/* ========================================================================== */

/**
 * @brief Test listener implementing BnThermalEventListener.
 *
 * Queues received ActionEvents so that test code can inspect them
 * with or without a timeout.
 */
class ThermalSensorEventListener : public com::rdk::hal::sensor::thermal::BnThermalEventListener
{
public:
    ThermalSensorEventListener() {}
    virtual ~ThermalSensorEventListener() = default;

    android::binder::Status onThermalStateChange(
        const ::com::rdk::hal::sensor::thermal::ActionEvent& event) override;

    bool getNextEvent(com::rdk::hal::sensor::thermal::ActionEvent& outEvent);
    bool getNextEvent(com::rdk::hal::sensor::thermal::ActionEvent& outEvent, std::chrono::milliseconds timeout);
    bool hasEvent();
    void resetEventQueue();

private:
    std::mutex mEventMutex;
    std::condition_variable mEventCv;
    std::queue<com::rdk::hal::sensor::thermal::ActionEvent> mEventQueue;
};

#endif // __VTS_THERMALSENSOR_H__

/** @} */ // End of THERMAL_SENSOR_HALTEST_HEADER
/** @} */ // End of THERMAL_SENSOR_HALTEST
/** @} */ // End of THERMAL_SENSOR Module
/** @} */ // End of HPK
