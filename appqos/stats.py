################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
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


class StatsStore:
    """
    Storage for stats
    """


    class General:
        """
        Helper class
        """
        #pylint: disable=too-few-public-methods
        NUM_APPS_MOVES = 'num_apps_moves'
        NUM_ERR = 'num_err'
        NUM_INV_ACCESS = 'num_invalid_access_attempts'


    def __init__(self):
        self.general_stats = common.MANAGER.dict()
        for cntr in [self.General.NUM_APPS_MOVES,\
                self.General.NUM_ERR,\
                self.General.NUM_INV_ACCESS]:
            self.general_stats[cntr] = 0


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


    def general_stats_inc_num_invalid_access(self):
        """
        Increases num invalid access attempts stat value by 1
        """
        self.general_stats_inc(StatsStore.General.NUM_INV_ACCESS)
