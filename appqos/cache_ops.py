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
Provides RDT related helper functions used to configure RDT.
"""

import common
import log
from pqosapi import pqos_init, pqos_fini # pylint: disable=import-error,no-name-in-module
from pqosapi import pqos_alloc_assoc_set, pqos_l3ca_set, pqos_mba_set # pylint: disable=import-error,no-name-in-module
from pqosapi import pqos_is_mba_supported, pqos_is_cat_supported, pqos_get_num_cores # pylint: disable=import-error,no-name-in-module
from pqosapi import pqos_cpu_get_sockets # pylint: disable=import-error,no-name-in-module
from pqosapi import pqos_get_l3ca_num_cos, pqos_get_mba_num_cos # pylint: disable=import-error,no-name-in-module

class Pqos:
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
    def l3ca_set(sockets, cos, ways_mask):
        """
        Configures L3 CAT for CoS

        Parameters:
            sockets: sockets list on which to configure L3 CAT
            cos: Class of Service
            ways_mask: L3 CAT CBM to set

        Returns:
            0 on success
            -1 otherwise
        """
        try:
            for socket in sockets:
                pqos_l3ca_set(socket, cos, ways_mask)
        except Exception as ex:
            log.error(str(ex))
            return -1

        return 0

    @staticmethod
    def mba_set(sockets, cos, mb_max):
        """
        Configures MBA for CoS

        Parameters:
            sockets: sockets list on which to configure L3 CAT
            cos: Class of Service
            mb_max: MBA to set

        Returns:
            0 on success
            -1 otherwise
        """
        try:
            for socket in sockets:
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
        return Pqos.get_num_cores() > 1

    @staticmethod
    def get_num_cores():
        """
        Gets number of cores in system

        Returns:
            num of cores
            0 otherwise
        """
        try:
            return pqos_get_num_cores()
        except Exception as ex:
            log.error(str(ex))
            return 0

    @staticmethod
    def get_sockets():
        """
        Gets list of sockets

        Returns:
            sockets list,
            None otherwise
        """
        try:
            return pqos_cpu_get_sockets()
        except Exception as ex:
            log.error(str(ex))
            return None

    @staticmethod
    def get_l3ca_num_cos():
        """
        Gets number of COS for L3 CAT

        Returns:
            num of COS for L3 CAT
            None otherwise
        """
        try:
            return pqos_get_l3ca_num_cos()
        except Exception as ex:
            log.error(str(ex))
            return None

    @staticmethod
    def get_mba_num_cos():
        """
        Gets number of COS for MBA

        Returns:
            num of COS for MBA
            None otherwise
        """
        try:
            return pqos_get_mba_num_cos()
        except Exception as ex:
            log.error(str(ex))
            return None

    @staticmethod
    def get_max_cos_id(alloc_type):
        """
        Gets max COS# (id) that can be used to configure set of allocation technologies

        Returns:
            Available COS# to be used
            None otherwise
        """
        max_cos_num = None
        max_cos_cat = Pqos.get_l3ca_num_cos()
        max_cos_mba = Pqos.get_mba_num_cos()

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

PQOS_API = Pqos()


class Pool:
    # pylint: disable=too-many-public-methods
    """
    Static table of pools
    """
    pools = {}


    def __init__(self, pool):
        """
        Constructor

        Parameters:
            pool: Pool ID
        """

        self.pool = pool

        if self.pool not in Pool.pools:
            Pool.pools[self.pool] = {}
            Pool.pools[self.pool]['cores'] = []
            Pool.pools[self.pool]['apps'] = []
            Pool.pools[self.pool]['pids'] = []


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
        return Pool.pools[self.pool].get('cbm')


    def mba_set(self, mba):
        """
        Set mba value for the pool

        Parameters:
            mba: new mba value
        """
        Pool.pools[self.pool]['mba'] = mba


    def mba_get(self):
        """
        Get mba value for the pool

        Returns:
            mba value, 0 on error
        """
        return Pool.pools[self.pool].get('mba')


    def configure(self):
        """
        Configure Pool, based on config content.
        """
        config = common.CONFIG_STORE
        cores = config.get_pool_attr('cores', self.pool)
        cbm = config.get_pool_attr('cbm', self.pool)
        mba = config.get_pool_attr('mba', self.pool)
        apps = config.get_pool_attr('apps', self.pool)

        self.cbm_set(cbm)
        self.mba_set(mba)
        self.cores_set(cores)

        if apps:
            pids = []
            for app in apps:
                pids.extend(config.get_app_attr('pids', app))

            if pids:
                self.pids_set(pids)

        return Pool.apply(self.pool)


    def pids_set(self, pids):
        """
        Set Pool's PIDs

        Parameters:
            pids: Pool's PIDs
        """
        old_pids = self.pids_get()

        # change core affinity for PIDs not assigned to any pool
        removed_pids = [pid for pid in old_pids if pid not in pids]

        # skip pids assigned to any pool
        for cos in Pool.pools:
            if cos == self.pool:
                continue
            removed_pids = \
                [pid for pid in removed_pids if pid not in Pool.pools[cos]['pids']]

        if removed_pids:
            log.debug("PIDs to be set to core affinity to 'Default' CPUs {}".\
                format(removed_pids))

        # remove pid form old pool
        if old_pids:
            for cos in Pool.pools:
                if cos == self.pool:
                    continue

                pids_moved_from_other_pool = \
                    [pid for pid in Pool.pools[cos]['pids'] if pid in pids]

                if pids_moved_from_other_pool:
                    log.debug("PIDs moved from other pools {}".\
                        format(pids_moved_from_other_pool))

                # update other pool PIDs
                Pool.pools[cos]['pids'] = \
                    [pid for pid in Pool.pools[cos]['pids'] if pid not in pids]

        Pool.pools[self.pool]['pids'] = pids


    def pids_get(self):
        """
        Get pids for the pool

        Returns:
            pids list, None on error
        """
        return Pool.pools[self.pool].get('pids')


    def cores_set(self, cores):
        """
        Set Pool's cores

        Parameters:
            cores: Pool's cores
        """
        old_cores = self.cores_get()

        # create a diff, create a list of cores that were removed from current pool
        removed_cores = [core for core in old_cores if core not in cores]

        # update pool with new core list
        Pool.pools[self.pool]['cores'] = cores
        # updated RDT configuration
        PQOS_API.alloc_assoc_set(cores, self.pool)

        # process list of removed cores
        for cos in Pool.pools:
            if cos == self.pool:
                continue

            # check if cores were assigned to another pool,
            # if they were, remove them from that pool
            Pool.pools[cos]['cores'] = \
                [core for core in Pool.pools[cos]['cores'] if core not in cores]

            # filter out cores assigned to other pools
            removed_cores = \
                [core for core in removed_cores if core not in Pool.pools[cos]['cores']]

        # Finally assign removed cores back to COS0/"Default" Pool
        if removed_cores:
            log.debug("Cores assigned to COS#0 {}".format(removed_cores))
            PQOS_API.alloc_assoc_set(removed_cores, 0)


    def cores_get(self):
        """
        Get cores for the pool

        Returns:
            cores list, None on error
        """
        return Pool.pools[self.pool].get('cores')


    @staticmethod
    def apply(pools):
        """
        Apply RDT configuration for Pools

        Parameters:
            pools: Pools to apply RDT config for

        Returns:
            0 on success
            -1 otherwise
        """
        if isinstance(pools, list):
            _pools = pools
        else:
            _pools = [pools]

        # configure RDT
        for item in _pools:
            if item not in Pool.pools:
                continue

            pool = Pool(item)
            cbm = pool.cbm_get()
            mba = pool.mba_get()
            cores = pool.cores_get()
            cos = item

            sockets = PQOS_API.get_sockets()
            if sockets is None:
                return -1

            if cbm:
                if PQOS_API.l3ca_set(sockets, cos, cbm) != 0:
                    log.error("Failed to apply CAT configuration!")
                    return -1

            if mba:
                if PQOS_API.mba_set(sockets, cos, mba) != 0:
                    log.error("Failed to apply MBA configuration!")
                    return -1

            if cores:
                if PQOS_API.alloc_assoc_set(cores, cos) != 0:
                    log.error("Failed to associate RDT COS!")
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


def configure_rdt():
    """
    Configure RDT

    Returns:
        0 on success
    """
    result = 0

    # detect removed pools
    old_pools = Pool.pools.copy()

    pool_ids = common.CONFIG_STORE.get_pool_attr('id', None)

    if old_pools:
        for cos in old_pools:
            if not pool_ids or cos not in pool_ids:
                log.debug("Pool {} removed...".format(cos))
                Pool(cos).cores_set([])

    Pool.reset()

    if not pool_ids:
        log.error("No Pools to configure...")
        return -1

    for cos in pool_ids:
        Pool(cos)

    for cos in Pool.pools:
        result = Pool(cos).configure()
        if result != 0:
            break

    return result
