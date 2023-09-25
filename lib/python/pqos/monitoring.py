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
The module defines PqosMon which can be used to monitor cache usage and memory
bandwidth.
"""

from __future__ import absolute_import, division, print_function
import ctypes

from pqos.common import pqos_handle_error
from pqos.native_struct import (
    CPqosEventValues, CPqosMonitor, PqosChannelT, RmidT
)
from pqos.pqos import Pqos


class CPqosMonData(ctypes.Structure):
    """
    pqos_mon_data structure
    This class is not in native_struct.py because it has additional methods
    that requires Pqos() - avoiding circular dependency.
    """

    _fields_ = [
        ('valid', ctypes.c_int),
        ('event', ctypes.c_uint),
        ('context', ctypes.c_void_p),
        ('values', CPqosEventValues),
        ('num_pids', ctypes.c_uint),
        ('pids', ctypes.POINTER(ctypes.c_uint)),
        ('tid_nr', ctypes.c_uint),
        ('tid_map', ctypes.POINTER(ctypes.c_uint)),
        ('cores', ctypes.POINTER(ctypes.c_uint)),
        ('num_cores', ctypes.c_uint),
        ('channels', ctypes.POINTER(PqosChannelT)),
        ('num_channels', ctypes.c_uint),
        ('intl', ctypes.c_void_p)
    ]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def stop(self):
        """
        Stops monitoring.
        """

        ref = self.get_ref()
        ret = Pqos().lib.pqos_mon_stop(ref)
        pqos_handle_error('pqos_mon_stop', ret)

    def add_pids(self, pids):
        """
        Adds PIDs to the monitoring group.
        """
        ref = self.get_ref()
        num_pids = len(pids)
        pids_arr = (ctypes.c_uint * num_pids)(*pids)
        ret = Pqos().lib.pqos_mon_add_pids(num_pids, pids_arr, ref)
        pqos_handle_error('pqos_mon_add_pids', ret)

    def remove_pids(self, pids):
        """
        Removes PIDs from the monitoring group.
        """
        ref = self.get_ref()
        num_pids = len(pids)
        pids_arr = (ctypes.c_uint * num_pids)(*pids)
        ret = Pqos().lib.pqos_mon_remove_pids(num_pids, pids_arr, ref)
        pqos_handle_error('pqos_mon_remove_pids', ret)

    def get_ref(self):
        """
        Gets a pointer to a monitoring data.

        Returns:
            a pointer to a monitoring data
        """

        return ctypes.pointer(self)

    def get_event_value(self, event):
        """
        Returns counter value for a given event.

        Parameters:
            event: an event identifier

        Returns:
            counter value
        """

        event_counter_map = {
            'l3_occup': 'llc',
            'lmem_bw': 'mbm_local_delta',
            'tmem_bw': 'mbm_total_delta',
            'rmem_bw': 'mbm_remote_delta',
            'perf_ipc': 'ipc',
            'perf_llc_miss': 'llc_misses_delta',
            'perf_llc_ref': 'llc_references_delta'
        }

        counter = event_counter_map.get(event)
        if not counter:
            return None

        return getattr(self.values, counter, None)


def _get_event_mask(events):
    "Converts a list of events into a binary mask accepted by PQoS library."

    event_map = {
        'l3_occup': CPqosMonitor.PQOS_MON_EVENT_L3_OCCUP,
        'lmem_bw': CPqosMonitor.PQOS_MON_EVENT_LMEM_BW,
        'tmem_bw': CPqosMonitor.PQOS_MON_EVENT_TMEM_BW,
        'rmem_bw': CPqosMonitor.PQOS_MON_EVENT_RMEM_BW,
        'perf_llc_miss': CPqosMonitor.PQOS_PERF_EVENT_LLC_MISS,
        'perf_llc_ref': CPqosMonitor.PQOS_PERF_EVENT_LLC_REF,
        'perf_ipc': CPqosMonitor.PQOS_PERF_EVENT_IPC
    }

    mask = 0
    for event in events:
        mask |= event_map.get(event, 0)

    return mask


class PqosMon:
    "PQoS Monitoring"

    def __init__(self):
        self.pqos = Pqos()

    def reset(self):
        """
        Resets monitoring configuration.
        """

        ret = self.pqos.lib.pqos_mon_reset()
        pqos_handle_error('pqos_mon_reset', ret)

    def reset_config(self, cfg):
        """
        Resets monitoring configuration.
        As part of monitoring reset I/O RDT & SNC reconfiguration can be performed.

        Parameters:
            cfg: CPqosMonConfig object
        """

        cfg_ptr = ctypes.pointer(cfg)
        ret = self.pqos.lib.pqos_mon_reset_config(cfg_ptr)
        pqos_handle_error('pqos_mon_reset_config', ret)

    def assoc_get(self, core):
        """
        Reads associated RMID for a given core.

        Parameters:
            core: core ID

        Returns:
            RMID for a given core
        """

        rmid = RmidT(0)
        rmid_ref = ctypes.byref(rmid)
        ret = self.pqos.lib.pqos_mon_assoc_get(core, rmid_ref)
        pqos_handle_error('pqos_mon_assoc_get', ret)
        return rmid.value

    def assoc_get_channel(self, channel_id):
        """
        Reads RMID association of a specified channel.

        Parameters:
            channel_id: control channel

        Returns:
            associated RMID
        """

        func_name = 'pqos_mon_assoc_get_channel'
        func = getattr(self.pqos.lib, func_name)
        func.restype = ctypes.c_int
        func.argtypes = [PqosChannelT, ctypes.POINTER(RmidT)]
        rmid = RmidT(0)
        rmid_ref = ctypes.byref(rmid)
        ret = func(channel_id, rmid_ref)
        pqos_handle_error(func_name, ret)
        return rmid.value

    def assoc_get_dev(self, segment, bdf, virtual_channel):
        """
        Reads RMID association of device channel.

        Parameters:
            segment: device segment/domain
            bdf: device ID
            virtual_channel: device virtual channel

        Returns:
            associated RMID
        """

        func_name = 'pqos_mon_assoc_get_dev'
        func = getattr(self.pqos.lib, func_name)
        func.restype = ctypes.c_int
        func.argtypes = [
            ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint,
            ctypes.POINTER(RmidT)
        ]
        rmid = RmidT(0)
        rmid_ref = ctypes.byref(rmid)
        ret = func(segment, bdf, virtual_channel, rmid_ref)
        pqos_handle_error(func_name, ret)
        return rmid.value

    def start(self, cores, events, context=None):
        """
        Starts resource monitoring on selected group of cores.

        Parameters:
            cores: a list of core IDs
            events: a list of events, available options: 'l3_occup', 'lmem_bw',
                    'tmem_bw', 'rmem_bw', 'perf_llc_miss', 'perf_ipc', 'perf_llc_ref'
            context: a pointer to additional information, by default None

        Returns:
            CPqosMonData monitoring data
        """
        return self.start_cores(cores, events, context)

    def start_cores(self, cores, events, context=None):
        """
        Starts resource monitoring on selected group of cores.

        Parameters:
            cores: a list of core IDs
            events: a list of events, available options: 'l3_occup', 'lmem_bw',
                    'tmem_bw', 'rmem_bw', 'perf_llc_miss', 'perf_ipc', 'perf_llc_ref'
            context: a pointer to additional information, by default None

        Returns:
            CPqosMonData monitoring data
        """
        group = ctypes.POINTER(CPqosMonData)()
        num_cores = len(cores)
        cores_arr = (ctypes.c_uint * num_cores)(*cores)
        event = _get_event_mask(events)
        ret = self.pqos.lib.pqos_mon_start_cores(num_cores, cores_arr, event, context,
                                                 ctypes.byref(group))
        pqos_handle_error('pqos_mon_start', ret)
        return group.contents


    def start_pids(self, pids, events, context=None):
        """
        Starts resource monitoring of a selected processes.

        Parameters:
            pids: a list of process IDs
            events: a list of events, available options: 'l3_occup', 'lmem_bw',
                    'tmem_bw', 'rmem_bw', 'perf_llc_miss', 'perf_ipc'
            context: a pointer to additional information, by default None

        Returns:
            CPqosMonData monitoring data
        """

        group = ctypes.POINTER(CPqosMonData)()
        num_pids = len(pids)
        pids_arr = (ctypes.c_uint * num_pids)(*pids)
        event = _get_event_mask(events)
        ret = self.pqos.lib.pqos_mon_start_pids2(num_pids, pids_arr, event,
                                                 context, ctypes.byref(group))
        pqos_handle_error('pqos_mon_start_pids', ret)
        return group.contents

    def start_channels(self, channels, events, context=None):
        """
        Starts resource monitoring on selected group of channels.

        Parameters:
            channels: a list of channel IDs
            events: a list of events, available options: 'l3_occup', 'lmem_bw',
                    'tmem_bw', 'rmem_bw', 'perf_llc_miss', 'perf_ipc'
            context: a pointer to additional information, by default None

        Returns:
            CPqosMonData monitoring data
        """

        func_name = 'pqos_mon_start_channels'
        func = getattr(self.pqos.lib, func_name)
        func.restype = ctypes.c_int
        func.argtypes = [
            ctypes.c_uint,
            ctypes.POINTER(PqosChannelT),
            ctypes.c_int,
            ctypes.c_void_p,
            ctypes.POINTER(CPqosMonData)
        ]

        group = ctypes.POINTER(CPqosMonData)()
        num_channels = len(channels)
        channels_arr = (PqosChannelT * num_channels)(*channels)
        event = _get_event_mask(events)
        ret = func(num_channels, channels_arr, event, context, ctypes.byref(group))
        pqos_handle_error(func_name, ret)
        return group.contents

    def start_dev(self, segment, bdf, virtual_channel, events, context=None):
        # pylint: disable=too-many-arguments
        """
        Starts resource monitoring on selected device channel.

        Parameters:
            segment: device segment/domain
            bdf: device ID
            virtual_channel: device virtual channel
            events: a list of events, available options: 'l3_occup', 'lmem_bw',
                    'tmem_bw', 'rmem_bw', 'perf_llc_miss', 'perf_ipc'
            context: a pointer to additional information, by default None

        Returns:
            CPqosMonData monitoring data
        """

        func_name = 'pqos_mon_start_dev'
        func = getattr(self.pqos.lib, func_name)
        func.argtypes = [
            ctypes.c_uint16,
            ctypes.c_uint16,
            ctypes.c_uint8,
            ctypes.c_int,
            ctypes.c_void_p,
            ctypes.POINTER(CPqosMonData)
        ]

        group = ctypes.POINTER(CPqosMonData)()
        event = _get_event_mask(events)
        ret = func(segment, bdf, virtual_channel, event, context, ctypes.byref(group))
        pqos_handle_error(func_name, ret)
        return group.contents

    def poll(self, groups):
        """
        Polls and updates monitoring data for given monitoring objects.

        Parameters:
            groups: a list of CPqosMonData monitoring object
        """

        refs = [group.get_ref() for group in groups]
        num_groups = len(groups)
        groups_arr = (ctypes.POINTER(CPqosMonData) * num_groups)(*refs)
        ret = self.pqos.lib.pqos_mon_poll(groups_arr, num_groups)
        pqos_handle_error('pqos_mon_poll', ret)
