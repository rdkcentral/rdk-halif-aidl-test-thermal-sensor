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
import time
import yaml

dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(dir_path)
sys.path.append(os.path.join(dir_path, "../"))

from thermalController.ThermalSensorControllerInterface import ThermalSensorControllerInterface


class virtualThermalSensor(ThermalSensorControllerInterface):
    """
    Virtual ThermalSensor controller that sends YAML commands to the vcomponent
    control-plane endpoint via utPlaneController to simulate thermal events.
    """

    def __init__(self, session, prompt: str = "~#", port: int = 8080):
        super().__init__(session, prompt, port)

    def _sendCommand(self, payload: dict):
        """
        Serialises *payload* as YAML, writes it to a temp file on the device
        via a bash heredoc, then POSTs it to the vcomponent control-plane.

        Args:
            payload (dict): Command payload to serialise and POST.
        """
        yaml_str = yaml.dump(payload)
        tmp = "/tmp/ts_cp.yaml"

        # Write YAML to a temp file using a bash heredoc.
        # session.write(list) sends each element as a separate line.
        heredoc_lines = [f"cat > {tmp} << 'YAML_EOF'"]
        heredoc_lines.extend(yaml_str.rstrip("\n").splitlines())
        heredoc_lines.append("YAML_EOF")
        self.session.write(heredoc_lines)

        # POST the file — no quoting issues, newlines preserved exactly.
        self.session.write(
            f'curl -s -X POST -H "Content-Type: application/x-yaml" '
            f'--data-binary @{tmp} "http://localhost:{self.port}/api/postKVP"'
        )

    def triggerThermalStateChange(self, state: str, sensorName: str = "",
                                  temperatureCelsius: float = 0.0,
                                  timestampMonotonicMs: int = 0):
        """
        Simulate a thermal state-change event.

        Sends a thermal_state_change command to the vcomponent control-plane.
        The vcomponent expects:
          IThermalSensor/command:              thermal_state_change
          IThermalSensor/state:                <state string>
          IThermalSensor/sensorName:           <sensorName>          (optional)
          IThermalSensor/temperatureCelsius:   <temp>                (optional)
          IThermalSensor/timestampMonotonicMs: <ts>

        Args:
            state (str): Target state.  One of:
                         "NORMAL" | "CRITICAL_TEMPERATURE_EXCEEDED" |
                         "CRITICAL_TEMPERATURE_RECOVERED" | "CRITICAL_SHUTDOWN_IMMINENT"
            sensorName (str): Optional sensor name for the TemperatureReading payload.
                              Pass empty string to omit the temperature reading.
            temperatureCelsius (float): Temperature to include in the payload (if sensorName given).
            timestampMonotonicMs (int): Monotonic timestamp (ms); 0 uses wall-clock ms.
        """
        if timestampMonotonicMs == 0:
            timestampMonotonicMs = int(time.time() * 1000)

        payload = {
            "IThermalSensor": {
                "command": "thermal_state_change",
                "state": state,
                "timestampMonotonicMs": timestampMonotonicMs,
            }
        }

        if sensorName:
            payload["IThermalSensor"]["sensorName"] = sensorName
            payload["IThermalSensor"]["temperatureCelsius"] = temperatureCelsius

        self._sendCommand(payload)

    def injectTemperatureUpdate(self, sensorName: str, temperatureCelsius: float,
                                timestampMonotonicMs: int = 0):
        """
        Inject a raw temperature reading into the vcomponent.

        Sends a temperature_update command to the vcomponent control-plane.
        The vcomponent evaluates the value against the configured operational
        thresholds and automatically fires the appropriate state-change event:

          temp >= shutdown_threshold             → CRITICAL_SHUTDOWN_IMMINENT
          temp >  max  ||  temp < min            → CRITICAL_TEMPERATURE_EXCEEDED
          temp <= recovered (from EXCEEDED)      → CRITICAL_TEMPERATURE_RECOVERED
          temp in safe range (from RECOVERED)    → NORMAL

        Args:
            sensorName (str): Name of the sensor reporting the temperature.
            temperatureCelsius (float): Current temperature in degrees Celsius.
            timestampMonotonicMs (int): Monotonic timestamp (ms); 0 uses wall-clock ms.
        """
        if timestampMonotonicMs == 0:
            timestampMonotonicMs = int(time.time() * 1000)

        payload = {
            "IThermalSensor": {
                "command": "temperature_update",
                "sensorName": sensorName,
                "temperatureCelsius": temperatureCelsius,
                "timestampMonotonicMs": timestampMonotonicMs,
            }
        }

        self._sendCommand(payload)
