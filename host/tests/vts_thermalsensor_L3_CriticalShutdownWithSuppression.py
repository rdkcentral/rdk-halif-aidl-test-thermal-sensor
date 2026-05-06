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
L3 test: CriticalShutdownWithSuppression (TC2)

Validates that once CRITICAL_SHUTDOWN_IMMINENT is entered the vcomponent
suppresses all subsequent state transitions, even when the temperature
drops back into the recovery or normal range:

  Phase 1 : inject sensorName at overTempCelsius
            → CRITICAL_TEMPERATURE_EXCEEDED
  Phase 2 : inject sensorName at shutdownTempCelsius (> enteringCriticalShutdown)
            → CRITICAL_SHUTDOWN_IMMINENT
  Phase 3 : inject sensorName at recoveryTempCelsius
            → suppressed (no event expected within suppressionTimeoutMs)
  Phase 4 : inject sensorName at a safe normal temperature
            → suppressed (no event expected within suppressionTimeoutMs)

The Python host sends TEMPERATURE_UPDATE commands via the vcomponent control-plane.
The C++ binary verifies event delivery (phases 1 to 2) and absence of events
(phases 3 to 4).
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

# Seconds to wait between successive temperature injections
INTER_PHASE_DELAY_S = 2.0


class VTS_THERMALSENSOR_L3_CriticalShutdownWithSuppression(thermalSensorHelperClass):
    """
    TC2: NORMAL → EXCEEDED → SHUTDOWN_IMMINENT; recovery and normal are suppressed.
    """

    def __init__(self, log: logModule = None):
        self.testName = "ThermalSensor_L3_CriticalShutdownWithSuppression"
        qcId = "6"
        super().__init__(self.testName, qcId, log)

    def testFunction(self) -> bool:
        """
        Main test function.

        Flow:
          1. Read injection temperatures and thresholds from L3 YAML.
          2. Build a trigger callable:
               a. Phase 1: overTempCelsius → triggers EXCEEDED event.
               b. Phase 2: shutdownTempCelsius → triggers SHUTDOWN_IMMINENT event.
               c. Phase 3: recoveryTempCelsius → must produce NO event.
               d. Phase 4: safe normal temp → must produce NO event.
          3. Run runL3Test("CriticalShutdownWithSuppression", triggerCallable=...).
          4. Return C++ pass/fail result.

        Returns:
            bool: True if the test passed, False otherwise.
        """
        with open(self.l3TestConfigFile, 'r') as fh:
            l3cfg = yaml.safe_load(fh)

        test_node        = l3cfg['thermalsensor']['L3']['CriticalShutdownWithSuppression']
        sensor_name      = str(test_node.get('sensorName', 'SoC Die'))
        over_temp        = float(test_node.get('over_temp_celsius', 98.0))
        shutdown_temp    = float(test_node.get('shutdown_temp_celsius', 115.0))
        recovery_temp    = float(test_node.get('recovery_temp_celsius', 80.0))
        exceeded_celsius = float(test_node.get('critical_temperature_exceeded_celsius', 95.0))
        timeout_ms       = int(test_node.get('event_timeout_ms', 15000))
        suppression_ms   = int(test_node.get('suppression_timeout_ms', 5000))

        # A safe temperature well below the exceeded threshold
        safe_temp = exceeded_celsius - 20.0

        trigger_delay = (timeout_ms / 1000.0) / 2.0

        self.log.info(
            f"[TC2] sensor={sensor_name} overTemp={over_temp} shutdownTemp={shutdown_temp} "
            f"recoveryTemp={recovery_temp} safeTemp={safe_temp} "
            f"suppressionMs={suppression_ms}")

        self.utilities.change_directory(self.hal_session, self.targetWorkspace)

        def _trigger():
            # Phase 1: over-temperature → EXCEEDED
            self.log.info(f"[TC2] Phase 1: {sensor_name} at {over_temp}°C → EXCEEDED")
            self.thermalSensorDevice.injectTemperatureUpdate(
                sensorName=sensor_name,
                temperatureCelsius=over_temp)
            time.sleep(INTER_PHASE_DELAY_S)

            # Phase 2: above shutdown threshold → SHUTDOWN_IMMINENT
            self.log.info(f"[TC2] Phase 2: {sensor_name} at {shutdown_temp}°C → SHUTDOWN_IMMINENT")
            self.thermalSensorDevice.injectTemperatureUpdate(
                sensorName=sensor_name,
                temperatureCelsius=shutdown_temp)
            # Wait long enough for the C++ binary to observe the event and reset its queue
            time.sleep(INTER_PHASE_DELAY_S * 2)

            # Phase 3: recovery temperature → must be suppressed
            self.log.info(f"[TC2] Phase 3: {sensor_name} at {recovery_temp}°C (suppressed)")
            self.thermalSensorDevice.injectTemperatureUpdate(
                sensorName=sensor_name,
                temperatureCelsius=recovery_temp)
            # Wait the full suppression window plus a small margin
            time.sleep((suppression_ms / 1000.0) + INTER_PHASE_DELAY_S)

            # Phase 4: safe normal temperature → must also be suppressed
            self.log.info(f"[TC2] Phase 4: {sensor_name} at {safe_temp}°C (suppressed)")
            self.thermalSensorDevice.injectTemperatureUpdate(
                sensorName=sensor_name,
                temperatureCelsius=safe_temp)
            time.sleep((suppression_ms / 1000.0) + INTER_PHASE_DELAY_S)

        return self.runL3Test(
            "CriticalShutdownWithSuppression",
            triggerCallable=_trigger,
            triggerDelay=trigger_delay)


if __name__ == "__main__":
    summaryLogName = os.path.splitext(os.path.basename(__file__))[0] + "_summary"
    summaryLog = logModule(summaryLogName, level=logModule.INFO)
    test = VTS_THERMALSENSOR_L3_CriticalShutdownWithSuppression(summaryLog)
    test.run(False)
