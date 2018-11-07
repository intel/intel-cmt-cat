################################################################################
# BSD LICENSE
#
# Copyright(c) 2018 Intel Corporation. All rights reserved.
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

from cache_ops import *
from flusher import *

class TestCacheOps(object):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self):
        Pool.pools= {}
    ## @endcond


    def test_enabled_get(self):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['enabled'] = True
        Pool.pools[Pool.Cos.P] = {}
        Pool.pools[Pool.Cos.P]['enabled'] = False

        assert Pool(Pool.Cos.SYS).enabled_get()
        assert not Pool(Pool.Cos.P).enabled_get()
        assert not Pool(Pool.Cos.PP).enabled_get()


    @mock.patch('cache_ops.Pool.apply')
    @pytest.mark.parametrize("pool_id", [Pool.Cos.SYS, Pool.Cos.P])
    def test_enabled_set_disable(self, mock_apply, pool_id):
        Pool.pools[pool_id] = {}
        Pool.pools[pool_id]['enabled'] = True
        Pool.pools[pool_id]['cbm_bits_min'] = 2

        Pool(pool_id).enabled_set(False)

        mock_apply.assert_called_once()
        if pool_id == Pool.Cos.SYS:
            mock_apply.assert_called_once_with(pool_id)
        else:
            mock_apply.assert_called_once_with([Pool.Cos.P, Pool.Cos.PP, Pool.Cos.BE])
        assert not Pool.pools[pool_id]['enabled']
        assert Pool.pools[pool_id]['cbm_bits'] == Pool.pools[pool_id]['cbm_bits_min']


    @mock.patch('cache_ops.Pool.apply')
    @mock.patch('cache_ops.Pool.cbm_regenerate')
    @pytest.mark.parametrize("pool_id", [Pool.Cos.SYS, Pool.Cos.P])
    def test_enabled_set_enable(self, mock_apply, mock_regenerate, pool_id):
        Pool.pools[pool_id] = {}
        Pool.pools[pool_id]['enabled'] = False
        Pool.pools[pool_id]['cbm_bits_min'] = 2

        Pool(pool_id).enabled_set(True)

        mock_apply.assert_called_once()
        mock_regenerate.assert_called_once()
        assert Pool.pools[pool_id]['enabled']


    def test_cbm_set_min_bits(self):
        Pool(Pool.Cos.SYS).cbm_set_min_bits(2)
        assert 'cbm_bits_min' in Pool.pools[Pool.Cos.SYS]
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits_min'] == 2

        Pool(Pool.Cos.P).cbm_set_min_bits(1)
        assert 'cbm_bits_min' in Pool.pools[Pool.Cos.P]
        assert Pool.pools[Pool.Cos.P]['cbm_bits_min'] == 1
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits_min'] == 2


    def test_cbm_get_min_bits(self):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['cbm_bits_min'] = 2
        Pool.pools[Pool.Cos.P] = {}

        assert Pool(Pool.Cos.SYS).cbm_get_min_bits() == 2
        assert Pool(Pool.Cos.P).cbm_get_min_bits() == 0
        assert Pool(Pool.Cos.PP).cbm_get_min_bits() == 0


    @mock.patch('cache_ops.Pool.flush_cache')
    def test_cbm_set(self, mock_flush_cache):
        Pool(Pool.Cos.SYS).cbm_set(0xf)
        assert Pool.pools[Pool.Cos.SYS]['cbm'] == 0xf
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits'] == 4

        Pool(Pool.Cos.SYS).cbm_set(0xff, True)
        assert Pool.pools[Pool.Cos.SYS]['cbm'] == 0xff
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits'] == 8
        mock_flush_cache.assert_called_once()



    def test_cbm_get(self):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['cbm'] = 0xf
        Pool.pools[Pool.Cos.P] = {}

        assert Pool(Pool.Cos.SYS).cbm_get() == 0xf
        assert Pool(Pool.Cos.P).cbm_get() == 0
        assert Pool(Pool.Cos.PP).cbm_get() == 0


    @mock.patch('cache_ops.Pool.flush_cache')
    def test_cbm_set_bits(self, mock_flush_cache):
        Pool(Pool.Cos.SYS).cbm_set_bits(4)
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits'] == 4

        Pool(Pool.Cos.SYS).cbm_set_bits(8, True)
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits'] == 8
        mock_flush_cache.assert_called_once()


    def test_cbm_get_bits(self):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['cbm_bits'] = 3
        Pool.pools[Pool.Cos.BE] = {}
        Pool.pools[Pool.Cos.BE]['cbm_bits'] = 2
        Pool.pools[Pool.Cos.P] = {}

        assert Pool(Pool.Cos.SYS).cbm_get_bits() == 3
        assert Pool(Pool.Cos.BE).cbm_get_bits() == 2
        assert Pool(Pool.Cos.P).cbm_get_bits() == 0
        assert Pool(Pool.Cos.PP).cbm_get_bits() == 0


    def test_cbm_set_max(self):
        Pool(Pool.Cos.SYS).cbm_set_max(0xff)
        assert 'cbm_max' in Pool.pools[Pool.Cos.SYS]
        assert Pool.pools[Pool.Cos.SYS]['cbm_max'] == 0xff
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits_max'] == 8


    def test_cbm_get_max(self):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['cbm_max'] = 0xf
        Pool.pools[Pool.Cos.P] = {}

        assert Pool(Pool.Cos.SYS).cbm_get_max() == 0xf
        assert Pool(Pool.Cos.P).cbm_get_max() == 0
        assert Pool(Pool.Cos.PP).cbm_get_max() == 0


    def test_cbm_get_max_bits(self):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['cbm_max'] = 0xf
        Pool.pools[Pool.Cos.P] = {}
        Pool.pools[Pool.Cos.BE] = {}
        Pool.pools[Pool.Cos.BE]['cbm_bits_max'] = 5

        assert Pool(Pool.Cos.SYS).cbm_get_max_bits() == 4
        assert Pool(Pool.Cos.P).cbm_get_max_bits() == 0
        assert Pool(Pool.Cos.PP).cbm_get_max_bits() == 0
        assert Pool(Pool.Cos.BE).cbm_get_max_bits() == 5


    def test_cbm_regenerate(self):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['cbm_max'] = 0xc00
        Pool.pools[Pool.Cos.SYS]['cbm_bits_min'] = 2
        Pool.pools[Pool.Cos.SYS]['enabled'] = True

        Pool.cbm_regenerate(Pool.Cos.SYS)
        assert Pool.pools[Pool.Cos.SYS]['cbm'] == 0xc00
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits'] == 2

        Pool.pools[Pool.Cos.P] = {}
        Pool.pools[Pool.Cos.P]['cbm_max'] = 0x3ff
        Pool.pools[Pool.Cos.P]['cbm_bits_min'] = 2
        Pool.pools[Pool.Cos.P]['cbm_bits'] = 3
        Pool.pools[Pool.Cos.P]['enabled'] = True
        Pool.cbm_regenerate(Pool.Cos.P)
        assert Pool.pools[Pool.Cos.SYS]['cbm'] == 0xc00
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits'] == 2
        assert Pool.pools[Pool.Cos.P]['cbm'] == 0x380
        assert Pool.pools[Pool.Cos.P]['cbm_bits'] == 3

        Pool.pools[Pool.Cos.PP] = {}
        Pool.pools[Pool.Cos.PP]['cbm_max'] = 0x3ff
        Pool.pools[Pool.Cos.PP]['cbm_bits_min'] = 2
        Pool.pools[Pool.Cos.PP]['cbm_bits'] = 4
        Pool.pools[Pool.Cos.PP]['enabled'] = True
        Pool.cbm_regenerate(Pool.Cos.PP)
        assert Pool.pools[Pool.Cos.SYS]['cbm'] == 0xc00
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits'] == 2
        assert Pool.pools[Pool.Cos.P]['cbm'] == 0x380
        assert Pool.pools[Pool.Cos.P]['cbm_bits'] == 3
        assert Pool.pools[Pool.Cos.PP]['cbm'] == 0xf
        assert Pool.pools[Pool.Cos.PP]['cbm_bits'] == 4

        Pool.pools[Pool.Cos.BE] = {}
        Pool.pools[Pool.Cos.BE]['cbm_max'] = 0x3ff
        Pool.pools[Pool.Cos.BE]['cbm_bits_min'] = 2
        Pool.pools[Pool.Cos.BE]['cbm_bits'] = 2
        Pool.pools[Pool.Cos.BE]['enabled'] = True
        Pool.cbm_regenerate(Pool.Cos.BE)
        assert Pool.pools[Pool.Cos.SYS]['cbm'] == 0xc00
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits'] == 2
        assert Pool.pools[Pool.Cos.P]['cbm'] == 0x380
        assert Pool.pools[Pool.Cos.P]['cbm_bits'] == 3
        assert Pool.pools[Pool.Cos.PP]['cbm'] == 0xf
        assert Pool.pools[Pool.Cos.PP]['cbm_bits'] == 4
        assert Pool.pools[Pool.Cos.BE]['cbm_bits'] == 2
        assert Pool.pools[Pool.Cos.BE]['cbm'] == 0x30

        Pool.pools[Pool.Cos.BE]['cbm_bits'] = 6
        Pool.cbm_regenerate(Pool.Cos.BE)
        assert Pool.pools[Pool.Cos.SYS]['cbm'] == 0xc00
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits'] == 2
        assert Pool.pools[Pool.Cos.P]['cbm'] == 0x300
        assert Pool.pools[Pool.Cos.P]['cbm_bits'] == 2
        assert Pool.pools[Pool.Cos.PP]['cbm'] == 0x3
        assert Pool.pools[Pool.Cos.PP]['cbm_bits'] == 2
        assert Pool.pools[Pool.Cos.BE]['cbm'] == 0xfc
        assert Pool.pools[Pool.Cos.BE]['cbm_bits'] == 6


    def test_flush_cache_disabled(self, caplog):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['enabled'] = False

        Pool(Pool.Cos.SYS).flush_cache()
        assert 'Unable to flush pool' in caplog.text


    def test_flush_cache_sys(self, caplog):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['enabled'] = True

        Pool(Pool.Cos.SYS).flush_cache()
        assert 'Only P, PP and BE pools are flushable, Unable to flush pool' in caplog.text


    @pytest.mark.parametrize("pool_id", [Pool.Cos.P, Pool.Cos.PP, Pool.Cos.BE])
    def test_flush_cache_no_pids(self, pool_id, caplog):
        Pool.pools[pool_id] = {}
        Pool.pools[pool_id]['enabled'] = True
        Pool.pools[pool_id]['pids'] = []

        Pool(pool_id).flush_cache()
        assert 'No PIDs configured, Unable to flush pool' in caplog.text


    @mock.patch('flusher.FlusherProcess.flush')
    @pytest.mark.parametrize("pool_id", [Pool.Cos.P, Pool.Cos.PP, Pool.Cos.BE])
    def test_flush_cache(self, mock_flush, pool_id):
        Pool.pools[pool_id] = {}
        Pool.pools[pool_id]['enabled'] = True
        Pool.pools[pool_id]['pids'] = [1]

        Pool(pool_id).flush_cache()
        mock_flush.assert_called_once_with([1])


    def test_cores_set_disabled(self):
        Pool(Pool.Cos.SYS).cores_set([1])
        assert Pool.pools[Pool.Cos.SYS]['cores'] == [1]

        Pool(Pool.Cos.SYS).cores_set([2])
        assert Pool.pools[Pool.Cos.SYS]['cores'] == [2]


    @mock.patch('cache_ops.Pqos.assign')
    def test_cores_set_enabled(self, mock_apply):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['enabled'] = True
        Pool.pools[Pool.Cos.SYS]['cores'] = []

        Pool(Pool.Cos.SYS).cores_set([1])
        assert Pool.pools[Pool.Cos.SYS]['cores'] == [1]
        mock_apply.assert_called_once_with(Pool.Cos.SYS, [1])


    @mock.patch('flusher.FlusherProcess.flush')
    def test_pids_move_flush(self, mock_flush):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['enabled'] = True
        Pool.pools[Pool.Cos.SYS]['cores'] = [1]
        Pool.pools[Pool.Cos.SYS]['pids'] = [11]

        Pool.pools[Pool.Cos.P] = {}
        Pool.pools[Pool.Cos.P]['enabled'] = True
        Pool.pools[Pool.Cos.P]['cores'] = [2]
        Pool.pools[Pool.Cos.P]['pids'] = [22]

        Pool(Pool.Cos.P).pids_set([11, 22])
        assert Pool.pools[Pool.Cos.P]['pids'] == [11, 22]
        assert Pool.pools[Pool.Cos.SYS]['pids'] == []
        mock_flush.assert_called_once_with([11])


    @mock.patch('flusher.FlusherProcess.flush')
    def test_pids_move_no_flush(self, mock_flush):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['enabled'] = True
        Pool.pools[Pool.Cos.SYS]['cores'] = [1]
        Pool.pools[Pool.Cos.SYS]['pids'] = [11]

        Pool.pools[Pool.Cos.P] = {}
        Pool.pools[Pool.Cos.P]['enabled'] = True
        Pool.pools[Pool.Cos.P]['cores'] = [2]
        Pool.pools[Pool.Cos.P]['pids'] = [22]

        Pool(Pool.Cos.SYS).pids_set([11, 22])
        assert Pool.pools[Pool.Cos.SYS]['pids'] == [11, 22]
        assert Pool.pools[Pool.Cos.P]['pids'] == []
        mock_flush.assert_not_called()


    @mock.patch('flusher.FlusherProcess.flush')
    def test_pids_remove_flush(self, mock_flush):
        Pool.pools[Pool.Cos.P] = {}
        Pool.pools[Pool.Cos.P]['enabled'] = True
        Pool.pools[Pool.Cos.P]['cores'] = [1]
        Pool.pools[Pool.Cos.P]['pids'] = [11]

        Pool(Pool.Cos.P).pids_set([])
        assert Pool.pools[Pool.Cos.P]['pids'] == []
        mock_flush.assert_called_once_with([11])


    def test_apply_not_configured(self, caplog):
        Pool.apply(Pool.Cos.SYS)
        assert 'Nothing to configure!' in caplog.text


    @mock.patch('cache_ops.Pqos.run')
    def test_apply(self, mock_pqos):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['enabled'] = True
        Pool.pools[Pool.Cos.SYS]['cores'] = [1]
        Pool.pools[Pool.Cos.SYS]['cbm'] = 0xc00

        Pool.pools[Pool.Cos.P] = {}
        Pool.pools[Pool.Cos.P]['enabled'] = False
        Pool.pools[Pool.Cos.P]['cores'] = [2]
        Pool.pools[Pool.Cos.P]['cbm'] = 0x300

        mock_pqos.return_value = 0, "", ""

        Pool.apply([Pool.Cos.SYS, Pool.Cos.P])
        mock_pqos.assert_called_once_with(" -a 'llc:3=1;llc:0=2;' -e 'llc:3=0xc00;'")


    def test_reset(self):
        Pool.pools[Pool.Cos.SYS] = {}
        Pool.pools[Pool.Cos.SYS]['enabled'] = True
        Pool.pools[Pool.Cos.SYS]['cores'] = [1]
        Pool.pools[Pool.Cos.SYS]['cbm'] = 0xc00

        Pool.reset()
        assert Pool.Cos.SYS not in Pool.pools


    @mock.patch('cache_ops.Pqos.run')
    def test_configure_cat(self, mock_pqos):
        def config(field, group, pool):
            if field == 'min_cws':
                return 2

            if field == 'cores':
                if group == common.TYPE_STATIC:
                    return [1]
                if group == common.TYPE_DYNAMIC and pool == common.PRODUCTION:
                    return [2]
                if group == common.TYPE_DYNAMIC and pool == common.PRE_PRODUCTION:
                    return [3]
                if group == common.TYPE_DYNAMIC and pool == common.BEST_EFFORT:
                    return [4]
                return None

            if field == 'cbm':
                if group == common.TYPE_STATIC:
                    return '0xc00'
                if group == common.TYPE_DYNAMIC:
                    return '0x3ff'

        mock_pqos.return_value = 0, "", ""

        with mock.patch('common.CONFIG_STORE.get_attr_list', new=config):
            configure_cat()

        assert Pool.pools[Pool.Cos.SYS]['enabled'] == True
        assert Pool.pools[Pool.Cos.P]['enabled'] == True
        assert Pool.pools[Pool.Cos.PP]['enabled'] == True
        assert Pool.pools[Pool.Cos.BE]['enabled'] == True
        assert Pool.pools[Pool.Cos.SYS]['cores'] == [1]
        assert Pool.pools[Pool.Cos.P]['cores'] == [2]
        assert Pool.pools[Pool.Cos.PP]['cores'] == [3]
        assert Pool.pools[Pool.Cos.BE]['cores'] == [4]
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits_min'] == 2
        assert Pool.pools[Pool.Cos.P]['cbm_bits_min'] == 2
        assert Pool.pools[Pool.Cos.PP]['cbm_bits_min'] == 2
        assert Pool.pools[Pool.Cos.BE]['cbm_bits_min'] == 2
        assert Pool.pools[Pool.Cos.SYS]['cbm_bits'] == 2
        assert Pool.pools[Pool.Cos.P]['cbm_bits'] == 2
        assert Pool.pools[Pool.Cos.PP]['cbm_bits'] == 2
        assert Pool.pools[Pool.Cos.BE]['cbm_bits'] == 2
