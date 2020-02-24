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
Common global constants and instances
"""

import errno
import multiprocessing
import os

import pqos_api # pylint: disable=cyclic-import
import config # pylint: disable=cyclic-import
import stats # pylint: disable=cyclic-import


CONFIG_FILENAME = "appqos.conf"
CAT_CAP = "cat"
MBA_CAP = "mba"
SSTBF_CAP = "sstbf"
POWER_CAP = "power"

RATE_LIMIT = 10 # rate limit of configuration changes in HZ

MANAGER = multiprocessing.Manager()
CONFIG_STORE = config.ConfigStore()
STATS_STORE = stats.StatsStore()
PQOS_API = pqos_api.PqosApi()


def check_link(path, flags):
    """
    A custom opener for "open" function.
    Rises PermissionError if path points to a link

    Parameters:
        path: path to file
        flags: flags for "os.open" function
    Returns:
        an open file descriptor
    """
    if os.path.islink(path):
        raise PermissionError(errno.EPERM, os.strerror(errno.EPERM) + ". Is a link.", path)
    return os.open(path, flags)
