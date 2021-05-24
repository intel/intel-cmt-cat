################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2021 Intel Corporation. All rights reserved.
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

from collections import defaultdict
import subprocess
import test
import re
import pytest
from testlib.resctrl import Resctrl
from testlib.env import Env
from priority import PRIORITY_HIGH

class TestRdtsetL2Cat(test.Test):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self, request):
        super().init(request)
        yield
        super().fini()
    ## @endcond


    ## RDTSET - L2 CAT Set COS definition (command)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Run command with provided L2 CAT mask
    #
    #  \b Instruction:
    #  1. Run the "rdtset [-I] -t 'l2=0xf;cpu=5-6' -c 5-6 memtester 10M" to set cache allocation
    #  2. Verify cache allocation with "pqos [-I] -s" command
    #  3. Terminate memtester process
    #
    #  \b Result:
    #  Observe in pqos output
    #  Core 5, L2ID 5, L3ID 0 => COS7
    #  Core 6, L2ID 6, L3ID 0 => COS7
    #  L2CA COS7 => MASK 0xf
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l2")
    @pytest.mark.rdt_unsupported("cat_l3", "mba")
    def test_rdtset_l2cat_set_command(self, iface):
        param = "-t l2=0xf;cpu=5-6 -c 5-6 memtester 10M"
        command = self.cmd_rdtset(iface, param)
        with subprocess.Popen(command.split(), stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE) as rdtset:

            self.stdout_wait(rdtset, b"memtester version")

            (stdout, _, exitstatus) = self.run_pqos(iface, "-s")
            assert exitstatus == 0
            if iface == "MSR":
                last_cos = Env().get('cat', 'l2', 'cos') - 1
            else:
                last_cos = Resctrl.get_ctrl_group_count() - 1

            assert re.search("Core 5, L2ID [0-9]+(, L3ID [0-9]+)? => COS%d" % last_cos, stdout) \
                    is not None
            assert re.search("Core 6, L2ID [0-9]+(, L3ID [0-9]+)? => COS%d" % last_cos, stdout) \
                    is not None
            assert "L2CA COS%d => MASK 0xf" % last_cos in stdout

            self.run("killall memtester")
            rdtset.communicate()


    ## RDTSET - L2 CAT Set COS definition (command)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Run command with provided L2 CAT mask for cores on the same socket but different clusters
    #
    #  \b Instruction:
    #  1. Select two cores from the same socket and different clusters
    #  2. Run the "rdtset [-I] -t 'l2=0xf;cpu=<core1>,<core2>' -c <core1>,<core2> memtester 10M"
    #     to set cache allocation
    #  3. Verify cache allocation with "pqos [-I] -s" command
    #  4. Terminate memtester process
    #
    #  \b Result:
    #  Observe in pqos output
    #  Core <core1>, L2ID <l2id>, L3ID <l3id> => COS7
    #  Core <core2>, L2ID <l2id>, L3ID <l3id> => COS6
    #  L2CA COS6 => MASK 0xf
    #  L2CA COS7 => MASK 0xf
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l2", "cat_l3")
    def test_rdtset_l2cat_set_command_same_l3id_diff_l2id(self, iface):
        # Get cache topology
        stdout, _, exitstatus  = self.run_pqos(iface, '-s')
        assert exitstatus == 0
        regex = r"Core (\d+), L2ID (\d+), L3ID (\d+) => COS(\d+)(, RMID\d+)?"
        match_iter = re.finditer(regex, stdout)
        topology = defaultdict(lambda: {'l2ids': defaultdict(lambda: [])})

        for match in match_iter:
            core = int(match.group(1))
            l2id = int(match.group(2))
            l3id = int(match.group(3))

            topology[l3id]['l2ids'][l2id].append(core)

        if len(topology) < 2:
            pytest.skip('Test applies for machines with at least 2 sockets')

        # Get cores from the same socket, but different cluster
        # The same L3 ID (MBA ID or socket), different L2 ID (cluster)
        cores = []
        for _, l3id_info in topology.items():
            for l2id, l2id_cores in l3id_info['l2ids'].items():
                if len(cores) >= 2:
                    break

                core = l2id_cores[len(l2id_cores) // 3]
                self.log.debug('Found core %d, L2 ID %d', core, l2id)
                cores.append(core)

            if len(cores) >= 2:
                break

        assert len(cores) >= 2

        self.log.debug('Selected cores: %s', cores)

        param = f"-t l2=0xf;cpu={cores[0]},{cores[1]} -c {cores[0]},{cores[1]} memtester 10M"
        command = self.cmd_rdtset(iface, param)
        with subprocess.Popen(command.split(), stdin=subprocess.PIPE,
                              stdout=subprocess.PIPE) as rdtset:

            self.stdout_wait(rdtset, b"memtester version")

            (stdout, _, exitstatus) = self.run_pqos(iface, "-s")
            assert exitstatus == 0
            if iface == "MSR":
                last_cos = Env().get('cat', 'l2', 'cos') - 1
            else:
                last_cos = Resctrl.get_ctrl_group_count() - 1

            assert last_cos > 0

            # Assigned two different COS
            regex_tpl = "Core %d, L2ID [0-9]+(, L3ID [0-9]+)? => COS(%d|%d)"
            regex1 = regex_tpl % (cores[0], last_cos, last_cos - 1)
            regex2 = regex_tpl % (cores[1], last_cos, last_cos - 1)
            assert re.search(regex1, stdout) is not None
            assert re.search(regex2, stdout) is not None
            assert "L2CA COS%d => MASK 0xf" % (last_cos - 1) in stdout
            assert "L2CA COS%d => MASK 0xf" % last_cos in stdout

            self.run("killall memtester")
            rdtset.communicate()


    ## RDTSET - L2 CAT Set COS definition (command) - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Unable to run command with too long L2 CAT bitmask
    #
    #  \b Instruction:
    #  Run the "rdtset [-I] -t 'l2=0xffffffff;cpu=5-6' -c 5-6 memtester 10M" to
    #  set cache allocation
    #
    #  \b Result:
    #  Observe "One or more of requested L2 CBMs (MASK: 0xffffffff) not
    #  supported by system (too long) in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_supported("cat_l2")
    def test_rdtset_l2cat_set_command_negative(self, iface):
        param = "-t l2=0xffffffff;cpu=5-6 -c 5-6 memtester 10M"
        (_, stderr, exitstatus) = self.run_rdtset(iface, param)
        assert exitstatus == 1
        assert ('One or more of requested L2 CBMs (MASK: 0xffffffff) not supported by system '
                '(too long)') in stderr


    ## RDTSET - L2 CAT Set COS definition (task)
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Set L2 CAT bitmask for Task Id
    #
    #  \b Instruction:
    #  1. Run the "Run the rdtset -I -t 'l2=0xf' -c 5-6 -p 1" to set cache
    #     allocation
    #  2. Verify cache allocation with "pqos -I -s" command
    #
    #  \b Result:
    # Observe in pqos output
    #  * COS7 => 1
    #  * L2CA COS7 => MASK 0xf
    @PRIORITY_HIGH
    @pytest.mark.iface_os
    @pytest.mark.rdt_supported("cat_l2")
    def test_rdtset_l3cat_set_task(self, iface):
        (_, _, exitstatus) = self.run_rdtset(iface, "-t l2=0xf -c 5-6 -p 1")
        assert exitstatus == 0

        (stdout, _, exitstatus) = self.run_pqos(iface, "-s")
        assert exitstatus == 0
        last_cos = Resctrl.get_ctrl_group_count() - 1
        assert ("COS{} => 1").format(last_cos) in stdout
        assert ("L2CA COS{} => MASK 0xf").format(last_cos) in stdout


    ## RDTSET - L2 CAT Set COS definition (task) - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Unable to set too long L2 CAT bitmask for the task ID
    #
    #  \b Instruction:
    #  Run the "Run the rdtset -I -t 'l2=0xffffffff' -p 1" to set cache allocation
    #
    #  \b Result:
    #  Observe "One or more of requested L2 CBMs (MASK: 0xffffffff) not supported
    #  by system (too long) in output
    @PRIORITY_HIGH
    @pytest.mark.iface_os
    @pytest.mark.rdt_supported("cat_l2")
    def test_rdtset_l2cat_set_task_negative(self, iface):
        (_, stderr, exitstatus) = self.run_rdtset(iface, "-t l2=0xffffffff -p 1")
        assert exitstatus == 1
        assert ('One or more of requested L2 CBMs (MASK: 0xffffffff) not supported by system '
                '(too long)') in stderr
