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

import subprocess
import re
import time
import test
import pytest
from testlib.resctrl import Resctrl
from testlib.env import Env
from priority import PRIORITY_HIGH

class TestRdtsetMba(test.Test):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self, request):
        super().init(request)
        yield
        super().fini()
    ## @endcond


    ## RDTSET - MBA Set COS definition (command)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Run command with provided MBA rate
    #
    #  \b Instruction:
    #  1. Run the "rdtset [-I] -t mba=50;cpu=5-6' -c 5-6 memtester 10M" to set cache allocation
    #  2. Verify cache allocation with "pqos [-I] -s" command
    #  3. Terminate memtester process
    #
    #  \b Result:
    #  Observe in pqos output
    #  MBA COS7 => 50% available
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("mba")
    def test_rdtset_mba_set_command(self, iface):
        command = self.cmd_rdtset(iface, "-t mba=50;cpu=5-6 -c 5-6 memtester 10M")
        rdtset = subprocess.Popen(command.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE)

        time.sleep(0.1)

        (stdout, _, exitstatus) = self.run_pqos(iface, "-s")
        assert exitstatus == 0

        if iface == "MSR":
            last_cos = Env().get('mba', 'cos') - 1
        else:
            last_cos = Resctrl.get_ctrl_group_count() - 1

        assert re.search("Core 5, L2ID [0-9]+, L3ID [0-9]+ => COS%d" % last_cos, stdout) \
               is not None
        assert re.search("Core 6, L2ID [0-9]+, L3ID [0-9]+ => COS%d" % last_cos, stdout) \
               is not None
        assert "MBA COS{} => 50% available".format(last_cos) in stdout

        self.run("killall memtester")
        rdtset.communicate()



    ## RDTSET - MBA Set COS definition (command) - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Unable to run command with invalid MBA rate
    #
    #  \b Instruction:
    #  Run the "rdtset [-I] -t mba=200;cpu=5-6' -c 5-6 memtester 10M" to set cache allocation
    #
    #  \b Result:
    #  Observe "Invalid RDT parameters" in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("mba")
    def test_rdtset_mba_command_negative(self, iface):
        (_, stderr, exitstatus) = self.run_rdtset(iface, "-t mba=200;cpu=5-6 -c 5-6 memtester 10M")
        assert exitstatus == 1
        assert "Invalid RDT parameters" in stderr
