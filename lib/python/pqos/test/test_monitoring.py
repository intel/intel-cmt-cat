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

"""
Unit tests for monitoring module.
"""

from __future__ import absolute_import, division, print_function
import ctypes
import unittest

from unittest.mock import MagicMock

from pqos.test.helper import ctypes_ref_set_uint
from pqos.test.mock_pqos import mock_pqos_lib

from pqos.capability import CPqosMonitor
from pqos.monitoring import PqosMon, CPqosEventValues, CPqosMonData


class TestPqosMon(unittest.TestCase):
    "Tests for PqosMon class."

    @mock_pqos_lib
    def test_reset(self, lib):
        "Tests reset() method."
        # pylint: disable=no-self-use

        def pqos_mon_reset_mock():
            "Mock pqos_mon_reset()."

            return 0

        func_mock = MagicMock(side_effect=pqos_mon_reset_mock)
        lib.pqos_mon_reset = func_mock

        mon = PqosMon()
        mon.reset()

        lib.pqos_mon_reset.assert_called_once()

    @mock_pqos_lib
    def test_assoc_get(self, lib):
        "Tests assoc_get() method."

        def pqos_mon_assoc_get_mock(core, rmid_ref):
            "Mock pqos_mon_assoc_get()."

            self.assertEqual(core, 3)
            ctypes_ref_set_uint(rmid_ref, 7)
            return 0

        func_mock = MagicMock(side_effect=pqos_mon_assoc_get_mock)
        lib.pqos_mon_assoc_get = func_mock

        mon = PqosMon()
        rmid = mon.assoc_get(3)

        self.assertEqual(rmid, 7)

        lib.pqos_mon_assoc_get.assert_called_once()

    @mock_pqos_lib
    def test_start(self, lib):
        "Tests start() method."

        values = CPqosEventValues(llc=123, mbm_local=456, mbm_total=789,
                                  mbm_remote=120, mbm_local_delta=456,
                                  mbm_total_delta=789, mbm_remote_delta=120)
        group_mock = CPqosMonData(values=values)

        def pqos_mon_start_mock(num_cores, cores_arr, event, _context,
                                group_ref):
            "Mock pqos_mon_start()."

            self.assertEqual(num_cores, 2)
            self.assertEqual(cores_arr[0], 1)
            self.assertEqual(cores_arr[1], 3)
            exp_event = CPqosMonitor.PQOS_MON_EVENT_L3_OCCUP
            exp_event |= CPqosMonitor.PQOS_MON_EVENT_LMEM_BW
            self.assertEqual(event, exp_event)
            ctypes.memmove(group_ref, ctypes.addressof(group_mock),
                           ctypes.sizeof(group_mock))
            return 0

        func_mock = MagicMock(side_effect=pqos_mon_start_mock)
        lib.pqos_mon_start = func_mock

        mon = PqosMon()
        group = mon.start([1, 3], ['l3_occup', 'lmem_bw'])

        lib.pqos_mon_start.assert_called_once()

        self.assertEqual(group.values.llc, 123)
        self.assertEqual(group.values.mbm_local, 456)

    @mock_pqos_lib
    def test_start_pids(self, lib):
        "Tests start_pids() method."
        values = CPqosEventValues(llc=678, mbm_local=653, mbm_total=721,
                                  mbm_remote=68, mbm_local_delta=653,
                                  mbm_total_delta=721, mbm_remote_delta=68,
                                  ipc=0.98, llc_misses=10, llc_misses_delta=10)
        group_mock = CPqosMonData(values=values)

        def pqos_mon_start_pids_mock(num_pids, pids_arr, event, _context,
                                     group_ref):
            "Mock pqos_mon_start_pids()."

            self.assertEqual(num_pids, 2)
            self.assertEqual(pids_arr[0], 1286)
            self.assertEqual(pids_arr[1], 2251)
            exp_event = CPqosMonitor.PQOS_MON_EVENT_L3_OCCUP
            exp_event |= CPqosMonitor.PQOS_MON_EVENT_TMEM_BW
            self.assertEqual(event, exp_event)
            ctypes.memmove(group_ref, ctypes.addressof(group_mock),
                           ctypes.sizeof(group_mock))
            return 0

        func_mock = MagicMock(side_effect=pqos_mon_start_pids_mock)
        lib.pqos_mon_start_pids = func_mock

        mon = PqosMon()
        group = mon.start_pids([1286, 2251], ['l3_occup', 'tmem_bw'])

        lib.pqos_mon_start_pids.assert_called_once()

        self.assertEqual(group.values.llc, 678)
        self.assertEqual(group.values.mbm_local, 653)
        self.assertAlmostEqual(group.values.ipc, 0.98, places=5)

    @mock_pqos_lib
    def test_poll(self, lib):
        "Tests poll() method."
        values = CPqosEventValues(llc=678, mbm_local=653, mbm_total=721,
                                  mbm_remote=68, mbm_local_delta=653,
                                  mbm_total_delta=721, mbm_remote_delta=68,
                                  ipc=0.98, llc_misses=10, llc_misses_delta=10)
        event = CPqosMonitor.PQOS_MON_EVENT_L3_OCCUP
        group = CPqosMonData(event=event, values=values)

        values2 = CPqosEventValues(llc=998, mbm_local=653, mbm_total=721,
                                   mbm_remote=68, mbm_local_delta=653,
                                   mbm_total_delta=721, mbm_remote_delta=68,
                                   ipc=0.98, llc_misses=10, llc_misses_delta=10)
        group_mock2 = CPqosMonData(values=values2)

        def pqos_mon_poll_mock(groups_arr, num_groups):
            "Mock pqos_mon_poll()."

            self.assertEqual(num_groups, 1)
            ctypes.memmove(groups_arr[0], ctypes.addressof(group_mock2),
                           ctypes.sizeof(group_mock2))
            return 0

        func_mock = MagicMock(side_effect=pqos_mon_poll_mock)
        lib.pqos_mon_poll = func_mock

        mon = PqosMon()
        mon.poll([group])

        lib.pqos_mon_poll.assert_called_once()

        self.assertEqual(group.values.llc, 998)


class TestCPqosMonData(unittest.TestCase):
    "Tests for CPqosMonData class."

    @mock_pqos_lib
    def test_stop(self, lib):
        "Tests stop() method."

        values = CPqosEventValues(llc=123, mbm_local=456, mbm_total=789,
                                  mbm_remote=120, mbm_local_delta=456,
                                  mbm_total_delta=789, mbm_remote_delta=120)
        group_mock = CPqosMonData(values=values)

        def pqos_mon_stop_mock(group_ref):
            "Mock pqos_mon_stop()."

            group_ptr = ctypes.cast(group_ref, ctypes.POINTER(CPqosMonData))
            group_ptr_addr = ctypes.addressof(group_ptr.contents)
            mock_addr = ctypes.addressof(group_mock)
            self.assertEqual(group_ptr_addr, mock_addr)
            return 0

        func_mock = MagicMock(side_effect=pqos_mon_stop_mock)
        lib.pqos_mon_stop = func_mock

        group_mock.stop()

        lib.pqos_mon_stop.assert_called_once()

    @mock_pqos_lib
    def test_add_pids(self, lib):
        "Tests add_pids() method."

        values = CPqosEventValues(llc=123, mbm_local=456, mbm_total=789,
                                  mbm_remote=120, mbm_local_delta=456,
                                  mbm_total_delta=789, mbm_remote_delta=120)
        group_mock = CPqosMonData(values=values)

        def pqos_mon_add_pids_mock(num_pids, pids_arr, group_ref):
            "Mock pqos_mon_add_pids()."

            group_ptr = ctypes.cast(group_ref, ctypes.POINTER(CPqosMonData))
            group_ptr_addr = ctypes.addressof(group_ptr.contents)
            mock_addr = ctypes.addressof(group_mock)
            self.assertEqual(group_ptr_addr, mock_addr)

            self.assertEqual(num_pids, 3)
            self.assertEqual(pids_arr[0], 101)
            self.assertEqual(pids_arr[1], 202)
            self.assertEqual(pids_arr[2], 303)

            return 0

        func_mock = MagicMock(side_effect=pqos_mon_add_pids_mock)
        lib.pqos_mon_add_pids = func_mock

        group_mock.add_pids([101, 202, 303])

        lib.pqos_mon_add_pids.assert_called_once()

    @mock_pqos_lib
    def test_remove_pids(self, lib):
        "Tests remove_pids() method."

        values = CPqosEventValues(llc=123, mbm_local=456, mbm_total=789,
                                  mbm_remote=120, mbm_local_delta=456,
                                  mbm_total_delta=789, mbm_remote_delta=120)
        group_mock = CPqosMonData(values=values)

        def pqos_mon_remove_pids_mock(num_pids, pids_arr, group_ref):
            "Mock pqos_mon_remove_pids()."

            group_ptr = ctypes.cast(group_ref, ctypes.POINTER(CPqosMonData))
            group_ptr_addr = ctypes.addressof(group_ptr.contents)
            mock_addr = ctypes.addressof(group_mock)
            self.assertEqual(group_ptr_addr, mock_addr)

            self.assertEqual(num_pids, 4)
            self.assertEqual(pids_arr[0], 555)
            self.assertEqual(pids_arr[1], 444)
            self.assertEqual(pids_arr[2], 321)
            self.assertEqual(pids_arr[3], 121)

            return 0

        func_mock = MagicMock(side_effect=pqos_mon_remove_pids_mock)
        lib.pqos_mon_remove_pids = func_mock

        group_mock.remove_pids([555, 444, 321, 121])

        lib.pqos_mon_remove_pids.assert_called_once()
