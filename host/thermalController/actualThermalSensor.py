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

dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(dir_path)


from thermalController.ThermalSensorControllerInterface import ThermalSensorControllerInterface


class actualThermalSensor(ThermalSensorControllerInterface):
    """
    Actual (hardware) Thermal Sensor controller.
    Currently not implemented — thermal state is managed by the platform firmware.
    """

    def __init__(self, session, prompt: str = "~#", port: int = 8080):
        super().__init__(session, prompt, port)

    def triggerThermalStateChange(self, state: str, sensorName: str = "",
                                  temperatureCelsius: float = 0.0,
                                  timestampMonotonicMs: int = 0):
        """
        Hardware thermal state-change trigger.
        Not implemented for real hardware — thermal events are raised by firmware.
        """
        raise NotImplementedError(
            "actualThermalSensor.triggerThermalStateChange: "
            "not implemented for real hardware"
        )

    def injectTemperatureUpdate(self, sensorName: str, temperatureCelsius: float,
                                timestampMonotonicMs: int = 0):
        """
        Raw temperature injection.
        Not implemented for real hardware — temperature is read from physical sensors.
        """
        raise NotImplementedError(
            "actualThermalSensor.injectTemperatureUpdate: "
            "not implemented for real hardware"
        )
