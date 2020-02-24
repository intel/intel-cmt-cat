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
The module defines PqosCpuInfo which can be used to read CPU information
like number of cores, L2/L3 cache ID etc.
"""

from __future__ import absolute_import, division, print_function
import ctypes

from pqos.common import pqos_handle_error, free_memory
from pqos.error import PqosError
from pqos.pqos import Pqos


class CPqosCoreInfo(ctypes.Structure):
    "pqos_coreinfo structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u"lcore", ctypes.c_uint),    # Logical core id
        (u"socket", ctypes.c_uint),   # Socket id in the system
        (u"l3_id", ctypes.c_uint),    # L3/LLC cluster id
        (u"l2_id", ctypes.c_uint),    # L2 cluster id
        (u"l3cat_id", ctypes.c_uint), # L3 CAT classes id
        (u"mba_id", ctypes.c_uint),   # MBA id
    ]


class CPqosCacheInfo(ctypes.Structure):
    "pqos_cacheinfo structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        (u"detected", ctypes.c_int),         # Indicates cache detected & valid
        (u"num_ways", ctypes.c_uint),        # Number of cache ways
        (u"num_sets", ctypes.c_uint),        # Number of sets
        (u"num_partitions", ctypes.c_uint),  # Number of partitions
        (u"line_size", ctypes.c_uint),       # Cache line size in bytes
        (u"total_size", ctypes.c_uint),      # Total cache size in bytes
        (u"way_size", ctypes.c_uint),        # Cache way size in bytes
    ]


class CPqosCpuInfo(ctypes.Structure):
    "pqos_cpuinfo structure"
    # pylint: disable=too-few-public-methods

    PQOS_VENDOR_UNKNOWN = 0
    PQOS_VENDOR_INTEL = 1
    PQOS_VENDOR_AMD = 2

    _fields_ = [
        (u"mem_size", ctypes.c_uint),   # Byte size of the structure
        (u"l2", CPqosCacheInfo),        # L2 cache information
        (u"l3", CPqosCacheInfo),        # L3 cache information
        (u"vendor", ctypes.c_int),      # CPU vendor
        (u"num_cores", ctypes.c_uint),  # Number of cores in the system
        (u"cores", CPqosCoreInfo * 0)   # Core information
    ]


class PqosCoreInfo(object):
    "Core information"
    # pylint: disable=too-few-public-methods, too-many-arguments

    def __init__(self, core, socket, l3_id, l2_id, l3cat_id, mba_id):
        self.core = core
        self.socket = socket
        self.l3_id = l3_id
        self.l2_id = l2_id
        self.l3cat_id = l3cat_id
        self.mba_id = mba_id


def _get_array_items(count, p_items):
    """
    Converts ctypes array (given a pointer to the first element
    and number of elements) to a list.

    Parameters:
        count: number of elements
        p_items: a pointer to the first element of the array

    Returns:
        a list of elements
    """

    if not count:
        return []

    items = [p_items[i] for i in range(count)]
    return items


class PqosCpuInfo(object):
    "PQoS CPU information"

    def __init__(self):
        self.pqos = Pqos()

        self.p_cpu = ctypes.POINTER(CPqosCpuInfo)()
        ret = self.pqos.lib.pqos_cap_get(None, ctypes.byref(self.p_cpu))
        pqos_handle_error(u'pqos_cap_get', ret)

    def _call_func_array(self, func, arg=None, use_arg=False):
        """
        Calls a function from PQoS library and returns the result as a list of
        integers.

        Parameters:
            func: a function from PQoS library
            arg: a function argument
            use_arg: if True then func will be invoked with arg as an argument

        Returns:
            a list of integers
        """

        count = ctypes.c_uint(0)
        count_ref = ctypes.byref(count)
        func.restype = ctypes.POINTER(ctypes.c_uint)

        if use_arg:
            p_items = func(self.p_cpu, arg, count_ref)
        else:
            p_items = func(self.p_cpu, count_ref)

        if not p_items:
            return []

        items = [p_items[i] for i in range(count.value)] if count.value else []
        free_memory(p_items)
        return items

    def _call_func_ref(self, func, arg):
        """
        Calls a function from PQoS library, handles errors and returns
        an integer set by the called function.

        Parameters:
            func: a function from PQoS library
            arg: a function argument

        Returns:
            an integer set by the called function
        """

        result = ctypes.c_uint(0)
        result_ref = ctypes.byref(result)
        ret = func(self.p_cpu, arg, result_ref)
        pqos_handle_error(func.__name__, ret)
        return result.value

    def get_vendor(self):
        """
        Retrieves CPU vendor information from CPU info structure

        Returns:
            CPU vendor
        """
        func = self.pqos.lib.pqos_get_vendor
        func.restype = ctypes.c_int

        vendor = func(self.p_cpu)

        if vendor == CPqosCpuInfo.PQOS_VENDOR_INTEL:
            return u"INTEL"
        if vendor == CPqosCpuInfo.PQOS_VENDOR_AMD:
            return u"AMD"

        return u"UNKNOWN"

    def get_sockets(self):
        """
        Retrieves socket IDs from CPU info structure.

        Returns:
            a list of socket IDs
        """

        return self._call_func_array(self.pqos.lib.pqos_cpu_get_sockets)

    def get_l2ids(self):
        """
        Retrieves L2 IDs from CPU info structure.

        Returns:
            a list of L2 IDs
        """

        return self._call_func_array(self.pqos.lib.pqos_cpu_get_l2ids)

    def get_cores_l3id(self, l3_id):
        """
        Creates a list of cores belonging to a given L3 cluster.

        Parameters:
            l3_id: L3 cluster ID

        Returns:
            a list of cores
        """

        return self._call_func_array(self.pqos.lib.pqos_cpu_get_cores_l3id,
                                     l3_id, use_arg=True)

    def get_cores(self, socket):
        """
        Retrieves core IDs from CPU info structure for a socket.

        Parameters:
            socket: socket ID

        Returns:
            a list of cores
        """

        return self._call_func_array(self.pqos.lib.pqos_cpu_get_cores,
                                     socket, use_arg=True)

    def get_core_info(self, core):
        """
        Retrieves core information from CPU info structure for a core.

        Parameters:
            core: core ID

        Returns:
            core information
        """

        restype = ctypes.POINTER(CPqosCoreInfo)
        self.pqos.lib.pqos_cpu_get_core_info.restype = restype
        p_coreinfo = self.pqos.lib.pqos_cpu_get_core_info(self.p_cpu, core)

        if not p_coreinfo:
            raise PqosError(u'Core information not found')

        coreinfo_struct = p_coreinfo.contents
        coreinfo = PqosCoreInfo(core=coreinfo_struct.lcore,
                                socket=coreinfo_struct.socket,
                                l3_id=coreinfo_struct.l3_id,
                                l2_id=coreinfo_struct.l2_id,
                                l3cat_id=coreinfo_struct.l3cat_id,
                                mba_id=coreinfo_struct.mba_id)
        return coreinfo

    def get_one_core(self, socket):
        """
        Retrieves one core ID from CPU info structure for a socket.

        Parameters:
            socket: socket ID

        Returns:
            core ID
        """

        return self._call_func_ref(self.pqos.lib.pqos_cpu_get_one_core, socket)

    def get_one_by_l2id(self, l2_id):
        """
        Retrieves one core ID from CPU info structure for L2 ID.

        Parameters:
            l2_id: L2 ID

        Returns:
            core ID
        """

        return self._call_func_ref(self.pqos.lib.pqos_cpu_get_one_by_l2id,
                                   l2_id)

    def check_core(self, core):
        """
        Verifies if a specified core is a valid logical core ID.

        Parameters:
            core: core ID

        Returns:
            True/False a given core number is valid/invalid
        """

        ret = self.pqos.lib.pqos_cpu_check_core(self.p_cpu, core)
        return ret == 0

    def get_socketid(self, core):
        """
        Retrieves socket ID for given logical core ID.

        Parameters:
            core: core ID

        Returns:
            socket ID
        """

        return self._call_func_ref(self.pqos.lib.pqos_cpu_get_socketid, core)

    def get_clusterid(self, core):
        """
        Retrieves monitoring cluster ID for given logical core ID.

        Parameters:
            core: core ID

        Returns:
            cluster ID
        """

        return self._call_func_ref(self.pqos.lib.pqos_cpu_get_clusterid, core)
