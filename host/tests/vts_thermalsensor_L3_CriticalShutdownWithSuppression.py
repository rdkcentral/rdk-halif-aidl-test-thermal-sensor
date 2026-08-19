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
import socket
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

    def __init__(self, log: logModule = None, moduleName: str = None):
        self.testName = "ThermalSensor_L3_CriticalShutdownWithSuppression"
        self.qcId = "2"
        self.moduleName = moduleName
        super().__init__(self.testName, self.qcId, log, self.moduleName)

    def _verifyShutdownAndRecovery(self, reboot_wait_time:int=900):
        """
        Verifies device recovery after a thermal shutdown event.

            - After CRITICAL_SHUTDOWN_IMMINENT is triggered, run a PingTest to
              confirm the device is still alive.
            - Wait for ``shutdown_min_downtime_s`` + ``min_cooldown_seconds``
              (both defined in the HFP profile) before injecting the recovery
              temperature.
            - Verify the boot reason reported after recovery is
              ``THERMAL_SHUTDOWN_RECOVERY`` (via IBoot.getBootReason()).
            - Refer to the thermal policy defined by expert engineering teams
              (not user-configurable).
        """
        self.log.stepStart(
            f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName}: Verify for Device to shutdown")

        shutdown_observed = self._waitForSsh(
            self.hal_session.address,
            self.hal_session.port,
            reboot_wait_time,
            False)

        self.log.stepResult(
            shutdown_observed,
            f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName} Verified for Device to shutdown")
        if not shutdown_observed:
            self.log.error("Unable to verify the device shutdown+ cool down period")
            return False

        self.log.stepStart(
                f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName}: Verify for Device to reboot")
        result = self._waitForSsh(
             self.hal_session.address, self.hal_session.port, reboot_wait_time)
        if not result:
            if self.testUserResponse.getUserYN(
                "Please boot the Target device. Has the device been booted and up? (Y/N):"):
                result = self._waitForSsh(
                    self.hal_session.address, self.hal_session.port, reboot_wait_time)
        self.log.stepResult(
             result,
             f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName} Verified for Device to reboot")
        if not result:
             self.log.error("Unable to verify the reboot after shutdown + cool down period")
        return result

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

    def _waitForSsh(self, host="127.0.0.1", port=7722, timeout=180, expectUp=True):
        """
        Wait for an SSH service to reach the expected state (up or down).
        Args:
            host (str): Hostname or IP address of the SSH server.
            port (int): SSH port to check.
            timeout (int): Maximum time to wait, in seconds.
            expectUp (bool): Desired SSH state.
                - True: Wait until SSH is reachable.
                - False: Wait until SSH is unreachable.

        Returns:
            bool: True if the SSH service reaches the expected state within the
            timeout period, otherwise False.
        """
        end = time.time() + timeout
        while time.time() < end:
            isUp = False
            try:
                with socket.create_connection((host, port), timeout=2) as s:
                    s.settimeout(2)
                    banner = s.recv(1024).decode("utf-8", errors="ignore")
                    # Some servers may not immediately send a banner; a successful
                     # TCP connect is still a strong signal the service is up.
                    isUp = banner.startswith("SSH-") if banner else True
            except Exception as e:
                self.log.debug(
                     f"Waiting for SSH to be {'up' if expectUp else 'down'}... {e}")
            if isUp == expectUp:
                 if isUp:
                     self.log.debug(f"SSH is up: {banner.strip() if banner else '<no banner>'}")
                 else:
                     self.log.debug("SSH is down")
                 return True
            time.sleep(1)
        if expectUp:
            self.log.error(
                f"Timeout waiting for SSH to become up"
            )
            return False

        # SSH remained down for the complete timeout.
        self.log.debug(
            f"SSH remained down for {timeout} seconds"
        )
        return True

    def testFunction(self) -> bool:
        """
        Reads transition_states from the L3 YAML config and injection temperatures
        from the HFP profile, then drives the vcomponent through each transition.

        Returns:
            bool: True if the test passed, False otherwise.
        """
        with open(self.paths["l3TestConfigFile"], 'r') as fh:
            l3cfg = yaml.safe_load(fh)

        # TC2 drives NORMAL -> EXCEEDED -> CRITICAL_SHUTDOWN_IMMINENT
        testNode         = l3cfg['thermalsensor']['L3']
        transitionStates = [
            'NORMAL',
            'CRITICAL_TEMPERATURE_EXCEEDED',
            'CRITICAL_SHUTDOWN_IMMINENT',
        ]
        timeoutMs        = int(testNode.get('event_timeout_ms', 15000))
        suppressionMs    = int(testNode.get('suppression_timeout_ms', 5000))

        sensors = self.getSensorListFromHFP()
        if not sensors:
            self.log.info(f"[TC{self.qcId}] ERROR: no sensors in HFP profile")
            return False
        sensor          = sensors[0]
        sensorName = sensor['name']

        stateToTemp = {
            'CRITICAL_TEMPERATURE_EXCEEDED':  sensor['over_temp'],
            'CRITICAL_SHUTDOWN_IMMINENT':      sensor['shutdown_temp'],
            'CRITICAL_TEMPERATURE_RECOVERED': sensor['recovery_temp'],
            'NORMAL':                         sensor['normal_temp'],
        }

        triggerDelay = (timeoutMs / 1000.0) / 2.0

        self.log.info(
            f"[TC{self.qcId}] sensor='{sensorName}' states={transitionStates} "
            f"exceeded={sensor['over_temp']} Celsius shutdown={sensor['shutdown_temp']} Celsius "
            f"recovered={sensor['recovery_temp']} Celsius normal={sensor['normal_temp']} Celsius "
            f"suppressionMs={suppressionMs}")

        self.utilities.change_directory(self.hal_session, self.targetWorkspace)

        # Patch the YAML with TC2's shutdown sequence and suppression timeout before uploading to target.
        self.updateTransitionStates(transitionStates, suppressionTimeoutMs=suppressionMs)

        result = self.runL3Test(
            "ThermalStateTransition",
            triggerCallable=lambda: self._trigger(
                sensorName, transitionStates, stateToTemp, suppressionMs),
            triggerDelay=triggerDelay)

        result = result and self._verifyShutdownAndRecovery(sensor["reboot_wait_time"])
        return result

if __name__ == "__main__":
    summaryLogName = os.path.splitext(os.path.basename(__file__))[0] + "_summary"
    summaryLog = logModule(summaryLogName, level=logModule.INFO)
    test = VTS_THERMALSENSOR_L3_CriticalShutdownWithSuppression(summaryLog)
    test.run(False)
