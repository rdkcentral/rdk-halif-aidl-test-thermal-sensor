# Test Suite For Thermal Sensor HAL

## Table of Contents

- [Acronyms, Terms and Abbreviations](#acronyms-terms-and-abbreviations)
- [Description](#description)
- [Reference Documents](#reference-documents)

## Acronyms, Terms and Abbreviations

- `HAL` - Hardware Abstraction Layer
- `L1` - Functional Tests
- `L2` - Module functional Testing
- `L3` - Module testing with External Stimulus is required to validate and control device
- `API` - Application Programming Interface
- `High-Level Test Specification` : This document provides a high-level view of test cases covered as part of `L2` and `L3` testing, focusing on major use cases, system behaviour, and overall test coverage.
- `Low-Level Test Specification` : This document provides technical details of how the tests are implemented, including specific test cases with inputs, expected outputs, pass/fail criteria, and implementation specifics for individual functionalities, modules, or APIs.

## Description

This repository contains the Test Suites(`L1` , `L2` and `L3`) for Thermal Sensor `HAL`.

## Reference Documents

|SNo|Document Name|Document Description|Document Link|
|---|-------------|--------------------|-------------|
|1|`HAL` Specification Document|This document provides specific information on the APIs for which tests are written in this module|[HAL Interface](https://github.com/rdkcentral/rdk-halif-aidl/blob/0.15.0/README.md)|
|2|High Level Test Specification|High Level Test Specification Documentation for the Thermal Sensor `HAL`|[thermal_sensor_High_Level_TestSpecification.md](docs/pages/thermal_sensor_High_Level_TestSpecification.md)|
|3|`L3` Low Level Test Specification|`L3` Low Level Test Specification for the Thermal Sensor `HAL`|[thermal_sensor_L3_Low-Level_TestSpecification.md](docs/pages/thermal_sensor_L3_Low-Level_TestSpecification.md)|
|4|`L1` `L2` and `L3` Test procedure document|This document contains Build Instructions and `L1` `L2` and `L3` Test procedure for the Thermal Sensor `HAL`|[vts_thermal_sensor_L1_L2_L3_TestProcedure.md](docs/pages/vts_thermal_sensor_L1_L2_L3_TestProcedure.md)|

