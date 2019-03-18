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

import pytest
import mock

from cache_ops import *

class TestCacheOps(object):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self):
        Pool.pools= {}
    ## @endcond


    def test_enabled_get(self):
        Pool.pools[Pool.Cos.PP] = {}
        Pool.pools[Pool.Cos.PP]['enabled'] = True
        Pool.pools[Pool.Cos.P] = {}
        Pool.pools[Pool.Cos.P]['enabled'] = False

        assert Pool(Pool.Cos.PP).enabled_get()
        assert not Pool(Pool.Cos.P).enabled_get()
        assert not Pool(Pool.Cos.BE).enabled_get()


    @mock.patch('cache_ops.Pool.apply')
    @pytest.mark.parametrize("pool_id", [Pool.Cos.PP, Pool.Cos.P])
    def test_enabled_set_enable(self, mock_apply, pool_id):
        Pool.pools[pool_id] = {}
        Pool.pools[pool_id]['enabled'] = False
        Pool.pools[pool_id]['cbm_bits_min'] = 2

        Pool(pool_id).enabled_set(True)

        mock_apply.assert_called_once()
        assert Pool.pools[pool_id]['enabled']


    def test_cbm_get(self):
        Pool.pools[Pool.Cos.BE] = {}
        Pool.pools[Pool.Cos.BE]['cbm'] = 0xf
        Pool.pools[Pool.Cos.P] = {}

        assert Pool(Pool.Cos.BE).cbm_get() == 0xf
        assert Pool(Pool.Cos.P).cbm_get() == 0
        assert Pool(Pool.Cos.PP).cbm_get() == 0


    def test_cores_set_disabled(self):
        Pool(Pool.Cos.P).cores_set([1])
        assert Pool.pools[Pool.Cos.P]['cores'] == [1]

        Pool(Pool.Cos.P).cores_set([2])
        assert Pool.pools[Pool.Cos.P]['cores'] == [2]


    @mock.patch('cache_ops.PQOS_API.alloc_assoc_set')
    def test_cores_set_enabled(self, mock_alloc_assoc_set):
        Pool.pools[Pool.Cos.P] = {}
        Pool.pools[Pool.Cos.P]['enabled'] = True
        Pool.pools[Pool.Cos.P]['cores'] = []

        Pool(Pool.Cos.P).cores_set([1])
        assert Pool.pools[Pool.Cos.P]['cores'] == [1]
        mock_alloc_assoc_set.assert_called_once_with([1], Pool.Cos.P)


    @mock.patch('cache_ops.PQOS_API.l3ca_set')
    @mock.patch('cache_ops.PQOS_API.alloc_assoc_set')
    def test_apply_not_configured(self, mock_l3ca_set, mock_alloc_assoc_set):
        result = Pool.apply(Pool.Cos.P)

        assert result == 0

        mock_l3ca_set.assert_not_called()
        mock_alloc_assoc_set.assert_not_called()


    @mock.patch('cache_ops.PQOS_API.l3ca_set')
    @mock.patch('cache_ops.PQOS_API.alloc_assoc_set')
    def test_apply(self, mock_alloc_assoc_set, mock_l3ca_set):
        Pool.pools[Pool.Cos.PP] = {}
        Pool.pools[Pool.Cos.PP]['enabled'] = True
        Pool.pools[Pool.Cos.PP]['cores'] = [1]
        Pool.pools[Pool.Cos.PP]['cbm'] = 0xc00

        Pool.pools[Pool.Cos.P] = {}
        Pool.pools[Pool.Cos.P]['enabled'] = False
        Pool.pools[Pool.Cos.P]['cores'] = [2, 3]
        Pool.pools[Pool.Cos.P]['cbm'] = 0x300

        mock_alloc_assoc_set.return_value = 0
        mock_l3ca_set.return_value = 0

        result = Pool.apply([Pool.Cos.PP, Pool.Cos.P])

        assert result == 0

        mock_l3ca_set.assert_called_once_with(0, Pool.Cos.PP, 0xc00)
        mock_alloc_assoc_set.assert_any_call([1], Pool.Cos.PP)

        mock_alloc_assoc_set.assert_any_call([2, 3], 0)

        # libpqos fails
        mock_alloc_assoc_set.return_value = -1
        mock_l3ca_set.return_value = 0
        result = Pool.apply([Pool.Cos.PP, Pool.Cos.P])
        assert result != 0

        mock_alloc_assoc_set.return_value = 0
        mock_l3ca_set.return_value = -1
        result = Pool.apply([Pool.Cos.PP, Pool.Cos.P])
        assert result != 0

        mock_alloc_assoc_set.return_value = -1
        mock_l3ca_set.return_value = -1
        result = Pool.apply([Pool.Cos.PP, Pool.Cos.P])
        assert result != 0


    def test_reset(self):
        Pool.pools[Pool.Cos.PP] = {}
        Pool.pools[Pool.Cos.PP]['enabled'] = True
        Pool.pools[Pool.Cos.PP]['cores'] = [1]
        Pool.pools[Pool.Cos.PP]['cbm'] = 0xc00

        Pool.reset()
        assert Pool.Cos.PP not in Pool.pools

