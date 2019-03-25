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

import common

from cache_ops import *

class TestCacheOps(object):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self):
        Pool.pools= {}
    ## @endcond


    def test_cbm_get(self):
        Pool.pools[3] = {}
        Pool.pools[3]['cbm'] = 0xf
        Pool.pools[1] = {}

        assert Pool(3).cbm_get() == 0xf
        assert not Pool(1).cbm_get()
        assert not Pool(2).cbm_get()


    def test_mba_get(self):
        Pool.pools[11] = {}
        Pool.pools[11]['mba'] = 10
        Pool.pools[33] = {}
        Pool.pools[33]['mba'] = 30

        assert Pool(11).mba_get() == 10
        assert not Pool(20).mba_get()
        assert Pool(33).mba_get() == 30


    def test_pids_get(self):
        Pool.pools[1] = {}
        Pool.pools[1]['pids'] = [1, 10,1010]
        Pool.pools[30] = {}
        Pool.pools[30]['pids'] = [3, 30, 3030]

        assert Pool(1).pids_get() == [1, 10,1010]
        assert Pool(2).pids_get() == []
        assert Pool(30).pids_get() == [3, 30, 3030]


    def test_cores_get(self):
        Pool.pools[12] = {}
        Pool.pools[12]['cores'] = [1, 10,11]
        Pool.pools[35] = {}
        Pool.pools[35]['cores'] = [3, 30, 33]

        assert Pool(12).cores_get() == [1, 10,11]
        assert Pool(20).cores_get() == []
        assert Pool(35).cores_get() == [3, 30, 33]


    @mock.patch('cache_ops.PQOS_API.alloc_assoc_set')
    def test_cores_set(self, mock_alloc_assoc_set):
        Pool.pools[1] = {}
        Pool.pools[1]['cores'] = []

        Pool(1).cores_set([1])
        assert Pool.pools[1]['cores'] == [1]
        mock_alloc_assoc_set.assert_called_once_with([1], 1)


    @mock.patch('cache_ops.PQOS_API.l3ca_set')
    @mock.patch('cache_ops.PQOS_API.alloc_assoc_set')
    def test_apply_not_configured(self, mock_l3ca_set, mock_alloc_assoc_set):
        result = Pool.apply(1)

        assert result == 0

        mock_l3ca_set.assert_not_called()
        mock_alloc_assoc_set.assert_not_called()


    @mock.patch('cache_ops.PQOS_API.l3ca_set')
    @mock.patch('cache_ops.PQOS_API.alloc_assoc_set')
    @mock.patch('cache_ops.PQOS_API.get_sockets')
    def test_apply(self, mock_get_socket, mock_alloc_assoc_set, mock_l3ca_set):
        Pool.pools[2] = {}
        Pool.pools[2]['cores'] = [1]
        Pool.pools[2]['cbm'] = 0xc00

        Pool.pools[1] = {}
        Pool.pools[1]['cores'] = [2, 3]
        Pool.pools[1]['cbm'] = 0x300

        mock_alloc_assoc_set.return_value = 0
        mock_l3ca_set.return_value = 0
        mock_get_socket.return_value = [0, 2]

        result = Pool.apply([2, 1])

        assert result == 0

        mock_l3ca_set.assert_any_call([0, 2], 2, 0xc00)
        mock_l3ca_set.assert_any_call([0, 2], 1, 0x300)
        mock_alloc_assoc_set.assert_any_call([1], 2)
        mock_alloc_assoc_set.assert_any_call([2, 3], 1)

        # libpqos fails
        mock_get_socket.return_value = None
        result = Pool.apply([2, 1])
        assert result != 0

        mock_get_socket.return_value = [0,2]
        mock_alloc_assoc_set.return_value = -1
        mock_l3ca_set.return_value = 0
        result = Pool.apply([2, 1])
        assert result != 0

        mock_alloc_assoc_set.return_value = 0
        mock_l3ca_set.return_value = -1
        result = Pool.apply([2, 1])
        assert result != 0

        mock_alloc_assoc_set.return_value = -1
        mock_l3ca_set.return_value = -1
        result = Pool.apply([2, 1])
        assert result != 0


    def test_reset(self):
        Pool.pools[2] = {}
        Pool.pools[2]['cores'] = [1]
        Pool.pools[2]['cbm'] = 0xc00

        Pool.reset()
        assert 2 not in Pool.pools


    @mock.patch('cache_ops.Pqos.get_l3ca_num_cos')
    @mock.patch('cache_ops.Pqos.get_mba_num_cos')
    def test_get_max_cos_id(self, mock_get_mba_num_cos, mock_get_l3ca_num_cos):

       mock_get_l3ca_num_cos.return_value = 16
       mock_get_mba_num_cos.return_value = 8
       assert 7 == Pqos.get_max_cos_id([common.CAT_CAP, common.MBA_CAP])
       assert 7 == Pqos.get_max_cos_id([common.MBA_CAP])
       assert 15 == Pqos.get_max_cos_id([common.CAT_CAP])
       assert None == Pqos.get_max_cos_id([])

       mock_get_l3ca_num_cos.return_value = 0
       assert None == Pqos.get_max_cos_id([common.CAT_CAP, common.MBA_CAP])
       assert 7 == Pqos.get_max_cos_id([common.MBA_CAP])
       assert None == Pqos.get_max_cos_id([common.CAT_CAP])
       assert None == Pqos.get_max_cos_id([])

       mock_get_mba_num_cos.return_value = 0
       assert None == Pqos.get_max_cos_id([common.CAT_CAP, common.MBA_CAP])
       assert None == Pqos.get_max_cos_id([common.MBA_CAP])
       assert None == Pqos.get_max_cos_id([common.CAT_CAP])
       assert None == Pqos.get_max_cos_id([])


    @mock.patch('cache_ops.pqos_get_l3ca_num_cos')
    def test_get_l3ca_num_cos(self, mock_pqos_get_l3ca_num_cos):
       mock_pqos_get_l3ca_num_cos.return_value = 32

       assert 32 == Pqos.get_l3ca_num_cos()


    @mock.patch('cache_ops.pqos_get_mba_num_cos')
    def test_get_mba_num_cos(self, mock_pqos_get_mba_num_cos):
       mock_pqos_get_mba_num_cos.return_value = 16

       assert 16 == Pqos.get_mba_num_cos()
