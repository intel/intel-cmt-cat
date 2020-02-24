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
SST-BF module
"""

import common
import power_common
import log

PWR_CFG_SST_BF = "sst_bf"
PWR_CFG_BASE = "base"

HP_CORES = None
STD_CORES = None

def init_sstbf():
    """
    Configure/Initialize SST-BF on start-up
    """
    result = 0
    data = common.CONFIG_STORE.get_config()

    if 'sstbf' in data:
        sstbf_cfg = data['sstbf']
        if 'configured' in sstbf_cfg:
            log.info("Configuring SST-BF")
            result = configure_sstbf(sstbf_cfg['configured'])

    _populate_cores()

    return result


def is_sstbf_enabled():
    """
    Returns SST-BF enabled status
    """
    sys = power_common.get_pwr_sys()
    if not sys:
        return None

    return sys.sst_bf_enabled


def is_sstbf_configured():
    """
    Returns SST-BF configured status
    """
    sys = power_common.get_pwr_sys()
    if not sys:
        return None

    sys.refresh_stats()
    return sys.sst_bf_configured


def configure_sstbf(configure):
    """
    Configures or unconfigures SST-BF
    """

    sys = power_common.get_pwr_sys()
    if not sys:
        return -1

    if configure:
        new_core_cfg = PWR_CFG_SST_BF
    else:
        new_core_cfg = PWR_CFG_BASE

    sys.commit(new_core_cfg)

    log.sys("Intel SST-BF {}configured.".format("" if configure else "un"))

    return 0


def get_hp_cores():
    """
    Returns HP cores
    """
    if not HP_CORES:
        _populate_cores()

    return HP_CORES


def get_std_cores():
    """
    Returns standard cores
    """
    if not STD_CORES:
        _populate_cores()

    return STD_CORES


def _populate_cores():
    """
    Gets HP and STD cores from PWR lib
    """
    cores = power_common.get_pwr_cores()
    if not cores:
        return

    global HP_CORES
    global STD_CORES

    HP_CORES = []
    STD_CORES = []

    for core in cores:
        if core.high_priority:
            HP_CORES.append(core.core_id)
        else:
            STD_CORES.append(core.core_id)
