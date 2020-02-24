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

from .features_rdt import FeaturesRdt
from .env import Env

class FeaturesRdtMsr(FeaturesRdt):
    def is_l2_cat_supported(self):
        return Env().exists('cat', 'l2')

    def is_l2_cdp_supported(self):
        return self.is_l2_cat_supported() and Env().get("cat", "l2", "cdp")

    def is_l3_cat_supported(self):
        return Env().exists('cat', 'l3')

    def is_l3_cdp_supported(self):
        return self.is_l3_cat_supported() and Env().get("cat", "l3", "cdp")

    def is_cqm_supported(self):
        return self.is_cqm_llc_occupancy_supported()

    def is_cqm_llc_occupancy_supported(self):
        return Env().exists('cmt', 'occup_llc') and Env().get('cmt', 'occup_llc')

    def is_mba_supported(self):
        return Env().exists('mba')

    def is_mba_ctrl_supported(self):
        return False

    def is_mbm_local_supported(self):
        return Env().exists('mbm', 'mbm_local') and Env().get('mbm', 'mbm_local')

    def is_mbm_total_supported(self):
        return Env().exists('mbm', 'mbm_total') and Env().get('mbm', 'mbm_total')
