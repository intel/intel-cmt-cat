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
Power module
"""

from copy import deepcopy

import log

import common
import power_common
import sstbf

VALID_EPP = ["performance", "balance_performance", "balance_power", "power"]
DEFAULT_EPP = "balance_power"

PREV_PROFILES = {}

class AdmissionControlError(Exception):
    """
    AdmissionControlError exception
    """


def _set_freqs_epp(cores, min_freq=None, max_freq=None, epp=None):
    """
     Set minimum, maximum frequency and EPP value

    Parameters:
        cores: list of core ids
        min_freq: min core frequency
        max_freq: max core frequency
        epp: EPP value
    """
    if not cores:
        log.error("POWER: No cores specified!")
        return -1

    if not min_freq and not max_freq and not epp:
        log.error("POWER: No power profile parameters specified!")
        return -1

    log.debug(("POWER: Setting min/max/epp on cores {} to {}/{}/{}")\
                .format(cores, min_freq, max_freq, epp))

    pwr_cores = power_common.get_pwr_cores()
    if not pwr_cores:
        log.error("POWER: Unable to get cores from PWR lib!")
        return -1

    for pwr_core in pwr_cores:
        if pwr_core.core_id in cores:
            if min_freq:
                pwr_core.min_freq = min_freq
            if max_freq:
                pwr_core.max_freq = max_freq
            if epp:
                pwr_core.epp = epp

            pwr_core.commit()

    return 0


def reset(cores):
    """
    Reset power setting

    Parameters:
        cores: list of core ids
    """
    log.debug(("POWER: Reset on cores {}. Setting to \"default\".").format(cores))

    if not cores:
        return -1

    pwr_cores = power_common.get_pwr_cores()
    if not pwr_cores:
        return -1

    for core in pwr_cores:
        if core.core_id in cores:
            core.commit("default")

    return 0


def is_sstcp_enabled():
    """
    Returns EPP enabled status
    """

    pwr_sys = power_common.get_pwr_sys()
    if not pwr_sys:
        return False

    return pwr_sys.sst_bf_enabled and pwr_sys.epp_enabled


def _is_min_freq_valid(freq):
    """
    Validate min. freq value
    Returns None on error.

    Parameters
        freq: frequency value
    """

    lowest_freq = power_common.get_pwr_lowest_freq()
    if not lowest_freq:
        return None

    if freq < lowest_freq:
        return False

    return True


def _is_max_freq_valid(freq):
    """
    Validate max. freq value
    Returns None on error.

    Parameters
        freq: frequency value
    """

    highest_freq = power_common.get_pwr_highest_freq()
    if not highest_freq:
        return None

    if freq > highest_freq:
        return False

    return True


def _is_epp_valid(epp):
    """
    Validate EPP configuration

    Parameters
        epp: epp value
    """

    return epp in VALID_EPP


def validate_power_profiles(data, admission_control):
    """
    Validate Power Profiles configuration

    Parameters
        data: configuration (dict)
    """

    if not 'power_profiles' in data:
        return

    # verify profiles
    profile_ids = []

    for profile in data['power_profiles']:
        # id
        if profile['id'] in profile_ids:
            raise ValueError("Power Profile {}, multiple profiles with same id."\
                .format(profile['id']))

        profile_ids.append(profile['id'])

        # profile's freqs validation
        if not _is_max_freq_valid(profile['max_freq']):
            raise ValueError("Power Profile {}, Invalid max. freq {}."\
                .format(profile['id'], profile['max_freq']))

        if not _is_min_freq_valid(profile['min_freq']):
            raise ValueError("Power Profile {}, Invalid min. freq {}."\
                .format(profile['id'], profile['min_freq']))

        if profile['min_freq'] > profile['max_freq']:
            raise ValueError("Power Profile {}, Invalid freqs!"\
                " min. freq is higher than max. freq.".format(profile['id']))

        if not _is_epp_valid(profile['epp']):
            raise ValueError("Power Profile {}, Invalid EPP value {}."\
                .format(profile['id'], profile['epp']))

    if admission_control and _do_admission_control_check():
        _admission_control_check(data)


def _do_admission_control_check():
    """
    Admission control check is done if
    - SST-BF is not configured
    """
    return not sstbf.is_sstbf_configured()


def _get_power_profiles_verify():
    return common.CONFIG_STORE.get_global_attr('power_profiles_verify', True)


def _admission_control_check(data):
    """
    Validate Power Profiles configuration,
    admission control check done on PWR lib side

    Parameters
        data: configuration (dict)
    """

    profiles = {}
    for profile in data['power_profiles']:
        profiles[profile['id']] = deepcopy(profile)
        profiles[profile['id']]['cores'] = []

    for pool in data['pools']:
        if 'power_profile' not in pool:
            continue
        profiles[pool['power_profile']]['cores'].extend(pool['cores'])

    cores = power_common.get_pwr_cores()

    for core in cores:
        for profile in profiles.values():
            if core.core_id in profile['cores']:
                core.min_freq = profile['min_freq']
                core.max_freq = profile['max_freq']
                core.epp = profile['epp']
                break

    pwr_sys = power_common.get_pwr_sys()
    result = pwr_sys.request_config()
    pwr_sys.refresh_all()

    if not result:
        raise AdmissionControlError\
            ("Power Profiles configuration would cause CPU to be oversubscribed.")


def configure_power():
    """
    Configures Power Profiles
    """

    global PREV_PROFILES

    # we do not configure power profiles is SST-BF is enabled
    if sstbf.is_sstbf_configured():
        PREV_PROFILES = {}
        return 0

    curr_profiles = _get_curr_profiles()
    if curr_profiles is None:
        return -1

    if not curr_profiles:
        return 0

    # has power profiles configuration changed since last run?
    if PREV_PROFILES == curr_profiles:
        return 0

    # in case there was a change, find out which profiles has been changed
    for profile in curr_profiles.values():
        if not profile['cores']:
            continue

        prev_profile = PREV_PROFILES.get(profile['id'], [])

        # apply configuration only for changed profiles
        if prev_profile != profile:
            log.debug("POWER: Power profile {} configuration...".format(profile['id']))
            min_freq = profile.get('min_freq', None)
            max_freq = profile.get('max_freq', None)
            epp = profile.get('epp', None)
            _set_freqs_epp(profile['cores'], min_freq, max_freq, epp)

    # reset power config for cores with no power profile assigned
    cores_to_reset = _get_cores_to_reset()
    if cores_to_reset:
        reset(cores_to_reset)

    # save current profiles configuration
    PREV_PROFILES = curr_profiles

    return 0


def _get_curr_profiles():
    """
    Creates a list of Power Profiles with cores assigned
    """

    # get list of pool ids
    pool_ids = common.CONFIG_STORE.get_pool_attr('id', None)
    if not pool_ids:
        log.error("POWER: No Pools configured...")
        return None

    # get list of power profiles assigned to pools
    power_ids = common.CONFIG_STORE.get_pool_attr('power_profile', None)

    # if there are no power profiles assigned to pools,
    # there is nothing to configure...
    if not power_ids:
        return []

    # remove dups
    power_ids = list(set(power_ids))

    # Get current power profiles configuration
    curr_profiles = {}
    for power_id in power_ids:
        profile = common.CONFIG_STORE.get_power_profile(power_id)
        if profile:
            curr_profiles[power_id] = deepcopy(profile)
            curr_profiles[power_id]['cores'] = []
        else:
            log.error("POWER: Profile {} does not exist!".format(power_id))
            return None

    # get through all pools to extend curr power profiles config with theirs cores
    for pool_id in pool_ids:
        cores = common.CONFIG_STORE.get_pool_attr('cores', pool_id)
        if not cores:
            continue

        power_id = common.CONFIG_STORE.get_pool_attr('power_profile', pool_id)
        if power_id is None:
            continue

        curr_profiles[power_id]['cores'].extend(cores)

    return curr_profiles


def _get_cores_to_reset():
    """
    Creates a list of cores to reset,
    ones with no power profiles assigned
    """

    cores_to_reset = []

    # get list of pool ids
    pool_ids = common.CONFIG_STORE.get_pool_attr('id', None)
    if not pool_ids:
        return None

    for pool_id in pool_ids:
        if common.CONFIG_STORE.get_pool_attr('power_profile', pool_id) is None:
            cores = common.CONFIG_STORE.get_pool_attr('cores', pool_id)
            if cores:
                cores_to_reset.extend(cores)

    return cores_to_reset
