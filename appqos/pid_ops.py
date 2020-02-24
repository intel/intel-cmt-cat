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
PIDs ops module
Provides PIDs related helper functions
"""

import common
import log

# list of valid PIDs' status, (R)unning, (S)leeping, (D)isk wait
PID_VALID_STATUS = set('RSD')


def get_pid_status(pid):
    """
    Gets PID status valid/running/sleeping or not

    Parameters:
        pid: PID to be tested

    Returns:
        (bool) test result
    """
    if pid != 0:
        pid_str = pid
    else:
        pid_str = 'self'

    try:
        state, pid_name = _read_pid_state(pid_str)
        return state, state in PID_VALID_STATUS, pid_name
    except PermissionError as ex:
        log.error("procfs file, {}".format(str(ex)))
    except OSError as ex:
        # handle silently, most likely PID has terminated
        pass

    return 'E', False, ''


def _read_pid_state(pid_str):
    """
    Reads PID's /proc/PID/stat file for state and process name

    Parameters:
        pid_str: PID to be tested (number or 'self')

    Returns:
        state, pid_name
    """
    # read procfs /proc/PID/stat file to get PID status
    with open("/proc/{}/stat".format(pid_str), opener=common.check_link) as stat_file:
        # split by '(' and ')' needed
        # as processes/threads could have spaces in the name
        line_split = stat_file.readline().strip().split(')')
        pid_name = line_split[0].split('(')[1]
        state = line_split[1].strip().split(' ')[0]
        return state, pid_name


def is_pid_valid(pid):
    """
    Check if PID is valid (get_pid_status wrapper)

    Parameters:
        pid: PID to be tested

    Returns:
        (bool) test result
    """
    return get_pid_status(pid)[1]
