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
Power common functions module
"""

import pwr

def get_pwr_sys():
    """
    Returns list of CPU objects or None on error
    """
    try:
        pwr_sys = pwr.get_system()
    except (IOError, ValueError):
        return None

    return pwr_sys


def get_pwr_cpus():
    """
    Returns list of CPU objects or None on error
    """
    try:
        cpus = pwr.get_cpus()
    except (IOError, ValueError):
        return None

    return cpus


def get_pwr_cores():
    """
    Returns list of CORE objects or None on error
    """

    try:
        cores = pwr.get_cores()
    except (IOError, ValueError):
        return None

    return cores


def get_pwr_lowest_freq():
    """
    Returns lowest supported freq or None on error
    """

    cpus = get_pwr_cpus()
    if not cpus:
        return None

    return cpus[0].lowest_freq


def get_pwr_base_freq():
    """
    Returns base freq or None on error
    """

    cpus = get_pwr_cpus()
    if not cpus:
        return None

    return cpus[0].base_freq


def get_pwr_highest_freq():
    """
    Returns highest supported freq or None on error
    """

    cpus = get_pwr_cpus()
    if not cpus:
        return None

    return cpus[0].highest_freq
