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
The module defines PqosMba which can be used to read or write Memory Bandwidth
Allocation configuration.
"""

from __future__ import absolute_import, division, print_function
import ctypes

from pqos.capability import PqosCap
from pqos.common import pqos_handle_error
from pqos.pqos import Pqos


class CPqosMba(ctypes.Structure):
    "pqos_mba structure"

    _fields_ = [
        (u"class_id", ctypes.c_uint),
        (u"mb_max", ctypes.c_uint),
        (u"ctrl", ctypes.c_int)
    ]

    @classmethod
    def from_cos(cls, cos):
        "Creates CPqosMba object from PqosMba.COS object."

        ctrl = 1 if cos.ctrl else 0
        return cls(class_id=cos.class_id, mb_max=cos.mb_max, ctrl=ctrl)

    def to_cos(self, cls):
        "Creates PqosMba.COS object from CPqosMba object."

        ctrl = bool(self.ctrl)
        return cls(self.class_id, self.mb_max, ctrl)


class PqosMba(object):
    "PQoS Memory Bandwidth Allocation"

    class COS:  # pylint: disable=too-few-public-methods
        "MBA class of service configuration"

        def __init__(self, class_id, mb_max, ctrl=False):
            self.class_id = class_id  # class of service
            self.mb_max = mb_max      # maximum available bandwidth
                                      # in percentage (without MBA controller)
                                      # or in MBps (with MBA controller),
                                      # depending on ctrl flag
            self.ctrl = ctrl          # MBA controller flag

    def __init__(self):
        self.pqos = Pqos()

    def set(self, socket, requested):
        """
        Sets classes of service defined by requested configuration on a socket.

        Parameters:
            socket: socket ID
            requested: a list of PqosMba.COS objects that define requested MBA
                       configuration

        Returns:
            a list of PqosMba.COS object with actual MBA configuration
        """

        requested_coses = [CPqosMba.from_cos(req) for req in requested]
        num_cos = len(requested_coses)
        cos_arr = (CPqosMba * num_cos)(*requested_coses)
        actual_arr = (CPqosMba * num_cos)()

        ret = self.pqos.lib.pqos_mba_set(socket, num_cos, cos_arr, actual_arr)
        pqos_handle_error(u'pqos_mba_set', ret)

        actual = [cos.to_cos(self.COS) for cos in actual_arr]
        return actual

    def get(self, socket):
        """
        Reads MBA configuration for a socket.

        Parameters:
            socket: socket ID

        Returns:
            a list of PqosMba.COS object with actual MBA configuration
            for a given socket
        """

        cap = PqosCap()
        max_num_cos = cap.get_mba_cos_num()

        num_cos = ctypes.c_uint(0)
        cos_arr = (CPqosMba * max_num_cos)()

        ret = self.pqos.lib.pqos_mba_get(socket, max_num_cos,
                                         ctypes.byref(num_cos), cos_arr)
        pqos_handle_error(u'pqos_mba_get', ret)

        coses = [cos_arr[i].to_cos(self.COS) for i in range(num_cos.value)]
        return coses
