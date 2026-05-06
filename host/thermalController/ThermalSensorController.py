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

from virtualThermalSensor import virtualThermalSensor
from actualThermalSensor import actualThermalSensor


class ThermalSensorController:
    """
    Factory class — returns the correct controller implementation based on platform.
    """

    def __new__(cls, platform: str, session, prompt: str = "~#", port: int = 8080):
        """
        Create and return the appropriate thermal sensor controller.

        Args:
            platform (str): Platform type.  "vDevice" returns a virtualThermalSensor.
                            Any other value returns an actualThermalSensor.
            session: Active pexpect / raft session on the target device.
            prompt (str): Shell prompt string used to detect command completion.
            port (int): Control-plane HTTP port the vcomponent is listening on.

        Returns:
            ThermalSensorControllerInterface implementation.
        """
        if platform.lower() == "vdevice":
            return virtualThermalSensor(session, prompt, port)
        return actualThermalSensor(session, prompt, port)
