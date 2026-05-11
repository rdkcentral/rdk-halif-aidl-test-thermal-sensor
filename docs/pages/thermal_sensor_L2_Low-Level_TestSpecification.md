# Thermal Sensor L2 Low Level Test Specification

## Table of Contents

- [Overview](#overview)
- [Acronyms, Terms and Abbreviations](#acronyms-terms-and-abbreviations)
- [References](#references)
- [Level 2 Test Specification](#level-2-test-specification)

---

## Overview

This document describes the Level 2 (L2) Low-Level Test Specification for the Thermal Sensor HAL module.

L2 tests validate the `IThermalSensor` API at a functional level using the vComponent as the HAL backend. No external hardware, temperature stimulus, or control-plane injection is required. Tests verify API correctness (positive and negative paths), listener registration semantics, state reporting consistency, and telemetry structure against the HFP profile.

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
- `HFP`    - Hardware Feature Profile
- `SoC`    - System on Chip

---

## References

- High Level Test Specification — [thermal_sensor_High_Level_TestSpecification.md](thermal_sensor_High_Level_TestSpecification.md)
- Thermal Sensor AIDL Interfaces:
  - [IThermalSensor](https://github.com/rdkcentral/rdk-halif-aidl/blob/main/sensor/current/com/rdk/hal/sensor/thermal/IThermalSensor.aidl)
  - [IThermalEventListener](https://github.com/rdkcentral/rdk-halif-aidl/blob/main/sensor/current/com/rdk/hal/sensor/thermal/IThermalEventListener.aidl)
  - [State](https://github.com/rdkcentral/rdk-halif-aidl/blob/main/sensor/current/com/rdk/hal/sensor/thermal/State.aidl)
  - [ActionEvent](https://github.com/rdkcentral/rdk-halif-aidl/blob/main/sensor/current/com/rdk/hal/sensor/thermal/ActionEvent.aidl)
  - [TemperatureReading](https://github.com/rdkcentral/rdk-halif-aidl/blob/main/sensor/current/com/rdk/hal/sensor/thermal/TemperatureReading.aidl)

---

## Level 2 Test Specification

The following test cases validate the Thermal Sensor HAL module at L2. All tests use the vComponent (hardware simulation) and do not require physical temperature hardware or external thermal stimulus.

---

### Test 1

| Title | Details |
| -- | -- |
| Function Name | `RegisterUnregisterEventListener_Positive` |
| Description | This test validates the positive path for `registerEventListener()` and `unregisterEventListener()`, including re-registration after unregistration. Verifies that a valid listener registers successfully, unregisters successfully, and can be re-registered after unregistration without error. |
| Test Group | 01 |
| Test Case ID | 001 |
| Priority | High |

**Pre-Conditions:**
- Thermal Sensor HAL service must be running.
- The vComponent must be started and configured via the HFP profile.

**Dependencies:**
- HFP profile: [profiles/hfp-thermalsensor-vcomponent.yaml](../../profiles/hfp-thermalsensor-vcomponent.yaml)

**User Interaction:**
None

**Test Procedure:**

| Variation / Steps | Description | Test Data | Expected Result | Notes |
|-------------------|-------------|-----------|----------------|-------|
| 1 | Create a valid listener instance | `ThermalSensorEventListener` | Listener object created | |
| 2 | Call `registerEventListener(listener)` | Valid `IThermalEventListener` | Returns `true`; status OK | Listener registered |
| 3 | Call `unregisterEventListener(listener)` | Same listener | Returns `true`; status OK | Listener unregistered |
| 4 | Call `registerEventListener(listener)` again | Same listener | Returns `true`; status OK | Re-registration succeeds after unregistration |
| 5 | Call `unregisterEventListener(listener)` | Same listener | Returns `true`; status OK | Final cleanup succeeds |

---

### Test 2

| Title | Details |
| -- | -- |
| Function Name | `RegisterUnregisterEventListener_MultipleListener` |
| Description | This test validates that multiple concurrent listeners can be registered and unregistered independently without interference. Verifies fan-out registration semantics: N distinct listener instances can all be registered simultaneously, and each can be unregistered independently. |
| Test Group | 01 |
| Test Case ID | 002 |
| Priority | High |

**Pre-Conditions:**
- Thermal Sensor HAL service must be running.
- The vComponent must be started and configured via the HFP profile.

**Dependencies:**
- HFP profile: [profiles/hfp-thermalsensor-vcomponent.yaml](../../profiles/hfp-thermalsensor-vcomponent.yaml)

**User Interaction:**
None

**Test Procedure:**

| Variation / Steps | Description | Test Data | Expected Result | Notes |
|-------------------|-------------|-----------|----------------|-------|
| 1 | Create 10 distinct `ThermalSensorEventListener` instances | 10 separate objects | 10 listener instances created | |
| 2 | Register all 10 listeners in forward order | Each listener in sequence | Each `registerEventListener()` returns `true`; status OK | All 10 registered concurrently |
| 3 | Unregister all 10 listeners in sequence | Each listener in sequence | Each `unregisterEventListener()` returns `true`; status OK | Each unregistration independent |

---

### Test 3

| Title | Details |
| -- | -- |
| Function Name | `RegisterUnregisterEventListener_Negative` |
| Description | This test validates all negative paths for `registerEventListener()` and `unregisterEventListener()`. Verifies binder-level rejection for null listeners (`EX_NULL_POINTER`) and application-level rejection for double-registration and never-registered-unregistration (`false` return with status OK). |
| Test Group | 01 |
| Test Case ID | 003 |
| Priority | High |

**Pre-Conditions:**
- Thermal Sensor HAL service must be running.
- The vComponent must be started and configured via the HFP profile.

**Dependencies:**
- HFP profile: [profiles/hfp-thermalsensor-vcomponent.yaml](../../profiles/hfp-thermalsensor-vcomponent.yaml)

**User Interaction:**
None

**Test Procedure:**

| Variation / Steps | Description | Test Data | Expected Result | Notes |
|-------------------|-------------|-----------|----------------|-------|
| 1 | Call `registerEventListener(null)` | `null` listener | Binder rejects with `EX_NULL_POINTER` | Binder-level null check |
| 2 | Create a valid listener and call `registerEventListener(listener)` | Valid `IThermalEventListener` | Returns `true`; status OK | Baseline registration |
| 3 | Call `registerEventListener(listener)` again with the same listener | Already-registered listener | Returns `false`; status OK, no exception | Duplicate registration rejected at application layer |
| 4 | Call `unregisterEventListener(listener)` | Registered listener | Returns `true`; status OK | Successfully unregistered |
| 5 | Create a second listener that was never registered and call `unregisterEventListener()` | Unregistered listener | Returns `false`; status OK, no exception | Not-found returns false, not an error |
| 6 | Call `unregisterEventListener(null)` | `null` listener | Binder rejects with `EX_NULL_POINTER` | Binder-level null check |

---

### Test 4

| Title | Details |
| -- | -- |
| Function Name | `GetCurrentThermalState_ValidStateAndHFPConsistency` |
| Description | This test validates that `getCurrentThermalState()` returns a valid `State` enum member, is consistent across repeated calls, and is consistent with the per-sensor temperature readings and HFP-defined policy thresholds. |
| Test Group | 01 |
| Test Case ID | 004 |
| Priority | High |

**Pre-Conditions:**
- Thermal Sensor HAL service must be running.
- The vComponent must be started and configured via the HFP profile.
- The system must be in a steady thermal state (no active temperature injection) at test start.

**Dependencies:**
- HFP profile: [profiles/hfp-thermalsensor-vcomponent.yaml](../../profiles/hfp-thermalsensor-vcomponent.yaml)

**User Interaction:**
None

**Test Procedure:**

| Variation / Steps | Description | Test Data | Expected Result | Notes |
|-------------------|-------------|-----------|----------------|-------|
| 1 | Call `getCurrentThermalState()` | — | Returns a `State` value that is one of: `NORMAL`, `CRITICAL_TEMPERATURE_EXCEEDED`, `CRITICAL_TEMPERATURE_RECOVERED`, `CRITICAL_SHUTDOWN_IMMINENT` | Unknown/out-of-range enum values must not be returned |
| 2 | Call `getCurrentTemperatures()` and collect all readings | — | Returns one reading per HFP-declared sensor | Used for cross-checking |
| 3 | For each sensor reading, look up the HFP-defined thresholds (`triggerRecoveredCelsius`, `triggerExceededCelsius`, `triggerShutdownCelsius`) and derive the expected worst-case system state | Readings from step 2, thresholds from HFP | Expected state derived from worst-case sensor temperature | If any sensor ≥ shutdown → `CRITICAL_SHUTDOWN_IMMINENT`; if any ≥ exceeded → `CRITICAL_TEMPERATURE_EXCEEDED` or higher; if all < recovered → `NORMAL` or `CRITICAL_TEMPERATURE_RECOVERED` |
| 4 | Assert that the state from step 1 matches the derived expected state from step 3 | State from step 1, derived state from step 3 | States match | Cross-check between telemetry and reported state |
| 5 | Call `getCurrentThermalState()` a second time | — | Returns the same state as step 1 | Consistency — state must not flip between two calls with no stimulus |

---

### Test 5

| Title | Details |
| -- | -- |
| Function Name | `GetCurrentTemperatures_CountStructureAndProfileMatch` |
| Description | This test validates that `getCurrentTemperatures()` returns a reading array whose count, field structure, and field values match the HFP profile declarations. Every reading must be mappable to an HFP-declared sensor by `sensorName`, and all fields must be within the HFP-specified bounds. |
| Test Group | 01 |
| Test Case ID | 005 |
| Priority | High |

**Pre-Conditions:**
- Thermal Sensor HAL service must be running.
- The vComponent must be started and configured via the HFP profile.

**Dependencies:**
- HFP profile: [profiles/hfp-thermalsensor-vcomponent.yaml](../../profiles/hfp-thermalsensor-vcomponent.yaml)

**User Interaction:**
None

**Test Procedure:**

| Variation / Steps | Description | Test Data | Expected Result | Notes |
|-------------------|-------------|-----------|----------------|-------|
| 1 | Call `getCurrentTemperatures()` | — | Call succeeds; status OK; no exception | |
| 2 | Assert the returned reading count matches the number of sensors declared in the HFP profile | `readings.size()` vs `hfp.sensors.size()` | Counts are equal | If HFP is empty, no assertion on count |
| 3 | For each reading: assert `sensorName` is non-empty | Each `TemperatureReading.sensorName` | Non-empty string | `sensorName` is the primary identifier |
| 4 | For each reading: assert `temperatureCelsius > -273.15` (above absolute zero) | Each `TemperatureReading.temperatureCelsius` | Greater than −273.15 | Physical lower bound |
| 5 | For each reading: assert `timestampMonotonicMs >= 0` | Each `TemperatureReading.timestampMonotonicMs` | Non-negative; 0 is valid if no event has been fired yet | |
| 6 | For each reading: locate the matching HFP sensor by `sensorName`; assert a match is found | `sensorName` vs HFP sensor list | Every reading maps to an HFP-declared sensor | No unknown sensor names allowed |
| 7 | For each reading: assert `location` matches the HFP-declared `location` for that sensor | `TemperatureReading.location` vs `hfp.sensor.location` | Strings match exactly | |
| 8 | For each reading: assert `temperatureCelsius` is within `[sensorReadingMinCelsius, sensorReadingMaxCelsius]` from the HFP profile | `temperatureCelsius` vs HFP range | Within declared range | |
| 9 | For each reading: assert `vendorCode` matches the HFP-declared `vendorCode` | `TemperatureReading.vendorCode` vs `hfp.sensor.vendorCode` | Integer values match | |
| 10 | For each reading: if `vendorInfo` is non-empty, assert it matches the HFP-declared `vendorInfo` | `TemperatureReading.vendorInfo` vs `hfp.sensor.vendorInfo` | Strings match (when non-empty) | Empty `vendorInfo` is allowed per AIDL |
| 11 | Call `getCurrentTemperatures()` a second time and assert each sensor's `timestampMonotonicMs >= 0` | Second reading array | All timestamps non-negative | Monotonic guarantee for live events is verified in L3 |
