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
Module collecting stats per PID via Linux kernel perf interface
"""

import multiprocessing
import signal

from time import sleep, time
from os import close

import common
import log
import perf


class CollectingProcessPerf(multiprocessing.Process):
    """
    Process collecting stats per PID via Linux kernel perf interface
    """


    def __init__(self, stop_event, trigger_event, stats_queue, mon_pid, events):
        multiprocessing.Process.__init__(self)
        self.stop_event = stop_event
        self.trigger_event = trigger_event
        self.stats_queue = stats_queue
        self.mon_pid = mon_pid
        self.events = events
        self.fds = {}


    def configure_perf(self):
        """
        Configures perf events/counters via per_event_open syscall.
        All configured events are disabled.

        Raises IOError in case of syscall fail
        """
        for event_list in self.events:
            group_fd = -1

            # configure perf to monitor events per PID, all events are disable
            # and will be enabled as a group (via group_fd) later on
            for event in event_list:
                fd = perf.perf_event_open(event_list[event]['type'], event_list[event]['id'],
                                          self.mon_pid, -1, group_fd, 0)
                self.fds[event] = fd

                if fd == -1:
                    raise IOError(
                        'perf_event_open(type:{}, id:{}/{}, mon_pid:{}, group_fd:{}) fd:{} failed!'
                        .format(
                            event_list[event]['type'],
                            event_list[event]['id'],
                            event,
                            self.mon_pid,
                            group_fd,
                            fd))

                if group_fd == -1:
                    group_fd = fd


    def get_mon_pid(self):
        """
        Get PID monitored by process

        Returns:
            Monitored PID
        """
        return self.mon_pid


    def run(self):
        """
        Collect perf stats for single PID
        Configures perf, reads stats and pushes them to the queue
        """
        # Ignore CTRL+C signal..., it is handled by AppQoS
        signal.signal(signal.SIGINT, signal.SIG_IGN)

        # configure linux kernel perf...
        try:
            self.configure_perf()
        except IOError as ioe:
            common.STATS_STORE.general_stats_inc_num_err()
            log.error('PID: {} {}, Terminating, Failed to configure perf - {}'.
                      format(self.pid, self.__class__.__name__, ioe.message))
            return

        result = {}

        log.info('PID: {} {}, Starting counters for PID: {}'.
                 format(self.pid, self.__class__.__name__, self.mon_pid))

        # enable perf events counters via group_fd
        for event_list in self.events:
            if self.fds[event_list.keys()[0]] != -1:
                perf.enable_counter(self.fds[event_list.keys()[0]], True, True)
            else:
                common.STATS_STORE.general_stats_inc_num_err()
                log_msg = 'PID:{} {}, Terminating, \
                    Failed to enable group fd for PID:{}, event:{} !'.\
                    format(self.pid, self.__class__.__name__, self.mon_pid, event_list.keys()[0])
                log.error(log_msg)
                self.disable_perf_events()
                return

        log.info('Monitoring for PID {} started.'.format(self.mon_pid))

        while not common.is_event_set(self.stop_event):
            # wait for the trigger
            self.trigger_event.wait(common.SAMPLING_INTERVAL * 2)

            # get a time stamp
            time_stamp = time()

            result.clear()
            # add a time stamp, PID and set type
            result['time_stamp'] = time_stamp
            result['pid'] = self.mon_pid

            # read perf events from file descriptors
            for event_list in self.events:
                for event in event_list:
                    if self.fds[event] != -1:
                        result[event] = {}
                        result[event]['raw'] =\
                            perf.get_counter(self.fds[event])[0]

                # reset perf events counters as a group (via group_fd)
                perf.reset_counter(self.fds[event_list.keys()[0]], True)

            # push stats to queue for processing
            self.stats_queue.put(result)

            if common.CONFIG_STORE.is_config_changed():
                pids = common.CONFIG_STORE.get_attr_list('pids', common.TYPE_DYNAMIC, None)
                if self.mon_pid not in pids:
                    break

            # if we are done, sleep for 50% of time left
            # workaround for trigger_event (python's event) issue
            # causes collecting loop to be executed twice in a pooling period
            spare_time = common.SAMPLING_INTERVAL - (time() - time_stamp)
            if spare_time > common.SAMPLING_INTERVAL * 0.5:
                sleep(spare_time * 0.5)

        log.info('PID: {} {}, Monitored PID: {} Stopping...'.
                 format(self.pid, self.__class__.__name__, self.mon_pid))

        # disable perf events counters via group_fd
        self.disable_perf_events()

        log.info('Monitoring for PID {} stopped.'.format(self.mon_pid))

        return


    def disable_perf_events(self):
        """
        Disable perf events counters via group file descriptor
        """
        for event_list in self.events:
            if self.fds[event_list.keys()[0]] != -1:
                perf.disable_counter(self.fds[event_list.keys()[0]], True)
            for event in event_list:
                close(self.fds[event])
