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
Timer module
Triggers an event to synchronise all processes
"""

import multiprocessing
import signal

from time import sleep, time

import common
import log


class TimerProcess(multiprocessing.Process):
    """
    Timer process
    """


    def __init__(self, stop_event, trigger_event, stats_queue, interval):
        multiprocessing.Process.__init__(self)
        self.stop_event = stop_event
        self.trigger_event = trigger_event
        self.stats_queue = stats_queue
        self.interval = interval


    def run(self):
        """
        Triggers event every configured interval
        """
        # Ignore CTRL+C signal..., it is handled by AppQoS
        signal.signal(signal.SIGINT, signal.SIG_IGN)

        log.info('PID: {} {} process started...'.\
                 format(self.pid, self.__class__.__name__))

        # wait one pooling interval to give other processes time to start
        sleep(self.interval)

        while not common.is_event_set(self.stop_event):
            time_stamp = time()

            # Flush stats queue, to remove old, unprocessed stats, if any
            TimerProcess.flush_queue(self.stats_queue)

            # Trigger trigger_event...
            self.trigger_event.set()
            self.trigger_event.clear()

            # sleep for the rest of the time
            # maybe we should use timer instead ?
            sleep(self.interval - (time() - time_stamp))

        log.info('PID: {} {}, Stopping...'.\
                 format(self.pid, self.__class__.__name__))

        return


    @staticmethod
    def flush_queue(queue):
        """
        Flushes multiprocessing queue

        Parameters:
            queue: queue to be flushed
        """
        if not queue.empty():
            log.debug("Queue not empty! Flushing {} elements!".\
                      format(queue.qsize()))
            # no other way to flush multiprocessing queue...
            while not queue.empty():
                queue.get_nowait()
