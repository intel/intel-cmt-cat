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
import re
import test
import pytest
from priority import PRIORITY_HIGH

class TestPqosMBM(test.Test):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self, request):
        super().init(request)
        yield
        super().fini()
    ## @endcond


    ## PQOS - MBM Detection
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify MBM capability is detected on platform
    #
    #  \b Instruction:
    #  Run "pqos [-I] -d" to print supported capabilities
    #
    #  \b Result:
    #  Observe "Total Memory Bandwidth" and "Local Memory Bandwidth" in
    #  "Memory Bandwidth Monitoring (CMT) events" section
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cqm_mbm_local", "cqm_mbm_total")
    def test_pqos_mbm_detection(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-d")
        assert exitstatus == 0

        assert "Memory Bandwidth Monitoring (MBM) events:" in stdout
        assert "Total Memory Bandwidth (TMEM)" in stdout
        assert "Local Memory Bandwidth (LMEM)" in stdout


    ## PQOS - MBM Monitor MBL and MBR (cores)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify MBM values for core
    #
    #  \b Instruction:
    #  1. Run "taskset -c 4 memtester 100M" in the background
    #  2. Run "pqos [-I] -m mbl:0-15 -m mbr:0-15" to start MBL and MBR monitoring
    #  3. Terminate memtester
    #
    #  \b Result:
    #  Value in MBL column for core 4 is much higher than for other cores
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cqm_mbm_local", "cqm_mbm_total")
    def test_pqos_mbm_cores(self, iface):
        def get_mbm(output, core):
            mbl = None
            mbr = None
            lines = output.split("\n")

            for line in lines:
                # pylint: disable=line-too-long
                match = re.search(r"^\s*([0-9]*)\s*[0-9]*\.[0-9]*\s*[0-9]*k\s*([0-9]*\.[0-9])\s*([0-9]*\.[0-9])\s*$", line)
                if match:
                    curr_core = int(match.group(1))
                    if curr_core == core:
                        mbl = float(match.group(2))
                        mbr = float(match.group(3))
            return mbl, mbr

        command = "taskset -c 4 memtester 100M"
        subprocess.Popen(command.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE)

        time.sleep(2)

        (stdout, _, exitcode) = self.run_pqos(iface, "-m mbl:0-15 -m mbr:0-15 -t 1")
        assert exitcode == 0
        assert re.search(r"CORE\s*IPC\s*MISSES\s*MBL\[MB/s\]\s*MBR\[MB/s\]", stdout) is not None

        (mbl, _) = get_mbm(stdout, 4)
        assert mbl > 100

        for core in range(15):
            if core == 4:
                continue

            (mbl_core, _) = get_mbm(stdout, core)
            assert mbl_core < mbl / 10


    ## PQOS - MBM Monitor MBL and MBR (tasks)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify MBM values for task id
    #
    #  \b Instruction:
    #  1. Run "memtester 100M" in the background
    #  2. Run "pqos -I -p mbl:<memtester pid> -p mbl:1 -p mbr:<memtester pid> -p mbr:1" to start
    #     MBL and MBR monitoring
    #  3. Terminate memtester
    #
    #  \b Result:
    #  MBL and MBR columns present in output. Value in MBL column for memtester PID is much higher
    #  than for other PID
    @PRIORITY_HIGH
    @pytest.mark.iface_os
    @pytest.mark.rdt_supported("cqm_mbm_local", "cqm_mbm_total")
    def test_pqos_mbm_tasks(self, iface):
        def get_mbm(output, pid):
            mbl = None
            mbr = None
            lines = output.split("\n")

            for line in lines:
                # pylint: disable=line-too-long
                match = re.search(r"^\s*([0-9]*)\s*[0-9,]*\s*[0-9]*\.[0-9]*\s*[0-9]*k\s*([0-9]*\.[0-9])\s*([0-9]*\.[0-9])\s*$", line)
                if match:
                    curr_pid = int(match.group(1))
                    if curr_pid == pid:
                        mbl = float(match.group(2))
                        mbr = float(match.group(3))
            return mbl, mbr

        command = "taskset -c 4 memtester 100M"
        memtester = subprocess.Popen(command.split(), stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE)

        time.sleep(2)

        (stdout, _, exitcode) = self.run_pqos(iface,
                                              "-p mbl:1 -p mbl:%d -p mbr:1 -p mbr:%d -t 1" % \
                                              (memtester.pid, memtester.pid))
        assert exitcode == 0
        assert re.search(r"PID\s*CORE\s*IPC\s*MISSES\s*MBL\[MB/s\]\s*MBR\[MB/s\]", stdout) \
            is not None

        (mbl, _) = get_mbm(stdout, memtester.pid)
        assert mbl > 100
        (mbl, _) = get_mbm(stdout, 1)
        assert mbl < 10
