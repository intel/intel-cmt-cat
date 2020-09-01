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
from priority import PRIORITY_HIGH

class TestPqosMba(test.Test):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self, request):
        super().init(request)
        yield
        super().fini()
    ## @endcond


    ## PQOS - MBA Detection
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify MBA capability is detected on platform
    #
    #  \b Instruction:
    #  Run "pqos [-I] -s -v" to print platform RDT capabilities and configuration in verbose
    #  mode
    #
    #  \b Result:
    #  Observe "MBA capability detected" in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("mba")
    def test_pqos_mba_detection(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-s -v")
        assert exitstatus == 0
        assert "MBA capability detected" in stdout


    ## PQOS - MBA Set COS definition
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify setting MBA COS definition to 50%
    #
    #  \b Instruction:
    #  Run "pqos [-I] -e 'mba:2=50'" to set mba rate.
    #
    #  \b Result:
    #  Observe the following in output
    #  SOCKET 0 MBA COS2 => 50% requested, 50% applied
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("mba")
    @pytest.mark.parametrize("rate", [20, 50, 90])
    def test_pqos_mba_set(self, iface, rate):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-e mba:2=%d" % rate)
        assert exitstatus == 0
        assert ("SOCKET 0 MBA COS2 => {rate}% requested, {rate}% applied").format(rate=rate) \
            in stdout


    ## PQOS - MBA Set COS definition - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Unable to set MBA COS definition to invalid value
    #
    #  \b Instruction:
    #  Run "pqos [-I] -e 'mba:2=200'" to set mba rate.
    #
    #  \b Result:
    #  Observe "MBA COS2 rate out of range (from 1-100)" in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("mba")
    def test_pqos_mba_set_negative(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-e mba:2=200")
        assert exitstatus == 1
        assert "MBA COS2 rate out of range (from 1-100)" in stdout


    ## PQOS - MBA Set COS association (core)
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
    @pytest.mark.rdt_supported("mba")
    def test_pqos_mba_association_core(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-a llc:7=1,3")
        assert exitstatus == 0
        assert "Allocation configuration altered." in stdout

        (stdout, _, exitstatus) = self.run_pqos(iface, "-s")
        assert exitstatus == 0
        assert re.search("Core 1, L2ID [0-9]+, L3ID [0-9]+ => COS7", stdout) is not None
        assert re.search("Core 3, L2ID [0-9]+, L3ID [0-9]+ => COS7", stdout) is not None


    ## PQOS - MBA Set COS association (core) - Negative
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
    @pytest.mark.rdt_supported("mba")
    def test_pqos_mba_association_core_negative(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-a llc:7=1000")
        assert exitstatus == 1
        assert "Core number or class id is out of bounds!" in stdout


    ## PQOS - MBA Set COS association (tasks)
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
    @pytest.mark.rdt_supported("mba")
    def test_pqos_mba_association_tasks(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-a pid:2=1")
        assert exitstatus == 0
        assert "Allocation configuration altered." in stdout


    ## PQOS - MBA Set COS association (tasks) - Negative
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
    @pytest.mark.rdt_supported("mba")
    def test_pqos_mba_association_tasks_negative(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-a pid:2=9999999999")
        assert exitstatus == 1
        assert "Task ID number or class id is out of bounds!" in stdout
