#!/usr/bin/env python3
#** *****************************************************************************
# *
# * Copyright 2026 RDK Management
# *
# * Licensed under the Apache License, Version 2.0 (the "License");
# * you may not use this file except in compliance with the License.
# * You may obtain a copy of the License at
# *
# * http://www.apache.org/licenses/LICENSE-2.0
# *
#* ******************************************************************************

"""
L3 test: ThermalExceedRecoveryLifecycle (TC1)

Validates the full exceed-and-recovery state-machine driven by raw temperature
injections, with a secondary sensor held at a safe temperature:

  Pre-warm : inject secondary sensor at a safe temperature (no state change)
  Phase 1  : inject primary sensor at overTempCelsius
             → CRITICAL_TEMPERATURE_EXCEEDED
             → event.temperatureReading reflects primary (highest severity)
  Phase 1b : inject primary sensor at the same overTempCelsius again
             → no event (idempotent; state already EXCEEDED)
  Phase 2  : inject primary sensor at recoveryTempCelsius (<= criticalRecovered)
             → CRITICAL_TEMPERATURE_RECOVERED
  Phase 3  : inject primary sensor at normalTempCelsius (well within range)
             → NORMAL

The Python host sends TEMPERATURE_UPDATE commands via the vcomponent control-plane.
The C++ binary waits for events and verifies their content, timestamps, and state
consistency between events and getCurrentThermalState().
"""

import os
import sys
import time
import yaml

dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(dir_path, "../"))
sys.path.append(dir_path)
sys.path.append(os.path.join(dir_path, "..", "raft"))

from raft.framework.core.logModule import logModule
from vts_thermalsensor_helper import thermalSensorHelperClass

# Seconds to wait between successive temperature injections so the C++ binary
# has time to detect the event and advance to the next getNextEvent() call.
INTER_PHASE_DELAY_S = 2.0
# A shorter wait used only for the idempotent injection (Phase 1b).
IDEMPOTENT_DELAY_S = 1.0


class VTS_THERMALSENSOR_L3_ThermalExceedRecoveryLifecycle(thermalSensorHelperClass):
    """
    TC1: NORMAL → EXCEEDED → RECOVERED → NORMAL with aggregation and
    dual-listener validation.
    """

    def __init__(self, log: logModule = None):
        self.testName = "ThermalSensor_L3_ThermalExceedRecoveryLifecycle"
        qcId = "5"
        super().__init__(self.testName, qcId, log)

    def testFunction(self) -> bool:
        """
        Main test function.

        Flow:
          1. Read temperature injection values and sensor names from L3 YAML.
          2. Build a trigger callable:
               a. Inject secondary sensor at a safe temperature (pre-warm, no transition).
               b. Inject primary sensor at overTempCelsius → expect EXCEEDED event.
               c. Inject primary sensor at overTempCelsius again → idempotent, no event.
               d. Inject primary sensor at recoveryTempCelsius → expect RECOVERED event.
               e. Inject primary sensor at normalTempCelsius → expect NORMAL event.
          3. Run runL3Test("ThermalExceedRecoveryLifecycle", triggerCallable=...).
          4. Return C++ pass/fail result.

        Returns:
            bool: True if the test passed, False otherwise.
        """
        with open(self.l3TestConfigFile, 'r') as fh:
            l3cfg = yaml.safe_load(fh)

        test_node          = l3cfg['thermalsensor']['L3']['ThermalExceedRecoveryLifecycle']
        primary_sensor     = str(test_node.get('primary_sensor', 'SoC Die'))
        secondary_sensor   = str(test_node.get('secondary_sensor', 'Mainboard Ambient'))
        over_temp          = float(test_node.get('over_temp_celsius', 98.0))
        recovery_temp      = float(test_node.get('recovery_temp_celsius', 80.0))
        normal_temp        = float(test_node.get('normal_temp_celsius', 50.0))
        exceeded_threshold = float(test_node.get('critical_temperature_exceeded_celsius', 95.0))
        timeout_ms         = int(test_node.get('event_timeout_ms', 15000))

        # Safe secondary temperature: 10 °C below the exceeded threshold
        secondary_safe_temp = exceeded_threshold - 10.0

        trigger_delay = (timeout_ms / 1000.0) / 2.0

        self.log.info(
            f"[TC1] primarySensor={primary_sensor} secondarySensor={secondary_sensor} "
            f"overTemp={over_temp} recoveryTemp={recovery_temp} normalTemp={normal_temp} "
            f"exceededThreshold={exceeded_threshold} secondarySafeTemp={secondary_safe_temp}")

        self.utilities.change_directory(self.hal_session, self.targetWorkspace)

        def _trigger():
            # Pre-warm: secondary sensor at safe temperature (no state change expected)
            self.log.info(f"[TC1] Pre-warm: {secondary_sensor} at {secondary_safe_temp}°C (safe)")
            self.thermalSensorDevice.injectTemperatureUpdate(
                sensorName=secondary_sensor,
                temperatureCelsius=secondary_safe_temp)
            time.sleep(INTER_PHASE_DELAY_S)

            # Phase 1: over-temperature on primary → EXCEEDED
            self.log.info(f"[TC1] Phase 1: {primary_sensor} at {over_temp}°C → EXCEEDED")
            self.thermalSensorDevice.injectTemperatureUpdate(
                sensorName=primary_sensor,
                temperatureCelsius=over_temp)
            time.sleep(INTER_PHASE_DELAY_S)

            # Phase 1b: same over-temperature on primary → idempotent, no event
            self.log.info(f"[TC1] Phase 1b: {primary_sensor} at {over_temp}°C (idempotent)")
            self.thermalSensorDevice.injectTemperatureUpdate(
                sensorName=primary_sensor,
                temperatureCelsius=over_temp)
            time.sleep(IDEMPOTENT_DELAY_S + INTER_PHASE_DELAY_S)

            # Phase 2: recovery temperature on primary → RECOVERED
            self.log.info(f"[TC1] Phase 2: {primary_sensor} at {recovery_temp}°C → RECOVERED")
            self.thermalSensorDevice.injectTemperatureUpdate(
                sensorName=primary_sensor,
                temperatureCelsius=recovery_temp)
            time.sleep(INTER_PHASE_DELAY_S)

            # Phase 3: normal temperature on primary → NORMAL
            self.log.info(f"[TC1] Phase 3: {primary_sensor} at {normal_temp}°C → NORMAL")
            self.thermalSensorDevice.injectTemperatureUpdate(
                sensorName=primary_sensor,
                temperatureCelsius=normal_temp)
            time.sleep(INTER_PHASE_DELAY_S)

        return self.runL3Test(
            "ThermalExceedRecoveryLifecycle",
            triggerCallable=_trigger,
            triggerDelay=trigger_delay)


if __name__ == "__main__":
    summaryLogName = os.path.splitext(os.path.basename(__file__))[0] + "_summary"
    summaryLog = logModule(summaryLogName, level=logModule.INFO)
    test = VTS_THERMALSENSOR_L3_ThermalExceedRecoveryLifecycle(summaryLog)
    test.run(False)
