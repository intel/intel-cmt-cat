################################################################################
# BSD LICENSE
#
# Copyright(c) 2018 Intel Corporation. All rights reserved.
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

import pytest
import mock
import common

from stats import *

class TestStats(object):

    def test_stats_init(self):
        stats_store = StatsStore()

        gen_stats = stats_store.general_stats_get()
        for cntr in [stats_store.General.NUM_FLUSHES, stats_store.General.NUM_APPS_MOVES, stats_store.General.NUM_ERR]:
            assert cntr in gen_stats
            assert gen_stats[cntr] == 0

    def test_stats_inc_num_flushes(self):
        stats_store = StatsStore()

        gen_stats = stats_store.general_stats_get()
        assert gen_stats['num_flushes'] == 0

        INC_CNT = 10

        for _ in range(INC_CNT):
            stats_store.general_stats_inc_num_flushes()

        gen_stats = stats_store.general_stats_get()
        assert gen_stats['num_flushes'] == INC_CNT

        gen_stats_flushes = stats_store.general_stats_get(StatsStore.General.NUM_FLUSHES)
        assert gen_stats_flushes == INC_CNT

    def test_stats_inc_num_apps_moves(self):
        stats_store = StatsStore()

        gen_stats = stats_store.general_stats_get()
        assert gen_stats['num_apps_moves'] == 0

        INC_CNT = 6

        for _ in range(INC_CNT):
            stats_store.general_stats_inc_apps_moves()

        gen_stats = stats_store.general_stats_get()
        assert gen_stats['num_apps_moves'] == INC_CNT

        gen_stats_moves = stats_store.general_stats_get(StatsStore.General.NUM_APPS_MOVES)
        assert gen_stats_moves == INC_CNT

    def test_stats_inc_num_err(self):
        stats_store = StatsStore()

        gen_stats = stats_store.general_stats_get()
        assert gen_stats['num_err'] == 0

        INC_CNT = 2

        for _ in range(INC_CNT):
            stats_store.general_stats_inc_num_err()

        gen_stats = stats_store.general_stats_get()
        assert gen_stats['num_err'] == INC_CNT

        gen_stats_err = stats_store.general_stats_get(StatsStore.General.NUM_ERR)
        assert gen_stats_err == INC_CNT

    def test_stats_get(self):
        stats_store = StatsStore()

        gen_stats = stats_store.general_stats_get("inexisting_stats")
        assert not gen_stats

    POOL_STATS = {'prod': {'llc_alloc': 1024, 'llc_occup': 0.0}, 'besteff': {'llc_alloc': 1024, 'llc_occup': 0.0}, 'preprod': {'llc_alloc': 1024, 'llc_occup': 64.0}}
    APP_STATS = {4: {'llc_occup': 0.0}, 5: {'llc_occup': 0.0}, 6: {'llc_occup': 64.0}}

    def test_stats_get_app_stats(self):
        stats_store = StatsStore()

        assert not stats_store.get_app_stats()

    def test_stats_set_app_stats(self):
        stats_store = StatsStore()
        stats_store.set_app_stats(self.APP_STATS)

        assert stats_store.get_app_stats() == self.APP_STATS

    def test_stats_get_pool_stats(self):
        stats_store = StatsStore()

        assert not stats_store.get_pool_stats()

    def test_stats_set_pool_stats(self):
        stats_store = StatsStore()
        stats_store.set_pool_stats(self.POOL_STATS)

        assert stats_store.get_pool_stats() == self.POOL_STATS

    POOL_CAT = {1: {'cws': 2}, 2: {'cws': 5}, 3: {'cws': 2}, 4: {'cws': 1}}

    def test_stats_get_pool_cat(self):
        stats_store = StatsStore()

        assert not stats_store.get_pool_cat()

    def test_stats_set_pool_cat(self):
        stats_store = StatsStore()
        stats_store.set_pool_cat(self.POOL_CAT)

        assert stats_store.get_pool_cat() == self.POOL_CAT

    APP_STATS_AGGR = {4: {'llc_occupancy': {'value': 524288.0}},\
                      5: {'llc_occupancy': {'value': 1048576.0}},\
                      6: {'llc_occupancy': {'value': 2097152.0}}}

    POOL_STATS_AGGR = {2: {'llc_occupancy': {'value': 524288.0}, 'cat_cw_size': 1024, 'cat_cws': 1},\
                       3: {'llc_occupancy': {'value': 2097152.0}, 'cat_cw_size': 1024, 'cat_cws': 3},
                       4: {'llc_occupancy': {'value': 1048576.0}, 'cat_cw_size': 1024, 'cat_cws': 2}}

    def test_stats_calculate_llc_stats(self):
        result = calculate_llc_stats(self.APP_STATS_AGGR)

        assert 'llc_occup' in result[4] and 'llc_alloc' not in result[4]
        assert 'llc_occup' in result[5] and 'llc_alloc' not in result[4]
        assert 'llc_occup' in result[6] and 'llc_alloc' not in result[4]

        assert result[4]['llc_occup'] == 512
        assert result[5]['llc_occup'] == 1024
        assert result[6]['llc_occup'] == 2048

        result.clear()

        result = calculate_llc_stats(self.POOL_STATS_AGGR)

        assert 'llc_occup' in result[2] and 'llc_alloc' in result[2]
        assert 'llc_occup' in result[3] and 'llc_alloc' in result[3]
        assert 'llc_occup' in result[4] and 'llc_alloc' in result[4]

        assert result[2]['llc_occup'] == 512 and result[2]['llc_alloc'] == 1024
        assert result[3]['llc_occup'] == 2048 and result[3]['llc_alloc'] == 3072
        assert result[4]['llc_occup'] ==  1024 and result[4]['llc_alloc'] == 2048

    def test_group_stats_per_pool(self):
        def pid_to_pool(pid):
            return pid % 2 + 1, None

        ITEMS_CNT_PER_POOL = 6
        item = {'pid': 1234}

        recv_stats_apps = {}

        with mock.patch('common.CONFIG_STORE.pid_to_pool', new=pid_to_pool):
            for _ in range(ITEMS_CNT_PER_POOL * 2):
                group_stats_per_pool(dict(item), recv_stats_apps)
                item['pid'] += 1

            # Check stats count per POOL
            assert 1 in recv_stats_apps and 2 in recv_stats_apps
            assert len(recv_stats_apps[1]) == ITEMS_CNT_PER_POOL
            assert len(recv_stats_apps[2]) == ITEMS_CNT_PER_POOL

    def test_group_stats_per_app(self):
        def pid_to_app(pid):
            return pid % 2 + 1

        ITEMS_CNT_PER_APP = 6
        item = {'pid': 1234}
        recv_stats_apps = {}

        with mock.patch('common.CONFIG_STORE.pid_to_app', new=pid_to_app):
            for _ in range(ITEMS_CNT_PER_APP * 2):
                group_stats_per_app(dict(item), recv_stats_apps)
                item['pid'] += 1

        # Check stats count per APP
        assert 1 in recv_stats_apps and 2 in recv_stats_apps
        assert len(recv_stats_apps[1]) == ITEMS_CNT_PER_APP
        assert len(recv_stats_apps[2]) == ITEMS_CNT_PER_APP

    def test_aggregate(self):
        grouped_items = {}

        result = aggregate(grouped_items)
