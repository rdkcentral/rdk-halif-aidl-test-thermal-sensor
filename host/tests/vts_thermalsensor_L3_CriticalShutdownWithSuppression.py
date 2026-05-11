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

Reads transition_states from the L3 YAML and all sensor thresholds from the
HFP profile.  Drives each state transition in sequence:

  States up to and including SHUTDOWN : inject temperature to trigger the event.
  States after SHUTDOWN               : inject temperature that the vcomponent
                                        must SUPPRESS (no event expected).

Injection temperatures are derived per-sensor from the HFP profile:
  EXCEEDED  -> exceeded + 2°C
  SHUTDOWN  -> shutdown + 2°C
  RECOVERED -> recovered - 2°C
  NORMAL    -> midpoint of sensor_reading_range_celsius

The primary sensor is the first sensor in the HFP profile.
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

INTER_PHASE_DELAY_S = 2.0


class VTS_THERMALSENSOR_L3_CriticalShutdownWithSuppression(thermalSensorHelperClass):
    """
    TC2: driven by transition_states from YAML; temps derived from HFP profile.
    """

    def __init__(self, log: logModule = None):
        self.testName = "ThermalSensor_L3_CriticalShutdownWithSuppression"
        qcId = "2"
        super().__init__(self.testName, qcId, log)

    def _verifyShutdownAndRecovery(self):
        """
        Verifies device recovery after a thermal shutdown event.

        TODO:
            - After CRITICAL_SHUTDOWN_IMMINENT is triggered, run a PingTest to
              confirm the device is still alive.
            - Wait for ``shutdown_min_downtime_s`` + ``min_cooldown_seconds``
              (both defined in the HFP profile) before injecting the recovery
              temperature.
            - Inject a recovery temperature and verify the device fires a
              CRITICAL_TEMPERATURE_RECOVERED event.
            - Verify the boot reason reported after recovery is
              ``THERMAL_SHUTDOWN_RECOVERY`` (via IBoot.getBootReason()).
            - Refer to the thermal policy defined by expert engineering teams
              (not user-configurable).
        """
        return True

    def _trigger(self, sensorName: str, transitionStates: list,
                 stateToTemp: dict, suppressionMs: int):
        """
        Inject temperature updates for each state transition in sequence.

        States up to and including CRITICAL_SHUTDOWN_IMMINENT fire the event normally.
        States after CRITICAL_SHUTDOWN_IMMINENT are injected but the vcomponent must suppress them.

        Args:
            sensorName (str): Name of the primary sensor to inject.
            transitionStates (list): Ordered state list from the YAML config.
            stateToTemp (dict): Maps state name -> injection temperature (°C).
            suppressionMs (int): suppression_timeout_ms from the YAML config.
        """
        inShutdown = False
        prevState  = transitionStates[0] if transitionStates else 'NORMAL'

        for state in transitionStates[1:]:
            injectTemp = stateToTemp.get(state)
            if injectTemp is None:
                self.log.info(f"[TC{self.qcId}] Unknown state '{state}', skipping")
                prevState = state
                continue

            if not inShutdown:
                self.log.info(
                    f"[TC{self.qcId}] {prevState} -> {state}: inject '{sensorName}' "
                    f"at {injectTemp} Celsius (expect event)")
                self.thermalSensorDevice.injectTemperatureUpdate(
                    sensorName=sensorName,
                    temperatureCelsius=injectTemp)
                delay = INTER_PHASE_DELAY_S * 2 if state == 'CRITICAL_SHUTDOWN_IMMINENT' else INTER_PHASE_DELAY_S
                time.sleep(delay)

                if state == 'CRITICAL_SHUTDOWN_IMMINENT':
                    inShutdown = True
            else:
                self.log.info(
                    f"[TC{self.qcId}] {prevState} -> {state}: inject '{sensorName}' "
                    f"at {injectTemp} Celsius (expect SUPPRESSED)")
                self.thermalSensorDevice.injectTemperatureUpdate(
                    sensorName=sensorName,
                    temperatureCelsius=injectTemp)
                time.sleep((suppressionMs / 1000.0) + INTER_PHASE_DELAY_S)

            prevState = state

    def testFunction(self) -> bool:
        """
        Reads transition_states from the L3 YAML config and injection temperatures
        from the HFP profile, then drives the vcomponent through each transition.

        Returns:
            bool: True if the test passed, False otherwise.
        """
        with open(self.l3TestConfigFile, 'r') as fh:
            l3cfg = yaml.safe_load(fh)

        # TC2 drives NORMAL -> EXCEEDED -> CRITICAL_SHUTDOWN_IMMINENT (pre-shutdown
        # phase, each event must be delivered), then injects additional states that
        # the vcomponent must SUPPRESS after shutdown is reached.  The C++ binary
        # waits suppressionTimeoutMs for each post-shutdown injection and asserts
        # that no event arrives.
        testNode         = l3cfg['thermalsensor']['L3']
        transitionStates = [
            'NORMAL',
            'CRITICAL_TEMPERATURE_EXCEEDED',
            'CRITICAL_SHUTDOWN_IMMINENT',
            # Post-shutdown states — injections must be suppressed by the vcomponent.
            'CRITICAL_TEMPERATURE_RECOVERED',
            'NORMAL',
        ]
        timeoutMs        = int(testNode.get('event_timeout_ms', 15000))
        suppressionMs    = int(testNode.get('suppression_timeout_ms', 5000))

        sensors = self.getSensorListFromHFP()
        if not sensors:
            self.log.info(f"[TC{self.qcId}] ERROR: no sensors in HFP profile")
            return False

        s          = sensors[0]
        sensorName = s['name']

        stateToTemp = {
            'CRITICAL_TEMPERATURE_EXCEEDED':  s['over_temp'],
            'CRITICAL_SHUTDOWN_IMMINENT':      s['shutdown_temp'],
            'CRITICAL_TEMPERATURE_RECOVERED': s['recovery_temp'],
            'NORMAL':                         s['normal_temp'],
        }

        triggerDelay = (timeoutMs / 1000.0) / 2.0

        self.log.info(
            f"[TC{self.qcId}] sensor='{sensorName}' states={transitionStates} "
            f"exceeded={s['over_temp']} Celsius shutdown={s['shutdown_temp']} Celsius "
            f"recovered={s['recovery_temp']} Celsius normal={s['normal_temp']} Celsius "
            f"suppressionMs={suppressionMs}")

        self.utilities.change_directory(self.hal_session, self.targetWorkspace)

        # Patch the YAML with TC2's shutdown sequence before uploading to target.
        self.updateTransitionStates(transitionStates)

        result = self.runL3Test(
            "CriticalShutdownWithSuppression",
            triggerCallable=lambda: self._trigger(
                sensorName, transitionStates, stateToTemp, suppressionMs),
            triggerDelay=triggerDelay)

        result = result and self._verifyShutdownAndRecovery()
        return result

if __name__ == "__main__":
    summaryLogName = os.path.splitext(os.path.basename(__file__))[0] + "_summary"
    summaryLog = logModule(summaryLogName, level=logModule.INFO)
    test = VTS_THERMALSENSOR_L3_CriticalShutdownWithSuppression(summaryLog)
    test.run(False)
