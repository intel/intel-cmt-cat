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
Linux kernel perf interface module
"""

import re

from fcntl import ioctl
from os import read
from os.path import exists
from struct import unpack, calcsize

from perfapi import perf_event_open as perf_event_open_c # pylint: disable=import-error,no-name-in-module

import log


class Defs(object):
    """
    Defines
    """
    perf_type_id = {
        'PERF_TYPE_HARDWARE': 0,
        'PERF_TYPE_SOFTWARE': 1,
        'PERF_TYPE_TRACEPOINT': 2,
        'PERF_TYPE_HW_CACHE': 3,
        'PERF_TYPE_RAW': 4,
        'PERF_TYPE_BREAKPOINT': 5
    }

    perf_hw_id = {
        'PERF_COUNT_HW_CPU_CYCLES': 0,
        'PERF_COUNT_HW_INSTRUCTIONS': 1,
        'PERF_COUNT_HW_CACHE_REFERENCES': 2,
        'PERF_COUNT_HW_CACHE_MISSES': 3,
        'PERF_COUNT_HW_BRANCH_INSTRUCTIONS': 4,
        'PERF_COUNT_HW_BRANCH_MISSES': 5,
        'PERF_COUNT_HW_BUS_CYCLES': 6,
        'PERF_COUNT_HW_STALLED_CYCLES_FRONTEND': 7,
        'PERF_COUNT_HW_STALLED_CYCLES_BACKEND': 8,
        'PERF_COUNT_HW_REF_CPU_CYCLES': 9
    }

    perf_hw_cache_id = {
        'PERF_COUNT_HW_CACHE_L1D': 0,
        'PERF_COUNT_HW_CACHE_L1I': 1,
        'PERF_COUNT_HW_CACHE_LL': 2,
        'PERF_COUNT_HW_CACHE_DTLB': 3,
        'PERF_COUNT_HW_CACHE_ITLB': 4,
        'PERF_COUNT_HW_CACHE_BPU': 5,
        'PERF_COUNT_HW_CACHE_NODE': 6
    }

    perf_sw_id = {
        'PERF_COUNT_SW_CPU_CLOCK': 0,
        'PERF_COUNT_SW_TASK_CLOCK': 1,
        'PERF_COUNT_SW_PAGE_FAULTS': 2,
        'PERF_COUNT_SW_CONTEXT_SWITCHES': 3,
        'PERF_COUNT_SW_CPU_MIGRATIONS': 4,
        'PERF_COUNT_SW_PAGE_FAULTS_MIN': 5,
        'PERF_COUNT_SW_PAGE_FAULTS_MAJ': 6,
        'PERF_COUNT_SW_ALIGNMENT_FAULTS': 7,
        'PERF_COUNT_SW_EMULATION_FAULTS': 8,
        'PERF_COUNT_SW_DUMMY': 9
    }

    perf_ioctls = {
        'PERF_IOC_FLAG_GROUP': 0x01,
        'PERF_EVENT_IOC_ENABLE': 0x2400,
        'PERF_EVENT_IOC_DISABLE': 0x2401,
        'PERF_EVENT_IOC_RESET': 0x2403
    }

    event_paths = {
        'llc_occupancy': '/sys/devices/intel_cqm/events/llc_occupancy',
        'mbm_total_bytes': '/sys/devices/intel_cqm/events/total_bytes',
        'mbm_local_bytes': '/sys/devices/intel_cqm/events/local_bytes'
    }

    event_type_path = '/sys/devices/intel_cqm/type'

    # should we go here for /sys/devices/cpu/events/{cpu-cycles,instructions} ?
    pmu_events = {
        'cpu-cycles': {
            'id': perf_hw_id['PERF_COUNT_HW_CPU_CYCLES'],
            'unit': 'cycles',
            'type': perf_type_id['PERF_TYPE_HARDWARE']},
        'instructions': {
            'id': perf_hw_id['PERF_COUNT_HW_INSTRUCTIONS'],
            'unit': 'instructions',
            'type': perf_type_id['PERF_TYPE_HARDWARE']},
        'task-clock': {
            'id': perf_sw_id['PERF_COUNT_SW_TASK_CLOCK'],
            'unit': 'ns?',
            'type': perf_type_id['PERF_TYPE_SOFTWARE']}
    }

    pmu_cache_events = {
        'cache-references': {
            'id': perf_hw_id['PERF_COUNT_HW_CACHE_REFERENCES'],
            'unit': 'hits',
            'type': perf_type_id['PERF_TYPE_HARDWARE']},
        'cache-misses': {
            'id': perf_hw_id['PERF_COUNT_HW_CACHE_MISSES'],
            'unit': 'misses',
            'type': perf_type_id['PERF_TYPE_HARDWARE']}
    }

    # CQM events are detected during the runtime
    cqm_events = {}


    @staticmethod
    def read_cqm_events():
        """
        Detects CQM events supported by kernel
        """
        Defs.cqm_events.clear()

        # parse sysfs files to get supported events data
        if not exists(Defs.event_type_path):
            log.warn("Intel CQM events not supported...")
            return -1

        with open(Defs.event_type_path) as evt_type_file:
            event_type = int(evt_type_file.read(), 10)

        for event in Defs.event_paths:
            if not exists(Defs.event_paths[event]):
                log.warn("{} event not supported...".format(event))
                continue

            Defs.cqm_events[event] = {}
            Defs.cqm_events[event]['type'] = event_type

            with open(Defs.event_paths[event]) as evt_file:
                event_id = int(re.findall(
                    r'0x[0-9A-F]+', evt_file.read(), re.I)[0], 16)
                Defs.cqm_events[event]['id'] = event_id

            with open(Defs.event_paths[event] + '.scale') as evt_scale_file:
                scale = float(evt_scale_file.read())
                Defs.cqm_events[event]['scale'] = scale

            with open(Defs.event_paths[event] + '.unit') as evt_unit_file:
                Defs.cqm_events[event]['unit'] = evt_unit_file.read()

        return 0


def perf_event_init():
    """
    Runs supported events detection and logs to console
    """
    # detect intel cqm events
    result = Defs.read_cqm_events()
    if result != 0:
        return result

    supported_events = get_supported_events()
    log.debug("Supported events:")
    for event_list in supported_events:
        log.debug(str(event_list.keys()))

    return 0


def get_supported_events():
    """
    Generates list of supported events

    Returns
        list of supported events
    """
    result = []
    # generate list of perf events/counters to be monitored
    if Defs.pmu_events:
        result.append(Defs.pmu_events)
    if Defs.pmu_cache_events:
        result.append(Defs.pmu_cache_events)
    if Defs.cqm_events:
        result.append(Defs.cqm_events)

    return result


def perf_event_open(evt_type, config, pid, cpu, group_fd, flags):
    """
    Wrapper for perf_event_open syscall

    Parameters:
        perf_event_open syscall params
    """
    return perf_event_open_c(evt_type, config, pid, cpu, group_fd, flags)


def get_counter(fd, reset=False):
    """
    Reads perf counter

    Returns:
        value: counter value
        time_enabled: time since enabled
        time_running: time counter was actually running
        id_event: monitored event ID
    """
    # format ('LLLL') depends on attrs.read_format value (see perfapi.c)
    read_size = calcsize('LLLL')
    value, time_enabled, time_running, id_event = unpack(
        'LLLL', read(fd, read_size))
    if reset:
        reset_counter(fd)

    return (value, time_enabled, time_running, id_event)


def reset_counter(fd, group=False):
    """
    Resets perf counter

    Parameters:
        fd: event's file descriptor
        group: flag to perform events' group reset
    """
    flag = 0
    if group:
        flag = Defs.perf_ioctls['PERF_IOC_FLAG_GROUP']

    ioctl(fd, Defs.perf_ioctls['PERF_EVENT_IOC_RESET'], flag)


def enable_counter(fd, enable=True, group=False):
    """
    Sets enable state for counter/event

    Parameters:
        fd: event's file descriptor
        enable: enable stat to be set
        group: flag to perform events' group operation
    """
    if enable:
        val = Defs.perf_ioctls['PERF_EVENT_IOC_ENABLE']
    else:
        val = Defs.perf_ioctls['PERF_EVENT_IOC_DISABLE']

    if group:
        ioctl(fd, val, Defs.perf_ioctls['PERF_IOC_FLAG_GROUP'])
    else:
        ioctl(fd, val)


def disable_counter(fd, group=False):
    """
    Disables counter/event

    Parameters:
        fd: event's file descriptor
        group: flag to perform events' group operation
    """
    enable_counter(fd, False, group)
