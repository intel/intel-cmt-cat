################################################################################
# BSD LICENSE
#
# Copyright(c) 2019 Intel Corporation. All rights reserved.
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
import log

import pwr # pylint: disable=import-error

PWR_CFG_SST_BF_BASE = "sst_bf_base"
PWR_CFG_BASE = "base"

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

    return result

def is_sstbf_enabled():
    """
    Returns SST-BF enabled status
    """

    try:
        cpus = pwr.get_cpus()
    except IOError:
        return False

    if not cpus:
        return False

    # check is it enabled on all sockets
    enabled = True
    for cpu in cpus:
        cpu.read_capabilities()  # refresh
        enabled = enabled & cpu.sst_bf_enabled

    return enabled


def is_sstbf_configured():
    """
    Returns SST-BF configured status
    """

    try:
        cpus = pwr.get_cpus()
    except IOError:
        return False

    if not cpus:
        return False

    # check is it configured on all sockets
    configured = True
    for cpu in cpus:
        for core in cpu.core_list:
            core.refresh_stats()
        cpu.refresh_stats()
        configured = configured & cpu.sst_bf_configured

    return configured


def configure_sstbf(configure):
    """
    Configures or unconfigures SST-BF
    """

    try:
        cores = pwr.get_cores()
    except IOError:
        return -1

    if not cores:
        return -1

    if configure:
        new_core_cfg = PWR_CFG_SST_BF_BASE
    else:
        new_core_cfg = PWR_CFG_BASE

    for core in cores:
        core.commit(new_core_cfg)

    log.sys("Intel SST-BF {}configured.".format("" if configure else "un"))

    return 0


def get_hp_cores():
    """
    Returns HP cores
    """
    try:
        cores = pwr.get_cores()
    except IOError:
        return None

    if not cores:
        return None

    hp_cores = []

    for core in cores:
        if core.high_priority:
            hp_cores.append(core.core_id)

    return hp_cores

def get_std_cores():
    """
    Returns standard cores
    """
    try:
        cores = pwr.get_cores()
    except IOError:
        return None

    if not cores:
        return None

    std_cores = []

    for core in cores:
        if not core.high_priority:
            std_cores.append(core.core_id)

    return std_cores
