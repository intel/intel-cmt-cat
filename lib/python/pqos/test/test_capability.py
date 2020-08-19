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
Unit tests for capabilities module.
"""

from __future__ import absolute_import, division, print_function
import ctypes
import unittest

from unittest.mock import MagicMock

from pqos.test.mock_pqos import mock_pqos_lib
from pqos.test.helper import ctypes_ref_set_int, ctypes_build_array

from pqos.capability import (
    PqosCap, CPqosMonitor, CPqosCapabilityMonitoring, CPqosCapabilityL3,
    CPqosCapabilityL2, CPqosCapabilityMBA, CPqosCapabilityUnion,
    CPqosCapability, CPqosCap
)
from pqos.error import PqosError


class PqosCapMockBuilder(object):
    "Builds a mock CPqosCap object."

    def __init__(self):
        self.mon = None
        self.l3ca = None
        self.l2ca = None
        self.mba = None
        self.cap = None
        self.buf = None

    def build_monitoring_capability(self):
        """
        Builds mock CPqosCapabilityMonitoring object. Might be overwritten
        in a subclass if necessary.
        """
        # pylint: disable=no-self-use

        mon_mem_size = ctypes.sizeof(CPqosCapabilityMonitoring)
        events = (CPqosMonitor * 0)()
        mon = CPqosCapabilityMonitoring(mem_size=mon_mem_size, max_rmid=0,
                                        l3_size=0, num_events=0, events=events)
        return mon

    def build_l3ca_capability(self):
        """
        Builds mock CPqosCapabilityL3 object. Might be overwritten
        in a subclass if necessary.
        """
        # pylint: disable=no-self-use

        l3ca = CPqosCapabilityL3(mem_size=ctypes.sizeof(CPqosCapabilityL3),
                                 num_classes=2, num_ways=8, way_size=1024*1024,
                                 way_contention=0, cdp=1, cdp_on=0)
        return l3ca

    def build_l2ca_capability(self):
        """
        Builds mock CPqosCapabilityL2 object. Might be overwritten
        in a subclass if necessary.
        """
        # pylint: disable=no-self-use

        l2ca = CPqosCapabilityL2(mem_size=ctypes.sizeof(CPqosCapabilityL3),
                                 num_classes=2, num_ways=8, way_size=1024*1024,
                                 way_contention=0, cdp=1, cdp_on=0)
        return l2ca

    def build_mba_capability(self):
        """
        Builds mock CPqosCapabilityMBA object. Might be overwritten
        in a subclass if necessary.
        """
        # pylint: disable=no-self-use

        mba = CPqosCapabilityMBA(mem_size=ctypes.sizeof(CPqosCapabilityMBA),
                                 num_classes=2, throttle_max=95, throttle_step=15,
                                 is_linear=1, ctrl=1, ctrl_on=0)
        return mba

    def build_capabilities(self):
        """
        Builds capabilities for monitoring, L3/L2 cache allocation and
        memory bandwidth allocation.
        """

        self.mon = self.build_monitoring_capability()
        self.l3ca = self.build_l3ca_capability()
        self.l2ca = self.build_l2ca_capability()
        self.mba = self.build_mba_capability()

    def build_capability_array(self):
        "Build capabilities array (ctypes array of CPqosCapability objects)."

        capabilities = []

        if self.mon:
            mon_u = CPqosCapabilityUnion(mon=ctypes.pointer(self.mon))
            mon_cap = CPqosCapability(type=CPqosCapability.PQOS_CAP_TYPE_MON,
                                      u=mon_u)
            capabilities.append(mon_cap)

        if self.l3ca:
            l3ca_u = CPqosCapabilityUnion(l3ca=ctypes.pointer(self.l3ca))
            l3ca_cap = CPqosCapability(type=CPqosCapability.PQOS_CAP_TYPE_L3CA,
                                       u=l3ca_u)
            capabilities.append(l3ca_cap)

        if self.l2ca:
            l2ca_u = CPqosCapabilityUnion(l2ca=ctypes.pointer(self.l2ca))
            l2ca_cap = CPqosCapability(type=CPqosCapability.PQOS_CAP_TYPE_L2CA,
                                       u=l2ca_u)
            capabilities.append(l2ca_cap)

        if self.mba:
            mba_u = CPqosCapabilityUnion(mba=ctypes.pointer(self.mba))
            mba_cap = CPqosCapability(type=CPqosCapability.PQOS_CAP_TYPE_MBA,
                                      u=mba_u)
            capabilities.append(mba_cap)

        return ctypes_build_array(capabilities)

    def build_cap(self, num_cap):
        "Builds mock CPqosCap object."

        cap_mem_size = ctypes.sizeof(CPqosCap) \
                       + num_cap * ctypes.sizeof(CPqosCapability)
        self.cap = CPqosCap(mem_size=cap_mem_size, version=123, num_cap=num_cap)

    def build(self):
        """
        Builds mock capabilities object and returns ctypes pointer
        to CPqosCap object.
        """

        self.build_capabilities()
        cap_arr = self.build_capability_array()
        num_cap = len(cap_arr)
        self.build_cap(num_cap)

        self.buf = (ctypes.c_char * self.cap.mem_size)()
        cap_size = ctypes.sizeof(self.cap)
        ctypes.memmove(self.buf, ctypes.addressof(self.cap), cap_size)
        ctypes.memmove(ctypes.byref(self.buf, cap_size),
                       ctypes.addressof(cap_arr), ctypes.sizeof(cap_arr))

        return ctypes.cast(ctypes.pointer(self.buf),
                           ctypes.POINTER(type(self.cap)))


def _prepare_get_type(lib, cap):
    "Initializes test for PqosCap.get_type() method."

    def pqos_cap_get_type_mock(_cap_ref, type_enum, p_cap_item_ref):
        "Mock pqos_cap_get_type()."

        if type_enum != cap.type:
            return 1

        cap_ptr = ctypes.pointer(cap)
        ctypes.memmove(p_cap_item_ref, ctypes.addressof(cap_ptr),
                       ctypes.sizeof(type(cap_ptr)))
        return 0

    lib.pqos_cap_get = MagicMock(return_value=0)
    lib.pqos_cap_get_type = MagicMock(side_effect=pqos_cap_get_type_mock)


class TestPqosCap(unittest.TestCase):
    "Tests for PqosCap class."

    @mock_pqos_lib
    def test_init(self, lib):
        """
        Tests if the pointer to capabilities object given to PQoS library APIs
        is the same returned from pqos_cap_get() API during
        an initialization of PqosCap.
        """

        builder = PqosCapMockBuilder()
        p_cap = builder.build()

        def pqos_cap_get_mock(cap_ref, _cpu_ref):
            "Mock pqos_cap_get()."

            ctypes.memmove(cap_ref, ctypes.addressof(p_cap),
                           ctypes.sizeof(p_cap))
            return 0

        def pqos_cap_get_type_mock(cap_ref, _type_enum, _p_cap_item_ref):
            "Mock pqos_cap_get_type()."

            cap_ref_addr = ctypes.addressof(cap_ref.contents)
            p_cap_addr = ctypes.addressof(p_cap.contents)
            self.assertEqual(cap_ref_addr, p_cap_addr)
            return 1

        lib.pqos_cap_get = MagicMock(side_effect=pqos_cap_get_mock)
        lib.pqos_cap_get_type = MagicMock(side_effect=pqos_cap_get_type_mock)

        pqos_cap = PqosCap()

        with self.assertRaises(PqosError):
            pqos_cap.get_type('mba')

    @mock_pqos_lib
    def test_get_type_l3ca(self, lib):
        "Tests get_type() method for L3 cache allocation."
        l3ca = CPqosCapabilityL3(mem_size=ctypes.sizeof(CPqosCapabilityL3),
                                 num_classes=2, num_ways=8, way_size=1024*1024,
                                 way_contention=0, cdp=1, cdp_on=0)
        l3ca_u = CPqosCapabilityUnion(l3ca=ctypes.pointer(l3ca))
        l3ca_cap = CPqosCapability(type=CPqosCapability.PQOS_CAP_TYPE_L3CA,
                                   u=l3ca_u)

        _prepare_get_type(lib, l3ca_cap)

        pqos_cap = PqosCap()
        l3ca_capability = pqos_cap.get_type('l3ca')

        self.assertEqual(l3ca_capability.num_classes, 2)
        self.assertEqual(l3ca_capability.num_ways, 8)
        self.assertEqual(l3ca_capability.way_size, 1024*1024)
        self.assertEqual(l3ca_capability.way_contention, 0)
        self.assertEqual(l3ca_capability.cdp, True)
        self.assertEqual(l3ca_capability.cdp_on, False)

    @mock_pqos_lib
    def test_get_type_l2ca(self, lib):
        "Tests get_type() method for L2 cache allocation."
        l2ca = CPqosCapabilityL2(mem_size=ctypes.sizeof(CPqosCapabilityL2),
                                 num_classes=4, num_ways=16,
                                 way_size=2*1024*1024, way_contention=0, cdp=1,
                                 cdp_on=0)
        l2ca_u = CPqosCapabilityUnion(l2ca=ctypes.pointer(l2ca))
        l2ca_cap = CPqosCapability(type=CPqosCapability.PQOS_CAP_TYPE_L2CA,
                                   u=l2ca_u)

        _prepare_get_type(lib, l2ca_cap)

        pqos_cap = PqosCap()
        l2ca_capability = pqos_cap.get_type('l2ca')

        self.assertEqual(l2ca_capability.num_classes, 4)
        self.assertEqual(l2ca_capability.num_ways, 16)
        self.assertEqual(l2ca_capability.way_size, 2*1024*1024)
        self.assertEqual(l2ca_capability.way_contention, 0)
        self.assertEqual(l2ca_capability.cdp, True)
        self.assertEqual(l2ca_capability.cdp_on, False)

    @mock_pqos_lib
    def test_get_type_mba(self, lib):
        "Tests get_type() method for MBA."

        mba = CPqosCapabilityMBA(mem_size=ctypes.sizeof(CPqosCapabilityMBA),
                                 num_classes=2, throttle_max=95,
                                 throttle_step=15, is_linear=1, ctrl=1,
                                 ctrl_on=0)
        mba_u = CPqosCapabilityUnion(mba=ctypes.pointer(mba))
        mba_cap = CPqosCapability(type=CPqosCapability.PQOS_CAP_TYPE_MBA,
                                  u=mba_u)

        _prepare_get_type(lib, mba_cap)

        pqos_cap = PqosCap()
        mba_capability = pqos_cap.get_type('mba')

        self.assertEqual(mba_capability.num_classes, 2)
        self.assertEqual(mba_capability.throttle_max, 95)
        self.assertEqual(mba_capability.throttle_step, 15)
        self.assertEqual(mba_capability.is_linear, True)
        self.assertEqual(mba_capability.ctrl, True)
        self.assertEqual(mba_capability.ctrl_on, False)

    @mock_pqos_lib
    def test_get_l3ca_cos_num(self, lib):
        "Tests get_l3ca_cos_num() method."

        def pqos_l3ca_cos_num_m(_cap_ref, cos_num_ref):
            "Mock pqos_l3ca_cos_num()."

            ctypes_ref_set_int(cos_num_ref, 3)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_l3ca_get_cos_num = MagicMock(side_effect=pqos_l3ca_cos_num_m)

        pqos_cap = PqosCap()
        cos_num = pqos_cap.get_l3ca_cos_num()

        self.assertEqual(cos_num, 3)

    @mock_pqos_lib
    def test_get_l2ca_cos_num(self, lib):
        "Tests get_l2ca_cos_num() method."

        def pqos_l2ca_cos_num_m(_cap_ref, cos_num_ref):
            "Mock pqos_l2ca_cos_num()."

            ctypes_ref_set_int(cos_num_ref, 4)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_l2ca_get_cos_num = MagicMock(side_effect=pqos_l2ca_cos_num_m)

        pqos_cap = PqosCap()
        cos_num = pqos_cap.get_l2ca_cos_num()

        self.assertEqual(cos_num, 4)

    @mock_pqos_lib
    def test_get_mba_cos_num(self, lib):
        "Tests get_mba_cos_num() method."

        def pqos_mba_cos_num_m(_cap_ref, cos_num_ref):
            "Mock pqos_mba_cos_num()."

            ctypes_ref_set_int(cos_num_ref, 9)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_mba_get_cos_num = MagicMock(side_effect=pqos_mba_cos_num_m)

        pqos_cap = PqosCap()
        cos_num = pqos_cap.get_mba_cos_num()

        self.assertEqual(cos_num, 9)

    @mock_pqos_lib
    def test_is_l3ca_cdp_enabled(self, lib):
        "Tests is_l3ca_cdp_enabled() method."

        def pqos_l3cdp_enabled_m(_cap_ref, supported_ref, enabled_ref):
            "Mock pqos_l3ca_cdp_enabled()."

            ctypes_ref_set_int(supported_ref, 1)
            ctypes_ref_set_int(enabled_ref, 0)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_l3ca_cdp_enabled = MagicMock(side_effect=pqos_l3cdp_enabled_m)

        pqos_cap = PqosCap()
        supported, enabled = pqos_cap.is_l3ca_cdp_enabled()

        self.assertEqual(supported, True)
        self.assertEqual(enabled, False)

    @mock_pqos_lib
    def test_is_l2ca_cdp_enabled(self, lib):
        "Tests is_l2ca_cdp_enabled() method."

        def pqos_l2cdp_enabled_m(_cap_ref, supported_ref, enabled_ref):
            "Mock pqos_l2ca_cdp_enabled()."

            ctypes_ref_set_int(supported_ref, 0)
            ctypes_ref_set_int(enabled_ref, 1)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_l2ca_cdp_enabled = MagicMock(side_effect=pqos_l2cdp_enabled_m)

        pqos_cap = PqosCap()
        supported, enabled = pqos_cap.is_l2ca_cdp_enabled()

        self.assertEqual(supported, False)
        self.assertEqual(enabled, True)

    @mock_pqos_lib
    def test_is_mba_ctrl_enabled(self, lib):
        "Tests is_mba_ctrl_enabled() method."

        def pqos_mba_ct_enabled_m(_cap_ref, supported_ref, enabled_ref):
            "Mock pqos_mba_ctrl_enabled()."

            ctypes_ref_set_int(supported_ref, 1)
            ctypes_ref_set_int(enabled_ref, -1)
            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_mba_ctrl_enabled = MagicMock(side_effect=pqos_mba_ct_enabled_m)

        pqos_cap = PqosCap()
        supported, enabled = pqos_cap.is_mba_ctrl_enabled()

        self.assertEqual(supported, True)
        self.assertEqual(enabled, None)
