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
Cache Flusher process
"""

import multiprocessing
import signal
import os.path
import Queue

from time import time

import common
import log
import pid_ops


class FlusherProcess(multiprocessing.Process):
    """
    Cache Flusher process
    """
    flusher_queue = common.MANAGER.Queue()


    def __init__(self, stop_event, flush_done_event):
        multiprocessing.Process.__init__(self)
        self.stop_event = stop_event
        self.flush_done_event = flush_done_event


    @staticmethod
    def flush(pids):
        """
        Add pids to flush queue

        Parameters:
            pids: PIDs to be enqueued for cache flush
        """
        FlusherProcess.flusher_queue.put(pids)


    def run(self):
        """
        Polls queue for PIDs and flush them
        """
        # Ignore CTRL+C signal..., it is handled by AppQoS
        signal.signal(signal.SIGINT, signal.SIG_IGN)

        # No flush in progress, propagate via event
        self.flush_done_event.set()

        log.info(
            'PID: {} {} process started...'.format(self.pid, self.__class__.__name__))

        while not common.is_event_set(self.stop_event):
            try:
                # poll queue for list of PIDs to be flushed
                pid_list_ref = FlusherProcess.flusher_queue.get_nowait()
                # pid_list_ref is a reference so make a copy first
                pid_list = list(pid_list_ref)

                # Flush in progress, propagate via event
                self.flush_done_event.clear()

                time_stamp = time()
                log.info("Flushing Cache for PIDs: {} ...".format(pid_list))
                # TBD: filter PIDs' list to eliminate threads from the same process
                # flushing cache for one of them is enough,
                # pid_ops.get_pid_tids() could be used to get all TIDs for PID
                FlusherProcess.flush_cache_per_pid(pid_list)
                delta_t = time() - time_stamp
                log.debug("Flushing cache for PIDs: {} took {} secs".format(pid_list, delta_t))

                # Flush done, propagate via event
                self.flush_done_event.set()

            except Queue.Empty:
                pass
            except (IOError, EOFError):
                break

        log.info('PID: {} {}, Stopping...'.format(self.pid, self.__class__.__name__))

        return


    @staticmethod
    def flush_cache_per_pid(pid, children=False):
        """
        Flush cache per PID, including PIDs' children if requested

        Parameters:
            pid: PID to be flush
            children(bool): flag to include children
        """
        if isinstance(pid, list):
            pids = pid
        else:
            pids = [pid]

        try:
            fd = open(common.PID_FLUSH_PROCFS_FILENAME, 'w')
        except IOError:
            # Update stats
            common.STATS_STORE.general_stats_inc_num_err()
            log.error('Failed to open {}!'.format(common.PID_FLUSH_PROCFS_FILENAME))
            return

        for _pid in pids:
            log.debug('Flushing {}/{}...'.format(_pid, pid))
            try:
                fd.write("{}\n".format(_pid))
                # Update stats
                common.STATS_STORE.general_stats_inc_num_flushes()
            except IOError:
                # Update stats
                common.STATS_STORE.general_stats_inc_num_err()
                log.error('Failed to write to {}!'.format(common.PID_FLUSH_PROCFS_FILENAME))

            fd.flush()

            if children:
                for child_id in pid_ops.get_pid_children(_pid):
                    log.info('Including child{}'.format(child_id))
                    try:
                        fd.write("{}\n".format(child_id))
                        # Update stats
                        common.STATS_STORE.general_stats_inc_num_flushes()
                    except IOError:
                        # Update stats
                        common.STATS_STORE.general_stats_inc_num_err()
                        log.error('Failed to write to {}!'.\
                            format(common.PID_FLUSH_PROCFS_FILENAME))

                    fd.flush()
        fd.close()


    @staticmethod
    def pid_flush_module_loaded():
        """
        Verifies that intel_pid_cache_flush.ko is loaded

        Returns:
            result (bool)
        """
        return os.path.isfile(common.PID_FLUSH_PROCFS_FILENAME)
