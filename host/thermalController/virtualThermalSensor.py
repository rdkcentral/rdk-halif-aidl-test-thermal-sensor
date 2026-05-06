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

import json
import time
from ThermalSensorControllerInterface import ThermalSensorControllerInterface


class virtualThermalSensor(ThermalSensorControllerInterface):
    """
    Virtual Thermal Sensor controller.
    Sends YAML commands to the vcomponent control-plane via HTTP POST.
    """

    def __init__(self, session, prompt: str = "~#", port: int = 8080):
        super().__init__(session, prompt, port)

    def triggerThermalStateChange(self, state: str, sensorName: str = "",
                                  temperatureCelsius: float = 0.0,
                                  timestampMonotonicMs: int = 0):
        """
        Send a THERMAL_STATE_CHANGE command to the vcomponent control-plane.

        The vcomponent expects:
          IThermalSensor/command: THERMAL_STATE_CHANGE
          IThermalSensor/state: <state>
          IThermalSensor/sensorName: <sensorName>          (optional)
          IThermalSensor/temperatureCelsius: <temp>         (optional)
          IThermalSensor/timestampMonotonicMs: <ts>
        """
        if timestampMonotonicMs == 0:
            # Use wall-clock milliseconds as a reasonable monotonic substitute
            timestampMonotonicMs = int(time.time() * 1000)

        payload = {
            "IThermalSensor": {
                "command": "THERMAL_STATE_CHANGE",
                "state": state,
                "timestampMonotonicMs": timestampMonotonicMs,
            }
        }

        if sensorName:
            payload["IThermalSensor"]["sensorName"] = sensorName
            payload["IThermalSensor"]["temperatureCelsius"] = temperatureCelsius

        yaml_body = json.dumps(payload)

        # POST to the control-plane endpoint running on the target device
        cmd = (
            "curl -s -o /dev/null -w '%{http_code}' "
            "-X POST "
            "-H 'Content-Type: application/json' "
            f"-d '{yaml_body}' "
            f"http://localhost:{self.port}/api/postKVP"
        )

        self.session.write(cmd)
        self.session.expect(self.prompt)

    def injectTemperatureUpdate(self, sensorName: str, temperatureCelsius: float,
                                timestampMonotonicMs: int = 0):
        """
        Send a TEMPERATURE_UPDATE command to the vcomponent control-plane.

        The vcomponent evaluates the temperature against its configured operational
        thresholds and auto-fires the appropriate state-change event.
        """
        if timestampMonotonicMs == 0:
            timestampMonotonicMs = int(time.time() * 1000)

        payload = {
            "IThermalSensor": {
                "command": "TEMPERATURE_UPDATE",
                "sensorName": sensorName,
                "temperatureCelsius": temperatureCelsius,
                "timestampMonotonicMs": timestampMonotonicMs,
            }
        }

        yaml_body = json.dumps(payload)

        cmd = (
            "curl -s -o /dev/null -w '%{http_code}' "
            "-X POST "
            "-H 'Content-Type: application/json' "
            f"-d '{yaml_body}' "
            f"http://localhost:{self.port}/api/postKVP"
        )

        self.session.write(cmd)
        self.session.expect(self.prompt)
