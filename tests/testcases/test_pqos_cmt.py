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
from priority import PRIORITY_HIGH

class TestPqosCMT(test.Test):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self, request):
        super().init(request)
        yield
        super().fini()
    ## @endcond


    ## PQOS - CMT Detection
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify CMT capability is detected on platform
    #
    #  \b Instruction:
    #  Run "pqos [-I] -d" to print supported capabilities
    #
    #  \b Result:
    #  Observe "LLC Occupancy" in "Cache Monitoring Technology (CMT) events" section
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cqm_occup_llc")
    def test_pqos_cmt_detection(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-d")
        assert exitstatus == 0
        assert re.search(r"Cache Monitoring Technology \(CMT\) events:\s*LLC Occupancy", stdout)


    ## PQOS - CMT Monitor LLC occupancy (cores)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify CMT values for core
    #
    #  \b Instruction:
    #  1. Run "taskset -c 4 memtester 100M" in the background
    #  2. Run "pqos [-I] -m llc:0-15" to start CMT monitoring
    #  3. Terminate memtester
    #
    #  \b Result:
    #  Value in LLC[KB] column for core 4 is much higher than for other cores
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cqm_occup_llc")
    def test_pqos_cmt_llc_occupancy_cores(self, iface):
        def get_cmt(output, core):
            cmt = None
            lines = output.split("\n")

            for line in lines:
                match = re.search(r"^\s*([0-9]*)\s*[0-9]*\.[0-9]*\s*[0-9]*k\s*([0-9]*\.[0-9])\s*$",
                                  line)
                if match:
                    curr_core = int(match.group(1))
                    if curr_core == core:
                        cmt = float(match.group(2))
            return cmt

        command = "taskset -c 4 memtester 100M"
        subprocess.Popen(command.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE)

        time.sleep(2)

        (stdout, _, exitcode) = self.run_pqos(iface, "-m llc:0-15 -t 1")
        assert exitcode == 0
        assert re.search(r"CORE\s*IPC\s*MISSES\s*LLC\[KB\]", stdout)

        cmt = get_cmt(stdout, 4)
        assert cmt > 1000

        for core in range(15):
            if core == 4:
                continue
            assert get_cmt(stdout, core) < cmt / 2


    ## PQOS - CMT Monitor LLC occupancy (tasks)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify CMT values for task id
    #
    #  \b Instruction:
    #  1. Run "memtester 100M" in the background
    #  2. Run "pqos -I -p llc:<memtester pid> -p llc:1" to start CMT monitoring
    #  3. Terminate memtester
    #
    #  \b Result:
    #  LLC column present in output. LLC value for memtester is much higher than for other PID
    @PRIORITY_HIGH
    @pytest.mark.iface_os
    @pytest.mark.rdt_supported("cqm_occup_llc")
    def test_pqos_cmt_llc_occupancy_tasks(self, iface):
        def get_cmt(output, pid):
            cmt = None
            lines = output.split("\n")

            for line in lines:
                # pylint: disable=line-too-long
                match = re.search(r"^\s*([0-9]*)\s*[0-9,]*\s*[0-9]*\.[0-9]*\s*[0-9]*k\s*([0-9]*\.[0-9])\s*$", line)
                if match:
                    curr_pid = int(match.group(1))
                    if curr_pid == pid:
                        cmt = float(match.group(2))
            return cmt

        command = "memtester 100M"
        memtester = subprocess.Popen(command.split(), stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE)

        time.sleep(2)

        (stdout, _, exitcode) = self.run_pqos(iface, "-p llc:1 -p llc:%d -t 1" % memtester.pid)
        assert exitcode == 0
        assert re.search(r"PID\s*CORE\s*IPC\s*MISSES\s*LLC\[KB\]", stdout) is not None

        assert get_cmt(stdout, memtester.pid) > 1000
        assert get_cmt(stdout, 1) < 500

    ## PQOS - CMT Monitor LLC occupancy - percentage LLC for tasks
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify CMT values for task id - value should be displayed as percentage of total cache
    #
    #  \b Instruction:
    #  1. Run "memtester 100M" in the background
    #  2. Run "pqos -I -p llc:<memtester pid> -p llc:1 -P" to start CMT monitoring and LLC
    #     displayed as percentage value
    #  3. Terminate memtester
    #
    #  \b Result:
    #  LLC column present in output and is shown in percents. LLC value for memtester is much
    #  higher than for other PID
    @PRIORITY_HIGH
    @pytest.mark.iface_os
    @pytest.mark.rdt_supported("cqm_occup_llc")
    def test_pqos_cmt_llc_occupancy_tasks_percent(self, iface):
        def get_cmt_percent(output, pid):
            cmt = None
            lines = output.split("\n")

            for line in lines:
                match = re.match(r"^\s*(\d+)"              # PID number
                                 r"(?:\s+\S+){3}"          # CORE, IPC and MISSES (ignored)
                                 r"\s+(\d{1,3}\.\d+)\s*$", # LLC[%] value
                                 line)
                if match:
                    curr_pid = int(match.group(1))
                    if curr_pid == pid:
                        cmt = float(match.group(2))

            return cmt

        command = "memtester 100M"
        memtester = subprocess.Popen(command.split(), stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE)

        time.sleep(2)

        (stdout, _, exitcode) = self.run_pqos(iface,
                                              "-p llc:1 -p llc:%d -t 2 -P" % memtester.pid)
        assert exitcode == 0
        assert re.search(r"PID\s*CORE\s*IPC\s*MISSES\s*LLC\[%\]", stdout) is not None

        memtester_percent = get_cmt_percent(stdout, memtester.pid)
        pid_one_percent = get_cmt_percent(stdout, 1)

        # assuming that memtester will show higher LLC load than other pid
        assert memtester_percent > pid_one_percent
