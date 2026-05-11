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
             -> CRITICAL_TEMPERATURE_EXCEEDED
             -> event.temperatureReading reflects primary (highest severity)
  Phase 1b : inject primary sensor at the same overTempCelsius again
             -> no event (idempotent; state already EXCEEDED)
  Phase 2  : inject primary sensor at recoveryTempCelsius (<= criticalRecovered)
             -> CRITICAL_TEMPERATURE_RECOVERED
  Phase 3  : inject primary sensor at normalTempCelsius (well within range)
             -> NORMAL

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


class VTS_THERMALSENSOR_L3_ThermalExceedRecoveryLifecycle(thermalSensorHelperClass):
    """
    TC1: NORMAL -> CRITICAL_TEMPERATURE_EXCEEDED -> CRITICAL_TEMPERATURE_RECOVERED -> NORMAL with aggregation and
    dual-listener validation.
    """

    def __init__(self, log: logModule = None):
        self.testName = "ThermalSensor_L3_ThermalExceedRecoveryLifecycle"
        qcId = "1"
        super().__init__(self.testName, qcId, log)

    def _trigger(self, pri, sec, states, stateToTempKey):

        # Reset secondary to its safe temperature before the sequence
        self.log.info(
            f"[TC{self.qcId}] Pre-warm: '{sec['name']}' at {sec['safe_temp']} Celsius")

        self.thermalSensorDevice.injectTemperatureUpdate(
            sensorName=sec['name'],
            temperatureCelsius=sec['safe_temp'])

        time.sleep(INTER_PHASE_DELAY_S)

        prevState = states[0] if states else 'NORMAL'

        for step, state in enumerate(states[1:], start=1):
            tempKey = stateToTempKey.get(state)
            if tempKey is None:
                self.log.info(
                    f"[TC{self.qcId}] Step {step}: unknown state '{state}', skipping")
                continue

            injectTemp = pri[tempKey]
            self.log.info(
                f"[TC{self.qcId}] Step {step}: '{pri['name']}' at {injectTemp} Celsius "
                f"({prevState} -> {state})")
            self.thermalSensorDevice.injectTemperatureUpdate(
                sensorName=pri['name'],
                temperatureCelsius=injectTemp)
            time.sleep(INTER_PHASE_DELAY_S)

            # For NORMAL transition also reset secondary sensor
            if state == 'NORMAL':
                self.log.info(
                    f"[TC{self.qcId}] Step {step}b: '{sec['name']}' at "
                    f"{sec['safe_temp']} Celsius (secondary reset for NORMAL)")
                self.thermalSensorDevice.injectTemperatureUpdate(
                    sensorName=sec['name'],
                    temperatureCelsius=sec['safe_temp'])
                time.sleep(INTER_PHASE_DELAY_S)

            prevState = state

    def _runSensorPair(self, primary, secondary, transitionStates, stateToTempKey, triggerDelay) -> bool:
        """
        Runs one (primary, secondary) sensor pair through the full transition
        sequence and returns the pass/fail result.

        When sensorCount == 1, primary and secondary are the same sensor; the
        pre-warm injection still sets a safe baseline before the sequence starts.
        """
        self.log.info(
            f"[TC{self.qcId}] primary='{primary['name']}' "
            f"secondary='{secondary['name']}'")
        self.updateTransitionStates(transitionStates)
        return self.runL3Test(
            "ThermalExceedRecoveryLifecycle",
            triggerCallable=lambda pri=primary, sec=secondary, states=transitionStates: self._trigger(pri, sec, states, stateToTempKey),
            triggerDelay=triggerDelay)

    def testFunction(self) -> bool:
        """
        Main test function.

        Reads the ordered transition_states list from the L3 YAML and all N
        sensors from the HFP profile.  For every (primary, secondary) sensor
        pair (i != j), drives the vcomponent through each consecutive state
        transition in the list and verifies the C++ binary receives the
        expected event for each step.

        When only one sensor is present in the HFP profile, the test runs once
        using that sensor as both primary and secondary.

        State-to-injection mapping (derived from HFP thresholds per primary):
          NORMAL   -> inject primary at normal_temp  (midpoint of reading range)
          CRITICAL_TEMPERATURE_EXCEEDED -> inject primary at over_temp    (exceeded threshold + 2°C)
          CRITICAL_TEMPERATURE_RECOVERED-> inject primary at recovery_temp(recovered threshold - 2°C)

        Before each sequence the secondary sensor is reset to its own safe_temp
        (recovered - 10°C) to avoid cross-sensor interference.

        Returns:
            bool: True if ALL sensor pairs passed, False otherwise.
        """
        with open(self.l3TestConfigFile, 'r') as fh:
            l3cfg = yaml.safe_load(fh)

        testNode  = l3cfg['thermalsensor']['L3']
        timeoutMs = int(testNode.get('event_timeout_ms', 15000))

        # TC1 always uses the fixed exceed-and-recovery sequence regardless of
        # what is currently stored in the YAML (which may have been patched by
        # a prior TC2 run).  updateTransitionStates() in _runSensorPair will
        # overwrite the YAML with this sequence before uploading to the DUT.
        transitionStates = [
            'NORMAL',
            'CRITICAL_TEMPERATURE_EXCEEDED',
            'CRITICAL_TEMPERATURE_RECOVERED',
            'NORMAL',
        ]

        sensors = self.getSensorListFromHFP()

        sensorCount = len(sensors)
        self.log.info(
            f"[TC{self.qcId}] HFP has {sensorCount} sensor(s): {[s['name'] for s in sensors]}")
        self.log.info(
            f"[TC{self.qcId}] transition_states sequence: {transitionStates}")

        # Map state name -> injection temperature key in the sensor dict
        stateToTempKey = {
            'CRITICAL_TEMPERATURE_EXCEEDED':  'over_temp',
            'CRITICAL_TEMPERATURE_RECOVERED': 'recovery_temp',
            'NORMAL':                         'normal_temp',
        }

        triggerDelay = (timeoutMs / 1000.0) / 2.0
        self.utilities.change_directory(self.hal_session, self.targetWorkspace)

        overallResult = True

        if sensorCount == 1:
            # Single sensor: use it as both primary and secondary
            self.log.info(f"[TC{self.qcId}] Single sensor — running self-pair")
            overallResult = self._runSensorPair(
                sensors[0], sensors[0], transitionStates, stateToTempKey, triggerDelay)
        else:
            # Multiple sensors: test every (primary, secondary) permutation
            for i in range(sensorCount):
                for j in range(sensorCount):
                    if j == i:
                        continue
                    pairResult = self._runSensorPair(
                        sensors[i], sensors[j], transitionStates, stateToTempKey, triggerDelay)
                    if not pairResult:
                        self.log.info(
                            f"[TC{self.qcId}] FAILED for primary='{sensors[i]['name']}' "
                            f"secondary='{sensors[j]['name']}'")
                        overallResult = False

        return overallResult


if __name__ == "__main__":
    summaryLogName = os.path.splitext(os.path.basename(__file__))[0] + "_summary"
    summaryLog = logModule(summaryLogName, level=logModule.INFO)
    test = VTS_THERMALSENSOR_L3_ThermalExceedRecoveryLifecycle(summaryLog)
    test.run(False)
