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
used to configure CAT and flush cache.
"""

import pexpect

import common
import flusher
import log


__CW_SIZE__ = None


class Pqos(object):
    """
    Wrapper for "pqos" tool.
    Uses "pqos" tool to assign Cores to COS
    """


    @staticmethod
    def run(params):
        """
        Executes "pqos" tool

        Parameters:
            params: "pqos" tool params

        Returns:
            exitstatus: exit status/code
            output: command output
            cmd: executed command
         """
        cmd = "pqos-os %s" % (params)
        (output, exitstatus) = pexpect.run(cmd, withexitstatus=True)
        return exitstatus, output, cmd


    def assign(self, cos, cores):
        """
        Assigns cores to CoS

        Parameters:
            cos: Class of Service
            cores: list of cores to be assigned to cos

        Returns:
            Value returned by run(...) method
         """
        if not cores:
            return 0

        cmd = "-a llc:{}={};".format(cos, ",".join(str(core) for core in cores))
        return self.run(cmd)


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
        SYS = 3
        P = 1
        PP = 2
        BE = 0


    class Priority(object):
        #pylint: disable=too-few-public-methods, invalid-name
        """
        Priority enum
        """
        SYS = 3
        P = 2
        PP = 1
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
            Pool.Cos.SYS: Pool.Priority.SYS,
            Pool.Cos.P: Pool.Priority.P,
            Pool.Cos.PP: Pool.Priority.PP,
            Pool.Cos.BE: Pool.Priority.BE
        }[cos]


    def get_common_type(self):
        """
        Get pool type

        Returns:
            Pool type
        """

        if self.pool == Pool.Cos.SYS:
            return common.TYPE_STATIC
        return common.TYPE_DYNAMIC


    def get_common_priority(self):
        """
        Get pool priority

        Returns:
            Pool priority
        """
        if self.pool == Pool.Cos.P or self.pool == Pool.Cos.SYS:
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
        return common.CONFIG_STORE.get_pool_id(self.get_common_type(), self.get_common_priority())


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

        if enabled:
            Pool.cbm_regenerate(self.pool)
        else:
            Pool.pools[self.pool]['cbm_bits'] = Pool.pools[self.pool]['cbm_bits_min']

        #pylint: disable=no-else-return
        if self.pool == Pool.Cos.P or self.pool == Pool.Cos.BE or self.pool == Pool.Cos.PP:
            return Pool.apply([Pool.Cos.P, Pool.Cos.PP, Pool.Cos.BE])
        else:
            return Pool.apply(self.pool)


    def cbm_set_min_bits(self, cbm_bits_min):
        """
        Set min enabled cbm bits for the pool

        Parameters:
            cbm_bits_min: min number of bits enabled in cbm
        """
        Pool.pools[self.pool]['cbm_bits_min'] = cbm_bits_min


    def cbm_get_min_bits(self):
        """
        Get min enabled cbm bits for the pool

        Returns:
            Min cbm bit allocated for CBM, 0 on error
        """
        return Pool.pools[self.pool].get('cbm_bits_min', 0)


    def cbm_set(self, cbm, flush=False):
        """
        Set cbm mask for the pool

        Parameters:
            cbm: new cbm mask
            flush(bool): flush pool cache
        """
        Pool.pools[self.pool]['cbm'] = cbm
        Pool.pools[self.pool]['cbm_bits'] = bin(cbm).count('1')

        if flush:
            self.flush_cache()


    def cbm_get(self):
        """
        Get cbm mask for the pool

        Returns:
            cbm mask, 0 on error
        """
        return Pool.pools[self.pool].get('cbm', 0)


    def cbm_set_bits(self, bits, flush=False):
        """
        Set number of bits set in CBM

        Parameters:
            bits: number of bits enables in cbm mask
            flush(bool): flush pool cache
        """
        Pool.pools[self.pool]['cbm_bits'] = bits

        if flush:
            self.flush_cache()


    def cbm_get_bits(self):
        """
        Get number of bits set in CBM

        Returns:
            bits set in cbm mask, 0 on error
        """
        return Pool.pools[self.pool].get('cbm_bits', 0)


    def cbm_set_max(self, cbm):
        """
        Set max cbm mask

        Parameters:
            cbm: max cbm mask
        """
        Pool.pools[self.pool]['cbm_max'] = cbm
        Pool.pools[self.pool]['cbm_bits_max'] = bin(cbm).count('1')


    def cbm_get_max(self):
        """
        Get max cbm mask

        Returns:
            max cbm mask for the pool, 0 on error
        """
        return Pool.pools[self.pool].get('cbm_max', 0)


    def cbm_get_max_bits(self):
        """
        Get max bits set in cbm mask

        Returns:
            max bits set cbm mask for the pool
        """
        if 'cbm_bits_max' not in Pool.pools[self.pool]:
            return bin(self.cbm_get_max()).count('1')

        return Pool.pools[self.pool]['cbm_bits_max']


    @staticmethod
    def cbm_regenerate(cos):
        """
        Regenerate Pool's CBM mask
        The following partitioning is assumed
        [SYS][---P---][---BE---][---PP---]

        Parameters:
            cos: Class of Service/Pool type
        """

        def get_last_bits(cbm, count):
            """
            Get "count" bits of "cbm" starting from LSB

            Parameters:
                cbm: CBM to get bits of
                count: number of bit to get

            Returns:
                result CBM
            """
            if count == 0:
                return 0

            mask = 0
            bit = 1
            while bin(mask).count('1') < count and bit <= cbm:
                mask |= (cbm & bit)
                bit = bit << 1
            return mask

        def get_first_bits(cbm, count):
            """
            Get "count" bits of "cbm" starting from MSB

            Parameters:
                cbm: CBM to get bits of
                count: number of bit to get

            Returns:
                result CBM
            """
            if count == 0:
                return 0

            mask = cbm
            bit = 0
            while bin(mask).count('1') > count:
                mask = mask >> 1
                bit += 1
            return mask << bit

        sys_cbm = 0
        p_cbm = 0
        p_cbm_max = 0
        p_cbm_min = 0
        p_cbm_bits = 0
        p_cbm_bits_min = 0
        pp_cbm = 0
        pp_cbm_max = 0
        pp_cbm_min = 0
        pp_cbm_bits = 0
        pp_cbm_bits_min = 0
        be_cbm = 0
        be_cbm_max = 0
        be_cbm_bits = 0
        be_cbm_bits_min = 0

        pool = Pool(Pool.Cos.SYS)
        if pool.enabled_get():
            sys_cbm = pool.cbm_get()
            sys_cbm_min = get_first_bits(pool.cbm_get_max(), pool.cbm_get_min_bits())

        pool = Pool(Pool.Cos.P)
        if pool.enabled_get():
            p_cbm_bits_min = pool.cbm_get_min_bits()
            p_cbm_bits = max(pool.cbm_get_bits(), p_cbm_bits_min)
            p_cbm_min = get_first_bits(pool.cbm_get_max(), p_cbm_bits_min)
            p_cbm_max = pool.cbm_get_max()
            p_cbm = get_first_bits(p_cbm_max, p_cbm_bits) | p_cbm_min

        pool = Pool(Pool.Cos.PP)
        if pool.enabled_get():
            pp_cbm_bits_min = pool.cbm_get_min_bits()
            pp_cbm_bits = max(pool.cbm_get_bits(), pp_cbm_bits_min)
            pp_cbm_min = get_last_bits(pool.cbm_get_max(), pp_cbm_bits_min)
            pp_cbm_max = pool.cbm_get_max()
            pp_cbm = get_last_bits(pp_cbm_max, pp_cbm_bits) | pp_cbm_min

        pool = Pool(Pool.Cos.BE)
        if pool.enabled_get():
            be_cbm_bits_min = pool.cbm_get_min_bits()
            be_cbm_bits = max(pool.cbm_get_bits(), be_cbm_bits_min)
            be_cbm_max = pool.cbm_get_max()

        if cos == Pool.Cos.SYS:
            sys_cbm |= sys_cbm_min

        if cos == Pool.Cos.P:
            be_cbm_min = get_last_bits(be_cbm_max & (~pp_cbm_min), be_cbm_bits_min)
            p_cbm &= ~(be_cbm_min | sys_cbm | pp_cbm_min) & p_cbm_max

            be_cbm_min = get_first_bits(be_cbm_max & (~p_cbm), be_cbm_bits_min)
            pp_cbm &= ~(be_cbm_min | sys_cbm | p_cbm) & pp_cbm_max

            be_cbm = get_last_bits(~(sys_cbm | p_cbm | pp_cbm) & be_cbm_max, be_cbm_bits)

        elif cos == Pool.Cos.PP:
            be_cbm_min = get_first_bits(be_cbm_max & (~p_cbm_min), be_cbm_bits_min)
            pp_cbm &= ~(be_cbm_min | sys_cbm | p_cbm_min) & pp_cbm_max

            be_cbm_min = get_last_bits(be_cbm_max & (~pp_cbm), be_cbm_bits_min)
            p_cbm &= ~(be_cbm_min | sys_cbm | pp_cbm) & p_cbm_max

            be_cbm = get_last_bits(~(sys_cbm | p_cbm | pp_cbm) & be_cbm_max, be_cbm_bits)

        elif cos == Pool.Cos.BE:
            be_cbm = get_last_bits(~(p_cbm | pp_cbm | sys_cbm) & be_cbm_max, be_cbm_bits)
            #get bits from PP
            be_bits = bin(be_cbm).count('1')
            if be_bits < be_cbm_bits:
                be_cbm |= get_first_bits(pp_cbm & (~pp_cbm_min), be_cbm_bits - be_bits)
            #get bits from P
            be_bits = bin(be_cbm).count('1')
            if be_bits < be_cbm_bits:
                be_cbm |= get_last_bits(p_cbm & (~p_cbm_min), be_cbm_bits - be_bits)
            p_cbm &= ~(sys_cbm | pp_cbm_min | be_cbm) & p_cbm_max
            pp_cbm &= ~(sys_cbm | p_cbm | be_cbm)  & pp_cbm_max

        pool = Pool(Pool.Cos.SYS)
        if pool.enabled_get():
            pool.cbm_set(sys_cbm)

        pool = Pool(Pool.Cos.P)
        if pool.enabled_get():
            pool.cbm_set(p_cbm)

        pool = Pool(Pool.Cos.PP)
        if pool.enabled_get():
            pool.cbm_set(pp_cbm)

        pool = Pool(Pool.Cos.BE)
        if pool.enabled_get():
            pool.cbm_set(be_cbm)


    def configure(self):
        """
        Configure Pool, based on config file content.
        """
        cores = common.CONFIG_STORE.get_attr_list('cores', self.get_common_type(),
                                                  self.get_common_priority())
        pids = common.CONFIG_STORE.get_attr_list('pids', self.get_common_type(),
                                                 self.get_common_priority())
        min_llc = common.CONFIG_STORE.get_attr_list('min_cws', self.get_common_type(),
                                                    self.get_common_priority())
        max_cbm = common.CONFIG_STORE.get_attr_list('cbm', self.get_common_type(), None)

        self.cbm_set_min_bits(int(min_llc))
        self.cbm_set_max(int(max_cbm, 16))
        self.cores_set(cores)
        self.pids_set(pids)
        return self.enabled_set(True)


    def flush_cache(self):
        """
        Flush Pool's cache
        """
        if not self.enabled_get():
            log.info('Unable to flush pool {} !'.format(self.pool))
            return

        if self.pool == Pool.Cos.SYS:
            log.info("Only P, PP and BE pools are flushable, Unable to flush pool")
            return

        if self.pids_get():
            flusher.FlusherProcess.flush(self.pids_get())
        else:
            log.info('No PIDs configured, Unable to flush pool {} !'.format(self.pool))
            return


    def pids_set(self, pids):
        """
        Set Pool's PIDs

        Parameters:
            pids: Pool's PIDs
        """
        old_pids = self.pids_get()

        # flush pids not assigned to any pool
        if self.enabled_get() and not self.pool == Pool.Cos.BE:
            removed_pids = [pid for pid in old_pids if pid not in pids]

            # skip pids assigned to any pool
            for cos in Pool.pools:
                if cos == self.pool:
                    continue
                removed_pids = \
                    [pid for pid in removed_pids if pid not in Pool.pools[cos]['pids']]

            if removed_pids:
                flusher.FlusherProcess.flush(removed_pids)

        # remove pid form old pool
        if old_pids:
            for cos in Pool.pools:
                if cos == self.pool:
                    continue

                # flush cache for pids that were assigned to pools with higher priority
                if self.priority_get() < self.priority_get(cos):
                    pids_to_flush = [pid for pid in Pool.pools[cos]['pids'] if pid in pids]
                    if pids_to_flush:
                        flusher.FlusherProcess.flush(pids_to_flush)

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
                Pqos().assign(0, removed_cores)

        Pqos().assign(self.pool if self.enabled_get() else 0, cores)


    @staticmethod
    def apply(pools):
        """
        Apply CAT configuration for Pools

        Parameters:
            pools: Pools to apply CAT config for

        Returns:
            0 on success
        """
        if isinstance(pools, list):
            _pools = pools
        else:
            _pools = [pools]

        classdef = ""
        class2id = ""
        cat = common.STATS_STORE.get_pool_cat()

        # generate command line params for pqos tool
        for item in _pools:
            if item not in Pool.pools:
                continue
            pool = Pool(item)
            if pool.enabled_get():
                cbm = pool.cbm_get()
                bits = pool.cbm_get_bits()
                cos = item
            else:
                cos = 0
                cbm = Pool(Pool.Cos.BE).cbm_get()
                bits = Pool(Pool.Cos.BE).cbm_get_bits()

            if cbm > 0:
                classdef += "llc:{}={};".format(item, hex(cbm))
            if 'cores' in Pool.pools[item] and Pool.pools[item]['cores']:
                class2id += "llc:{}={};".format(
                    cos, ",".join(str(core) for core in Pool.pools[item]['cores']))

            pool_id = pool.get_pool_id()
            if pool_id:
                if pool_id not in cat:
                    cat[pool_id] = {}
                cat[pool_id]['cws'] = bits

        common.STATS_STORE.set_pool_cat(cat)

        # generate command to be executed
        pqos_args = ""
        if class2id:
            pqos_args += " -a '{}'".format(class2id)
        if classdef:
            pqos_args += " -e '{}'".format(classdef)

        # execute command/pqos
        if class2id or classdef:
            result, output, cmd = Pqos().run(pqos_args)
            if result != 0:
                log.error("{} failed!\n{}".format(cmd, output))

            return result
        else:
            log.info("Nothing to configure!")
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
    for cos in [Pool.Cos.SYS, Pool.Cos.P, Pool.Cos.PP, Pool.Cos.BE]:
        result = Pool(cos).configure()
        if result != 0:
            break

    return result


def detect_cw_size():
    """
    Detect single CacheWay's size
    """
    global __CW_SIZE__
    __CW_SIZE__ = None

    result, output, cmd = Pqos().run("-D")

    if result != 0:
        log.error("{} failed!\n{}".format(cmd, output))
        return

    if "Cache information" not in output:
        return

    cache_info = output.split("Cache information")[1]
    if "L3 Cache" not in cache_info:
        return

    cw_size_bytes = int(cache_info.split("Way size:")[1].split('bytes')[0].strip())
    __CW_SIZE__ = int(round(cw_size_bytes / 1024, 0))


def get_cw_size():
    """
    Get single CacheWay's size

    Returns:
        CW size, None on error
    """
    if not __CW_SIZE__:
        detect_cw_size()

    return __CW_SIZE__
