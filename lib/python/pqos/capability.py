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
The module defines PqosCap which can be used to read PQoS capabilities.
"""

from __future__ import absolute_import, division, print_function
import ctypes

from pqos.common import pqos_handle_error
from pqos.pqos import Pqos


class CPqosCapabilityL3(ctypes.Structure):
    "pqos_cap_l3ca structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u"mem_size", ctypes.c_uint),
        (u"num_classes", ctypes.c_uint),
        (u"num_ways", ctypes.c_uint),
        (u"way_size", ctypes.c_uint),
        (u"way_contention", ctypes.c_uint64),
        (u"cdp", ctypes.c_int),
        (u"cdp_on", ctypes.c_int),
    ]


class CPqosCapabilityL2(ctypes.Structure):
    "pqos_cap_l2ca structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u"mem_size", ctypes.c_uint),
        (u"num_classes", ctypes.c_uint),
        (u"num_ways", ctypes.c_uint),
        (u"way_size", ctypes.c_uint),
        (u"way_contention", ctypes.c_uint64),
        (u"cdp", ctypes.c_int),
        (u"cdp_on", ctypes.c_int),
    ]


class CPqosCapabilityMBA(ctypes.Structure):
    "pqos_cap_mba structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u"mem_size", ctypes.c_uint),
        (u"num_classes", ctypes.c_uint),
        (u"throttle_max", ctypes.c_uint),
        (u"throttle_step", ctypes.c_uint),
        (u"is_linear", ctypes.c_int),
        (u"ctrl", ctypes.c_int),
        (u"ctrl_on", ctypes.c_int),
    ]


class CPqosMonitor(ctypes.Structure):
    "pqos_monitor structure"
    # pylint: disable=too-few-public-methods

    PQOS_MON_EVENT_L3_OCCUP = 1
    PQOS_MON_EVENT_LMEM_BW = 2
    PQOS_MON_EVENT_TMEM_BW = 4
    PQOS_MON_EVENT_RMEM_BW = 8
    RESERVED1 = 0x1000
    RESERVED2 = 0x2000
    PQOS_PERF_EVENT_LLC_MISS = 0x4000
    PQOS_PERF_EVENT_IPC = 0x8000

    _fields_ = [
        (u"type", ctypes.c_int),
        (u"max_rmid", ctypes.c_uint),
        (u"scale_factor", ctypes.c_uint32),
        (u"counter_length", ctypes.c_uint),
    ]


class CPqosCapabilityMonitoring(ctypes.Structure):
    "pqos_cap_mon structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u"mem_size", ctypes.c_uint),
        (u"max_rmid", ctypes.c_uint),
        (u"l3_size", ctypes.c_uint),
        (u"num_events", ctypes.c_uint),
        (u"events", CPqosMonitor * 0),
    ]


class CPqosCapabilityUnion(ctypes.Union):
    "Union from pqos_capability structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u"mon", ctypes.POINTER(CPqosCapabilityMonitoring)),
        (u"l3ca", ctypes.POINTER(CPqosCapabilityL3)),
        (u"l2ca", ctypes.POINTER(CPqosCapabilityL2)),
        (u"mba", ctypes.POINTER(CPqosCapabilityMBA)),
        (u"generic_ptr", ctypes.c_void_p),
    ]


class CPqosCapability(ctypes.Structure):
    "pqos_capability structure"
    # pylint: disable=too-few-public-methods

    PQOS_CAP_TYPE_MON = 0
    PQOS_CAP_TYPE_L3CA = 1
    PQOS_CAP_TYPE_L2CA = 2
    PQOS_CAP_TYPE_MBA = 3
    PQOS_CAP_TYPE_NUMOF = 4

    _fields_ = [
        (u"type", ctypes.c_int),
        (u"u", CPqosCapabilityUnion)
    ]


class CPqosCap(ctypes.Structure):
    "pqos_cap structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u"mem_size", ctypes.c_uint),
        (u"version", ctypes.c_uint),
        (u"num_cap", ctypes.c_uint),
        (u"capabilities", CPqosCapability * 0)
    ]


class PqosCapabilityMonitoring(object):
    "PQoS monitoring capability"
    # pylint: disable=too-few-public-methods

    def __init__(self):
        self.mem_size = 0    # byte size of the structure
        self.max_rmid = 0    # max RMID supported by socket
        self.l3_size = 0     # L3 cache size in bytes
        self.events = []     # a list of supported events


class PqosCapabilityL3Ca(object):
    "PQoS L3 cache allocation capability"
    # pylint: disable=too-few-public-methods

    def __init__(self):
        self.mem_size = 0           # byte size of the structure
        self.num_classes = 0        # number of classes of service
        self.num_ways = 0           # number of cache ways
        self.way_size = 0           # way size in bytes
        self.way_contention = 0     # ways contention bit mask
        self.cdp = False            # code data prioritization feature support
        self.cdp_on = False         # code data prioritization on or off


class PqosCapabilityL2Ca(object):
    "PQoS L2 cache allocation capability"
    # pylint: disable=too-few-public-methods

    def __init__(self):
        self.mem_size = 0           # byte size of the structure
        self.num_classes = 0        # number of classes of service
        self.num_ways = 0           # number of cache ways
        self.way_size = 0           # way size in bytes
        self.way_contention = 0     # ways contention bit mask
        self.cdp = False            # code data prioritization feature support
        self.cdp_on = False         # code data prioritization on or off


class PqosCapabilityMba(object):
    "PQoS memory bandwidth allocation capability"
    # pylint: disable=too-few-public-methods

    def __init__(self):
        self.mem_size = 0              # byte size of the structure
        self.num_classes = 0           # number of classes of service
        self.throttle_max = 0          # the max MBA can be throttled
        self.throttle_step = 0         # MBA granularity
        self.is_linear = False         # the type of MBA linear/nonlinear
        self.ctrl = False              # MBA controller support
        self.ctrl_on = False           # MBA controller on or off


def _get_tristate_bool(c_val):
    "Converts tri-state integer value -1, 0 or 1 to None, False or True."
    tristate_map = {
        1: True,
        0: False,
        -1: None
    }

    return tristate_map.get(c_val)


def _get_cap_mon(p_capability):
    """
    Converts low-level pqos_cap_mon structure to
    high-level PqosCapabilityMonitoring object.
    """
    c_capability = p_capability.contents
    capability = PqosCapabilityMonitoring()
    capability.mem_size = c_capability.mem_size
    capability.max_rmid = c_capability.max_rmid
    capability.l3_size = c_capability.l3_size

    events_ptr = ctypes.cast(c_capability.events, ctypes.POINTER(CPqosMonitor))

    for i in range(c_capability.num_events):
        capability.events.append(events_ptr[i])

    return capability


def _get_cap_l3ca(p_capability):
    """
    Converts low-level pqos_cap_l3ca structure to
    high-level PqosCapabilityL3Ca object.
    """
    c_capability = p_capability.contents
    capability = PqosCapabilityL3Ca()
    capability.mem_size = c_capability.mem_size
    capability.num_classes = c_capability.num_classes
    capability.num_ways = c_capability.num_ways
    capability.way_size = c_capability.way_size
    capability.way_contention = c_capability.way_contention
    capability.cdp = _get_tristate_bool(c_capability.cdp)
    capability.cdp_on = _get_tristate_bool(c_capability.cdp_on)
    return capability


def _get_cap_l2ca(p_capability):
    """
    Converts low-level pqos_cap_l2ca structure to
    high-level PqosCapabilityL2Ca object.
    """
    c_capability = p_capability.contents
    capability = PqosCapabilityL2Ca()
    capability.mem_size = c_capability.mem_size
    capability.num_classes = c_capability.num_classes
    capability.num_ways = c_capability.num_ways
    capability.way_size = c_capability.way_size
    capability.way_contention = c_capability.way_contention
    capability.cdp = _get_tristate_bool(c_capability.cdp)
    capability.cdp_on = _get_tristate_bool(c_capability.cdp_on)
    return capability


def _get_cap_mba(p_capability):
    """
    Converts low-level pqos_cap_mba structure to
    high-level PqosCapabilityMba object.
    """
    c_capability = p_capability.contents
    capability = PqosCapabilityMba()
    capability.mem_size = c_capability.mem_size
    capability.num_classes = c_capability.num_classes
    capability.throttle_max = c_capability.throttle_max
    capability.throttle_step = c_capability.throttle_step
    capability.is_linear = c_capability.is_linear == 1
    capability.ctrl = _get_tristate_bool(c_capability.ctrl)
    capability.ctrl_on = _get_tristate_bool(c_capability.ctrl_on)
    return capability


def pqos_get_type_enum(type_str):
    "Converts capability type string to pqos_capability's enum."
    type_enum_map = {
        'mon': CPqosCapability.PQOS_CAP_TYPE_MON,
        'l3ca': CPqosCapability.PQOS_CAP_TYPE_L3CA,
        'l2ca': CPqosCapability.PQOS_CAP_TYPE_L2CA,
        'mba': CPqosCapability.PQOS_CAP_TYPE_MBA
    }

    return type_enum_map[type_str.lower()]


def _get_capability(cap_item, type_str):
    "Converts capability type string to pqos_capability's enum."
    type_cls_map = {
        'mon': (_get_cap_mon, lambda c: c.u.mon),
        'l3ca': (_get_cap_l3ca, lambda c: c.u.l3ca),
        'l2ca': (_get_cap_l2ca, lambda c: c.u.l2ca),
        'mba': (_get_cap_mba, lambda c: c.u.mba)
    }
    capability_func, cap_item_func = type_cls_map[type_str.lower()]
    return capability_func(cap_item_func(cap_item))


class PqosCap(object):
    "PQoS capabilities"

    def __init__(self):
        "Initializes capabilities, calls pqos_cap_get."
        self.pqos = Pqos()
        self.p_cap = ctypes.POINTER(CPqosCap)()
        ret = self.pqos.lib.pqos_cap_get(ctypes.byref(self.p_cap), None)
        pqos_handle_error(u'pqos_cap_get', ret)

    def get_type(self, type_str):
        """Retrieves a type of capability from a cap structure.

        Parameters:
            type_str: a string indicating a type of capability, available
                      options: mon, l3ca, l2ca and mba
        """
        type_enum = pqos_get_type_enum(type_str)
        p_cap_item = ctypes.POINTER(CPqosCapability)()
        ret = self.pqos.lib.pqos_cap_get_type(self.p_cap, type_enum,
                                              ctypes.byref(p_cap_item))
        pqos_handle_error(u'pqos_cap_get_type', ret)

        cap_item = p_cap_item.contents
        capability = _get_capability(cap_item, type_str)
        return capability

    def get_l3ca_cos_num(self):
        """
        Retrieves number of L3 allocation classes of service from
        a cap structure.
        """
        cos_num = ctypes.c_uint(0)
        ret = self.pqos.lib.pqos_l3ca_get_cos_num(self.p_cap,
                                                  ctypes.byref(cos_num))
        pqos_handle_error(u'pqos_l3ca_get_cos_num', ret)
        return cos_num.value

    def get_l2ca_cos_num(self):
        """
        Retrieves number of L2 allocation classes of service from
        a cap structure.
        """
        cos_num = ctypes.c_uint(0)
        ret = self.pqos.lib.pqos_l2ca_get_cos_num(self.p_cap,
                                                  ctypes.byref(cos_num))
        pqos_handle_error(u'pqos_l2ca_get_cos_num', ret)
        return cos_num.value

    def get_mba_cos_num(self):
        """
        Retrieves number of memory B/W allocation classes of service from
        a cap structure.
        """
        cos_num = ctypes.c_uint(0)
        ret = self.pqos.lib.pqos_mba_get_cos_num(self.p_cap,
                                                 ctypes.byref(cos_num))
        pqos_handle_error(u'pqos_mba_get_cos_num', ret)
        return cos_num.value

    def is_l3ca_cdp_enabled(self):
        "Retrieves L3 CDP status."
        supported = ctypes.c_int(0)
        enabled = ctypes.c_int(0)
        ret = self.pqos.lib.pqos_l3ca_cdp_enabled(self.p_cap,
                                                  ctypes.byref(supported),
                                                  ctypes.byref(enabled))
        pqos_handle_error(u'pqos_l3ca_cdp_enabled', ret)
        return (_get_tristate_bool(supported.value),
                _get_tristate_bool(enabled.value))

    def is_l2ca_cdp_enabled(self):
        "Retrieves L2 CDP status."
        supported = ctypes.c_int(0)
        enabled = ctypes.c_int(0)
        ret = self.pqos.lib.pqos_l2ca_cdp_enabled(self.p_cap,
                                                  ctypes.byref(supported),
                                                  ctypes.byref(enabled))
        pqos_handle_error(u'pqos_l2ca_cdp_enabled', ret)
        return (_get_tristate_bool(supported.value),
                _get_tristate_bool(enabled.value))

    def is_mba_ctrl_enabled(self):
        "Retrieves MBA CTRL status."
        supported = ctypes.c_int(0)
        enabled = ctypes.c_int(0)
        ret = self.pqos.lib.pqos_mba_ctrl_enabled(self.p_cap,
                                                  ctypes.byref(supported),
                                                  ctypes.byref(enabled))
        pqos_handle_error(u'pqos_mba_ctrl_enabled', ret)
        return (_get_tristate_bool(supported.value),
                _get_tristate_bool(enabled.value))
