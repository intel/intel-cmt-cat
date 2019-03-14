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
Cache Ops module.
Provides Cache related helper functions
used to configure CAT.
"""

import common
import log
from pqosapi import pqos_init, pqos_fini # pylint: disable=import-error,no-name-in-module
from pqosapi import pqos_alloc_assoc_set, pqos_l3ca_set, pqos_mba_set # pylint: disable=import-error,no-name-in-module
from pqosapi import pqos_is_mba_supported, pqos_is_cat_supported, pqos_is_multicore # pylint: disable=import-error,no-name-in-module


class Pqos(object):
    """
    Wrapper for "libpqos".
    Uses "libpqos" to configure Intel(R) RDT
    """

    @staticmethod
    def init():
        """
        Initializes libpqos

        Returns:
            0 on success
            -1 otherwise
        """
        try:
            pqos_init()
        except Exception as ex:
            log.error(str(ex))
            return -1

        return 0

    @staticmethod
    def fini():
        """
        De-initializes libpqos
        """
        pqos_fini()

        return 0

    @staticmethod
    def alloc_assoc_set(cores, cos):
        """
        Assigns cores to CoS

        Parameters:
            cos: Class of Service
            cores: list of cores to be assigned to cos

        Returns:
            0 on success
            -1 otherwise
        """
        if not cores:
            return 0

        try:
            for core in cores:
                pqos_alloc_assoc_set(core, cos)
        except Exception as ex:
            log.error(str(ex))
            return -1

        return 0


    @staticmethod
    def l3ca_set(socket, cos, ways_mask):
        """
        Configures L3 CAT for CoS

        Parameters:
            socket: socket on which to configure L3 CAT
            cos: Class of Service
            ways_mask: L3 CAT CBM to set

        Returns:
            0 on success
            -1 otherwise
        """

        try:
            pqos_l3ca_set(socket, cos, ways_mask)
        except Exception as ex:
            log.error(str(ex))
            return -1

        return 0

    @staticmethod
    def mba_set(socket, cos, mb_max):
        """
        Configures MBA for CoS

        Parameters:
            socket: socket on which to configure L3 CAT
            cos: Class of Service
            mb_max: MBA to set

        Returns:
            0 on success
            -1 otherwise
        """
        try:
            pqos_mba_set(socket, cos, mb_max)
        except Exception as ex:
            log.error(str(ex))
            return -1

        return 0

    @staticmethod
    def is_mba_supported():
        """
        Checks for MBA support

        Returns:
            1 if supported
            0 otherwise
        """
        try:
            return pqos_is_mba_supported()
        except Exception as ex:
            log.error(str(ex))
            return 0

    @staticmethod
    def is_cat_supported():
        """
        Checks for CAT support

        Returns:
            1 if supported
            0 otherwise
        """
        try:
            return pqos_is_cat_supported()
        except Exception as ex:
            log.error(str(ex))
            return 0

    @staticmethod
    def is_multicore():
        """
        Checks if system is multicore

        Returns:
            1 if multicore
            0 otherwise
        """
        try:
            return pqos_is_multicore()
        except Exception as ex:
            log.error(str(ex))
            return 0

PQOS_API = Pqos()


class Pool(object):
    # pylint: disable=too-many-public-methods
    """
    Static table of pools
    """
    pools = {}


    class Cos(object):
        #pylint: disable=too-few-public-methods, invalid-name
        """
        Cos enum
        """
        P = 1
        PP = 2
        BE = 0


    class Priority(object):
        #pylint: disable=too-few-public-methods, invalid-name
        """
        Priority enum
        """
        P = 1
        PP = 2
        BE = 0


    def __init__(self, pool):
        """
        Constructor

        Parameters:
            pool: Pool ID
        """

        self.pool = pool

        if self.pool not in Pool.pools:
            Pool.pools[self.pool] = {}
            Pool.pools[self.pool]['enabled'] = False
            Pool.pools[self.pool]['cores'] = []
            Pool.pools[self.pool]['pids'] = []


    def priority_get(self, cos=None):
        """
        Get pool priority

        Parameters:
            cos: Class of Service

        Returns:
            Priority of Class of Service or current Pool
        """

        if cos is None:
            return self.priority_get(self.pool)

        return {
            Pool.Cos.P: Pool.Priority.P,
            Pool.Cos.PP: Pool.Priority.PP,
            Pool.Cos.BE: Pool.Priority.BE
        }[cos]


    def get_common_priority(self):
        """
        Get pool priority

        Returns:
            Pool priority
        """
        if self.pool == Pool.Cos.P:
            return common.PRODUCTION
        elif self.pool == Pool.Cos.BE:
            return common.BEST_EFFORT
        elif self.pool == Pool.Cos.PP:
            return common.PRE_PRODUCTION
        return None


    def get_pool_id(self):
        """
        Get pool ID

        Returns:
            Pool ID
        """
        return common.CONFIG_STORE.get_pool_id(self.get_common_priority())


    def enabled_get(self):
        """
        Check if pool is enabled

        Parameters:
            pool: Pool ID

        Returns:
            bool: pool enable status
        """
        return Pool.pools[self.pool]['enabled']


    def enabled_set(self, enabled):
        """
        Set pool enabled/disabled

        Parameters:
            enabled: Flag for enabling/disabling the pool

        Returns:
            result: 0 on success
        """
        if Pool.pools[self.pool]['enabled'] == enabled:
            return 0

        Pool.pools[self.pool]['enabled'] = enabled

        return Pool.apply(self.pool)


    def cbm_set(self, cbm):
        """
        Set cbm mask for the pool

        Parameters:
            cbm: new cbm mask
        """
        Pool.pools[self.pool]['cbm'] = cbm


    def cbm_get(self):
        """
        Get cbm mask for the pool

        Returns:
            cbm mask, 0 on error
        """
        return Pool.pools[self.pool].get('cbm', 0)


    def configure(self):
        """
        Configure Pool, based on config file content.
        """
        cores = common.CONFIG_STORE.get_attr_list('cores', self.get_common_priority())
        pids = common.CONFIG_STORE.get_attr_list('pids', self.get_common_priority())
        cbm = common.CONFIG_STORE.get_attr_list('min_cws', self.get_common_priority())

        self.cbm_set(int(cbm))
        self.cores_set(cores)
        self.pids_set(pids)
        return self.enabled_set(True)


    def pids_set(self, pids):
        """
        Set Pool's PIDs

        Parameters:
            pids: Pool's PIDs
        """
        old_pids = self.pids_get()

        # change core affinity for PIDs not assigned to any pool
        if self.enabled_get() and not self.pool == Pool.Cos.BE:
            removed_pids = [pid for pid in old_pids if pid not in pids]

            # skip pids assigned to any pool
            for cos in Pool.pools:
                if cos == self.pool:
                    continue
                removed_pids = \
                    [pid for pid in removed_pids if pid not in Pool.pools[cos]['pids']]
                # core affinity logic here

        # remove pid form old pool
        if old_pids:
            for cos in Pool.pools:
                if cos == self.pool:
                    continue

                # core affinity logic here
                # pids_to_change_core_affinity = \
                # [pid for pid in Pool.pools[cos]['pids'] if pid in pids]

                Pool.pools[cos]['pids'] = \
                    [pid for pid in Pool.pools[cos]['pids'] if pid not in pids]

        Pool.pools[self.pool]['pids'] = pids


    def pids_get(self):
        """
        Get Pool's PIDs

        Returns:
            Pool's PIDs
        """
        return Pool.pools[self.pool]['pids']


    def cores_set(self, cores):
        """
        Set Pool's cores

        Parameters:
            cores: Pool's cores
        """
        old_cores = Pool.pools[self.pool]['cores']
        Pool.pools[self.pool]['cores'] = cores

        # remove core form old pool
        for cos in Pool.pools:
            if cos == self.pool:
                continue
            Pool.pools[cos]['cores'] = \
                [core for core in Pool.pools[cos]['cores'] if core not in cores]

        if self.enabled_get():
            removed_cores = [core for core in old_cores if core not in cores]

            # skip cores assigned to any pool
            for cos in Pool.pools:
                if cos == self.pool:
                    continue
                removed_cores = \
                    [core for core in removed_cores if core not in Pool.pools[cos]['cores']]

            # Assign unasigned removed cores back to COS0
            if removed_cores:
                PQOS_API.alloc_assoc_set(removed_cores, 0)

        PQOS_API.alloc_assoc_set(cores, self.pool if self.enabled_get() else 0)


    @staticmethod
    def apply(pools):
        """
        Apply CAT configuration for Pools

        Parameters:
            pools: Pools to apply CAT config for

        Returns:
            0 on success
            -1 otherwise
        """
        if isinstance(pools, list):
            _pools = pools
        else:
            _pools = [pools]

        # configure CAT
        for item in _pools:
            if item not in Pool.pools:
                continue
            pool = Pool(item)
            if pool.enabled_get():
                cbm = pool.cbm_get()
                cos = item
            else:
                cos = 0
                cbm = Pool(Pool.Cos.BE).cbm_get()

            if cbm > 0:
                if PQOS_API.l3ca_set(0, cos, cbm) != 0:
                    return -1

            if 'cores' in Pool.pools[item] and Pool.pools[item]['cores']:
                if PQOS_API.alloc_assoc_set(Pool.pools[item]['cores'], cos) != 0:
                    return -1

        return 0


    @staticmethod
    def reset():
        """
        Reset pool configuration
        """
        Pool.pools = {}


    @staticmethod
    def dump():
        """
        Dump pool CAT configuration
        """
        log.info("Pools:{}".format(Pool.pools))


def configure_cat():
    """
    Configure cache allocation

    Returns:
        0 on success
    """
    result = 0
    for cos in [Pool.Cos.P, Pool.Cos.PP, Pool.Cos.BE]:
        result = Pool(cos).configure()
        if result != 0:
            break

    return result
