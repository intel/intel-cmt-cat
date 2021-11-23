################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2021 Intel Corporation. All rights reserved.
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
import psutil
import log

# list of valid PIDs' status, Running, Sleeping, Disk wait
PID_VALID_STATUS = {psutil.STATUS_RUNNING, psutil.STATUS_SLEEPING,
                    psutil.STATUS_DISK_SLEEP}

def get_pid_status(pid):
    """
    Gets PID status valid/running/sleeping or not

    Parameters:
        pid: PID to be tested

    Returns:
        (bool) test result
    """
    if pid == 0:
        pid = os.getpid()

    try:
        process = psutil.Process(pid)
        status = process.status()
        pid_name = process.name()
        return status, status in PID_VALID_STATUS, pid_name

    except psutil.ZombieProcess:
        return psutil.STATUS_ZOMBIE, False, ''
    except psutil.NoSuchProcess:
        # handle silently, most likely PID has terminated
        pass
    except psutil.Error as ex:
        log.error("psutil, {}".format(str(ex)))

    return 'Error', False, ''

def is_pid_valid(pid):
    """
    Check if PID is valid (get_pid_status wrapper)

    Parameters:
        pid: PID to be tested

    Returns:
        (bool) test result
    """
    return get_pid_status(pid)[1]


def set_affinity(pids, cores):
    """
    Sets PIDs' core affinity

    Parameters:
    pids: PIDs to set core affinity for
    cores: cores to set to
    """

    # set core affinity for each PID,
    # even if operation fails for one PID, continue with other PIDs
    for pid in pids:
        try:
            psutil.Process(pid).cpu_affinity(cores)
        except psutil.Error:
            log.error("Failed to set {} PID affinity".format(pid))
