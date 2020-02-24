################################################################################
# BSD LICENSE
#
# Copyright(c) 2018-2020 Intel Corporation. All rights reserved.
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
        for cntr in [stats_store.General.NUM_APPS_MOVES, stats_store.General.NUM_ERR]:
            assert cntr in gen_stats
            assert gen_stats[cntr] == 0


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


    def test_stats_inc_num_invalid_access(self):
        stats_store = StatsStore()

        gen_stats = stats_store.general_stats_get()
        assert gen_stats['num_invalid_access_attempts'] == 0

        INC_CNT = 4

        for _ in range(INC_CNT):
            stats_store.general_stats_inc_num_invalid_access()

        gen_stats = stats_store.general_stats_get()
        assert gen_stats['num_invalid_access_attempts'] == INC_CNT

        gen_stats_invalid_access = stats_store.general_stats_get(StatsStore.General.NUM_INV_ACCESS)
        assert gen_stats_invalid_access == INC_CNT


    def test_stats_get(self):
        stats_store = StatsStore()

        gen_stats = stats_store.general_stats_get("inexisting_stats")
        assert not gen_stats
