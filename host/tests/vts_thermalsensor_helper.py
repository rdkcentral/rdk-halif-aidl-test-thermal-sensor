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

import os
import sys
import tarfile
import time
import threading

import yaml

dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(dir_path, "../"))
sys.path.append(dir_path)
sys.path.append(os.path.join(dir_path, "..", "raft"))

from raft.framework.plugins.ut_raft.configRead import ConfigRead
from raft.framework.plugins.ut_raft import utHelperClass
from raft.framework.core.logModule import logModule
from raft.framework.plugins.ut_raft.utSuiteNavigator import UTSuiteNavigatorClass
from raft.framework.plugins.ut_raft.utBaseUtils import utBaseUtils
from thermalController.ThermalSensorController import ThermalSensorController

THERMALSENSOR_TEST_MENU_CONFIGURATION_FILE = os.path.join(
    dir_path, "vts_thermalsensor_testMenuConfiguration.yaml")
THERMALSENSOR_L3_TEST_CONFIG_FILE = os.path.join(
    os.path.join(dir_path, "../../test_configuration"), "vts_l3_ThermalSensor.yaml")

THERMALSENSOR_L3_TEST_SUITE_NAME = "VTS_L3_THERMALSENSOR"


class thermalSensorHelperClass(utHelperClass):
    """
    Base helper for ThermalSensor L3 host-side tests.
    Handles artifact deployment, session setup, and control-plane trigger helpers.
    """

    def __init__(
        self,
        testName: str,
        qcId: str,
        log: logModule = None,
        moduleName: str = None,
    ):
        self.moduleName = "thermalsensor" if moduleName is None else moduleName
        isUnifiedTest = False if moduleName is None else True
        self.interfaceName = "thermalsensor"
        super().__init__(testName, qcId, log)

        self.log = log
        if log is None:
            self.log = logModule(self.__class__.__name__)
            self.log.setLevel(self.log.INFO)

        # Parse device configuration first (used by path selection below)
        self.deviceTestSetup = self.cpe.get("test")
        self.waitTime = self.cpe.get("wait_time")
        self.platform = self.cpe.get("platform")
        self.targetWorkspace = os.path.join(
            self.cpe.get("target_directory"), self.moduleName)

        component_root = os.path.normpath(os.path.join(dir_path, "../.."))
        unified_root = os.path.normpath(os.path.join(dir_path, "../../../.."))

        if not isUnifiedTest:
            self.paths = {
                "testMenuConfigFile": THERMALSENSOR_TEST_MENU_CONFIGURATION_FILE,
                "l3TestConfigFile": THERMALSENSOR_L3_TEST_CONFIG_FILE,
                "moduleHFPFile": os.path.join(dir_path, self.deviceTestSetup.get("profile")),
            }
            artifactBaseDir = dir_path
            targetCfgBase = self.targetWorkspace
        else:
            self.paths = {
                "testMenuConfigFile": os.path.join(
                    unified_root, "host/tests/TestCases/vts_l3_test_setup.yaml"
                ),
                "l3TestConfigFile": os.path.join(
                    component_root, "test_configuration/vts_l3_ThermalSensor.yaml"
                ),
                "moduleHFPFile": os.path.join(
                    component_root, "profiles/hfp-thermalsensor-vcomponent.yaml"
                ),
            }
            artifactBaseDir = os.path.join(component_root, "host")
            targetCfgBase = os.path.join(self.targetWorkspace, "bin").replace("\\", "/")

        for attr, path in self.paths.items():
            normalized_path = os.path.normpath(path).replace("\\", "/")
            setattr(self, attr, normalized_path)
            self.paths[attr] = normalized_path

        profileOnTarget = os.path.join(
            targetCfgBase, "profiles", os.path.basename(self.paths["moduleHFPFile"])
        ).replace("\\", "/")

        # Load test menu configuration
        controllerPort = self.deviceTestSetup.get("thermalsensor_controller_port", 8081)
        self.testMenuConfiguration = ConfigRead(
            self.paths["testMenuConfigFile"], self.moduleName)
        self.testMenuConfiguration.test.execute = (
            self.testMenuConfiguration.test.execute + f" -p {profileOnTarget}")
        self.l3TestConfigFolder = os.path.join(
            targetCfgBase, "test_configuration").replace("\\", "/")

        self.hal_session = self.dut.getConsoleSession("ssh_hal_test_0")
        self.hal_session_1 = self.dut.getConsoleSession("ssh_hal_test_1")

        # Initialize UT menu navigator and utilities
        self.utMenu = UTSuiteNavigatorClass(
            self.testMenuConfiguration, None, self.hal_session)
        self.utilities = utBaseUtils()

        # Copy required artifacts to target device
        self.utilities.change_directory(self.hal_session, self.targetWorkspace)
        for artifact in self.testMenuConfiguration.test.artifacts:
            filesPath = os.path.normpath(os.path.join(artifactBaseDir, artifact))
            self.utilities.sftpCopy(
                self.hal_session, filesPath, self.targetWorkspace)
            if tarfile.is_tarfile(filesPath):
                targetTarfile = os.path.join(
                    self.targetWorkspace,
                    os.path.basename(artifact)).replace("\\", "/")
                self.utilities.untar(
                    self.hal_session, targetTarfile, self.targetWorkspace)

        self.vcomponentLogFile = "/tmp/vcomponent_thermalsensor.log"
        self.targetWorkspace = targetCfgBase
        self.utilities.change_directory(self.hal_session, self.targetWorkspace)
        self.utilities.change_directory(self.hal_session_1, self.targetWorkspace)
        time.sleep(2)

        self.thermalSensorDevice = ThermalSensorController(
            self.platform,
            self.hal_session_1,
            self.cpe.get("prompt"),
            controllerPort)

    def readYaml(self, yaml_path: str) -> dict:
        """
        Reads a YAML file and returns its content as a dictionary.
        """
        with open(yaml_path, 'r') as fh:
            return yaml.safe_load(fh)

    def writeYaml(self, yaml_path: str, data: dict):
        """
        Writes a dictionary to a YAML file.
        """
        with open(yaml_path, 'w') as fh:
            yaml.dump(data, fh, sort_keys=False, default_flow_style=False, allow_unicode=True)

    def getSensorListFromHFP(self) -> list:
        """
        Reads all thermal sensors from the HFP profile and returns a list of
        sensor dicts with pre-computed injection temperatures.

        Each dict contains:
          name          - sensor name string
          over_temp     - exceeded threshold + 2°C  (triggers EXCEEDED)
          shutdown_temp - shutdown threshold + 2°C  (triggers SHUTDOWN_IMMINENT)
          recovery_temp - recovered threshold - 2°C (triggers RECOVERED)
          normal_temp   - midpoint of (recovered, exceeded] range, satisfying
                          recovered < normal_temp <= exceeded so that a
                          TEMPERATURE_UPDATE from RECOVERED state transitions
                          the vcomponent to NORMAL
          safe_temp     - recovered threshold - 10°C (secondary pre-warm)

        Returns:
            list: One dict per sensor in HFP profile order.
        """
        with open(self.paths["moduleHFPFile"], 'r') as fh:
            hfp = yaml.safe_load(fh)

        sensors = []
        for s in hfp.get('sensor', {}).get('thermal', []):
            name      = str(s.get('sensorName', ''))
            triggers  = s.get('triggers', {})
            exceeded  = float(triggers.get('critical_temperature_exceeded_celsius',  95.0))
            recovered = float(triggers.get('critical_temperature_recovered_celsius', 80.0))
            shutdown  = float(triggers.get('entering_critical_shutdown_celsius',     110.0))
            sensors.append({
                'name':          name,
                'over_temp':     exceeded  + 2.0,
                'shutdown_temp': shutdown  + 2.0,
                'recovery_temp': recovered - 2.0,
                # normal_temp must satisfy: recovered < normal_temp <= exceeded
                # so that TEMPERATURE_UPDATE from RECOVERED state transitions to NORMAL.
                # Using reading range midpoint would be BELOW recovered for both sensors.
                'normal_temp':   (recovered + exceeded) / 2.0,
                'safe_temp':     recovered - 10.0,
            })
        return sensors

    def updateTransitionStates(self, states: list, suppressionTimeoutMs: int = 0):
        """
        Patches ``thermalsensor.L3.transition_states`` (and optionally
        ``suppression_timeout_ms``) in the L3 YAML config file in-place.

        Call this before ``runL3Test`` so the patched YAML is uploaded to the
        target and read by the C++ binary.

        Args:
            states (list): Ordered list of state name strings, e.g.
                ``['NORMAL', 'CRITICAL_TEMPERATURE_EXCEEDED', 'CRITICAL_TEMPERATURE_RECOVERED', 'NORMAL']``.
            suppressionTimeoutMs (int): Value written to
                ``thermalsensor.L3.suppression_timeout_ms``.  0 disables
                post-shutdown suppression (TC1 path); > 0 enables it (TC2 path).
                Defaults to 0.
        """
        data = self.readYaml(self.paths["l3TestConfigFile"])
        data['thermalsensor']['L3']['transition_states']     = list(states)
        data['thermalsensor']['L3']['suppression_timeout_ms'] = int(suppressionTimeoutMs)
        self.writeYaml(self.paths["l3TestConfigFile"], data)
        self.log.info(
            f"[updateTransitionStates] transition_states={states} "
            f"suppression_timeout_ms={suppressionTimeoutMs}")

    def runL3Test(self, testcase: str, triggerCallable=None, triggerDelay: float = 25.0) -> bool:
        """
        Uploads the L3 config YAML to the target, starts the test binary, fires
        the optional trigger callback in a background thread (after the binary is
        running), selects the test case in the UT menu, collects results, and
        returns the pass/fail bool.

        Call ``updateTransitionStates()`` before this method to patch the YAML
        with the required state sequence for the test case.

        Args:
            testcase (str): Test case name within VTS_L3_THERMALSENSOR.
            triggerCallable (callable, optional): No-arg callable to invoke in a
                background thread ``triggerDelay`` seconds after utMenu.start().
            triggerDelay (float): Seconds to wait after utMenu.start() before
                invoking triggerCallable. Defaults to 25.0.

        Returns:
            bool: True if the test passed, False otherwise.
        """
        self.utilities.sftpCopy(
            self.hal_session, self.paths["l3TestConfigFile"], self.l3TestConfigFolder)
        self.utMenu.start()

        triggerThread = None
        if triggerCallable is not None:
            def _run():
                time.sleep(triggerDelay)
                triggerCallable()
            triggerThread = threading.Thread(target=_run, daemon=True)
            triggerThread.start()

        self.log.stepStart(
            f"Test Suite: {THERMALSENSOR_L3_TEST_SUITE_NAME}  Test Case: {testcase}")
        output = self.utMenu.select(
            THERMALSENSOR_L3_TEST_SUITE_NAME, testcase, None, self.waitTime, False)
        if triggerThread is not None:
            triggerThread.join()
        results = self.utMenu.collect_results(output, True)
        if results is None:
            results = False

        self._emitVcomponentLogs(testcase)

        self.log.stepResult(
            results,
            f"Test Suite: {THERMALSENSOR_L3_TEST_SUITE_NAME}  Test Case: {testcase}")
        return results

    def _emitVcomponentLogs(self, testcase: str):
        """
        Reads the vcomponent log file from the target via hal_session_1 and emits
        each line into the VTS log, then truncates the file so the next test case
        sees only its own output.

        Args:
            testcase (str): Test case name, used only for the log header.
        """
        try:
            prompt = self.cpe.get("prompt", "~#")
            self.hal_session_1.write(f"cat {self.vcomponentLogFile}")
            logContent = self.hal_session_1.read_until(prompt, timeout=5)
            self.log.info(f"--- vcomponent logs for {testcase} ---")
            import re
            _ansiRe = re.compile(r'\x1b\[[0-9;]*[mK]')
            for line in logContent.splitlines():
                stripped = _ansiRe.sub('', line).strip()
                if stripped:
                    # Replace non-ASCII characters (e.g. → arrows) so the
                    # message can be encoded on any platform (including
                    # Windows cp1252 terminals).
                    safe = stripped.encode('ascii', errors='replace').decode('ascii')
                    self.log.info(f"[vcomponent] {safe}")
            self.log.info(f"--- end vcomponent logs for {testcase} ---")
            self.hal_session_1.write(f"> {self.vcomponentLogFile}")
        except Exception as exc:
            self.log.warning(f"[vcomponent log capture] Could not read log file: {exc}")
