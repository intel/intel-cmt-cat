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
import pytest
from priority import PRIORITY_HIGH

class TestRdtsetAffinity(test.Test):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self, request):
        super().init(request)
        yield
        super().fini()
    ## @endcond


    ## RDTSET - Set CPU affinity single core (command)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  CPU affinity for command is set to single CPU
    #
    #  \b Instruction:
    #  1. Run the "rdtset [-I] -c 4 memtester 10M" to set cpu affinity
    #  2. Verify affinity with "taskset -p <memtester pid>" command
    #  3. Terminate memtester process
    #
    #  \b Result:
    #  Observe "current affinity mask: 10" in taskset output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("rdt_a")
    def test_rdtset_affinity_command_single_core(self, iface):
        command = self.cmd_rdtset(iface, "-c 4 memtester 10M")
        rdtset = subprocess.Popen(command.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE)

        time.sleep(0.1)

        child = self.get_pid_children(rdtset.pid)
        assert len(child) == 1

        (stdout, _, exitstatus) = self.run("taskset -p %s" % child[0])
        assert exitstatus == 0
        assert "current affinity mask: 10" in stdout

        self.run("killall memtester")
        rdtset.communicate()


    ## RDTSET - Set CPU affinity multiple cores (command)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  CPU affinity for command is set to CPU group
    #
    #  \b Instruction:
    #  1. Run the "rdtset [-I] -c 4-5 memtester 10M" to set cpu affinity
    #  2. Verify affinity with "taskset -p <memtester pid>" command
    #  3. Terminate memtester process
    #
    #  \b Result:
    #  Observe "current affinity mask: 30" in taskset output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("rdt_a")
    def test_rdtset_affinity_command_multiple_cores(self, iface):
        command = self.cmd_rdtset(iface, "-c 4-5 memtester 10M")
        rdtset = subprocess.Popen(command.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE)

        time.sleep(0.1)

        child = self.get_pid_children(rdtset.pid)
        assert len(child) == 1

        (stdout, _, exitstatus) = self.run("taskset -p %s" % child[0])
        assert exitstatus == 0
        assert "current affinity mask: 30" in stdout

        self.run("killall memtester")
        rdtset.communicate()


    ## RDTSET - Set CPU affinity single core (task)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  CPU affinity for command is set to single CPU
    #
    #  \b Instruction:
    #  1. Run the "rdtset [-I] -c 4 -p 1" to set cpu affinity
    #  2. Verify affinity with "taskset -p 1" command
    #
    #  \b Result:
    #  Observe "current affinity mask: 10" in taskset output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("rdt_a")
    def test_rdtset_affinity_task_single_core(self, iface):
        (_, _, exitstatus) = self.run_rdtset(iface, "-c 4 -p 1")
        assert exitstatus == 0

        (stdout, _, exitstatus) = self.run("taskset -p 1")
        assert exitstatus == 0
        assert "current affinity mask: 10" in stdout


    ## RDTSET - Set CPU affinity multiple cores (task)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  CPU affinity for command is set to single CPU
    #
    #  \b Instruction:
    #  1. Run the "rdtset [-I] -c 4-5 -p 1" to set cpu affinity
    #  2. Verify affinity with "taskset -p 1" command
    #
    #  \b Result:
    #  Observe "current affinity mask: 10" in taskset output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("rdt_a")
    def test_rdtset_affinity_task_multiple_cores(self, iface):
        (_, _, exitstatus) = self.run_rdtset(iface, "-c 4-5 -p 1")
        assert exitstatus == 0

        (stdout, _, exitstatus) = self.run("taskset -p 1")
        assert exitstatus == 0
        assert "current affinity mask: 30" in stdout
