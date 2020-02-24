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
The module defines PqosCatL2 which can be used to read or write L2 CAT
configuration.
"""

from __future__ import absolute_import, division, print_function
import ctypes

from pqos.capability import PqosCap
from pqos.common import (
    pqos_handle_error, convert_from_cos, convert_to_cos, COSBase
)
from pqos.pqos import Pqos


class CPqosL2CaMaskCDP(ctypes.Structure):
    "CDP structure from union from pqos_l2ca structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u"data_mask", ctypes.c_uint64),
        (u"code_mask", ctypes.c_uint64),
    ]


class CPqosL2CaMask(ctypes.Union):
    "Union from pqos_l2ca structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u"ways_mask", ctypes.c_uint64),
        (u"s", CPqosL2CaMaskCDP)
    ]


class CPqosL2Ca(ctypes.Structure):
    "pqos_l2ca structure"

    _fields_ = [
        (u"class_id", ctypes.c_uint),
        (u"cdp", ctypes.c_int),
        (u"u", CPqosL2CaMask),
    ]

    @classmethod
    def from_cos(cls, cos):
        "Creates CPqosL2Ca object from PqosCatL2.COS object."

        return convert_from_cos(cos, cls)

    def to_cos(self, cls):
        "Creates PqosCatL2.COS object from CPqosL2Ca object."

        return convert_to_cos(self, cls)


class PqosCatL2(object):
    "PQoS L2 Cache Allocation Technology"

    class COS(COSBase):  # pylint: disable=too-few-public-methods
        "L2 class of service configuration"

    def __init__(self):
        self.pqos = Pqos()

    def set(self, socket, coses):
        """
        Sets class of service on a specified socket.

        Parameters:
            socket: a socket number
            coses: a list of PqosCatL2.COS objects, class of service
                   configuration
        """

        pqos_l2_cas = [CPqosL2Ca.from_cos(cos) for cos in coses]
        pqos_l2_ca_arr = (CPqosL2Ca * len(pqos_l2_cas))(*pqos_l2_cas)
        ret = self.pqos.lib.pqos_l2ca_set(socket, len(pqos_l2_cas),
                                          pqos_l2_ca_arr)
        pqos_handle_error(u'pqos_l2ca_set', ret)

    def get(self, socket):
        """
        Reads classes of service from a socket.

        Parameters:
            socket: a socket number
        """

        cap = PqosCap()
        cos_num = cap.get_l2ca_cos_num()

        l2cas = (CPqosL2Ca * cos_num)()
        num_ca = ctypes.c_uint(0)
        num_ca_ref = ctypes.byref(num_ca)
        ret = self.pqos.lib.pqos_l2ca_get(socket, cos_num, num_ca_ref, l2cas)
        pqos_handle_error(u'pqos_l2ca_get', ret)

        coses = [l2ca.to_cos(self.COS) for l2ca in l2cas[:num_ca.value]]
        return coses

    def get_min_cbm_bits(self):
        """Gets minimum number of bits which must be set in L2 way mask when
        updating a class of service."""

        min_cbm_bits = ctypes.c_uint(0)
        min_cbm_bits_ref = ctypes.byref(min_cbm_bits)
        ret = self.pqos.lib.pqos_l2ca_get_min_cbm_bits(min_cbm_bits_ref)
        pqos_handle_error(u'pqos_l2ca_get_min_cbm_bits', ret)
        return min_cbm_bits.value
