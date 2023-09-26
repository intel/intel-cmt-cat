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
The module defines PqosAlloc which can be used to associate allocation classes
of service to logical cores.
"""

from __future__ import absolute_import, division, print_function
import ctypes

from pqos.capability import pqos_get_type_enum
from pqos.common import pqos_handle_error, free_memory
from pqos.native_struct import PqosChannelT
from pqos.pqos import Pqos


def _get_feature_config(feature_cfg):
    feature_cfg_map = {
        'any': 0,
        'off': 1,
        'on': 2
    }

    return feature_cfg_map[feature_cfg.lower()]

def _get_cdp_config(cdp_config):
    return _get_feature_config(cdp_config)

def _get_mba_config(mba_config):
    mba_config_map = {
        'any': 0,
        'default': 1,
        'ctrl': 2
    }

    return mba_config_map[mba_config.lower()]

class CPqosAllocConfig(ctypes.Structure):
    "pqos_alloc_config structure"
    # pylint: disable=attribute-defined-outside-init

    _fields_ = [
        ("l3_cdp", ctypes.c_int),
        ("l2_cdp", ctypes.c_int),
        ("mba", ctypes.c_int),
        ("mba40", ctypes.c_int)
    ]

    def set_l3_cdp(self, l3_cdp):
        """
        Sets L3 CDP configuration.

        Parameter:
            l3_cdp: L3 CDP configuration ('any', 'on' or 'off')
        """

        self.l3_cdp = _get_cdp_config(l3_cdp)

    def set_l2_cdp(self, l2_cdp):
        """
        Sets L2 CDP configuration.

        Parameter:
            l2_cdp: L2 CDP configuration ('any', 'on' or 'off')
        """

        self.l2_cdp = _get_cdp_config(l2_cdp)

    def set_mba(self, mba):
        """
        Sets MBA configuration.

        Parameter:
            mba: MBA configuration ('any', 'ctrl' or 'default')
        """

        self.mba = _get_mba_config(mba)

    def set_mba40(self, mba40):
        """
        Sets MBA 4.0 configuration.

        Parameter:
            mba40: MBA 4.0 configuration ('any', 'on' or 'off')
        """

        self.mba40 = _get_feature_config(mba40)

def _get_technology_mask(technologies):
    mask = 0
    for technology in technologies:
        tech_mask = 1 << pqos_get_type_enum(technology)
        mask |= tech_mask

    return mask

def _get_list_of_cores(cores):
    core_array_len = len(cores)
    core_array = (ctypes.c_uint * core_array_len)(*cores)
    return core_array

def _get_list_of_pids(pids):
    pid_array_len = len(pids)
    pid_array = (ctypes.c_uint * pid_array_len)(*pids)
    return pid_array

class PqosAlloc(object):
    """
    Handles associations between allocation classes of service and logical
    cores.
    """

    def __init__(self):
        self.pqos = Pqos()

    def assoc_set(self, core, class_id):
        """
        Associates a logical core with a given class of service.

        Parameters:
            core: a logical core number
            class_id: class of service
        """

        ret = self.pqos.lib.pqos_alloc_assoc_set(core, class_id)
        pqos_handle_error('pqos_alloc_assoc_set', ret)

    def assoc_get(self, core):
        """
        Reads association of a logical core with a class of service.

        Parameters:
            core: a logical core number

        Returns:
            class of service
        """

        class_id = ctypes.c_uint(0)
        ret = self.pqos.lib.pqos_alloc_assoc_get(core, ctypes.byref(class_id))
        pqos_handle_error('pqos_alloc_assoc_get', ret)
        return class_id.value

    def assoc_set_pid(self, pid, class_id):
        """
        OS interface to associate a task with a given class of service.

        Parameters:
            pid: process ID
            class_id: class of service
        """

        ret = self.pqos.lib.pqos_alloc_assoc_set_pid(pid, class_id)
        pqos_handle_error('pqos_alloc_assoc_set_pid', ret)

    def assoc_get_pid(self, pid):
        """
        OS interface to read association of a task with class of service.

        Parameters:
            pid: process ID

        Returns:
            class of service
        """

        class_id = ctypes.c_uint(0)
        class_id_ref = ctypes.byref(class_id)
        ret = self.pqos.lib.pqos_alloc_assoc_get_pid(pid, class_id_ref)
        pqos_handle_error('pqos_alloc_assoc_get_pid', ret)
        return class_id.value

    def assign(self, technologies, cores):
        """
        Assigns a first available class of service to cores.

        While searching for available class of service, it takes into account
        technologies it is intended to use with.
        Note on technologies and cores:
        - if L2 CAT technology is requested then cores need to belong to
          one L2 cluster (same L2ID)
        - if only L3 CAT is requested then cores need to belong to one socket
        - if only MBA is selected then cores need to belong to one socket

        Parameters:
            technologies: a list of technologies, available options: mon, l3ca,
                          l2ca and mba
            cores: a list of cores

        Returns:
            class of service
        """

        mask = _get_technology_mask(technologies)
        class_id = ctypes.c_uint(0)
        class_id_ref = ctypes.byref(class_id)
        core_array_len = len(cores)
        core_array = _get_list_of_cores(cores)
        ret = self.pqos.lib.pqos_alloc_assign(mask, core_array, core_array_len,
                                              class_id_ref)
        pqos_handle_error('pqos_alloc_assign', ret)
        return class_id.value

    def release(self, cores):
        """
        Reassigns cores to default class of service #0.

        Parameters:
            cores: a list of cores
        """

        core_array_len = len(cores)
        core_array = _get_list_of_cores(cores)
        ret = self.pqos.lib.pqos_alloc_release(core_array, core_array_len)
        pqos_handle_error('pqos_alloc_release', ret)

    def assign_pid(self, technologies, pids):
        """
        Assigns a first available class of service to tasks specified by pids.
        Searches all COS directories from the highest to the lowest.

        While searching for available class of service, it takes into account
        technologies it is intended to use with.
        Note on technologies:
        - this parameter is currently reserved for future use
        - resctrl (Linux interface) will only provide the highest class id common
          to all supported technologies

        Parameters:
            technologies: a list of technologies, available options: mon, l3ca,
                          l2ca and mba
            pids: a list of process IDs

        Returns:
            class of service
        """

        mask = _get_technology_mask(technologies)
        pid_array = _get_list_of_cores(pids)
        pid_array_len = len(pid_array)
        class_id = ctypes.c_uint(0)
        class_id_ref = ctypes.byref(class_id)
        ret = self.pqos.lib.pqos_alloc_assign_pid(mask, pid_array,
                                                  pid_array_len, class_id_ref)
        pqos_handle_error('pqos_alloc_assign_pid', ret)
        return class_id.value

    def release_pid(self, pids):
        """
        Reassigns tasks specified by pids to default class of service #0.

        Parameters:
            pids: a list of process IDs
        """

        pid_array = _get_list_of_cores(pids)
        pid_array_len = len(pid_array)
        ret = self.pqos.lib.pqos_alloc_release_pid(pid_array, pid_array_len)
        pqos_handle_error('pqos_alloc_release_pid', ret)

    def get_pids(self, class_id):
        """
        Retrieves process IDs from resctrl task file for
        a given class of service.

        Parameters:
            class_id: class of service

        Returns:
            a list of process IDs
        """

        count = ctypes.c_uint(0)
        count_ref = ctypes.byref(count)

        restype = ctypes.POINTER(ctypes.c_uint)
        self.pqos.lib.pqos_pid_get_pid_assoc.restype = restype
        p_pids = self.pqos.lib.pqos_pid_get_pid_assoc(class_id, count_ref)

        if p_pids:
            pids = [p_pids[i] for i in range(count.value)]
            free_memory(p_pids)
        else:
            pids = []

        return pids

    def reset(self, l3_cdp_cfg, l2_cdp_cfg, mba_cfg):
        """
        Resets configuration of allocation technologies.

        Reverts CAT/MBA state to the one after reset:
        - all cores associated with COS0
        - all COS are set to give access to entire resource

        As part of allocation reset CDP and MBA reconfiguration
        can be performed.

        Parameters:
            l3_cdp_cfg: L3 CAT CDP configuration
            l2_cdp_cfg: L2 CAT CDP configuration
            mba_cfg: MBA configuration
        """

        l3_cdp_cfg_enum = _get_cdp_config(l3_cdp_cfg)
        l2_cdp_cfg_enum = _get_cdp_config(l2_cdp_cfg)
        mba_cfg_enum = _get_mba_config(mba_cfg)

        ret = self.pqos.lib.pqos_alloc_reset(l3_cdp_cfg_enum, l2_cdp_cfg_enum,
                                             mba_cfg_enum)
        pqos_handle_error('pqos_alloc_reset', ret)


    def reset_config(self, cfg):
        """
        Resets configuration of allocation technologies.

        Parameters:
            cfg: allocation configuration (CPqosAllocConfig)
        """

        cfg_ptr = ctypes.pointer(cfg)
        ret = self.pqos.lib.pqos_alloc_reset_config(cfg_ptr)
        pqos_handle_error('pqos_alloc_reset_config', ret)

    def assoc_get_channel(self, channel):
        """
        Gets a class of service associated with a given channel.

        Parameters:
            channel: a channel

        Returns:
            a class of service
        """

        class_id = ctypes.c_uint(0)
        ref = ctypes.byref(class_id)
        argtypes = [PqosChannelT, ctypes.POINTER(ctypes.c_uint)]
        self.pqos.lib.pqos_alloc_assoc_get_channel.argtypes = argtypes
        ret = self.pqos.lib.pqos_alloc_assoc_get_channel(channel, ref)
        pqos_handle_error('pqos_alloc_assoc_get_channel', ret)
        return class_id.value

    def assoc_get_dev(self, segment, bdf, virtual_channel):
        """
        Gets a class of service associated with a given device virtual channel.

        Parameters:
            segment: device segment/domain
            bdf: device ID
            virtual_channel: device virtual channel

        Returns:
            a class of service
        """

        class_id = ctypes.c_uint(0)
        ref = ctypes.byref(class_id)
        argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint,
                    ctypes.POINTER(ctypes.c_uint)]
        self.pqos.lib.pqos_alloc_assoc_get_dev.argtypes = argtypes
        ret = self.pqos.lib.pqos_alloc_assoc_get_dev(segment, bdf,
                                                     virtual_channel, ref)
        pqos_handle_error('pqos_alloc_assoc_get_dev', ret)
        return class_id.value

    def assoc_set_channel(self, channel, class_id):
        """
        Associates a channel with a given class of service.

        Parameters:
            channel: a channel
            class_id: a class of service
        """

        argtypes = [PqosChannelT, ctypes.c_uint]
        self.pqos.lib.pqos_alloc_assoc_set_channel.argtypes = argtypes
        ret = self.pqos.lib.pqos_alloc_assoc_set_channel(channel, class_id)
        pqos_handle_error('pqos_alloc_assoc_set_channel', ret)

    def assoc_set_dev(self, segment, bdf, virtual_channel, class_id):
        """
        Associates a channel with a given class of service.

        Parameters:
            segment: device segment/domain
            bdf: device ID
            virtual_channel: device virtual channel
            class_id: a class of service
        """

        segment = ctypes.c_uint16(segment)
        bdf = ctypes.c_uint16(bdf)
        virtual_channel = ctypes.c_uint(virtual_channel)
        class_id = ctypes.c_uint(class_id)
        argtypes = [ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint,
                    ctypes.c_uint]
        self.pqos.lib.pqos_alloc_assoc_set_dev.argtypes = argtypes
        ret = self.pqos.lib.pqos_alloc_assoc_set_dev(segment, bdf,
                                                     virtual_channel, class_id)
        pqos_handle_error('pqos_alloc_assoc_set_dev', ret)
