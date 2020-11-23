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
PQoS API module.
Provides RDT related helper functions used to configure RDT.
"""

import os

from pqos import Pqos
from pqos.capability import PqosCap
from pqos.l3ca import PqosCatL3
from pqos.mba import PqosMba
from pqos.allocation import PqosAlloc
from pqos.cpuinfo import PqosCpuInfo

import common
import log


class PqosApi:
# pylint: disable=too-many-instance-attributes
    """
    Wrapper for libpqos wrapper.
    """


    def __init__(self):
        self.pqos = Pqos()
        self.cap = None
        self.l3ca = None
        self.mba = None
        self.alloc = None
        self.cpuinfo = None
        self._supported_iface = []
        self._current_iface = None


    def detect_supported_ifaces(self):
        """
        Detects supported RDT interfaces
        """
        for iface in ["msr","os"]:
            try:
                self.pqos.init(iface.upper())
                self.pqos.fini()
                self._supported_iface.append(iface)
            except Exception as ex:
                log.debug("Supported RDT interfaces detection: " + str(ex))

        log.info("Supported RDT interfaces: " + str(self.supported_iface()))


    def init(self, iface):
        """
        Initializes libpqos

        Returns:
            0 on success
            -1 otherwise
        """

        if not iface in self.supported_iface():
            log.error("RDT does not support '%s' interface!" % (iface))
            return -1

        # deinitialize lib first
        if self._current_iface:
            self.fini()

        # umount restcrl to improve caps detection
        if iface == "os":
            result = os.system("/bin/umount -a -t resctrl") # nosec - string literal
            if result:
                log.error("Failed to umount resctrl fs! status code: %d"\
                     % (os.WEXITSTATUS(result)))
                return -1

        try:
            self.pqos.init(iface.upper())
            self.cap = PqosCap()
            self.l3ca = PqosCatL3()
            self.mba = PqosMba()
            self.alloc = PqosAlloc()
            self.cpuinfo = PqosCpuInfo()
        except Exception as ex:
            log.error(str(ex))
            return -1

        log.info("RDT initialized with '%s' interface" % (iface))
        self._current_iface = iface
        return 0


    def current_iface(self):
        """
        Returns current RDT interface

        Returns:
            interface name on success
            None when libpqos is not initialized
        """
        return self._current_iface


    def supported_iface(self):
        """
        Returns list of supported RDT interfaces

        Returns:
            list of supported interfaces
        """
        return self._supported_iface


    def fini(self):
        """
        De-initializes libpqos
        """
        self.pqos.fini()
        self._current_iface = None

        return 0


    def release(self, cores):
        """
        Release cores, assigns cores to CoS#0

        Parameters:
            cores: list of cores to be released

        Returns:
            0 on success
            -1 otherwise
        """
        if cores is None:
            return 0

        try:
            self.alloc.release(cores)
        except Exception as ex:
            log.error(str(ex))
            return -1

        return 0


    def alloc_assoc_set(self, cores, cos):
        """
        Assigns cores to CoS

        Parameters:
            cores: list of cores to be assigned to cos
            cos: Class of Service

        Returns:
            0 on success
            -1 otherwise
        """
        if not cores:
            return 0

        try:
            for core in cores:
                self.alloc.assoc_set(core, cos)
        except Exception as ex:
            log.error(str(ex))
            return -1

        return 0


    def l3ca_set(self, sockets, cos_id, ways_mask):
        """
        Configures L3 CAT for CoS

        Parameters:
            sockets: sockets list on which to configure L3 CAT
            cos_id: Class of Service
            ways_mask: L3 CAT CBM to set

        Returns:
            0 on success
            -1 otherwise
        """
        try:
            cos = self.l3ca.COS(cos_id, ways_mask)
            for socket in sockets:
                self.l3ca.set(socket, [cos])
        except Exception as ex:
            log.error(str(ex))
            return -1

        return 0


    def mba_set(self, sockets, cos_id, mb_max):
        """
        Configures MBA rate for CoS

        Parameters:
            sockets: sockets list on which to configure L3 CAT
            cos_id: Class of Service
            mb_max: MBA rate to set

        Returns:
            0 on success
            -1 otherwise
        """
        try:
            cos = self.mba.COS(cos_id, mb_max)
            for socket in sockets:
                self.mba.set(socket, [cos])
        except Exception as ex:
            log.error(str(ex))
            return -1

        return 0


    def is_mba_supported(self):
        """
        Checks for MBA support

        Returns:
            True if supported
            False otherwise
        """
        try:
            return bool(self.get_mba_num_cos())
        except Exception as ex:
            log.error(str(ex))
            return False


    def is_l3_cat_supported(self):
        """
        Checks for L3 CAT support

        Returns:
            True if supported
            False otherwise
        """
        try:
            return bool(self.get_l3ca_num_cos())
        except Exception as ex:
            log.error(str(ex))
            return False


    def is_multicore(self):
        """
        Checks if system is multicore

        Returns:
            True if multicore
            False otherwise
        """
        return self.get_num_cores() > 1


    def get_num_cores(self):
        """
        Gets number of cores in system

        Returns:
            num of cores
            None otherwise
        """
        try:
            sockets = self.cpuinfo.get_sockets()
            return sum([len(self.cpuinfo.get_cores(socket)) for socket in sockets])
        except Exception as ex:
            log.error(str(ex))
            return None


    def check_core(self, core):
        """
        Verifies if a specified core is a valid logical core ID.

        Parameters:
            core: core ID

        Returns:
            True/False a given core number is valid/invalid
            None otherwise
        """
        try:
            return self.cpuinfo.check_core(core)
        except Exception as ex:
            log.error(str(ex))
            return None


    def get_sockets(self):
        """
        Gets list of sockets

        Returns:
            sockets list,
            None otherwise
        """

        try:
            return self.cpuinfo.get_sockets()
        except Exception as ex:
            log.error(str(ex))
            return None


    def get_l3ca_num_cos(self):
        """
        Gets number of COS for L3 CAT

        Returns:
            num of COS for L3 CAT
            None otherwise
        """
        try:
            return self.cap.get_l3ca_cos_num()
        except Exception as ex:
            log.error(str(ex))
            return None


    def get_mba_num_cos(self):
        """
        Gets number of COS for MBA

        Returns:
            num of COS for MBA
            None otherwise
        """
        try:
            return self.cap.get_mba_cos_num()
        except Exception as ex:
            log.error(str(ex))
            return None


    def get_max_cos_id(self, alloc_type):
        """
        Gets max COS# (id) that can be used to configure set of allocation technologies

        Returns:
            Available COS# to be used
            None otherwise
        """
        max_cos_num = None
        max_cos_cat = self.get_l3ca_num_cos()
        max_cos_mba = self.get_mba_num_cos()

        if common.CAT_CAP not in alloc_type and common.MBA_CAP not in alloc_type:
            return None

        if common.CAT_CAP in alloc_type and not max_cos_cat:
            return None

        if common.MBA_CAP in alloc_type and not max_cos_mba:
            return None

        if common.CAT_CAP in alloc_type:
            max_cos_num = max_cos_cat

        if common.MBA_CAP in alloc_type:
            if max_cos_num is not None:
                max_cos_num = min(max_cos_mba, max_cos_num)
            else:
                max_cos_num = max_cos_mba

        return max_cos_num - 1


    def get_max_l3_cat_cbm(self):
        """
        Gets Max L3 CAT CBM

        Returns:
            Max L3 CAT CBM
            None otherwise
        """

        if not self.is_l3_cat_supported():
            return None

        try:
            l3ca_caps = self.cap.get_type("l3ca")
            return 2**l3ca_caps.num_ways - 1
        except Exception as ex:
            log.error(str(ex))
            return None
