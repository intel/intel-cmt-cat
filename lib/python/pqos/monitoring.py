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
The module defines PqosMon which can be used to monitor cache usage and memory
bandwidth.
"""

from __future__ import absolute_import, division, print_function
import ctypes

from pqos.common import pqos_handle_error
from pqos.pqos import Pqos
from pqos.capability import CPqosMonitor


RmidT = ctypes.c_uint32


class CPqosEventValues(ctypes.Structure):
    "pqos_event_values structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u'llc', ctypes.c_uint64),
        (u'mbm_local', ctypes.c_uint64),
        (u'mbm_total', ctypes.c_uint64),
        (u'mbm_remote', ctypes.c_uint64),
        (u'mbm_local_delta', ctypes.c_uint64),
        (u'mbm_total_delta', ctypes.c_uint64),
        (u'mbm_remote_delta', ctypes.c_uint64),
        (u'ipc_retired', ctypes.c_uint64),
        (u'ipc_retired_delta', ctypes.c_uint64),
        (u'ipc_unhalted', ctypes.c_uint64),
        (u'ipc_unhalted_delta', ctypes.c_uint64),
        (u'ipc', ctypes.c_double),
        (u'llc_misses', ctypes.c_uint64),
        (u'llc_misses_delta', ctypes.c_uint64),
    ]


class CPqosMonData(ctypes.Structure):
    "pqos_mon_data structure"

    _fields_ = [
        (u'valid', ctypes.c_int),
        (u'event', ctypes.c_uint),
        (u'context', ctypes.c_void_p),
        (u'values', CPqosEventValues),
        (u'num_pids', ctypes.c_uint),
        (u'pids', ctypes.POINTER(ctypes.c_uint)),
        (u'tid_nr', ctypes.c_uint),
        (u'tid_map', ctypes.POINTER(ctypes.c_uint)),
        (u'cores', ctypes.POINTER(ctypes.c_uint)),
        (u'num_cores', ctypes.c_uint),
        (u'intl', ctypes.c_void_p)
    ]

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.pqos = Pqos()

    def stop(self):
        """
        Stops monitoring.
        """

        ref = self.get_ref()
        ret = self.pqos.lib.pqos_mon_stop(ref)
        pqos_handle_error(u'pqos_mon_stop', ret)

    def add_pids(self, pids):
        """
        Adds PIDs to the monitoring group.
        """
        ref = self.get_ref()
        num_pids = len(pids)
        pids_arr = (ctypes.c_uint * num_pids)(*pids)
        ret = self.pqos.lib.pqos_mon_add_pids(num_pids, pids_arr, ref)
        pqos_handle_error(u'pqos_mon_add_pids', ret)

    def remove_pids(self, pids):
        """
        Removes PIDs from the monitoring group.
        """
        ref = self.get_ref()
        num_pids = len(pids)
        pids_arr = (ctypes.c_uint * num_pids)(*pids)
        ret = self.pqos.lib.pqos_mon_remove_pids(num_pids, pids_arr, ref)
        pqos_handle_error(u'pqos_mon_remove_pids', ret)

    def get_ref(self):
        """
        Gets a pointer to a monitoring data.

        Returns:
            a pointer to a monitoring data
        """

        return ctypes.pointer(self)


def _get_event_mask(events):
    "Converts a list of events into a binary mask accepted by PQoS library."

    event_map = {
        'l3_occup': CPqosMonitor.PQOS_MON_EVENT_L3_OCCUP,
        'lmem_bw': CPqosMonitor.PQOS_MON_EVENT_LMEM_BW,
        'tmem_bw': CPqosMonitor.PQOS_MON_EVENT_TMEM_BW,
        'rmem_bw': CPqosMonitor.PQOS_MON_EVENT_RMEM_BW,
        'perf_llc_miss': CPqosMonitor.PQOS_PERF_EVENT_LLC_MISS,
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
        pqos_handle_error(u'pqos_mon_reset', ret)

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
        pqos_handle_error(u'pqos_mon_assoc_get', ret)
        return rmid.value

    def start(self, cores, events, context=None):
        """
        Starts resource monitoring on selected group of cores.

        Parameters:
            cores: a list of core IDs
            events: a list of events, available options: 'l3_occup', 'lmem_bw',
                    'tmem_bw', 'rmem_bw', 'perf_llc_miss', 'perf_ipc'
            context: a pointer to additional information, by default None

        Returns:
            CPqosMonData monitoring data
        """

        group = CPqosMonData()
        group_ref = group.get_ref()
        num_cores = len(cores)
        cores_arr = (ctypes.c_uint * num_cores)(*cores)
        event = _get_event_mask(events)
        ret = self.pqos.lib.pqos_mon_start(num_cores, cores_arr, event, context,
                                           group_ref)
        pqos_handle_error(u'pqos_mon_start', ret)
        return group

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

        group = CPqosMonData()
        group_ref = group.get_ref()
        num_pids = len(pids)
        pids_arr = (ctypes.c_uint * num_pids)(*pids)
        event = _get_event_mask(events)
        ret = self.pqos.lib.pqos_mon_start_pids(num_pids, pids_arr, event,
                                                context, group_ref)
        pqos_handle_error(u'pqos_mon_start_pids', ret)
        return group

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
        pqos_handle_error(u'pqos_mon_poll', ret)
