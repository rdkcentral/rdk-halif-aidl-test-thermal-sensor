# MOTION SENSOR High Level Test Specification Document
 
## Table of Contents
 
- [Acronyms, Terms and Abbreviations](#acronyms-terms-and-abbreviations)
- [Introduction](#introduction)
- [Module Description](#module-description)
- [L2 Test Specification](#l2-test-specification)
- [L3 Test Specification](#l3-test-specification)
 
---
 
## Acronyms, Terms and Abbreviations
 
- `HAL`    - Hardware Abstraction Layer
- `VTS`    - Vendor Test Suite
- `L1`     - Level 1 Testing
- `L2`     - Level 2 Testing
- `L3`     - Level 3 Testing
- `DUT`    - Device Under Test
- `RAFT`   - Rapid Automation Framework for Testing
- `YAML`   - YAML Ain't Markup Language
- `AIDL`   - Android Interface Definition Language
 
---
 
## Introduction
 
The Motion Sensor HAL provides a unified interface for motion detection sensors such as PIR and radar devices.
 
It enables:
- Discovery of available motion sensors
- Lifecycle management (start/stop)
- Configuration of sensitivity and timing behavior
- Event-driven motion/no-motion reporting
- Scheduling via active time windows
- Optional deep sleep autonomous operation
 
The HAL follows a **single-owner lifecycle model**, where one client controls the sensor lifecycle while multiple clients may subscribe to events.
 
---
 
## Module Description
 
The Motion Sensor HAL consists of:
 
### Key Interfaces
 
- **IMotionSensorManager**
  - getMotionSensorIds()
  - getMotionSensor()
 
- **IMotionSensor**
  - start(), stop()
  - getState()
  - getCapabilities()
  - set/getSensitivity()
  - set/getActiveWindows(), clearActiveWindows()
  - set/isAutonomousDuringDeepSleepEnabled()
  - registerEventListener(), unregisterEventListener()
 
- **IMotionSensorEventListener**
  - onEvent()
 
---
 
### Lifecycle Behavior
 
State transitions:
- STOPPED → STARTING → STARTED
- STARTED → STOPPING → STOPPED
- ANY → ERROR
Constraints:
- start() valid only in STOPPED
- stop() valid in STARTED
- configuration APIs require STOPPED state
 
---
 
### Operational Behavior
 
- Modes:
  - MOTION → event on motion detection
  - NO_MOTION → event after inactivity duration
 
- Timing Controls:
  - activeStartSeconds → delayed activation
  - activeStopSeconds → automatic stop
  - noMotionSeconds → inactivity threshold
 
- Active Windows:
  - Define time-based event suppression
  - Support wrap-around (midnight crossing)
  - Union of windows defines active periods
 
- Capabilities:
  - Immutable per sensor instance
  - Derived from YAML configuration
 
---
 
## L2 Test Specification
 
| Test | Description | HAL APIs |
|------|------------|----------|
| Verify lifecycle | Start sensor and verify:<br/>• Initial transition **STOPPED → STARTING →  STARTED** when monitoring begins<br/>• While monitoring is active, `getOperationalMode()` succeeds and returns the configured mode<br/>• Monitoring eventually stops call `stop()` call)<br/>• Sensor reports **STARTED → STOPPING → STOPPED** state once monitoring is inactive<br/><br/>No timing assertions are made; only eventual state correctness is validated.  | `start()`, `getState()`, `getOperationalMode()`, `stop()`, `getState()` |
| Verify configuration methods| Validate configuration APIs are allowed only in STOPPED and rejected otherwise | setSensitivity(), setActiveWindows(), setAutonomousDuringDeepSleep() |
 
---
 
## L3 Test Specification
 
| # | Test | Description | HAL APIs used |
|---|------|------------|---------------|
| 1 | **Motion Event, Timing, Accuracy & Listener Validation** | Start sensor in **MOTION** mode and verify:<br/>• Events are delivered only after successful listener registration<br/>• No events before `activeStartSeconds` activation delay<br/>• Motion events are generated upon motion detection<br/>• Event type correctness (`OperationalMode.MOTION`)<br/>• Event delivery latency is within acceptable bounds<br/>• Events stop after `activeStopSeconds` auto-stop (if configured)<br/>• No events are received after listener unregistration<br/>• No spurious or stale events during normal operation or after stop | `start()`, `getState()`, `registerEventListener()`, `unregisterEventListener()`, `onEvent()` |
| 2 | **No-Motion Event, Timing, Accuracy & Listener Validation** | Start sensor in **NO_MOTION** mode and verify:<br/>• Event is generated only after `noMotionSeconds` of continuous inactivity<br/>• No events before `activeStartSeconds` activation delay<br/>• Event type correctness (`OperationalMode.NO_MOTION`)<br/>• Event delivery latency after inactivity window is acceptable<br/>• Events stop after `activeStopSeconds` auto-stop (if configured)<br/>• No events are received after listener unregistration<br/>• No erroneous events during state transitions or stop | `start()`, `getState()`, `registerEventListener()`, `unregisterEventListener()`, `onEvent()` |
| 3 | **Active Window Behavior** | Validate that active windows gate **event emission only**, while monitoring lifecycle is governed solely by `start()` timing controls, verifying that:<br/><br/>• Motion events are emitted **only when monitoring is active AND current time falls within a configured active window**<br/>• If `(startTime + activeStartSeconds)` occurs **before** a window starts, events are suppressed until the window opens<br/>• If `(startTime + activeStartSeconds)` occurs **within** a window, events are emitted starting at activation time<br/>• If `(startTime + activeStartSeconds)` occurs **after** a window ends, the window is skipped and no events are emitted<br/>• If `activeStopSeconds` causes monitoring to stop while a window is still active, event emission stops at auto-stop time<br/>• Events outside configured windows are suppressed even if monitoring is active<br/>• Windows that wrap across midnight (endTime < startTime) are handled correctly<br/>• Multiple or overlapping windows are treated as a **union** of active periods | `setActiveWindows()`, `start()`, `onEvent()` |
| 4 | **Deep Sleep Autonomy** | Validate motion detection and event behavior during simulated deep sleep when autonomy is enabled versus disabled | `setAutonomousDuringDeepSleep()` |
 
---
