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

from .resctrl import Resctrl
from .perf import Perf
from .features_rdt import FeaturesRdt

class FeaturesRdtOs(FeaturesRdt):
    def __init__(self):
        self.resctrl = Resctrl()
        self.perf = Perf()

    def _check_resctrl_info_dir(self, dir_name):
        if not self.resctrl.is_mounted():
            return False

        if not self.resctrl.info_dir_exists(dir_name):
            return False

        return True

    def is_l2_cat_supported(self):
        self.resctrl.umount()
        if not self.resctrl.mount():
            return False
        result = self._check_resctrl_info_dir("L2")
        self.resctrl.umount()
        return result

    def is_l2_cdp_supported(self):
        self.resctrl.umount()
        if not self.resctrl.mount(cdpl2=True):
            return False
        result = self._check_resctrl_info_dir("L2CODE") or \
                 self._check_resctrl_info_dir("L2DATA")
        self.resctrl.umount()
        return result

    def is_l3_cat_supported(self):
        self.resctrl.umount()
        if not self.resctrl.mount():
            return False
        result = self._check_resctrl_info_dir("L3")
        self.resctrl.umount()
        return result

    def is_l3_cdp_supported(self):
        self.resctrl.umount()
        if not self.resctrl.mount(cdp=True):
            return False
        result = self._check_resctrl_info_dir("L3CODE") or \
                 self._check_resctrl_info_dir("L3DATA")
        self.resctrl.umount()
        return result

    def is_cqm_supported(self):
        self.resctrl.umount()
        if not self.resctrl.mount():
            return False
        result = self._check_resctrl_info_dir("L3_MON") or \
                 self._check_resctrl_info_dir("L2_MON")
        self.resctrl.umount()
        return result

    def _resctrl_is_cqm_llc_occupancy_supported(self):
        self.resctrl.umount()
        if not self.resctrl.mount():
            return False
        mon_features = self.resctrl.get_mon_features()
        self.resctrl.umount()
        return 'llc_occupancy' in mon_features

    def _perf_is_cqm_llc_occupancy_supported(self):
        return self.perf.is_mon_event_supported("llc_occupancy")

    def is_cqm_llc_occupancy_supported(self):
        return self._resctrl_is_cqm_llc_occupancy_supported() or \
               self._perf_is_cqm_llc_occupancy_supported()

    def is_mba_supported(self):
        self.resctrl.umount()
        if not self.resctrl.mount():
            return False
        result = self._check_resctrl_info_dir("MB")
        self.resctrl.umount()
        return result

    def is_mba_ctrl_supported(self):
        self.resctrl.umount()
        if not self.resctrl.mount(mba_mbps=True):
            return False

        result = self._check_resctrl_info_dir("MB") and \
                 self._check_resctrl_info_dir("L3_MON")
        if result:
            schemata = self.resctrl.get_schemata()
            result = schemata.get('MB', 0) > 100

        self.resctrl.umount()
        return result

    def _resctrl_is_mbm_local_supported(self):
        self.resctrl.umount()
        if not self.resctrl.mount():
            return False
        mon_features = self.resctrl.get_mon_features()
        self.resctrl.umount()
        return 'mbm_local_bytes' in mon_features

    def _perf_is_mbm_local_supported(self):
        return self.perf.is_mon_event_supported("local_bytes")

    def is_mbm_local_supported(self):
        return self._resctrl_is_mbm_local_supported() or \
               self._perf_is_mbm_local_supported()

    def _resctrl_is_mbm_total_supported(self):
        self.resctrl.umount()
        if not self.resctrl.mount():
            return False
        mon_features = self.resctrl.get_mon_features()
        self.resctrl.umount()
        return 'mbm_total_bytes' in mon_features

    def _perf_is_mbm_total_supported(self):
        return self.perf.is_mon_event_supported("total_bytes")

    def is_mbm_total_supported(self):
        return self._resctrl_is_mbm_total_supported() or \
               self._perf_is_mbm_total_supported()
