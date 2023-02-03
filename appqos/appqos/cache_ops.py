################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2023 Intel Corporation. All rights reserved.
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

from appqos import caps
from appqos import log
from appqos import power
from appqos.config_store import ConfigStore
from appqos.pqos_api import PQOS_API
from appqos.pid_ops import set_affinity

class Apps:
    """
    Apps options
    """
    @staticmethod
    def configure(config):
        """
        Configure Apps, based on config content.

        Parameters
            config: configuration
        """
        if 'apps' not in config:
            return 0

        # set pid affinity
        for app in config['apps']:
            if 'pids' not in app:
                continue

            app_cores = app['cores'] if 'cores' in app else []
            pool_id = config.app_to_pool(app['id'])
            pool_cores = config.get_pool_attr('cores', pool_id)

            # if there are no cores configured for App, or cores configured are
            # not a subset of Pool cores, revert to all Pool cores
            if not app_cores or not set(app_cores).issubset(pool_cores):
                app_cores = pool_cores

            if not app_cores:
                continue

            set_affinity(app['pids'], app_cores)

        return 0


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

    def l2cbm_set(self, l2cbm):
        """
        Set l2cbm mask for the pool

        Parameters:
            l2cbm: new l2cbm mask
        """
        Pool.pools[self.pool]['l2cbm'] = l2cbm

    def l2cbm_set_data(self, cbm):
        """
        Set l2 data mask for the pool

        Parameters:
            cbm: new cbm mask
        """
        Pool.pools[self.pool]['l2cbm_data'] = cbm

    def l2cbm_set_code(self, cbm):
        """
        Set l2 code mask for the pool

        Parameters:
            cbm: new cbm mask
        """
        Pool.pools[self.pool]['l2cbm_code'] = cbm

    def l3cbm_set(self, cbm):
        """
        Set cbm mask for the pool

        Parameters:
            cbm: new cbm mask
        """
        Pool.pools[self.pool]['l3cbm'] = cbm

    def l3cbm_set_data(self, cbm):
        """
        Set l3 data mask for the pool

        Parameters:
            cbm: new cbm mask
        """
        Pool.pools[self.pool]['l3cbm_data'] = cbm

    def l3cbm_set_code(self, cbm):
        """
        Set l3 code mask for the pool

        Parameters:
            cbm: new cbm mask
        """
        Pool.pools[self.pool]['l3cbm_code'] = cbm

    def l3cbm_get(self):
        """
        Get cbm mask for the pool

        Returns:
            cbm mask, 0 on error
        """
        return Pool.pools[self.pool].get('l3cbm')

    def l3cbm_get_data(self):
        """
        Get l3 data mask for the pool

        Returns:
            cbm mask, 0 on error
        """
        l3cbm_data = Pool.pools[self.pool].get('l3cbm_data', None)
        if l3cbm_data is not None:
            return l3cbm_data
        return self.l3cbm_get()

    def l3cbm_get_code(self):
        """
        Get l3 data mask for the pool

        Returns:
            cbm mask, 0 on error
        """
        l3cbm_code = Pool.pools[self.pool].get('l3cbm_code', None)
        if l3cbm_code is not None:
            return l3cbm_code
        return self.l3cbm_get()

    def l2cbm_get(self):
        """
        Get cbm mask for the pool
        Returns:
            cbm mask, 0 on error
        """
        return Pool.pools[self.pool].get('l2cbm')

    def l2cbm_get_data(self):
        """
        Get l2 data mask for the pool

        Returns:
            cbm mask, 0 on error
        """
        l2cbm_data = Pool.pools[self.pool].get('l2cbm_data', None)
        if l2cbm_data is not None:
            return l2cbm_data
        return self.l2cbm_get()

    def l2cbm_get_code(self):
        """
        Get l2 data mask for the pool

        Returns:
            cbm mask, 0 on error
        """
        l2cbm_code = Pool.pools[self.pool].get('l2cbm_code', None)
        if l2cbm_code is not None:
            return l2cbm_code
        return self.l2cbm_get()

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


    def mba_bw_set(self, mba_bw):
        """
        Set mba_bw value for the pool

        Parameters:
            mba_bw: new mba_bw value
        """
        Pool.pools[self.pool]['mba_bw'] = mba_bw


    def mba_bw_get(self):
        """
        Get mba_bw value for the pool

        Returns:
            mba_bw value, 0 on error
        """
        return Pool.pools[self.pool].get('mba_bw')


    def configure(self, config):
        """
        Configure Pool, based on config content.

        Parameters
            config: configuration
        """
        iface = config.get_rdt_iface()
        cores = config.get_pool_attr('cores', self.pool)
        self.cores_set(cores)

        if caps.cat_l2_supported(iface):
            l2cbm = config.get_pool_attr('l2cbm', self.pool)
            self.l2cbm_set(l2cbm)
            if config.get_l2cdp_enabled():
                self.l2cbm_set_data(config.get_pool_attr('l2cbm_data', self.pool))
                self.l2cbm_set_code(config.get_pool_attr('l2cbm_code', self.pool))
            else:
                self.l2cbm_set_data(None)
                self.l2cbm_set_code(None)
        else:
            self.l2cbm_set(None)
            self.l2cbm_set_data(None)
            self.l2cbm_set_code(None)

        if caps.cat_l3_supported(iface):
            l3cbm = config.get_pool_attr('l3cbm', self.pool)
            self.l3cbm_set(l3cbm)
            if config.get_l3cdp_enabled():
                self.l3cbm_set_data(config.get_pool_attr('l3cbm_data', self.pool))
                self.l3cbm_set_code(config.get_pool_attr('l3cbm_code', self.pool))
            else:
                self.l3cbm_set_data(None)
                self.l3cbm_set_code(None)
        else:
            self.l3cbm_set(None)
            self.l3cbm_set_data(None)
            self.l3cbm_set_code(None)

        if caps.mba_supported(iface):
            if caps.mba_bw_enabled():
                self.mba_bw_set(config.get_pool_attr('mba_bw', self.pool))
                self.mba_set(None)
            else:
                self.mba_bw_set(None)
                self.mba_set(config.get_pool_attr('mba', self.pool))
        else:
            self.mba_set(None)
            self.mba_bw_set(None)

        apps = config.get_pool_attr('apps', self.pool)
        if apps is not None:
            pids = []
            for app in apps:
                app_pids = config.get_app_attr('pids', app)
                if app_pids:
                    pids.extend(app_pids)

            self.pids_set(pids, config.get_pool_attr('cores', 0))

        return Pool.apply(self.pool)


    def pids_set(self, pids, default_cores):
        """
        Set Pool's PIDs

        Parameters:
            pids: Pool's PIDs
            default_cores: cores assigned to pool 0
        """
        old_pids = self.pids_get()

        # change core affinity for PIDs not assigned to any pool
        removed_pids = [pid for pid in old_pids if pid not in pids]

        # skip pids assigned to any pool e.g.: apps moved to other pool
        # pylint: disable=consider-using-dict-items
        for pool_id in Pool.pools:
            if pool_id == self.pool:
                continue
            removed_pids = \
                [pid for pid in removed_pids if pid not in Pool.pools[pool_id]['pids']]

        # remove pid form old pool
        if old_pids:
            # pylint: disable=consider-using-dict-items
            for pool_id in Pool.pools:
                if pool_id == self.pool:
                    continue

                pids_moved_from_other_pool = \
                    [pid for pid in Pool.pools[pool_id]['pids'] if pid in pids]

                if pids_moved_from_other_pool:
                    log.debug(f"PIDs moved from other pools {pids_moved_from_other_pool}")

                # update other pool PIDs
                Pool.pools[pool_id]['pids'] = \
                    [pid for pid in Pool.pools[pool_id]['pids'] if pid not in pids]

        Pool.pools[self.pool]['pids'] = pids

        # set affinity of removed pids to default
        if removed_pids:
            log.debug(f"PIDs to be set to core affinity to 'Default' CPUs {removed_pids}")

            # get cores for Default Pool #0
            set_affinity(removed_pids, default_cores)


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
        # pylint: disable=consider-using-dict-items
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
            log.debug(f"Cores assigned to COS#0 {removed_cores}")
            PQOS_API.release(removed_cores)

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
        # pylint: disable=too-many-return-statements
        # pylint: disable=too-many-branches
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

        l3cbm = pool.l3cbm_get()
        l3cbm_data = pool.l3cbm_get_data()
        l3cbm_code = pool.l3cbm_get_code()

        l2cbm = pool.l2cbm_get()
        l2cbm_data = pool.l2cbm_get_data()
        l2cbm_code = pool.l2cbm_get_code()

        mba = pool.mba_bw_get()
        if mba:
            ctrl = True
        else:
            ctrl = False
            mba = pool.mba_get()

        cores = pool.cores_get()

        # Apply same RDT configuration on all sockets in the system
        sockets = PQOS_API.get_sockets()
        if sockets is None:
            log.error("Failed to get sockets info!")
            return -1

        # pool id to COS, 1:1 mapping
        if PQOS_API.is_l2_cdp_enabled():
            if PQOS_API.l2ca_set(PQOS_API.get_l2ids(), pool_id, \
                                        code_mask=l2cbm_code, data_mask=l2cbm_data) != 0:
                log.error("Failed to apply L2 CDP configuration!")
                return -1
        elif l2cbm:
            if PQOS_API.l2ca_set(PQOS_API.get_l2ids(), pool_id, mask=l2cbm) != 0:
                log.error("Failed to apply L2 CAT configuration!")
                return -1

        if PQOS_API.is_l3_cdp_enabled():
            if PQOS_API.l3ca_set(sockets, pool_id, code_mask=l3cbm_code, \
                                        data_mask=l3cbm_data) != 0:
                log.error("Failed to apply L3 CDP configuration!")
                return -1
        elif l3cbm:
            if PQOS_API.l3ca_set(sockets, pool_id, mask=l3cbm) != 0:
                log.error("Failed to apply CAT configuration!")
                return -1

        if mba:
            if PQOS_API.mba_set(sockets, pool_id, mba, ctrl) != 0:
                log.error("Failed to apply MBA configuration!")
                return -1

        if cores:
            if PQOS_API.alloc_assoc_set(cores, pool_id) != 0:
                log.error("Failed to associate RDT COS!")
                return -1

        return 0


    @staticmethod
    def reset():
        """
        Reset pool configuration
        """
        Pool.pools = {}


def configure_rdt(cfg):
    """
    Configure RDT

    Parameters
        cfg: configuration

    Returns:
        0 on success
    """
    result = 0
    recreate_default = False

    cfg_rdt_iface = cfg.get_rdt_iface()

    def rdt_interface():
        """
        Change RDT interface if needed

        Returns:
            False interface not changed
        """
        if cfg_rdt_iface != PQOS_API.current_iface():
            if PQOS_API.init(cfg_rdt_iface):
                raise RuntimeError("Failed to initialize RDT interface!")

            log.info(f"RDT initialized with {cfg_rdt_iface.upper()} interface.")
            return True

        return False

    def rdt_reset():
        """
        Change RDT features enabled status

        Returns:
            False nothing changed
        """

        def get_mba_cfg():
            """
            Obtain MBA configuration
            """
            if caps.mba_supported(cfg_rdt_iface):
                cfg_mba_ctrl_enabled = cfg.get_mba_ctrl_enabled()
                # Change MBA BW/CTRL state if needed
                if cfg_mba_ctrl_enabled == PQOS_API.is_mba_bw_enabled():
                    return "any"

                if cfg_mba_ctrl_enabled:
                    return "ctrl"
                return "default"

            return "any"

        def get_l2cdp_cfg():
            """
            Obtain L2 CDP configuration
            """
            if caps.cat_l2_supported(cfg_rdt_iface) and caps.cdp_l2_supported(cfg_rdt_iface):
                cfg_l2cdp_enabled = cfg.get_l2cdp_enabled()
                # Change L2CDP state if needed
                if cfg_l2cdp_enabled == PQOS_API.is_l2_cdp_enabled():
                    return "any"

                if cfg_l2cdp_enabled:
                    return "on"
                return "off"

            return "any"

        def get_l3cdp_cfg():
            """
            Obtain L3 CDP configuration
            """
            if caps.cat_l3_supported(cfg_rdt_iface) and caps.cdp_l3_supported(cfg_rdt_iface):
                cfg_l3cdp_enabled = cfg.get_l3cdp_enabled()
                # Change L3CDP state if needed
                if cfg_l3cdp_enabled == PQOS_API.is_l3_cdp_enabled():
                    return "any"

                if cfg_l3cdp_enabled:
                    return "on"
                return "off"

            return "any"

        l3cdp_cfg = get_l3cdp_cfg()
        l2cdp_cfg = get_l2cdp_cfg()
        mba_cfg = get_mba_cfg()

        if l3cdp_cfg != "any" or l2cdp_cfg != "any" or mba_cfg != "any":
            if PQOS_API.reset(l3_cdp_cfg = l3cdp_cfg, l2_cdp_cfg = l2cdp_cfg, \
                                     mba_cfg = mba_cfg):
                if l3cdp_cfg != "any":
                    raise RuntimeError("Failed to change L3 CDP state!")
                if l2cdp_cfg != "any":
                    raise RuntimeError("Failed to change L2 CDP state!")
                if mba_cfg != "any":
                    raise RuntimeError("Failed to change MBA BW state!")

            log.info(f"RDT MBA BW {'en' if PQOS_API.is_mba_bw_enabled() else 'dis'}abled.")
            log.info(f"RDT L3 CDP {'en' if PQOS_API.is_l3_cdp_enabled() else 'dis'}abled.")
            log.info(f"RDT L2 CDP {'en' if PQOS_API.is_l2_cdp_enabled() else 'dis'}abled.")

    try:
        if rdt_interface():
            recreate_default = True
        if rdt_reset():
            recreate_default = True

    except Exception as ex:
        log.error(str(ex))
        return -1

    if recreate_default:
        # On interface or MBA BW state change it is needed to recreate Default Pool #0
        ConfigStore().recreate_default_pool()

    # detect removed pools
    old_pools = Pool.pools.copy()

    pool_ids = cfg.get_pool_attr('id', None)

    if old_pools:
        for pool_id in old_pools:
            if not pool_ids or pool_id not in pool_ids:
                log.debug(f"Pool {pool_id} removed...")
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
        result = Pool(pool_id).configure(cfg)
        if result != 0:
            return result

    # Configure Apps, core affinity
    result = Apps().configure(cfg)

    return result
