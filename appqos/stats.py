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
Stats module
Stats processing helper functions and storage for stats
"""

import common
import perf


class StatsStore(object):
    """
    Storage for stats
    """


    class General(object):
        """
        Helper class
        """
        #pylint: disable=too-few-public-methods
        NUM_FLUSHES = 'num_flushes'
        NUM_APPS_MOVES = 'num_apps_moves'
        NUM_ERR = 'num_err'


    def __init__(self):
        self.nspc = common.MANAGER.Namespace()
        self.nspc.app_stats = {}
        self.nspc.pool_stats = {}
        self.nspc.pool_cat = {}

        self.general_stats = common.MANAGER.dict()
        for cntr in [self.General.NUM_FLUSHES, self.General.NUM_APPS_MOVES, self.General.NUM_ERR]:
            self.general_stats[cntr] = 0


    def set_app_stats(self, data):
        """
        Setter for Apps' stat

        Paramaters:
            data: value to set handled stat to
        """
        self.nspc.app_stats = data


    def get_app_stats(self):
        """
        Getter for Apps' stat

        Returns:
            Stat value
        """
        return self.nspc.app_stats


    def set_pool_stats(self, data):
        """
        Setter for Pools' stat

        Paramaters:
            data: value to set handled stat to
        """
        self.nspc.pool_stats = data


    def get_pool_stats(self):
        """
        Getter for Pools' stat

        Returns:
            Stat value
        """
        return self.nspc.pool_stats


    def set_pool_cat(self, data):
        """
        Setter for Pools' CAT stat

        Paramaters:
            data: value to set handled stat to
        """
        self.nspc.pool_cat = data


    def get_pool_cat(self):
        """
        Getter for Pools' CAT stats

        Returns:
            Stat value
        """
        return self.nspc.pool_cat


    def general_stats_inc(self, gen_stats_id):
        """
        Increases general stat value by 1

        Parameters:
            gen_stat_id: stat's id
        """
        self.general_stats[gen_stats_id] += 1


    def general_stats_get(self, get_stats_id=None):
        """
        Getter for general stats, all or single one

        Parameters:
            get_stat_id: stat's ID

        Returns:
            Single general stat, all general stats, 0 on error
        """
        if get_stats_id is None:
            return self.general_stats

        if get_stats_id not in self.general_stats:
            return 0

        return self.general_stats[get_stats_id]


    def general_stats_inc_num_flushes(self):
        """
        Increases num flushes stat value by 1
        """
        self.general_stats_inc(StatsStore.General.NUM_FLUSHES)


    def general_stats_inc_apps_moves(self):
        """
        Increases apps moves stat value by 1
        """
        self.general_stats_inc(StatsStore.General.NUM_APPS_MOVES)


    def general_stats_inc_num_err(self):
        """
        Increases num errors stat value by 1
        """
        self.general_stats_inc(StatsStore.General.NUM_ERR)


def calculate(item):
    """
    Process single item/stat

    Parameters:
        item: stats to be processed
    """
    # if needed, scale values
    _scale_values(item)


def _scale_values(item):
    """
    Scale values in item/stat if needed

    Paramaters:
        item: stats to be processed
    """
    supported_events = perf.get_supported_events()

    for event in item.keys():
        for event_list in supported_events:
            if event in event_list:
                item[event]['value'] = item[event]['raw']

                if 'scale' in event_list[event]:
                    item[event]['value'] *= event_list[event]['scale']


def aggregate(grouped_items):
    """
    Aggregate values in item/stat

    Paramaters:
        grouped_items: grouped stats to be processed

    Returns:
        item with aggregated values
    """
    result = {}

    # iterate through all items
    for group in grouped_items.keys():
        # Create aggregated item with UID
        result[group] = agg_item = {}
        agg_item['uid'] = group

        # iterate through all the item of selected type
        for item in grouped_items[group]:

            # iterate through all stats in item
            # and process them, sum or create a list
            for stat in item.keys():

                # if it is dict, look for 'raw' key
                if isinstance(item[stat], dict):
                    if 'raw' not in item[stat]:
                        continue

                    if not stat in agg_item:
                        agg_item[stat] = {}

                    if 'raw' not in agg_item[stat]:
                        agg_item[stat]['raw'] = item[stat]['raw']
                    else:
                        agg_item[stat]['raw'] += item[stat]['raw']
                else:
                    # skip those stats
                    if stat in ['time_stamp']:
                        continue

                    # if it is not a dict, creat a list of those stats
                    # e.g.: timestamps, pids, etc.
                    stat_list = '{}_list'.format(stat)
                    if stat_list not in agg_item:
                        agg_item[stat_list] = []

                    agg_item[stat_list].append(item[stat])

        if 'pid_list' in agg_item:
            agg_item['pids_no'] = len(agg_item['pid_list'])

    return result


def calculate_llc_stats(stats):
    """
    Calculate LLC stats

    Paramaters:
        stats: LLC stats

    Returns:
        result(dict), calculated LLC stats
    """
    result = {}

    for key in stats:
        if "llc_occupancy" in stats[key]:
            result[key] = {}
            result[key]['llc_occup'] = round(stats[key]['llc_occupancy']['value'] / 1024, 0)

        if "cat_cws" in stats[key] and "cat_cw_size" in stats[key]:
            if key not in result:
                result[key] = {}

            result[key]['llc_alloc'] = stats[key]['cat_cws'] * stats[key]['cat_cw_size']

    return result


def group_stats_per_pool(item, recv_stats_pool):
    """
    Group perf stats by pool

    Parameters:
        item: input stats
        recv_stats_pool(dict): destination
    """
    pool_id, _ = common.CONFIG_STORE.pid_to_pool(item['pid'])

    if pool_id not in recv_stats_pool:
        recv_stats_pool[pool_id] = []

    recv_stats_pool[pool_id].append(item)


def group_stats_per_app(item, recv_stats_apps):
    """
    Group perf stats by APP

    Parameters:
        item: input stats
        recv_stats_apps(dict): destination
    """
    app_id = common.CONFIG_STORE.pid_to_app(item['pid'])

    if app_id:
        if app_id not in recv_stats_apps:
            recv_stats_apps[app_id] = []

        recv_stats_apps[app_id].append(item)
