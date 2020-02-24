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
Unit tests for CPU information module.
"""

from __future__ import absolute_import, division, print_function
import ctypes
import unittest

from unittest.mock import MagicMock, patch

from pqos.test.mock_pqos import mock_pqos_lib
from pqos.test.helper import ctypes_ref_set_int, ctypes_build_array

from pqos.cpuinfo import (
    PqosCpuInfo, CPqosCacheInfo, CPqosCoreInfo, CPqosCpuInfo
)
from pqos.error import PqosError


class PqosCpuInfoMockBuilder(object):
    "Builds a mock CPqosCpuInfo object."

    def __init__(self):
        self.buf = None

    def build_l2_cache_info(self):  # pylint: disable=no-self-use
        "Builds L2 cache information."

        cache_info = CPqosCacheInfo(detected=1, num_ways=2, num_sets=1,
                                    num_partitions=1, line_size=64 * 1024,
                                    total_size=2 * 1024 * 1024,
                                    way_size=1024 * 1024)
        return cache_info

    def build_l3_cache_info(self):  # pylint: disable=no-self-use
        "Builds L3 cache information."

        cache_info = CPqosCacheInfo(detected=1, num_ways=2, num_sets=1,
                                    num_partitions=1, line_size=64 * 1024,
                                    total_size=2 * 1024 * 1024,
                                    way_size=1024 * 1024)
        return cache_info

    def build_core_infos(self):  # pylint: disable=no-self-use
        "Builds core information."

        core_info1 = CPqosCoreInfo(lcore=0, socket=0, l3_id=0, l2_id=0, l3cat_id=0, mba_id=0)
        core_info2 = CPqosCoreInfo(lcore=1, socket=0, l3_id=0, l2_id=1, l3cat_id=0, mba_id=0)
        return [core_info1, core_info2]

    def build(self):
        "Builds CPU information and returns a pointer to that object."

        l2_cache_info = self.build_l2_cache_info()
        l3_cache_info = self.build_l3_cache_info()
        core_infos = self.build_core_infos()

        num_cores = len(core_infos)
        core_infos_size = num_cores * ctypes.sizeof(CPqosCoreInfo)
        cpuinfo_mem_size = ctypes.sizeof(CPqosCpuInfo) + core_infos_size

        self.buf = (ctypes.c_char * cpuinfo_mem_size)()

        cpuinfo = CPqosCpuInfo(mem_size=cpuinfo_mem_size, l2=l2_cache_info,
                               l3=l3_cache_info, num_cores=num_cores)

        cpu_size = ctypes.sizeof(cpuinfo)
        cpuinfo_array = ctypes_build_array(core_infos)
        ctypes.memmove(self.buf, ctypes.addressof(cpuinfo), cpu_size)
        ctypes.memmove(ctypes.byref(self.buf, cpu_size),
                       ctypes.addressof(cpuinfo_array),
                       ctypes.sizeof(cpuinfo_array))

        return ctypes.cast(ctypes.pointer(self.buf),
                           ctypes.POINTER(type(cpuinfo)))


class TestPqosCpuInfo(unittest.TestCase):
    "Tests for PqosCpuInfo class."

    @mock_pqos_lib
    def test_init(self, lib):
        """
        Tests if the pointer to CPU information object given
        to PQoS library APIs is the same returned from pqos_cap_get() API during
        an initialization of PqosCpuInfo.
        """

        builder = PqosCpuInfoMockBuilder()
        p_cpu = builder.build()

        def pqos_cap_get_mock(_cap_ref, cpu_ref):
            "Mock pqos_cap_get()."

            ctypes.memmove(cpu_ref, ctypes.addressof(p_cpu),
                           ctypes.sizeof(p_cpu))
            return 0

        def pqos_socketid_m(cpu_ref, _core, _socket_ref):
            "Mock pqos_cpu_get_socketid()."

            cpu_ref_addr = ctypes.addressof(cpu_ref.contents)
            p_cpu_addr = ctypes.addressof(p_cpu.contents)
            self.assertEqual(cpu_ref_addr, p_cpu_addr)
            return 1

        lib.pqos_cap_get = MagicMock(side_effect=pqos_cap_get_mock)
        lib.pqos_cpu_get_socketid = MagicMock(side_effect=pqos_socketid_m,
                                              __name__=u'pqos_cpu_get_socketid')

        pqos_cpu = PqosCpuInfo()

        with self.assertRaises(PqosError):
            pqos_cpu.get_socketid(0)

        lib.pqos_cpu_get_socketid.assert_called_once()
        lib.pqos_cap_get.assert_called_once()

    @mock_pqos_lib
    def test_get_vendor(self, lib):
        "Tests get_vendor() method"

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_get_vendor = MagicMock(return_value=1)

        cpu = PqosCpuInfo()

        self.assertEqual(cpu.get_vendor(), "INTEL")

    @mock_pqos_lib
    def test_get_sockets(self, lib):
        "Tests get_sockets() method."

        sockets_mock = [ctypes.c_uint(socket) for socket in [0, 1, 2, 3]]
        sockets_arr = ctypes_build_array(sockets_mock)

        def pqos_cpu_get_sockets_m(_p_cpu, count_ref):
            "Mock pqos_cpu_get_sockets()."

            ctypes_ref_set_int(count_ref, len(sockets_arr))
            return ctypes.cast(sockets_arr, ctypes.POINTER(ctypes.c_uint))

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_cpu_get_sockets = MagicMock(side_effect=pqos_cpu_get_sockets_m)

        cpu = PqosCpuInfo()

        with patch('pqos.cpuinfo.free_memory'):
            sockets = cpu.get_sockets()

        self.assertEqual(len(sockets), 4)
        self.assertEqual(sockets[0], 0)
        self.assertEqual(sockets[1], 1)
        self.assertEqual(sockets[2], 2)
        self.assertEqual(sockets[3], 3)

    @mock_pqos_lib
    def test_get_l2ids(self, lib):
        "Tests get_l2ids() method."

        l2ids_mock = [ctypes.c_uint(l2id) for l2id in [7, 2, 3, 5]]
        l2ids_arr = ctypes_build_array(l2ids_mock)

        def pqos_cpu_get_l2ids_m(_p_cpu, count_ref):
            "Mock pqos_cpu_get_l2ids()."

            ctypes_ref_set_int(count_ref, len(l2ids_arr))
            return ctypes.cast(l2ids_arr, ctypes.POINTER(ctypes.c_uint))

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_cpu_get_l2ids = MagicMock(side_effect=pqos_cpu_get_l2ids_m)

        cpu = PqosCpuInfo()

        with patch('pqos.cpuinfo.free_memory'):
            l2ids = cpu.get_l2ids()

        self.assertEqual(len(l2ids), 4)
        self.assertEqual(l2ids[0], 7)
        self.assertEqual(l2ids[1], 2)
        self.assertEqual(l2ids[2], 3)
        self.assertEqual(l2ids[3], 5)

    @mock_pqos_lib
    def test_get_cores_l3id(self, lib):
        "Tests get_cores_l3id() method."

        cores_mock = [ctypes.c_uint(core) for core in [4, 2, 5]]
        cores_arr = ctypes_build_array(cores_mock)

        def pqos_cores_l3id_m(_p_cpu, l3_id, count_ref):
            "Mock pqos_cpu_get_cores_l3id()."

            self.assertEqual(l3_id, 2)
            ctypes_ref_set_int(count_ref, len(cores_arr))
            return ctypes.cast(cores_arr, ctypes.POINTER(ctypes.c_uint))

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_cpu_get_cores_l3id = MagicMock(side_effect=pqos_cores_l3id_m)

        cpu = PqosCpuInfo()

        with patch('pqos.cpuinfo.free_memory'):
            cores = cpu.get_cores_l3id(2)

        self.assertEqual(len(cores), 3)
        self.assertEqual(cores[0], 4)
        self.assertEqual(cores[1], 2)
        self.assertEqual(cores[2], 5)

        lib.pqos_cpu_get_cores_l3id.assert_called_once()

    @mock_pqos_lib
    def test_get_cores(self, lib):
        "Tests get_cores() method."

        cores_mock = [ctypes.c_uint(core) for core in [8, 7, 3]]
        cores_arr = ctypes_build_array(cores_mock)

        def pqos_cpu_get_cores_m(_p_cpu, socket, count_ref):
            "Mock pqos_cpu_get_cores()."

            self.assertEqual(socket, 0)
            ctypes_ref_set_int(count_ref, len(cores_arr))
            return ctypes.cast(cores_arr, ctypes.POINTER(ctypes.c_uint))

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_cpu_get_cores = MagicMock(side_effect=pqos_cpu_get_cores_m)

        cpu = PqosCpuInfo()

        with patch('pqos.cpuinfo.free_memory'):
            cores = cpu.get_cores(0)

        self.assertEqual(len(cores), 3)
        self.assertEqual(cores[0], 8)
        self.assertEqual(cores[1], 7)
        self.assertEqual(cores[2], 3)

        lib.pqos_cpu_get_cores.assert_called_once()

    @mock_pqos_lib
    def test_get_core_info(self, lib):
        "Tests get_core_info() method."

        coreinfo_mock = CPqosCoreInfo(lcore=1, socket=0, l3_id=1, l2_id=7, l3cat_id=1, mba_id=1)

        def pqos_get_core_info_m(_p_cpu, core):
            "Mock pqos_cpu_get_core_info()."

            self.assertEqual(core, 1)
            return ctypes.pointer(coreinfo_mock)

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_cpu_get_core_info = MagicMock(side_effect=pqos_get_core_info_m)

        cpu = PqosCpuInfo()
        coreinfo = cpu.get_core_info(1)

        self.assertEqual(coreinfo.core, 1)
        self.assertEqual(coreinfo.socket, 0)
        self.assertEqual(coreinfo.l3_id, 1)
        self.assertEqual(coreinfo.l2_id, 7)

        lib.pqos_cpu_get_core_info.assert_called_once()

    @mock_pqos_lib
    def test_get_one_core(self, lib):
        "Tests get_one_core() method."

        def pqos_get_one_core_m(_p_cpu, socket, core_ref):
            "Mock pqos_cpu_get_one_core()."

            self.assertEqual(socket, 1)
            ctypes_ref_set_int(core_ref, 5)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_cpu_get_one_core = MagicMock(side_effect=pqos_get_one_core_m,
                                              __name__=u'pqos_cpu_get_one_core')

        cpu = PqosCpuInfo()
        core = cpu.get_one_core(1)

        self.assertEqual(core, 5)

        lib.pqos_cpu_get_one_core.assert_called_once()

    @mock_pqos_lib
    def test_get_one_by_l2id(self, lib):
        "Tests get_one_by_l2id() method."

        def pqos_get_by_l2id_m(_p_cpu, l2_id, core_ref):
            "Mock pqos_cpu_get_one_core()."

            self.assertEqual(l2_id, 8)
            ctypes_ref_set_int(core_ref, 15)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        func_mock = MagicMock(side_effect=pqos_get_by_l2id_m,
                              __name__=u'pqos_cpu_get_one_by_l2id')
        lib.pqos_cpu_get_one_by_l2id = func_mock

        cpu = PqosCpuInfo()
        core = cpu.get_one_by_l2id(8)

        self.assertEqual(core, 15)

        lib.pqos_cpu_get_one_by_l2id.assert_called_once()

    @mock_pqos_lib
    def test_check_core_valid(self, lib):
        "Tests check_core() method when the given core is valid."

        def pqos_cpu_check_core_m(_p_cpu, core):
            "Mock pqos_cpu_check_core()."

            self.assertEqual(core, 4)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_cpu_check_core = MagicMock(side_effect=pqos_cpu_check_core_m)

        cpu = PqosCpuInfo()
        result = cpu.check_core(4)

        self.assertTrue(result)

        lib.pqos_cpu_check_core.assert_called_once()

    @mock_pqos_lib
    def test_check_core_invalid(self, lib):
        "Tests check_core() method when the given core is invalid."

        def pqos_cpu_check_core_m(_p_cpu, core):
            "Mock pqos_cpu_check_core()."

            self.assertEqual(core, 99999)
            return 1

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_cpu_check_core = MagicMock(side_effect=pqos_cpu_check_core_m)

        cpu = PqosCpuInfo()
        result = cpu.check_core(99999)

        self.assertFalse(result)

        lib.pqos_cpu_check_core.assert_called_once()

    @mock_pqos_lib
    def test_get_socketid(self, lib):
        "Tests get_socketid() method."

        def pqos_get_socketid_m(_p_cpu, core, socket_ref):
            "Mock pqos_cpu_get_socketid()."

            self.assertEqual(core, 3)
            ctypes_ref_set_int(socket_ref, 4)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_cpu_get_socketid = MagicMock(side_effect=pqos_get_socketid_m,
                                              __name__=u'pqos_cpu_get_socketid')

        cpu = PqosCpuInfo()
        socket = cpu.get_socketid(3)

        self.assertEqual(socket, 4)

        lib.pqos_cpu_get_socketid.assert_called_once()

    @mock_pqos_lib
    def test_get_clusterid(self, lib):
        "Tests get_clusterid() method."

        def pqos_get_clusterid_m(_p_cpu, core, cluster_ref):
            "Mock pqos_cpu_get_clusterid()."

            self.assertEqual(core, 1)
            ctypes_ref_set_int(cluster_ref, 0)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        func_mock = MagicMock(side_effect=pqos_get_clusterid_m,
                              __name__=u'pqos_cpu_get_clusterid')
        lib.pqos_cpu_get_clusterid = func_mock

        cpu = PqosCpuInfo()
        cluster = cpu.get_clusterid(1)

        self.assertEqual(cluster, 0)

        lib.pqos_cpu_get_clusterid.assert_called_once()
