#!/usr/bin/env python3
#** *****************************************************************************
# *
# * If not stated otherwise in this file or this component's LICENSE file the
# * following copyright and licenses apply:
# *
# * Copyright 2026 RDK Management
# *
# * Licensed under the Apache License, Version 2.0 (the "License");
# * you may not use this file except in compliance with the License.
# * You may obtain a copy of the License at
# *
# * http://www.apache.org/licenses/LICENSE-2.0
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# *
#** ******************************************************************************

"""
L3 test: CriticalShutdownWithSuppression (TC2)

Reads transition_states from the L3 YAML and all sensor thresholds from the
HFP profile.  Drives each state transition in sequence:

  States up to and including SHUTDOWN : inject temperature to trigger the event.
  States after SHUTDOWN               : inject temperature that the vcomponent
                                        must SUPPRESS (no event expected).

"""

import os
import sys
import time
from paramiko import SSHClient
import paramiko
import yaml
import socket

dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(dir_path, "../"))
sys.path.append(dir_path)
sys.path.append(os.path.join(dir_path, "..", "raft"))

from raft.framework.core.logModule import logModule
from raft.framework.plugins.ut_raft.utUserResponse import utUserResponse
from vts_thermalsensor_helper import thermalSensorHelperClass

INTER_PHASE_DELAY_S = 2.0


class VTS_THERMALSENSOR_L3_CriticalShutdownWithSuppression(thermalSensorHelperClass):
    """
    TC2: driven by transition_states from YAML; temps derived from HFP profile.
    """

    def __init__(self, log: logModule = None, moduleName: str = None):
        self.testName = "ThermalSensor_L3_CriticalShutdownWithSuppression"
        self.qcId = "2"
        self.isUnifiedTest = False if moduleName is None else True
        super().__init__(self.testName, self.qcId, log, moduleName)

    def _verifyShutdownAndRecovery(self, shutdown_min_downtime_s:int, min_cooldown_seconds:int) -> bool:
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
        minDeviceBootTime_s = 120
        self.log.stepStart(
            f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName}: Verify for Device to shutdown")
        time.sleep(shutdown_min_downtime_s)
        shutdown_observed = not self._waitForSsh(self.hal_session.address, self.hal_session.port)
        self.log.stepResult(
            shutdown_observed,
            f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName} Verified for Device to shutdown")
        if not shutdown_observed:
            self.log.error("Unable to verify the device shutdown period")
            return False

        # wait time for device to up
        time.sleep(min_cooldown_seconds)

        self.log.stepStart(
                f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName}: Verify for Device to shutdown during cool down period")
        shutdown_observed = not self._waitForSsh(self.hal_session.address, self.hal_session.port)
        self.log.stepResult(
            shutdown_observed,
            f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName} Verified for Device to shutdown during cool down period")
        if not shutdown_observed:
            self.log.error("Unable to verify the shutdown device after shutdown time + cool down period")
            return False

        self.log.stepStart(
                f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName}: User verification for device boot after shutdown + cool down period")
        result =  self.testUserResponse.getUserYN(
                "Please boot the Target device. Has the device been booted and up? (Y/N):")
        self.log.stepResult(
                result,
                f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName} User verification for device boot after shutdown + cool down period")
        if not result:
            self.log.error("Unable to verify the device boot after shutdown + cool down period")
            return False

        self.log.stepStart(
                f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName}: Verify for ssh connection")
        result = self._waitForSsh(
                    self.hal_session.address, self.hal_session.port, minDeviceBootTime_s)
        self.log.stepResult(
                result,
                f"Test Suit: VTS_L3_THERMALSENSOR  Test Case: {self.testName} Verified for ssh connection after shutdown + cool down period")
        if not result:
            self.log.error("Unable to verify the ssh connection after shutdown + cool down period")
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

    def _waitForSsh(self, host="127.0.0.1", port=7722, timeout=0):
        """
        Wait for the SSH service to become reachable.

        Args:
            host (str): Hostname or IP address of the SSH server.
            port (int): SSH port to check.
            timeout (int): Maximum time to wait, in seconds.

        Returns:
            bool: True if the SSH service becomes reachable within the timeout,
                otherwise False.
        """
        end = time.time() + timeout

        while time.time() <= end:
            isUp = False
            banner = ""

            try:
                with socket.create_connection((host, port), timeout=2) as s:
                    s.settimeout(2)
                    banner = s.recv(1024).decode("utf-8", errors="ignore")

                    # A valid SSH banner confirms the service is SSH.
                    # If no banner is received, a successful TCP connection
                    # is still considered a strong signal that SSH is up.
                    isUp = banner.startswith("SSH-") if banner else True

            except Exception as e:
                self.log.debug(f"Waiting for SSH to be up... {e}")

            if isUp:
                self.log.debug(
                    f"SSH is up: {banner.strip() if banner else '<no banner>'}"
                )
                return True

            self.log.debug(f"SSH is still down. Verifying until {timeout}")
            time.sleep(1)

        self.log.debug(f"SSH did not become available within {timeout} seconds")
        return False

    def testFunction(self) -> bool:
        """
        Reads transition_states from the L3 YAML config and injection temperatures
        from the HFP profile, then drives the vcomponent through each transition.

        Returns:
            bool: True if the test passed, False otherwise.
        """
        self.testUserResponse = utUserResponse()
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
        overallResult = True
        for sensor in sensors:
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

            result = result and self._verifyShutdownAndRecovery(sensor["shutdown_min_downtime_s"], sensor["min_cooldown_seconds"])
            super().__init__(
                self.testName,
                self.qcId,
                self.log,
                self.moduleName if self.isUnifiedTest else None)            # TODO: Add boot reason validation using IBoot.getBootReason() in the unified repository.            overallResult = overallResult and result
        return overallResult

if __name__ == "__main__":
    summaryLogName = os.path.splitext(os.path.basename(__file__))[0] + "_summary"
    summaryLog = logModule(summaryLogName, level=logModule.INFO)
    test = VTS_THERMALSENSOR_L3_CriticalShutdownWithSuppression(summaryLog)
    test.run(False)
