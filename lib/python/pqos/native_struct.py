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
"Structures mapped from C to Python using Python's ctypes module."

import ctypes
from pqos.common import convert_from_cos, convert_to_cos

# Configuration

class CPqosConfig(ctypes.Structure):
    "pqos_config structure"
    # pylint: disable=too-few-public-methods

    PQOS_INTER_MSR = 0
    PQOS_INTER_OS = 1
    PQOS_INTER_OS_RESCTRL_MON = 2
    PQOS_INTER_AUTO = 3

    LOG_VER_SILENT = -1
    LOG_VER_DEFAULT = 0
    LOG_VER_VERBOSE = 1
    LOG_VER_SUPER_VERBOSE = 2

    LOG_CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_size_t,
                                    ctypes.c_char_p)

    _fields_ = [
        ("fd_log", ctypes.c_int),
        ("callback_log", LOG_CALLBACK),
        ("context_log", ctypes.c_void_p),
        ("verbose", ctypes.c_int),
        ("interface", ctypes.c_int),
        ("reserved", ctypes.c_int),
    ]

# Capabilities

class CPqosCapabilityL3(ctypes.Structure):
    "pqos_cap_l3ca structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("mem_size", ctypes.c_uint),
        ("num_classes", ctypes.c_uint),
        ("num_ways", ctypes.c_uint),
        ("way_size", ctypes.c_uint),
        ("way_contention", ctypes.c_uint64),
        ("cdp", ctypes.c_int),
        ("cdp_on", ctypes.c_int),
        ('non_contiguous_cbm', ctypes.c_uint),
        ("iordt", ctypes.c_int),
        ("iordt_on", ctypes.c_int),
    ]


class CPqosCapabilityL2(ctypes.Structure):
    "pqos_cap_l2ca structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ('mem_size', ctypes.c_uint),
        ('num_classes', ctypes.c_uint),
        ('num_ways', ctypes.c_uint),
        ('way_size', ctypes.c_uint),
        ('way_contention', ctypes.c_uint64),
        ('cdp', ctypes.c_int),
        ('cdp_on', ctypes.c_int),
        ('non_contiguous_cbm', ctypes.c_uint),
    ]


class CPqosCapabilityMBA(ctypes.Structure):
    "pqos_cap_mba structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("mem_size", ctypes.c_uint),
        ("num_classes", ctypes.c_uint),
        ("throttle_max", ctypes.c_uint),
        ("throttle_step", ctypes.c_uint),
        ("is_linear", ctypes.c_int),
        ("ctrl", ctypes.c_int),
        ("ctrl_on", ctypes.c_int),
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
    PQOS_PERF_EVENT_LLC_REF = 0x10000

    _fields_ = [
        ("type", ctypes.c_int),
        ("max_rmid", ctypes.c_uint),
        ("scale_factor", ctypes.c_uint32),
        ("counter_length", ctypes.c_uint),
        ("iordt", ctypes.c_int),
    ]


class CPqosCapabilityMonitoring(ctypes.Structure):
    "pqos_cap_mon structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("mem_size", ctypes.c_uint),
        ("max_rmid", ctypes.c_uint),
        ("l3_size", ctypes.c_uint),
        ("num_events", ctypes.c_uint),
        ("iordt", ctypes.c_int),
        ("iordt_on", ctypes.c_int),
        ('snc_num', ctypes.c_uint),
        ('snc_mode', ctypes.c_uint),
        ('events', CPqosMonitor * 0),
    ]


class CPqosCapabilityUnion(ctypes.Union):
    "Union from pqos_capability structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("mon", ctypes.POINTER(CPqosCapabilityMonitoring)),
        ("l3ca", ctypes.POINTER(CPqosCapabilityL3)),
        ("l2ca", ctypes.POINTER(CPqosCapabilityL2)),
        ("mba", ctypes.POINTER(CPqosCapabilityMBA)),
        ("generic_ptr", ctypes.c_void_p),
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
        ("type", ctypes.c_int),
        ("u", CPqosCapabilityUnion)
    ]


class CPqosCap(ctypes.Structure):
    "pqos_cap structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("mem_size", ctypes.c_uint),
        ("version", ctypes.c_uint),
        ("num_cap", ctypes.c_uint),
        ("capabilities", CPqosCapability * 0)
    ]

# CPU info

class CPqosCoreInfo(ctypes.Structure):
    "pqos_coreinfo structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ('lcore', ctypes.c_uint),    # Logical core id
        ('socket', ctypes.c_uint),   # Socket id in the system
        ('l3_id', ctypes.c_uint),    # L3/LLC cluster id
        ('l2_id', ctypes.c_uint),    # L2 cluster id
        ('l3cat_id', ctypes.c_uint), # L3 CAT classes id
        ('mba_id', ctypes.c_uint),   # MBA id
        ('numa', ctypes.c_uint)      # Numa node
    ]


class CPqosCacheInfo(ctypes.Structure):
    "pqos_cacheinfo structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("detected", ctypes.c_int),         # Indicates cache detected & valid
        ("num_ways", ctypes.c_uint),        # Number of cache ways
        ("num_sets", ctypes.c_uint),        # Number of sets
        ("num_partitions", ctypes.c_uint),  # Number of partitions
        ("line_size", ctypes.c_uint),       # Cache line size in bytes
        ("total_size", ctypes.c_uint),      # Total cache size in bytes
        ("way_size", ctypes.c_uint),        # Cache way size in bytes
    ]


class CPqosCpuInfo(ctypes.Structure):
    "pqos_cpuinfo structure"
    # pylint: disable=too-few-public-methods

    PQOS_VENDOR_UNKNOWN = 0
    PQOS_VENDOR_INTEL = 1
    PQOS_VENDOR_AMD = 2

    _fields_ = [
        ("mem_size", ctypes.c_uint),   # Byte size of the structure
        ("l2", CPqosCacheInfo),        # L2 cache information
        ("l3", CPqosCacheInfo),        # L3 cache information
        ("vendor", ctypes.c_int),      # CPU vendor
        ("num_cores", ctypes.c_uint),  # Number of cores in the system
        ("cores", CPqosCoreInfo * 0)   # Core information
    ]

# I/O RDT

PQOS_DEV_MAX_CHANNELS = 8
PqosChannelT = ctypes.c_uint64

class CPqosIordtConfig:
    "pqos_iordt_config enumeration"
    native_type = ctypes.c_int
    PQOS_REQUIRE_IORDT_ANY = 0
    PQOS_REQUIRE_IORDT_OFF = 1
    PQOS_REQUIRE_IORDT_ON = 2

    values = [
        ('any', PQOS_REQUIRE_IORDT_ANY),
        ('off', PQOS_REQUIRE_IORDT_OFF),
        ('on', PQOS_REQUIRE_IORDT_ON)
    ]

    @classmethod
    def get_value(cls, label):
        """
        Converts text label ('any', 'off' or 'on') to its native
        representation.

        Parameters:
            label: a text label

        Returns:
            native representation of configuration or None
        """

        for cfg_label, cfg_value in cls.values:
            if cfg_label == label:
                return cfg_value

        return None

    @classmethod
    def get_label(cls, value):
        """
        Converts native representation to a text label ('any', 'off' or 'on').

        Parameters:
            native representation of configuration

        Returns:
            label: a text label or None
        """

        for cfg_label, cfg_value in cls.values:
            if cfg_value == value:
                return cfg_label

        return None

class CPqosChannel(ctypes.Structure):
    "pqos_channel structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ('channel_id', PqosChannelT),
        ('rmid_tagging', ctypes.c_int),
        ('clos_tagging', ctypes.c_int),
        ('min_rmid', ctypes.c_uint),
        ('max_rmid', ctypes.c_uint),
        ('min_clos', ctypes.c_uint),
        ('max_clos', ctypes.c_uint)
    ]

class CPqosDevType:
    "pqos_dev_type enumeration"
    # pylint: disable=too-few-public-methods

    native_type = ctypes.c_int
    PQOS_DEVICE_TYPE_PCI = 1
    PQOS_DEVICE_TYPE_PCI_BRIDGE = 2

class CPqosDev(ctypes.Structure):
    "pqos_dev structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ('type', CPqosDevType.native_type),
        ('segment', ctypes.c_uint16),
        ('bdf', ctypes.c_uint16),
        ('channel', PqosChannelT * PQOS_DEV_MAX_CHANNELS)
    ]

class CPqosDevinfo(ctypes.Structure):
    "pqos_devinfo structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ('num_channels', ctypes.c_uint),
        ('channels', ctypes.POINTER(CPqosChannel)),
        ('num_devs', ctypes.c_uint),
        ('devs', ctypes.POINTER(CPqosDev))
    ]

# System configuration

class CPqosSysconfig(ctypes.Structure):
    "pqos_sysconfig structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("cap", ctypes.POINTER(CPqosCap)),
        ("cpu", ctypes.POINTER(CPqosCpuInfo)),
        ("dev", ctypes.POINTER(CPqosDevinfo)),
    ]

# Allocation

class CPqosCdpConfig:
    "pqos_cdp_config enumeration"

    native_type = ctypes.c_int
    PQOS_REQUIRE_CDP_ANY = 0
    PQOS_REQUIRE_CDP_OFF = 1
    PQOS_REQUIRE_CDP_ON = 2

    values = [
        ('any', PQOS_REQUIRE_CDP_ANY),
        ('off', PQOS_REQUIRE_CDP_OFF),
        ('on', PQOS_REQUIRE_CDP_ON)
    ]

    @classmethod
    def get_value(cls, label):
        """
        Converts text label ('any', 'off' or 'on') to its native
        representation.

        Parameters:
            label: a text label

        Returns:
            native representation of configuration or None
        """

        for cfg_label, cfg_value in cls.values:
            if cfg_label == label:
                return cfg_value

        return None

    @classmethod
    def get_label(cls, value):
        """
        Converts native representation to a text label ('any', 'off' or 'on').

        Parameters:
            native representation of configuration

        Returns:
            label: a text label or None
        """

        for cfg_label, cfg_value in cls.values:
            if cfg_value == value:
                return cfg_label

        return None

class CPqosMbaConfig:
    "pqos_mba_config enumeration"

    native_type = ctypes.c_int
    PQOS_MBA_ANY = 0
    PQOS_MBA_DEFAULT = 1
    PQOS_MBA_CTRL = 2

    values = [
        ('any', PQOS_MBA_ANY),
        ('default', PQOS_MBA_DEFAULT),
        ('ctrl', PQOS_MBA_CTRL)
    ]

    @classmethod
    def get_value(cls, label):
        """
        Converts text label ('any', 'default' or 'ctrl') to its native
        representation.

        Parameters:
            label: a text label

        Returns:
            native representation of configuration or None
        """

        for cfg_label, cfg_value in cls.values:
            if cfg_label == label:
                return cfg_value

        return None

    @classmethod
    def get_label(cls, value):
        """
        Converts native representation to a text label ('any', 'default' or 'ctrl').

        Parameters:
            native representation of configuration

        Returns:
            label: a text label or None
        """

        for cfg_label, cfg_value in cls.values:
            if cfg_value == value:
                return cfg_label

        return None

class CPqosAllocConfig(ctypes.Structure):
    "pqos_alloc_config structure"
    # pylint: disable=attribute-defined-outside-init

    _fields_ = [
        ('l3_cdp', CPqosCdpConfig.native_type),
        ('l3_iordt', CPqosIordtConfig.native_type),
        ('l2_cdp', CPqosCdpConfig.native_type),
        ('mba', CPqosMbaConfig.native_type),
    ]

    def set_l3_cdp(self, l3_cdp):
        """
        Sets L3 CDP configuration.

        Parameter:
            l3_cdp: L3 CDP configuration ('any', 'on' or 'off')
        """

        self.l3_cdp = CPqosCdpConfig.get_value(l3_cdp)

    def set_l3_iordt(self, l3_iordt):
        """
        Sets L3 I/O RDT configuration.

        Parameter:
            l3_iordt: L3 I/O RDT configuration ('any', 'on' or 'off')
        """

        self.l3_iordt = CPqosIordtConfig.get_value(l3_iordt)

    def set_l2_cdp(self, l2_cdp):
        """
        Sets L2 CDP configuration.

        Parameter:
            l2_cdp: L2 CDP configuration ('any', 'on' or 'off')
        """

        self.l2_cdp = CPqosCdpConfig.get_value(l2_cdp)

    def set_mba(self, mba):
        """
        Sets MBA configuration.

        Parameter:
            mba: MBA configuration ('any', 'ctrl' or 'default')
        """

        self.mba = CPqosMbaConfig.get_value(mba)

# Allocation - L2 CAT

class CPqosL2CaMaskCDP(ctypes.Structure):
    "CDP structure from union from pqos_l2ca structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("data_mask", ctypes.c_uint64),
        ("code_mask", ctypes.c_uint64),
    ]


class CPqosL2CaMask(ctypes.Union):
    "Union from pqos_l2ca structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("ways_mask", ctypes.c_uint64),
        ("s", CPqosL2CaMaskCDP)
    ]


class CPqosL2Ca(ctypes.Structure):
    "pqos_l2ca structure"

    _fields_ = [
        ("class_id", ctypes.c_uint),
        ("cdp", ctypes.c_int),
        ("u", CPqosL2CaMask),
    ]

    @classmethod
    def from_cos(cls, cos):
        "Creates CPqosL2Ca object from PqosCatL2.COS object."

        return convert_from_cos(cos, cls)

    def to_cos(self, cls):
        "Creates PqosCatL2.COS object from CPqosL2Ca object."

        return convert_to_cos(self, cls)

# Allocation - L3 CAT

class CPqosL3CaMaskCDP(ctypes.Structure):
    "CDP structure from union from pqos_l3ca structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("data_mask", ctypes.c_uint64),
        ("code_mask", ctypes.c_uint64),
    ]


class CPqosL3CaMask(ctypes.Union):
    "Union from pqos_l3ca structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ("ways_mask", ctypes.c_uint64),
        ("s", CPqosL3CaMaskCDP)
    ]


class CPqosL3Ca(ctypes.Structure):
    "pqos_l3ca structure"

    _fields_ = [
        ("class_id", ctypes.c_uint),
        ("cdp", ctypes.c_int),
        ("u", CPqosL3CaMask),
    ]

    @classmethod
    def from_cos(cls, cos):
        "Creates CPqosL3Ca object from PqosCatL3.COS object."

        return convert_from_cos(cos, cls)

    def to_cos(self, cls):
        "Creates PqosCatL3.COS object from CPqosL3Ca object."

        return convert_to_cos(self, cls)

# Allocation - MBA

class CPqosMba(ctypes.Structure):
    "pqos_mba structure"

    _fields_ = [
        ("class_id", ctypes.c_uint),
        ("mb_max", ctypes.c_uint),
        ("ctrl", ctypes.c_int)
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

# Monitoring

RmidT = ctypes.c_uint32


class CPqosEventValues(ctypes.Structure):
    "pqos_event_values structure"
    # pylint: disable=too-few-public-methods

    _fields_ = [
        ('llc', ctypes.c_uint64),
        ('mbm_local', ctypes.c_uint64),
        ('mbm_total', ctypes.c_uint64),
        ('mbm_remote', ctypes.c_uint64),
        ('mbm_local_delta', ctypes.c_uint64),
        ('mbm_total_delta', ctypes.c_uint64),
        ('mbm_remote_delta', ctypes.c_uint64),
        ('ipc_retired', ctypes.c_uint64),
        ('ipc_retired_delta', ctypes.c_uint64),
        ('ipc_unhalted', ctypes.c_uint64),
        ('ipc_unhalted_delta', ctypes.c_uint64),
        ('ipc', ctypes.c_double),
        ('llc_misses', ctypes.c_uint64),
        ('llc_misses_delta', ctypes.c_uint64),
        ('llc_references', ctypes.c_uint64),
        ('llc_references_delta', ctypes.c_uint64),
    ]

class CPqosSNCConfig:
    "pqos_iordt_config enumeration"
    native_type = ctypes.c_int
    PQOS_REQUIRE_SNC_ANY = 0
    PQOS_REQUIRE_SNC_LOCAL = 1
    PQOS_REQUIRE_SNC_TOTAL = 2

    values = [
        ('any', PQOS_REQUIRE_SNC_ANY),
        ('local', PQOS_REQUIRE_SNC_LOCAL),
        ('total', PQOS_REQUIRE_SNC_TOTAL)
    ]

    @classmethod
    def get_value(cls, label):
        """
        Converts text label ('any', 'local' or 'total') to its native
        representation.
        Parameters:
            label: a text label
        Returns:
            native representation of configuration or None
        """

        for cfg_label, cfg_value in cls.values:
            if cfg_label == label:
                return cfg_value

        return None

    @classmethod
    def get_label(cls, value):
        """
        Converts native representation to a text label ('any', 'local' or 'total').
        Parameters:
            native representation of configuration
        Returns:
            label: a text label or None
        """

        for cfg_label, cfg_value in cls.values:
            if cfg_value == value:
                return cfg_label

        return None

class CPqosMonConfig(ctypes.Structure):
    "pqos_mon_config structure"
    # pylint: disable=too-few-public-methods
    # pylint: disable=attribute-defined-outside-init

    _fields_ = [
        ('l3_iordt', CPqosIordtConfig.native_type),
        ('snc', CPqosSNCConfig.native_type)
    ]

    def set_l3_iordt(self, l3_iordt):
        """
        Sets L3 I/O RDT configuration.

        Parameter:
            l3_iordt: L3 I/O RDT configuration ('any', 'on' or 'off')
        """

        self.l3_iordt = CPqosIordtConfig.get_value(l3_iordt)

    def set_snc(self, snc):
        """
        Sets SNC configuration.
        Parameter:
            snc: SNC configuration ('any', 'local' or 'total')
        """

        self.snc = CPqosSNCConfig.get_value(snc)
