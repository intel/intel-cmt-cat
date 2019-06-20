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

import os.path
import shutil
import subprocess # nosec

import common
import log

SST_BF_SCRIPT = "sst_bf.py"

BF_PATH = "/sys/devices/system/cpu/cpu{}/cpufreq/base_frequency"
MAX_PATH = "/sys/devices/system/cpu/cpu{}/cpufreq/scaling_max_freq"
MIN_PATH = "/sys/devices/system/cpu/cpu{}/cpufreq/scaling_min_freq"

ALL_CORES = None
HP_CORES = None
STD_CORES = None


def configure_sstbf():
    """
    Configure SST-BF on start-up
    """
    result = 0
    data = common.CONFIG_STORE.get_config()

    if 'sstbf' in data:
        sstbf_cfg = data['sstbf']
        if 'enabled' in sstbf_cfg:
            log.info("Configuring SST-BF")
            result = enable_sstbf(sstbf_cfg['enabled'])

    return result

def is_sstbf_supported():
    """
    Returns SST-BF support status
    """
    if not os.path.isfile(BF_PATH.format(0)):
        log.warn("SST-BF not supported. {} missing.".format(BF_PATH.format(0)))
        return False

    if shutil.which(SST_BF_SCRIPT) is None:
        log.warn("SST-BF not supported. Executable {} not found.".format(SST_BF_SCRIPT))
        return False

    return True


def is_sstbf_enabled():
    """
    Returns SST-BF enabled status
    """
    if not is_sstbf_supported():
        return False

    all_cores = _get_all_cores()
    if not all_cores:
        return False

    for core in all_cores:
        base_freq, min_freq, max_freq = _get_core_freqs(core)
        if not base_freq == min_freq == max_freq:
            return False

    return True


def _get_core_freqs(core):
    """
    Reads core's frequencies, sst-bf base, min scaling, max scaling
    """
    freqs = {BF_PATH: None, MIN_PATH: None, MAX_PATH: None}

    for path in freqs:
        freqs[path] = _read_int_from_file(path.format(core))

    return freqs[BF_PATH], freqs[MIN_PATH], freqs[MAX_PATH]


def _read_int_from_file(fname):
    try:
        with open(fname) as file:
            return int(file.read())
    except Exception as ex:
        log.error("{} {}".format(str(fname), str(ex)))
        return None


def enable_sstbf(enable):
    """
    Enables or disables SST-BF
    """
    if enable:
        arg = '-s'
    else:
        arg = '-m'

    result = _run_sstbf(arg)
    if result is None:
        return -1

    log.sys("Intel SST-BF {}configured.".format("" if enable else "un"))

    return 0


def get_hp_cores():
    """
    Returns HP cores
    """
    global HP_CORES

    if HP_CORES is None:
        HP_CORES = _sstbf_get_hp_cores()

    return HP_CORES


def get_std_cores():
    """
    Returns standard cores
    """
    global STD_CORES

    if STD_CORES is None:
        all_cores = _get_all_cores()
        hp_cores = get_hp_cores()
        if hp_cores and all_cores:
            STD_CORES = sorted(set(all_cores).difference(hp_cores))

    return STD_CORES


def _get_all_cores():
    """
    Returns all cores
    """
    global ALL_CORES

    if ALL_CORES is None:
        num_cores = common.PQOS_API.get_num_cores()
        ALL_CORES = list(range(num_cores)) if num_cores else None

    return ALL_CORES


def _sstbf_get_hp_cores():
    """
    Runs sst_bf.py to get list of HP cores
    """
    result = _run_sstbf('-l')

    if result is None:
        return None

    cores_str = result.stdout.split('\n')[0]
    cores_list = cores_str.split(',')

    try:
        return [int(i) for i in cores_list]
    except Exception as ex:
        log.error("{} {}".format(str(result.stdout), str(ex)))
        return None


def _run_sstbf(arg):
    """
    Runs sst_bf.py with requested cmd line param
    """
    try:
        result = subprocess.run( # nosec
            [SST_BF_SCRIPT, arg],
            stdout=subprocess.PIPE,
            universal_newlines=True,
            check=True)
    except Exception as ex:
        log.error(str(ex))
        return None

    return result
