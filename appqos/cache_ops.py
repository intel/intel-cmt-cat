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
Cache Ops module.
Provides RDT related helper functions used to configure RDT.
"""

import os

import caps
import common
import log
import power

class Apps:
    """
    Apps options
    """
    @staticmethod
    def configure():
        """
        Configure Apps, based on config content.
        """

        config = common.CONFIG_STORE.get_config()

        if 'apps' not in config:
            return 0

        # set pid affinity
        for app in config['apps']:
            if 'pids' not in app:
                continue

            app_cores = app['cores'] if 'cores' in app else []
            pool_id = common.CONFIG_STORE.app_to_pool(app['id'])
            pool_cores = common.CONFIG_STORE.get_pool_attr('cores', pool_id)

            # if there are no cores configured for App, or cores configured are
            # not a subset of Pool cores, revert to all Pool cores
            if not app_cores or not set(app_cores).issubset(pool_cores):
                app_cores = pool_cores

            if not app_cores:
                continue

            Apps.set_affinity(app['pids'], app_cores)

        return 0


    @staticmethod
    def set_affinity(pids, cores):
        """
        Sets PIDs' core affinity

        Parameters:
            pids: PIDs to set core affinity for
            cores: cores to set to
        """

        # set core affinity for each PID,
        # even if operation fails for one PID, continue with other PIDs
        for pid in pids:
            try:
                os.sched_setaffinity(pid, cores)
            except OSError:
                log.error("Failed to set {} PID affinity".format(pid))


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
        self.cores_set(cores)

        if caps.cat_supported():
            cbm = config.get_pool_attr('cbm', self.pool)
            self.cbm_set(cbm)

        if caps.mba_supported():
            mba = config.get_pool_attr('mba', self.pool)
            self.mba_set(mba)

        apps = config.get_pool_attr('apps', self.pool)
        if apps is not None:
            pids = []
            for app in apps:
                app_pids = config.get_app_attr('pids', app)
                if app_pids:
                    pids.extend(app_pids)

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

        # skip pids assigned to any pool e.g.: apps moved to other pool
        for pool_id in Pool.pools:
            if pool_id == self.pool:
                continue
            removed_pids = \
                [pid for pid in removed_pids if pid not in Pool.pools[pool_id]['pids']]

        # remove pid form old pool
        if old_pids:
            for pool_id in Pool.pools:
                if pool_id == self.pool:
                    continue

                pids_moved_from_other_pool = \
                    [pid for pid in Pool.pools[pool_id]['pids'] if pid in pids]

                if pids_moved_from_other_pool:
                    log.debug("PIDs moved from other pools {}".\
                        format(pids_moved_from_other_pool))

                # update other pool PIDs
                Pool.pools[pool_id]['pids'] = \
                    [pid for pid in Pool.pools[pool_id]['pids'] if pid not in pids]

        Pool.pools[self.pool]['pids'] = pids

        # set affinity of removed pids to default
        if removed_pids:
            log.debug("PIDs to be set to core affinity to 'Default' CPUs {}".\
                format(removed_pids))

            # get cores for Default Pool #0
            cores = common.CONFIG_STORE.get_pool_attr('cores', 0)
            for pid in removed_pids:
                try:
                    os.sched_setaffinity(pid, cores)
                except OSError:
                    pass


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
        common.PQOS_API.alloc_assoc_set(cores, self.pool)

        # process list of removed cores
        for pool_id in Pool.pools:
            if pool_id == self.pool:
                continue

            # check if cores were assigned to another pool,
            # if they were, remove them from that pool
            Pool.pools[pool_id]['cores'] = \
                [core for core in Pool.pools[pool_id]['cores'] if core not in cores]

            # filter out cores assigned to other pools
            removed_cores = \
                [core for core in removed_cores if core not in Pool.pools[pool_id]['cores']]

        # Finally assign removed cores back to COS0/"Default" Pool
        if removed_cores:
            log.debug("Cores assigned to COS#0 {}".format(removed_cores))
            common.PQOS_API.release(removed_cores)

        # Reset power profile settings
        if caps.sstcp_enabled() and removed_cores:
            power.reset(removed_cores)


    def cores_get(self):
        """
        Get cores for the pool

        Returns:
            cores list, None on error
        """
        return Pool.pools[self.pool].get('cores')


    @staticmethod
    def apply(pool_id):
        """
        Apply RDT configuration for Pool

        Parameters:
            pool_id: Pool to apply RDT config for

        Returns:
            0 on success
            -1 otherwise
        """
        # configure RDT
        if pool_id not in Pool.pools:
            return -1

        pool = Pool(pool_id)
        cbm = pool.cbm_get()
        mba = pool.mba_get()
        cores = pool.cores_get()

        # Apply same RDT configuration on all sockets in the system
        sockets = common.PQOS_API.get_sockets()
        if sockets is None:
            log.error("Failed to get sockets info!")
            return -1

        # pool id to COS, 1:1 mapping
        if cbm:
            if common.PQOS_API.l3ca_set(sockets, pool_id, cbm) != 0:
                log.error("Failed to apply CAT configuration!")
                return -1

        if mba:
            if common.PQOS_API.mba_set(sockets, pool_id, mba) != 0:
                log.error("Failed to apply MBA configuration!")
                return -1

        if cores:
            if common.PQOS_API.alloc_assoc_set(cores, pool_id) != 0:
                log.error("Failed to associate RDT COS!")
                return -1

        return 0


    @staticmethod
    def reset():
        """
        Reset pool configuration
        """
        Pool.pools = {}


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
        for pool_id in old_pools:
            if not pool_ids or pool_id not in pool_ids:
                log.debug("Pool {} removed...".format(pool_id))
                Pool(pool_id).cores_set([])

                # remove pool
                Pool.pools.pop(pool_id)

    if not pool_ids:
        log.error("No Pools to configure...")
        return -1

    for pool_id in pool_ids:
        Pool(pool_id)

    # Configure Pools, Intel RDT (CAT, MBA)
    for pool_id in Pool.pools:
        result = Pool(pool_id).configure()
        if result != 0:
            return result

    # Configure Apps, core affinity
    result = Apps().configure()

    return result
