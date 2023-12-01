################################################################################
# BSD LICENSE
#
# Copyright(c) 2023 Intel Corporation. All rights reserved.
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
Unit tests for device information module.
"""

import ctypes
import unittest

from unittest.mock import MagicMock, patch

from pqos.test.helper import ctypes_ref_set_int, ctypes_build_array

from pqos.devinfo import PqosDevInfo
from pqos.native_struct import (
    CPqosChannel, CPqosDev, CPqosDevType, CPqosDevinfo, CPqosSysconfig,
    PqosChannelT, PQOS_DEV_MAX_CHANNELS
)


class PqosDevInfoMockBuilder:
    "Builds a mock CPqosDevinfo object."

    def __init__(self):
        self.devinfo = None

    def build_channels(self):  # pylint: disable=no-self-use
        "Builds channel information."

        channels = []
        channel = CPqosChannel(channel_id=0x10100, rmid_tagging=True,
                               clos_tagging=True, min_rmid=1, max_rmid=10,
                               min_clos=1, max_clos=10)
        channels.append(channel)
        channel = CPqosChannel(channel_id=0x20200, rmid_tagging=False,
                               clos_tagging=True, min_rmid=1, max_rmid=8,
                               min_clos=2, max_clos=12)
        channels.append(channel)
        return channels

    def build_devices(self):  # pylint: disable=no-self-use
        "Builds device information."

        devices = []
        dev = CPqosDev(type=CPqosDevType.PQOS_DEVICE_TYPE_PCI,
                       segment=1, bdf=2)
        dev.channel[0] = 0x10100
        for i in range(1, PQOS_DEV_MAX_CHANNELS):
            dev.channel[i] = 0

        devices.append(dev)
        return devices

    def build(self):
        "Builds device information and returns a pointer to that object."

        channels = self.build_channels()
        devices = self.build_devices()

        channels_array = ctypes_build_array(channels)
        devices_array = ctypes_build_array(devices)

        self.devinfo = CPqosDevinfo(num_channels=len(channels),
                                    channels=channels_array,
                                    num_devs=len(devices), devs=devices_array)

        return ctypes.pointer(self.devinfo)


class TestPqosDevInfo(unittest.TestCase):
    "Tests for PqosDevInfo class."

    def setUp(self):
        self.builder = PqosDevInfoMockBuilder()

    def _mock_sysconfig(self, pqos_mock_cls):
        p_dev = self.builder.build()
        sysconfig = CPqosSysconfig(cap=None, cpu=None, dev=p_dev)

        def pqos_get_sysconfig():
            "Mock pqos_get_sysconfig()."

            return sysconfig

        func_mock = MagicMock(side_effect=pqos_get_sysconfig)
        pqos_mock_cls.return_value.get_sysconfig = func_mock

        return p_dev

    @patch('pqos.devinfo.Pqos')
    def test_init(self, pqos_mock_cls):
        """
        Tests if the pointer to device information object given
        to PQoS library APIs is the same returned
        from pqos_get_sysconfig() API during an initialization of PqosDevInfo.
        """

        # Mock pqos_get_sysconfig()
        ptr_dev = self._mock_sysconfig(pqos_mock_cls)

        def pqos_devinfo_get_channel_id_m(p_dev, _segment, _bdf, _virt_ch):
            "Mock pqos_devinfo_get_channel_id()."

            dev_addr = ctypes.addressof(p_dev.contents)
            ptr_dev_addr = ctypes.addressof(ptr_dev.contents)
            self.assertEqual(dev_addr, ptr_dev_addr)

            return 0x10100

        # Mock example PQoS API that takes CPqosDevInfo as a parameter
        # e.g. pqos_devinfo_get_channel_id
        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_devinfo_get_channel_id_m)
        lib.pqos_devinfo_get_channel_id = func_mock

        # Call get_channel_id() that in turn calls PQoS API
        devinfo = PqosDevInfo()
        devinfo.get_channel_id(0, 0, 0)

        # Check functions have been called
        # See mock of pqos_devinfo_get_channel_id
        # for a check against CPqosDevInfo
        pqos_mock = pqos_mock_cls.return_value
        pqos_mock.get_sysconfig.assert_called_once()
        lib.pqos_devinfo_get_channel_id.assert_called_once()

    @patch('pqos.devinfo.Pqos')
    def test_get_channel_id(self, pqos_mock_cls):
        "Tests get_channel_id() method"

        def pqos_devinfo_get_channel_id_m(_p_dev, segment, bdf, virt_ch):
            self.assertEqual(segment, 1)
            self.assertEqual(bdf, 7)
            self.assertEqual(virt_ch, 2)

            return 0x10100

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_devinfo_get_channel_id_m)
        lib.pqos_devinfo_get_channel_id = func_mock

        dev = PqosDevInfo()

        self.assertEqual(dev.get_channel_id(1, 7, 2), 0x10100)
        lib.pqos_devinfo_get_channel_id.assert_called_once()

    @patch('pqos.devinfo.Pqos')
    def test_get_channel_ids(self, pqos_mock_cls):
        "Tests get_channel_ids() method"

        channels_org = [0x10100, 0x10200, 0x20100, 0x30100]
        channels_mock = [PqosChannelT(chan) for chan in channels_org]
        channels_arr = ctypes_build_array(channels_mock)

        def pqos_devinfo_get_channel_ids_m(_p_dev, segment, bdf, count_ref):
            "Mock pqos_devinfo_get_channel_ids()."

            self.assertEqual(segment, 4)
            self.assertEqual(bdf, 3)

            ctypes_ref_set_int(count_ref, len(channels_arr))
            return ctypes.cast(channels_arr, ctypes.POINTER(PqosChannelT))

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_devinfo_get_channel_ids_m)
        lib.pqos_devinfo_get_channel_ids = func_mock

        dev = PqosDevInfo()

        with patch('pqos.devinfo.free_memory'):
            channel_ids = dev.get_channel_ids(4, 3)

        for chan_id, chan_id_org in zip(channel_ids, channels_org):
            self.assertEqual(chan_id, chan_id_org)

        lib.pqos_devinfo_get_channel_ids.assert_called_once()

    @patch('pqos.devinfo.Pqos')
    def test_get_channel(self, pqos_mock_cls):
        "Tests get_channel() method"

        cpqos_chan = CPqosChannel(channel_id=0x20200, rmid_tagging=True,
                                  clos_tagging=True, min_rmid=1, max_rmid=10,
                                  min_clos=2, max_clos=12)

        def pqos_devinfo_get_channel_m(_p_dev, channel_id):
            "Mock pqos_devinfo_get_channel()."

            self.assertEqual(channel_id, 0x20200)

            return ctypes.pointer(cpqos_chan)

        lib = pqos_mock_cls.return_value.lib
        func_mock = MagicMock(side_effect=pqos_devinfo_get_channel_m)
        lib.pqos_devinfo_get_channel = func_mock

        dev = PqosDevInfo()
        chan_info = dev.get_channel(0x20200)

        self.assertEqual(chan_info.channel_id, 0x20200)
        self.assertEqual(chan_info.rmid_tagging, True)
        self.assertEqual(chan_info.clos_tagging, True)
        self.assertEqual(chan_info.min_rmid, 1)
        self.assertEqual(chan_info.max_rmid, 10)
        self.assertEqual(chan_info.min_clos, 2)
        self.assertEqual(chan_info.max_clos, 12)
        lib.pqos_devinfo_get_channel.assert_called_once()
