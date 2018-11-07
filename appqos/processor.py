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
Stats processing module
"""

import multiprocessing
import Queue
import signal

from collections import OrderedDict
from time import time, sleep

import cache_ops
import common
import log
import stats


class ProcessingProcess(multiprocessing.Process):
    """
    Stats processing process
    """


    def __init__(self, stop_event, trigger_event, stats_queue, flush_done_event,
                 acqn_max_time):
        multiprocessing.Process.__init__(self)
        self.stop_event = stop_event
        self.trigger_event = trigger_event
        self.stats_queue = stats_queue
        self.flush_done_event = flush_done_event
        self.acqn_max_time = acqn_max_time
        self.collectors_num = 0


    def run(self):
        """
        Polls queue for stats and process them
        """
        # Ignore CTRL+C signal..., it is handled by AppQoS
        signal.signal(signal.SIGINT, signal.SIG_IGN)

        log.info('PID: {} {}, process started'.\
                 format(self.pid, self.__class__.__name__))

        recv_stats_apps = {}
        recv_stats_pool = {}

        while not common.is_event_set(self.stop_event):
            # wait for trigger
            self.trigger_event.wait(common.SAMPLING_INTERVAL * 2)

            time_stamp = time()
            items_num = 0

            recv_stats_apps.clear()
            recv_stats_pool.clear()

            try:
                pids = common.CONFIG_STORE.get_attr_list('pids', common.TYPE_DYNAMIC, None)
            except IOError:
                break

            self.collectors_num = len(pids)
            while items_num < self.collectors_num and not common.is_event_set(self.stop_event):
                # poll queue for data to be processed, blocking pool with timeout
                # acquisition of all stats cannot take longer then self.acqn_max_time
                get_timeout = self.acqn_max_time - (time() - time_stamp)
                if get_timeout < 0:
                    get_timeout = 0

                try:
                    item_ref = self.stats_queue.get(True, get_timeout)
                except Queue.Empty:
                    log.debug('PID: {} {}, Q get timeout...'.\
                              format(self.pid, self.__class__.__name__))
                    break

                # item_ref is a reference so make a (sorted) copy first
                item = OrderedDict(sorted(item_ref.items()))

                # If stat is from prev. period, ignore it...
                # should not happen as queue is flushed before triggering
                # collectors and processor processes
                if item["time_stamp"] + self.acqn_max_time < time_stamp:
                    log.warn("Stat from prev. period, ignored...")
                    continue

                # count number of stats read
                items_num += 1

                # group received stats
                stats.group_stats_per_pool(item, recv_stats_pool)
                stats.group_stats_per_app(item, recv_stats_apps)

            # appqos termination in progress...
            if common.is_event_set(self.stop_event):
                break

            if not items_num:
                log.debug('PID: {} {}, No stats received...'.\
                          format(self.pid, self.__class__.__name__))
                continue

            # we have all the data ready, now lets process it
            # per pool
            aggr_stats_pool = stats.aggregate(recv_stats_pool)
            for pool_id in aggr_stats_pool:
                stats.calculate(aggr_stats_pool[pool_id])
            cat = common.STATS_STORE.get_pool_cat()
            for pool_id in cat:
                if 'cws' in cat[pool_id]:
                    if pool_id not in aggr_stats_pool:
                        aggr_stats_pool[pool_id] = {}
                    aggr_stats_pool[pool_id]['cat_cws'] = cat[pool_id]['cws']
                    aggr_stats_pool[pool_id]['cat_cw_size'] = cache_ops.get_cw_size()

            pool_llc_stats = stats.calculate_llc_stats(aggr_stats_pool)

            # per app
            aggr_stats_apps = stats.aggregate(recv_stats_apps)
            for app_id in aggr_stats_apps:
                stats.calculate(aggr_stats_apps[app_id])

            app_llc_stats = stats.calculate_llc_stats(aggr_stats_apps)

            # Update stats_store, to propagate stats to other modules
            common.STATS_STORE.set_pool_stats(pool_llc_stats)
            common.STATS_STORE.set_app_stats(app_llc_stats)

            # log unexpected behaviour
            time_spent = time() - time_stamp
            if items_num != self.collectors_num or time_spent > self.acqn_max_time:
                log.warn("Item_nums {}/{} recieved in {}".\
                         format(items_num, self.collectors_num, time_spent))

            # if we are done, sleep for 50% of time left
            # workaround for trigger_event (python's event) issue
            # causes processing loop to be executed twice in pooling period
            spare_time = common.SAMPLING_INTERVAL - (time() - time_stamp)
            if spare_time > common.SAMPLING_INTERVAL * 0.5:
                sleep(spare_time * 0.5)

        log.info('PID: {} {}, Stopping...'.\
                 format(self.pid, self.__class__.__name__))

        return
