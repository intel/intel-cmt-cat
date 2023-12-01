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
"Device information module."

import ctypes
from pqos.common import free_memory
from pqos.native_struct import CPqosChannel, CPqosDevinfo, PqosChannelT
from pqos.pqos import Pqos


class PqosDevInfo:
    "Device information"

    def __init__(self):
        self.pqos = Pqos()

        # Allow get_sysconfig() to raise an exception
        sysconfig = self.pqos.get_sysconfig()
        self.p_devinfo = sysconfig.dev

    def get_channel_id(self, segment, bdf, virtual_channel):
        """
        Reads control channel for device's virtual channel.

        Parameters:
            segment: device segment/domain
            bdf: device ID
            virtual_channel: device virtual channel

        Returns:
            channel or None on error
        """

        func = self.pqos.lib.pqos_devinfo_get_channel_id
        func.restype = PqosChannelT
        func.argtypes = [
            ctypes.POINTER(CPqosDevinfo),
            ctypes.c_uint16,
            ctypes.c_uint16,
            ctypes.c_uint
        ]

        result = func(self.p_devinfo, segment, bdf, virtual_channel)

        if result == 0:
            return None

        return result

    def get_channel_ids(self, segment, bdf):
        """
        Reads control channels for device.

        Parameters:
            segment: device segment/domain
            bdf: device ID

        Returns:
            a list of control channels or empty list
        """

        func = self.pqos.lib.pqos_devinfo_get_channel_ids
        func.restype = ctypes.POINTER(PqosChannelT)
        func.argtypes = [
            ctypes.POINTER(CPqosDevinfo),
            ctypes.c_uint16,
            ctypes.c_uint16,
            ctypes.POINTER(ctypes.c_uint)
        ]

        num_channels = ctypes.c_uint(0)
        num_channels_ref = ctypes.byref(num_channels)
        p_items = func(self.p_devinfo, segment, bdf, num_channels_ref)

        if not p_items or num_channels.value <= 0:
            return []

        items = [p_items[i] for i in range(num_channels.value)]
        free_memory(p_items)
        return items

    def get_channel(self, channel_id):
        """
        Retrieves channel information from dev info structure.

        Parameters:
            channel_id: channel ID

        Returns:
            channel information or None on error
        """

        func = self.pqos.lib.pqos_devinfo_get_channel
        func.restype = ctypes.POINTER(CPqosChannel)
        func.argtypes = [
            ctypes.POINTER(CPqosDevinfo),
            PqosChannelT
        ]

        p_item = func(self.p_devinfo, channel_id)

        if not p_item:
            return None

        item = p_item.contents
        return item
