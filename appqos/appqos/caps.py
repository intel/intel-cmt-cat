################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2022 Intel Corporation. All rights reserved.
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
System capabilities module
"""

from appqos import common
from appqos import log
from appqos import sstbf
from appqos import power
from appqos.pqos_api import PQOS_API


# System capabilities are detected during the runtime
SYSTEM_CAPS = {}


def caps_detect():
    """
    Detects supported capabilities
    """
    # no need to keep it in shared dict as it does not changed during runtime.
    global SYSTEM_CAPS

    SYSTEM_CAPS = {}

    for iface in ["msr", "os"]:
        if PQOS_API.init(iface) == 0:
            log.info(f"Interface {iface.upper()}, " \
                     f"MBA BW: {'un' if not PQOS_API.is_mba_bw_supported() else ''}supported.")

            SYSTEM_CAPS[iface] = detect_supported_caps()
            PQOS_API.fini()

    log.info(f"Supported RDT interfaces: {list(SYSTEM_CAPS.keys())}")


def caps_iface():
    """
    Returns list of supported RDT interfaces

    Returns:
        list of supported interfaces
    """
    return list(SYSTEM_CAPS.keys())


def caps_get(iface):
    """
    Get system capabilities
    """
    return SYSTEM_CAPS[iface]


def caps_init(iface):
    """
    Runs supported capabilities detection and logs to console
    """

    log.info(f"Supported capabilities: {caps_get(iface)}")

    features = [
        cat_l2_supported(iface),
        cat_l3_supported(iface),
        mba_supported(iface),
        sstbf_enabled(),
        sstcp_enabled()
    ]

    if any(features) and PQOS_API.is_multicore():
        return 0

    return -1


def cat_l3_supported(iface):
    """
    Returns L3 CAT support status
    """
    return common.CAT_L3_CAP in caps_get(iface)


def cdp_l3_supported(iface):
    """
    Returns L3 CDP support status
    """
    return common.CDP_L3_CAP in caps_get(iface)


def cat_l2_supported(iface):
    """
    Returns L2 CAT support status
    """
    return common.CAT_L2_CAP in caps_get(iface)


def cdp_l2_supported(iface):
    """
    Returns L2 CDP support status
    """
    return common.CDP_L2_CAP in caps_get(iface)


def mba_supported(iface):
    """
    Returns MBA support status
    """
    return common.MBA_CAP in caps_get(iface)


def mba_bw_supported():
    """
    Returns MBA BW support status
    """
    return PQOS_API.is_mba_bw_supported()


def mba_bw_enabled():
    """
    Returns MBA BW enabled status
    """
    return PQOS_API.is_mba_bw_enabled()


def sstbf_enabled():
    """
    Returns SST-BF support status
    """
    return common.SSTBF_CAP in caps_get("msr")


def sstcp_enabled():
    """
    Returns SST-CP support status
    """
    return common.POWER_CAP in caps_get("msr")


def detect_supported_caps():
    """
    Generates list of supported caps

    Returns
        list of supported caps
    """
    result = []
    # generate list of supported capabilities

    # Intel RDT L3 CAT
    if PQOS_API.is_l3_cat_supported():
        result.append(common.CAT_L3_CAP)
        if PQOS_API.is_l3_cdp_supported():
            result.append(common.CDP_L3_CAP)

    # Intel RDT L2 CAT
    if PQOS_API.is_l2_cat_supported():
        result.append(common.CAT_L2_CAP)
        if PQOS_API.is_l2_cdp_supported():
            result.append(common.CDP_L2_CAP)

    # Intel RDT MBA
    if PQOS_API.is_mba_supported():
        result.append(common.MBA_CAP)

    if sstbf.is_sstbf_enabled():
        result.append(common.SSTBF_CAP)

    if power.is_sstcp_enabled():
        result.append(common.POWER_CAP)

    return result


def mba_info():
    """
    Returns MBA information:
    * a number of supported classes of service
    * MBA status (enabled/disabled)
    * MBA CTRL status (enabled/disabled)

    Returns:
        MBA information
    """

    info = {
        'clos_num': PQOS_API.get_mba_num_cos(),
        'enabled': not mba_bw_enabled(),
        'ctrl_enabled': mba_bw_enabled()
    }
    return info


def mba_ctrl_info():
    """
    Returns MBA CTRL information:
    * MBA CTRL support
    * MBA CTRL status (enabled/disabled)

    Returns:
        MBA CTRL information
    """

    info = {
        'supported': mba_bw_supported(),
        'enabled': mba_bw_enabled()
    }
    return info


def l3ca_info():
    """
    Returns L3 cache allocation information:
    * L3 cache size
    * L3 cache way size
    * a number of L3 cache ways
    * a number of supported classes of service
    * L3 CDP support
    * L3 CDP status (enabled/disabled)

    Returns:
        L3 cache allocation information
    """

    rdt_api = PQOS_API
    info = {
        'cache_size': rdt_api.get_l3_cache_size(),
        'cache_way_size': rdt_api.get_l3_cache_way_size(),
        'cache_ways_num': rdt_api.get_l3_num_cache_ways(),
        'clos_num': rdt_api.get_l3ca_num_cos(),
        'cdp_supported': rdt_api.is_l3_cdp_supported(),
        'cdp_enabled': rdt_api.is_l3_cdp_enabled()
    }
    return info


def l2ca_info():
    """
    Returns L2 cache allocation information:
    * L2 cache size
    * L2 cache way size
    * a number of L2 cache ways
    * a number of supported classes of service
    * L2 CDP support
    * L2 CDP status (enabled/disabled)

    Returns:
        L2 cache allocation information
    """

    rdt_api = PQOS_API
    info = {
        'cache_size': rdt_api.get_l2_cache_size(),
        'cache_way_size': rdt_api.get_l2_cache_way_size(),
        'cache_ways_num': rdt_api.get_l2_num_cache_ways(),
        'clos_num': rdt_api.get_l2ca_num_cos(),
        'cdp_supported': rdt_api.is_l2_cdp_supported(),
        'cdp_enabled': rdt_api.is_l2_cdp_enabled()
    }
    return info
