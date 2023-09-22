################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2023 Intel Corporation. All rights reserved.
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
from unittest.mock import MagicMock, patch

from pqos.test.helper import ctypes_ref_set_uint
from pqos.monitoring import PqosMon, CPqosMonData
from pqos.native_struct import (
    CPqosEventValues, CPqosIordtConfig, CPqosMonConfig, CPqosMonitor
)

class TestPqosMon(unittest.TestCase):
    "Tests for PqosMon class."

    @patch('pqos.monitoring.Pqos')
    def test_reset(self, pqos_mock_cls):
        "Tests reset() method."
        # pylint: disable=no-self-use

        def pqos_mon_reset_mock():
            "Mock pqos_mon_reset()."

            return 0

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_reset_mock)
        lib.pqos_mon_reset = func_mock

        mon = PqosMon()
        mon.reset()

        lib.pqos_mon_reset.assert_called_once()

    @patch('pqos.monitoring.Pqos')
    def test_assoc_get(self, pqos_mock_cls):
        "Tests assoc_get() method."

        def pqos_mon_assoc_get_mock(core, rmid_ref):
            "Mock pqos_mon_assoc_get()."

            self.assertEqual(core, 3)
            ctypes_ref_set_uint(rmid_ref, 7)
            return 0

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_assoc_get_mock)
        lib.pqos_mon_assoc_get = func_mock

        mon = PqosMon()
        rmid = mon.assoc_get(3)

        self.assertEqual(rmid, 7)

        lib.pqos_mon_assoc_get.assert_called_once()

    @patch('pqos.monitoring.Pqos')
    def test_assoc_get_channel(self, pqos_mock_cls):
        "Tests assoc_get_channel() method."

        def pqos_mon_assoc_get_channel_mock(channel_id, rmid_ref):
            "Mock pqos_mon_assoc_get_channel()."

            self.assertEqual(channel_id, 0x10100)
            ctypes_ref_set_uint(rmid_ref, 15)
            return 0

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_assoc_get_channel_mock)
        lib.pqos_mon_assoc_get_channel = func_mock

        mon = PqosMon()
        rmid = mon.assoc_get_channel(0x10100)

        self.assertEqual(rmid, 15)

        lib.pqos_mon_assoc_get_channel.assert_called_once()

    @patch('pqos.monitoring.Pqos')
    def test_assoc_get_dev(self, pqos_mock_cls):
        "Tests assoc_get_dev() method."

        def pqos_mon_assoc_get_dev_mock(segment, bdf, virtual_channel,
                                        rmid_ref):
            "Mock pqos_mon_assoc_get_dev()."

            self.assertEqual(segment, 0x10)
            self.assertEqual(bdf, 0x7)
            self.assertEqual(virtual_channel, 3)
            ctypes_ref_set_uint(rmid_ref, 109)
            return 0

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_assoc_get_dev_mock)
        lib.pqos_mon_assoc_get_dev = func_mock

        mon = PqosMon()
        rmid = mon.assoc_get_dev(0x10, 0x7, 3)

        self.assertEqual(rmid, 109)

        lib.pqos_mon_assoc_get_dev.assert_called_once()


    # def pqos_cap_get_type_mock(_cap_ref, type_enum, p_cap_item_ref):
    #     "Mock pqos_cap_get_type()."

    #     if type_enum != cap.type:
    #         return 1

    #     cap_ptr = ctypes.pointer(cap)
    #     ctypes.memmove(p_cap_item_ref, ctypes.addressof(cap_ptr),
    #                    ctypes.sizeof(type(cap_ptr)))

    @patch('pqos.monitoring.Pqos')
    def test_start(self, pqos_mock_cls):
        "Tests start() method."

        values = CPqosEventValues(llc=123, mbm_local=456, mbm_total=789,
                                  mbm_remote=120, mbm_local_delta=456,
                                  mbm_total_delta=789, mbm_remote_delta=120)
        group_mock = CPqosMonData(values=values)

        def pqos_mon_start_cores_mock(num_cores, cores_arr, event, _context,
                                      group_ref):
            "Mock pqos_mon_start_cores()."

            self.assertEqual(num_cores, 2)
            self.assertEqual(cores_arr[0], 1)
            self.assertEqual(cores_arr[1], 3)
            exp_event = CPqosMonitor.PQOS_MON_EVENT_L3_OCCUP
            exp_event |= CPqosMonitor.PQOS_MON_EVENT_LMEM_BW
            self.assertEqual(event, exp_event)

            group_ptr = ctypes.pointer(group_mock)
            ctypes.memmove(group_ref, ctypes.addressof(group_ptr),
                           ctypes.sizeof(type(group_ptr)))
            return 0

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_start_cores_mock)
        lib.pqos_mon_start_cores = func_mock

        mon = PqosMon()
        group = mon.start([1, 3], ['l3_occup', 'lmem_bw'])

        lib.pqos_mon_start_cores.assert_called_once()

        self.assertEqual(group.values.llc, 123)
        self.assertEqual(group.values.mbm_local, 456)

    @patch('pqos.monitoring.Pqos')
    def test_start_pids(self, pqos_mock_cls):
        "Tests start_pids() method."

        values = CPqosEventValues(llc=678, mbm_local=653, mbm_total=721,
                                  mbm_remote=68, mbm_local_delta=653,
                                  mbm_total_delta=721, mbm_remote_delta=68,
                                  ipc=0.98, llc_misses=10, llc_misses_delta=10)
        group_mock = CPqosMonData(values=values)

        def pqos_mon_start_pids2_mock(num_pids, pids_arr, event, _context,
                                     group_ref):
            "Mock pqos_mon_start_pids2()."

            self.assertEqual(num_pids, 2)
            self.assertEqual(pids_arr[0], 1286)
            self.assertEqual(pids_arr[1], 2251)
            exp_event = CPqosMonitor.PQOS_MON_EVENT_L3_OCCUP
            exp_event |= CPqosMonitor.PQOS_MON_EVENT_TMEM_BW
            self.assertEqual(event, exp_event)

            group_ptr = ctypes.pointer(group_mock)
            ctypes.memmove(group_ref, ctypes.addressof(group_ptr),
                           ctypes.sizeof(type(group_ptr)))
            return 0

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_start_pids2_mock)
        lib.pqos_mon_start_pids2 = func_mock

        mon = PqosMon()
        group = mon.start_pids([1286, 2251], ['l3_occup', 'tmem_bw'])

        lib.pqos_mon_start_pids2.assert_called_once()

        self.assertEqual(group.values.llc, 678)
        self.assertEqual(group.values.mbm_local, 653)
        self.assertAlmostEqual(group.values.ipc, 0.98, places=5)

    @patch('pqos.monitoring.Pqos')
    def test_start_channels(self, pqos_mock_cls):
        "Tests start_channels() method."

        values = CPqosEventValues(llc=1150, mbm_local=220, mbm_total=250,
                                  mbm_remote=30, mbm_local_delta=158,
                                  mbm_total_delta=181, mbm_remote_delta=23,
                                  ipc=1.98, llc_misses=5888, llc_misses_delta=856)
        group_mock = CPqosMonData(values=values)

        def pqos_mon_start_channels_mock(num_channels, channels, event,
                                         _context, group_ref):
            "Mock pqos_mon_start_channels()."

            self.assertEqual(num_channels, 2)
            self.assertEqual(channels[0], 0x20200)
            self.assertEqual(channels[1], 0x30100)
            exp_event = CPqosMonitor.PQOS_MON_EVENT_L3_OCCUP
            exp_event |= CPqosMonitor.PQOS_MON_EVENT_TMEM_BW
            self.assertEqual(event, exp_event)

            group_ptr = ctypes.pointer(group_mock)
            ctypes.memmove(group_ref, ctypes.addressof(group_ptr),
                           ctypes.sizeof(type(group_ptr)))
            return 0

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_start_channels_mock)
        lib.pqos_mon_start_channels = func_mock

        mon = PqosMon()
        group = mon.start_channels([0x20200, 0x30100], ['l3_occup', 'tmem_bw'])

        lib.pqos_mon_start_channels.assert_called_once()

        self.assertEqual(group.values.llc, 1150)
        self.assertEqual(group.values.mbm_local, 220)
        self.assertAlmostEqual(group.values.ipc, 1.98, places=5)

    @patch('pqos.monitoring.Pqos')
    def test_start_dev(self, pqos_mock_cls):
        "Tests start_dev() method."

        values = CPqosEventValues(llc=875, mbm_local=110, mbm_total=235,
                                  mbm_remote=25, mbm_local_delta=100,
                                  mbm_total_delta=101, mbm_remote_delta=1,
                                  ipc=1.54, llc_misses=1254, llc_misses_delta=54)
        group_mock = CPqosMonData(values=values)

        def pqos_mon_start_dev_mock(segment, bdf, virtual_channel, event,
                                    _context, group_ref):
            "Mock pqos_mon_start_dev()."

            self.assertEqual(segment, 0x11)
            self.assertEqual(bdf, 0x2)
            self.assertEqual(virtual_channel, 5)
            exp_event = CPqosMonitor.PQOS_MON_EVENT_L3_OCCUP
            exp_event |= CPqosMonitor.PQOS_MON_EVENT_TMEM_BW
            self.assertEqual(event, exp_event)

            group_ptr = ctypes.pointer(group_mock)
            ctypes.memmove(group_ref, ctypes.addressof(group_ptr),
                           ctypes.sizeof(type(group_ptr)))
            return 0

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_start_dev_mock)
        lib.pqos_mon_start_dev = func_mock

        mon = PqosMon()
        group = mon.start_dev(0x11, 0x2, 5, ['l3_occup', 'tmem_bw'])

        lib.pqos_mon_start_dev.assert_called_once()

        self.assertEqual(group.values.llc, 875)
        self.assertEqual(group.values.mbm_local, 110)
        self.assertAlmostEqual(group.values.ipc, 1.54, places=5)

    @patch('pqos.monitoring.Pqos')
    def test_poll(self, pqos_mock_cls):
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

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_poll_mock)
        lib.pqos_mon_poll = func_mock

        mon = PqosMon()
        mon.poll([group])

        lib.pqos_mon_poll.assert_called_once()

        self.assertEqual(group.values.llc, 998)

    @patch('pqos.monitoring.Pqos')
    def test_reset_config(self, pqos_mock_cls):
        "Tests reset_config() method."

        def pqos_mon_reset_cfg_m(config):
            "Mock pqos_mon_reset_config()."

            self.assertEqual(config.contents.l3_iordt,
                             CPqosIordtConfig.PQOS_REQUIRE_IORDT_ON)

            return 0

        # Setup mock function
        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_reset_cfg_m)
        lib.pqos_mon_reset_config = func_mock

        # Build reset configuration
        cfg = CPqosMonConfig()
        cfg.set_l3_iordt('on')

        # Reset using configuration
        mon = PqosMon()
        mon.reset_config(cfg)

        # Ensure mock function has been called
        lib.pqos_mon_reset_config.assert_called_once()


class TestCPqosMonData(unittest.TestCase):
    "Tests for CPqosMonData class."

    @patch('pqos.monitoring.Pqos')
    def test_stop(self, pqos_mock_cls):
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

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_stop_mock)
        lib.pqos_mon_stop = func_mock

        group_mock.stop()

        lib.pqos_mon_stop.assert_called_once()

    @patch('pqos.monitoring.Pqos')
    def test_add_pids(self, pqos_mock_cls):
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

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_add_pids_mock)
        lib.pqos_mon_add_pids = func_mock

        group_mock.add_pids([101, 202, 303])

        lib.pqos_mon_add_pids.assert_called_once()

    @patch('pqos.monitoring.Pqos')
    def test_remove_pids(self, pqos_mock_cls):
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

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_mon_remove_pids_mock)
        lib.pqos_mon_remove_pids = func_mock

        group_mock.remove_pids([555, 444, 321, 121])

        lib.pqos_mon_remove_pids.assert_called_once()
