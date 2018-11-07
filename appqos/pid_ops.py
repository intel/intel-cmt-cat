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
PIDs ops module
Provides PIDs related helper functions
"""

import os

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

    # read procfs /proc/PID/stat file to get PID status
    try:
        with open("/proc/{}/stat".format(pid_str)) as stat_file:
            # split by '(' and ')' needed
            # as processes/threads could have spaces in the name
            line_split = stat_file.readline().strip().split(')')
            pid_name = line_split[0].split('(')[1]
            state = line_split[1].strip().split(' ')[0]
            return state, state in PID_VALID_STATUS, pid_name
    except EnvironmentError:
        pass

    return 'E', False, ''


def is_pid_valid(pid):
    """
    Check if PID is valid (get_pid_status wrapper)

    Parameters:
        pid: PID to be tested

    Returns:
        (bool) test result
    """
    return get_pid_status(pid)[1]


def get_pid_processor(pid):
    """
    Get processor/core that PID was scheduled on last time

    Parameters:
        pid: PID to be tested

    Returns:
        processor/core number, -1 on error
    """
    if pid != 0:
        pid_str = pid
    else:
        pid_str = 'self'

    # read procfs /proc/PID/stat file to get info about processor
    # that PID was scheduled on last time
    try:
        with open("/proc/{}/stat".format(pid_str)) as stat_file:
            proc_stat = stat_file.readline().strip().split(' ')
            return int(proc_stat[39])
    except EnvironmentError:
        return -1


def get_pid_children(pid):
    """
    Get PID's children

    Parameters:
        pid: parent PID

    Returns:
        array of PIDs, empty array on error
    """
    if int(pid) < 0:
        return []

    # read procfs /proc/PID/task/PID/children file to get info about PIDs children
    try:
        with open("/proc/{}/task/{}/children".format(pid, pid)) as children_file:
            return children_file.readline().strip().split(' ')
    except EnvironmentError:
        return []


def get_pid_tids(pid):
    """
    Get PID's threads

    Parameters:
        pid: PID

    Returns:
        array of TIDs, empty array on error
    """
    if int(pid) < 0:
        return []
    # list procfs /proc/PID/task/ directories to get list of PID's threads
    try:
        return map(int, os.listdir("/proc/{}/task/".format(pid)))
    except EnvironmentError:
        return []
