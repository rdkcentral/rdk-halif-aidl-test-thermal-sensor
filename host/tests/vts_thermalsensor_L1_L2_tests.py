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

import os
import sys

dir_path = os.path.dirname(os.path.realpath(__file__))
sys.path.append(os.path.join(dir_path, "../"))
sys.path.append(os.path.join(dir_path, "..", "raft"))

from raft.framework.plugins.ut_raft.configRead import ConfigRead
from raft.framework.core.logModule import logModule
from vts_thermalsensor_helper import thermalSensorHelperClass

THERMALSENSOR_TESTCASES_CONFIGURATION_FILE = os.path.join(
    dir_path, "../configs/vts_thermalsensor_testcase_configuration.yaml")

THERMALSENSOR_L1_TEST_SUITE_NAME = "VTS_L1_THERMALSENSOR"



class VTS_THERMALSENSOR_L1_L2_Tests(thermalSensorHelperClass):
    """
    Runs automated L1 and L2 ThermalSensor tests on the target device.
    Handles setup, service restarts, asset management.
    Collects and aggregates test results for pass/fail reporting.
    """

    def __init__(self, log: logModule = None, moduleName: str = None):
        """
        Initializes the test class.
        """
        qcId = "1"
        self.testName = "ThermalSensor_L1_L2"
        super().__init__(self.testName, qcId, log, moduleName)

        self.testCaseConfiguration = ConfigRead(
            THERMALSENSOR_TESTCASES_CONFIGURATION_FILE, self.moduleName)

    def runTest(self, testSuite, test_case: str = None):
        """
        Runs the test case passed to this function.

        Args:
            testSuite (dict): Test suite dict containing 'name' and 'test_cases'.
            test_case (str, optional): Test case name to run; None runs all tests.

        Returns:
            bool: True - test pass, False - test fails.
        """
        testsuite_name = testSuite.get("name")
        output = self.utMenu.select(testsuite_name, test_case, None, self.waitTime, False)
        results = self.utMenu.collect_results(output, True)
        if results is None:
            results = False
        return results

    def testFunction(self):
        """
        Main test function to run L1 and L2 tests.

        Returns:
            bool: Final aggregated result of all test cases.
        """
        self.utilities.change_directory(self.hal_session, self.targetWorkspace)
        self.utMenu.start()

        testsuites = self.testCaseConfiguration.fields.get('test_suites')
        finalresult = True

        for testsuite in testsuites:
            testsuite_name = testsuite.get("name")
            if testsuite_name not in (
                THERMALSENSOR_L1_TEST_SUITE_NAME
            ):
                continue

            test_cases = testsuite.get("test_cases")

            if len(test_cases) == 1 and test_cases[0] == "all":
                self.log.stepStart(f'Test Suite: {testsuite_name} Run all Test Cases')
                result = self.runTest(testsuite)
                self.log.stepResult(result, f'Test Suite: {testsuite_name} Run all Test Cases')
                if not result:
                    finalresult = False
            else:
                for test_case in test_cases:
                    self.log.stepStart(f'Test Suite: {testsuite_name} Test Case: {test_case}')
                    result = self.runTest(testsuite, test_case)
                    if not result:
                        finalresult = False
                    self.log.stepResult(result, f'Test Suite: {testsuite_name} Test Case: {test_case}')

        return finalresult


if __name__ == '__main__':
    summaryLogName = os.path.splitext(os.path.basename(__file__))[0] + "_summary"
    summaryLog = logModule(summaryLogName, level=logModule.INFO)
    test = VTS_THERMALSENSOR_L1_L2_Tests(summaryLog)
    test.run(False)
