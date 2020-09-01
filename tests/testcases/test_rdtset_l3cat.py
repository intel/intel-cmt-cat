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
import time
import test
import re
import pytest
from testlib.resctrl import Resctrl
from testlib.env import Env
from priority import PRIORITY_HIGH

class TestRdtsetL3Cat(test.Test):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self, request):
        super().init(request)
        yield
        super().fini()
    ## @endcond


    ## RDTSET - L3 CAT Set COS definition (command)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Run command with provided L3 CAT mask
    #
    #  \b Instruction:
    #  1. Run the "rdtset [-I] -t 'l3=0xf;cpu=5-6' -c 5-6 memtester 10M" to set cache allocation
    #  2. Verify cache allocation with "pqos [-I] -s" command
    #  3. Terminate memtester process
    #
    #  \b Result:
    #  Observe in pqos output
    #  Core 5, L2ID 5, L3ID 0 => COS7
    #  Core 6, L2ID 6, L3ID 0 => COS7
    #  L3CA COS7 => MASK 0xf
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l3")
    def test_rdtset_l3cat_set_command(self, iface):
        param = "-t l3=0xf;cpu=5-6 -c 5-6 memtester 10M"
        command = self.cmd_rdtset(iface, param)
        rdtset = subprocess.Popen(command.split(), stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE)

        time.sleep(0.1)

        (stdout, _, exitstatus) = self.run_pqos(iface, "-s")
        assert exitstatus == 0
        if iface == "MSR":
            last_cos = Env().get('cat', 'l3', 'cos') - 1
        else:
            last_cos = Resctrl.get_ctrl_group_count() - 1

        assert re.search("Core 5, L2ID [0-9]+, L3ID [0-9]+ => COS%d" % last_cos, stdout) \
               is not None
        assert re.search("Core 6, L2ID [0-9]+, L3ID [0-9]+ => COS%d" % last_cos, stdout) \
               is not None
        assert "L3CA COS%d => MASK 0xf" % last_cos in stdout

        self.run("killall memtester")
        rdtset.communicate()



    ## RDTSET - L3 CAT Set COS definition (command) - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Unable to run command with too long L3 CAT bitmask
    #
    #  \b Instruction:
    #  Run the "rdtset [-I] -t 'l3=0xffffffff;cpu=5-6' -c 5-6 memtester 10M" to
    #  set cache allocation
    #
    #  \b Result:
    #  Observe "One or more of requested L3 CBMs (MASK: 0xffffffff) not
    #  supported by system (too long) in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l3")
    def test_rdtset_l3cat_set_command_negative(self, iface):
        param = "-t l3=0xffffffff;cpu=5-6 -c 5-6 memtester 10M"
        (_, stderr, exitstatus) = self.run_rdtset(iface, param)
        assert exitstatus == 1
        assert ('One or more of requested L3 CBMs (MASK: 0xffffffff) not supported by system '
                '(too long)') in stderr


    ## RDTSET - L3 CAT Set COS definition (task)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Set L3 CAT bitmask for Task Id
    #
    #  \b Instruction:
    #  1. Run the "Run the rdtset -I -t 'l3=0xf' -c 5-6 -p 1" to set cache
    #     allocation
    #  2. Verify cache allocation with "pqos -I -s" command
    #
    #  \b Result:
    # Observe in pqos output
    #  * COS7 => 1
    #  * L3CA COS7 => MASK 0xf
    @PRIORITY_HIGH
    @pytest.mark.iface_os
    @pytest.mark.rdt_supported("cat_l3")
    def test_rdtset_l3cat_set_task(self, iface):
        (_, _, exitstatus) = self.run_rdtset(iface, "-t l3=0xf -c 5-6 -p 1")
        assert exitstatus == 0

        (stdout, _, exitstatus) = self.run_pqos(iface, "-s")
        assert exitstatus == 0
        last_cos = Resctrl.get_ctrl_group_count() - 1
        assert ("COS{} => 1").format(last_cos) in stdout
        assert ("L3CA COS{} => MASK 0xf").format(last_cos) in stdout


    ## RDTSET - L3 CAT Set COS definition (task) - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Unable to set too long L3 CAT bitmask for the task ID
    #
    #  \b Instruction:
    #  Run the "Run the rdtset -I -t 'l3=0xffffffff' -p 1" to set cache allocation
    #
    #  \b Result:
    #  Observe "One or more of requested L3 CBMs (MASK: 0xffffffff) not supported
    #  by system (too long) in output
    @PRIORITY_HIGH
    @pytest.mark.iface_os
    @pytest.mark.rdt_supported("cat_l3")
    def test_rdtset_l3cat_set_task_negative(self, iface):
        (_, stderr, exitstatus) = self.run_rdtset(iface, "-t l3=0xffffffff -p 1")
        assert exitstatus == 1
        assert ('One or more of requested L3 CBMs (MASK: 0xffffffff) not supported by system '
                '(too long)') in stderr
