################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
################################################################################

import test
import re
import pytest
from testlib.env import Env
from priority import PRIORITY_HIGH, PRIORITY_MEDIUM

class TestPqosL3Cat(test.Test):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self, request):
        super().init(request)
        yield
        super().fini()
    ## @endcond

    ## PQOS - L3 CAT Detection - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify L2 CAT capability is not detected on platform
    #
    #  \b Instruction:
    #  Run "pqos [-I] -s -v" to print platform RDT capabilities and configuration in verbose
    #  mode
    #
    #  \b Result:
    #  Observe "L2CA capability not detected" in output
    @PRIORITY_MEDIUM
    @pytest.mark.rdt_unsupported("cat_l3")
    def test_pqos_l3cat_detection_negative(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-s -v")
        assert exitstatus == 0
        assert "L3CA capability not detected" in stdout


    ## PQOS - L3 CAT Detection
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify L3 CAT capability is detected on platform
    #
    #  \b Instruction:
    #  Run "pqos [-I] -s -v" to print platform RDT capabilities and configuration in verbose
    #  mode
    #
    #  \b Result:
    #  Observe "L3CA capability detected" in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l3")
    def test_pqos_l3cat_detection(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-s -v")
        assert exitstatus == 0
        assert "L3CA capability detected" in stdout


    ## PQOS - L3 CAT Set COS definition
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Able to set up allocation COS
    #
    #  \b Instruction:
    #  1. Run the "pqos [-I] -e 'llc:1=0xf;llc:2=0xf0'" to set COS bitmask.
    #  2. Verify values with "pqos -s"
    #
    #  \b Result:
    #  1. Observe in output
    #     SOCKET 0 L3CA COS1 => MASK 0xf
    #     SOCKET 0 L3CA COS2 => MASK 0xf0
    #     Allocation configuration altered.
    #  2. L3CA COS1 MASK for socket 0 is set to 0xf
    #     L3CA COS2 MASK for socket 0 is set to 0xf0
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l3")
    def test_pqos_l3cat_set(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-e llc:1=0xf;llc:2=0xf0")
        assert exitstatus == 0
        assert "SOCKET 0 L3CA COS1 => MASK 0xf" in stdout
        assert "SOCKET 0 L3CA COS2 => MASK 0xf0" in stdout
        assert "Allocation configuration altered" in stdout

        (stdout, _, exitstatus) = self.run_pqos(iface, "-s")
        assert exitstatus == 0
        assert "L3CA COS1 => MASK 0xf" in stdout
        assert "L3CA COS2 => MASK 0xf0" in stdout


    ## PQOS - L3 CAT Set COS definition - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Unable to set up allocation COS with a bitmask more than 28 bits
    #
    #  \b Instruction:
    #  Run "pqos [-I] -e llc:2=0xffffffff" to set the COS bitmask which is more than 28 bits.
    #
    #  \b Result:
    #  Observe "SOCKET 0 L3CA COS2 - FAILED!" and "Allocation configuration error!" in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l3")
    def test_pqos_l3cat_set_negative(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-e llc:2=0xffffffff")
        assert exitstatus == 1
        assert "SOCKET 0 L3CA COS2 - FAILED" in stdout
        assert "Allocation configuration error!" in stdout


    ## PQOS - L3 CAT Set COS association (core)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Allocate COS for the given cores
    #
    #  \b Instruction:
    #  Run "pqos [-I] -a llc:7=1,3" to set core association. Verify core association with
    #  "pqos [-I] -s".
    #
    #  \b Result:
    #  Observe "Allocation configuration altered." in output. Cores 1 and 3 are assigned to COS 7.
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l3")
    def test_pqos_l3cat_association_core(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-a llc:7=1,3")
        assert exitstatus == 0
        assert "Allocation configuration altered." in stdout

        (stdout, _, exitstatus) = self.run_pqos(iface, "-s")
        assert exitstatus == 0
        assert re.search("Core 1, L2ID [0-9]+, L3ID [0-9]+ => COS7", stdout) is not None
        assert re.search("Core 3, L2ID [0-9]+, L3ID [0-9]+ => COS7", stdout) is not None


    ## PQOS - L3 CAT Set COS association (core) - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Unable to allocate COS for the unknown/offline cores
    #
    #  \b Instruction:
    #  Run "pqos [-I] -a llc:7=1000" to set core association.
    #
    #  \b Result:
    #  Observe "Core number or class id is out of bounds!" in output.
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l3")
    def test_pqos_l3cat_association_core_negative(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-a llc:7=1000")
        assert exitstatus == 1
        assert "Core number or class id is out of bounds!" in stdout


    ## PQOS - L3 CAT Set COS association (tasks)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Allocate COS for the given pids
    #
    #  \b Instruction:
    #  Run "pqos -I -a pid:2=1" to set pid association.
    #
    #  \b Result:
    #  Observe "Allocation configuration altered." in output.
    @PRIORITY_HIGH
    @pytest.mark.iface_os
    @pytest.mark.rdt_supported("cat_l3")
    def test_pqos_l3cat_association_tasks(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-a pid:2=1")
        assert exitstatus == 0
        assert "Allocation configuration altered." in stdout


    ## PQOS - L3 CAT Set COS association (tasks) - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Unable to allocate COS for the invalid/unknown pids
    #
    #  \b Instruction:
    #  Run "pqos -I -a pid:2=9999999999" to set pid association.
    #
    #  \b Result:
    #  Observe "Task ID number or class id is out of bounds!" in output.
    @PRIORITY_HIGH
    @pytest.mark.iface_os
    @pytest.mark.rdt_supported("cat_l3")
    def test_pqos_l3cat_association_tasks_negative(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-a pid:2=9999999999")
        assert exitstatus == 1
        assert "Task ID number or class id is out of bounds!" in stdout


    ## PQOS - L3 CAT Reset
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Reset L3 CAT settings
    #
    #  \b Instruction:
    #  Run "pqos [-I]  -R" to reset L3 CAT configuration.
    #
    #  \b Result:
    #  Observe "Allocation reset successful" in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l3")
    def test_pqos_l3cat_reset(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-R")
        assert exitstatus == 0
        assert "Allocation reset successful" in stdout


    ## PQOS - L3 CDP Detection
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify L3 CDP capability is detected on platform
    #
    #  \b Instruction:
    #  Run "pqos [-I] -s -v" to print platform RDT capabilities and configuration in verbose mode
    #
    #  \b Result:
    #  Observe "INFO: L3 CAT details: CDP support=1" in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cdp_l3")
    def test_pqos_l3cat_cdp_detection(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-s -v")

        assert exitstatus == 0
        assert "INFO: L3 CAT details: CDP support=1" in stdout


    ## PQOS - L3 CDP enable
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify L3 CDP can be enabled on platform
    #
    #  \b Instruction:
    #  Run "pqos [-I]  -R l3cdp-on -v" to enable L3 CDP.
    #  Verify that CDP was enabled with "pqos [-I] -s -V"
    #
    #  \b Result:
    #  Observe "Turning L3 CDP ON" and "L3 CDP is enabled" in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cdp_l3")
    def test_pqos_l3cat_cdp_enable(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-R l3cdp-on -v")
        assert exitstatus == 0
        assert iface != "MSR" or "Turning L3 CDP ON" in stdout

        (stdout, _, exitstatus) = self.run_pqos(iface, "-s -V")
        assert exitstatus == 0
        assert "L3 CAT details: CDP support=1, CDP on=1" in stdout


    ## PQOS - L3 CDP disable
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify L3 CAT can be disabled platform
    #
    #  \b Instruction:
    #  Run "pqos [-I]  -R l3cdp-off -v" to disable L3 CDP.
    #  Verify that CDP was disabled with "pqos [-I] -s -V"
    #
    #  \b Result:
    #  Observe "Turning L3 CDP OFF" and "L3 CDP is disabled" in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cdp_l3")
    def test_pqos_l3cat_cdp_disable(self, iface):
        self.run_pqos(iface, "-R l3cdp-on")

        (stdout, _, exitstatus) = self.run_pqos(iface, "-R l3cdp-off -v")
        assert exitstatus == 0
        assert iface != "MSR" or "Turning L3 CDP OFF" in stdout

        (stdout, _, exitstatus) = self.run_pqos(iface, "-s -V")
        assert exitstatus == 0
        assert "L3 CAT details: CDP support=1, CDP on=0" in stdout


    ## PQOS - L3 CDP Set COS definition - Code & Data
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify L3 CAT COS definition can be set for Code and Data
    #
    #  \b Instruction:
    #  Run "pqos [-I] -R l3cdp-on" to enable L3 CDP.
    #  Run "pqos [-I] -e llc:2=0xe" to set both code and data cbm.
    #
    #  \b Result:
    #  Observe the following in output "SOCKET 0 L3CA COS2 => DATA 0xe,CODE 0xe"
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cdp_l3")
    def test_pqos_l3cat_set_code_and_data(self, iface):
        self.run_pqos(iface, "-R l3cdp-on")

        (stdout, _, exitstatus) = self.run_pqos(iface, "-e llc:2=0xe")
        assert exitstatus == 0
        assert "SOCKET 0 L3CA COS2 => DATA 0xe,CODE 0xe" in stdout


    ## PQOS - L3 CDP Set COS definition - Code only
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify L3 CAT COS definition can be set for Code only
    #
    #  \b Instruction:
    #  Run "pqos [-I] -R l3cdp-on" to enable L3 CDP.
    #  Run "pqos [-I] -e llc:4c=0xf" to set both code and data cbm.
    #
    #  \b Result:
    #  Observe the following in output "SOCKET 0 L3CA COS4 => DATA 0x7ff,CODE 0xf"
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cdp_l3")
    def test_pqos_l3cat_set_code(self, iface):
        self.run_pqos(iface, "-R l3cdp-on")

        default_mask = (1 << Env().get('cat', 'l3', 'ways')) - 1

        (stdout, _, exitstatus) = self.run_pqos(iface, "-e llc:4c=0xf")
        assert exitstatus == 0
        assert ("SOCKET 0 L3CA COS4 => DATA {},CODE 0xf").format(hex(default_mask)) in stdout


    ## PQOS - L3 CDP Set COS definition - Data only
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify L3 CAT COS definition can be set for Code and Data
    #
    #  \b Instruction:
    #  Run "pqos [-I] -R l3cdp-on" to enable L3 CDP.
    #  Run "pqos [-I] -e llc:3d=0xf" to set both code and data cbm.
    #
    #  \b Result:
    #  Observe the following in output "SOCKET 0 L3CA COS3 => DATA 0xf,CODE 0x7ff"
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cdp_l3")
    def test_pqos_l3cat_set_data(self, iface):
        self.run_pqos(iface, "-R l3cdp-on")

        default_mask = (1 << Env().get('cat', 'l3', 'ways')) - 1

        (stdout, _, exitstatus) = self.run_pqos(iface, "-e llc:3d=0xf")
        assert exitstatus == 0
        assert ("SOCKET 0 L3CA COS3 => DATA 0xf,CODE {}").format(hex(default_mask)) in stdout
