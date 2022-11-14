################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2022 Intel Corporation. All rights reserved.
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
Unit tests for appqos.cache_ops module
"""

import pytest
import mock

from appqos import common
from appqos.cache_ops import *
from appqos.config import Config

@mock.patch("appqos.caps.caps_get", mock.MagicMock(return_value=[]))
def test_configure_rdt():
    Pool.pools[1] = {}
    Pool.pools[1]['cores'] = [1, 101]
    Pool.pools[1]['cbm'] = 0x100
    Pool.pools[1]['mba'] = 11

    Pool.pools[2] = {}
    Pool.pools[2]['cores'] = [2, 202]
    Pool.pools[2]['cbm'] = 0x200
    Pool.pools[2]['mba'] = 22

    with mock.patch('appqos.config.Config.get_pool_attr', return_value=[1]) as mock_get_pool_attr,\
         mock.patch('appqos.cache_ops.Pool.cores_set') as mock_cores_set,\
         mock.patch('appqos.cache_ops.Pool.configure', return_value=0) as mock_pool_configure,\
         mock.patch('appqos.cache_ops.Apps.configure', return_value=0) as mock_apps_configure,\
         mock.patch('appqos.pqos_api.PQOS_API.init', return_value=0),\
         mock.patch('appqos.pqos_api.PQOS_API.enable_mba_bw', return_value=0),\
         mock.patch('appqos.config_store.ConfigStore.recreate_default_pool', return_value=0):

        cfg = Config({})

        assert not configure_rdt(cfg)

        mock_cores_set.assert_called_once_with([])
        mock_pool_configure.assert_called_once()
        mock_apps_configure.assert_called_once()

        mock_pool_configure.return_value = -1
        assert configure_rdt(cfg) == -1

        mock_pool_configure.return_value = 0
        mock_get_pool_attr.return_value = []
        assert configure_rdt(cfg) == -1


class TestPools(object):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self):
        Pool.pools= {}
    ## @endcond


    @mock.patch("appqos.caps.cat_l3_supported", mock.MagicMock(return_value=True))
    @mock.patch("appqos.caps.cat_l2_supported", mock.MagicMock(return_value=True))
    @mock.patch("appqos.caps.mba_supported", mock.MagicMock(return_value=True))
    def test_configure(self):
        def get_attr(cls, attr, pool_id):
            config = {
                'cores': [1, 2],
                'l3cbm': 15,
                'l2cbm': 7,
                'mba': 88,
                'apps': [1],
                'pids': [11, 22]
            }

            if attr in config:
                return config[attr]
            else:
                return None

        with mock.patch('appqos.config.Config.get_pool_attr', new=get_attr),\
             mock.patch('appqos.config.Config.get_app_attr', new=get_attr),\
             mock.patch('appqos.cache_ops.Pool.l3cbm_set') as mock_l3cbm_set,\
             mock.patch('appqos.cache_ops.Pool.l2cbm_set') as mock_l2cbm_set,\
             mock.patch('appqos.cache_ops.Pool.mba_set') as mock_mba_set,\
             mock.patch('appqos.cache_ops.Pool.cores_set') as mock_cores_set,\
             mock.patch('appqos.cache_ops.Pool.pids_set') as mock_pids_set,\
             mock.patch('appqos.cache_ops.Pool.apply') as mock_apply:

             Pool(1).configure(Config({}))

             mock_l3cbm_set.assert_called_once_with(15)
             mock_l2cbm_set.assert_called_once_with(7)
             mock_mba_set.assert_called_once_with(88)
             mock_cores_set.assert_called_once_with([1,2])
             mock_pids_set.assert_called_once_with([11,22], get_attr(None, 'cores', 0))
             mock_apply.assert_called_once_with(1)


    def test_l3cbm_get(self):
        Pool.pools[3] = {}
        Pool.pools[3]['l3cbm'] = 0xf
        Pool.pools[1] = {}

        assert Pool(3).l3cbm_get() == 0xf
        assert not Pool(1).l3cbm_get()
        assert not Pool(2).l3cbm_get()


    def test_l3cbm_set(self):
        Pool.pools[3] = {}
        Pool.pools[3]['l3cbm'] = 0xf
        Pool.pools[1] = {}

        Pool(3).l3cbm_set(0x1)

        assert 0x1 == Pool.pools[3]['l3cbm']


    def test_l2cbm_set(self):
        Pool.pools[3] = {}
        Pool.pools[3]['l2cbm'] = 0xf
        Pool.pools[1] = {}

        Pool(3).l2cbm_set(0x1)

        assert 0x1 == Pool.pools[3]['l2cbm']


    def test_mba_get(self):
        Pool.pools[11] = {}
        Pool.pools[11]['mba'] = 10
        Pool.pools[33] = {}
        Pool.pools[33]['mba'] = 30

        assert Pool(11).mba_get() == 10
        assert not Pool(20).mba_get()
        assert Pool(33).mba_get() == 30


    def test_mba_set(self):
        Pool.pools[11] = {}
        Pool.pools[11]['mba'] = 10
        Pool.pools[33] = {}
        Pool.pools[33]['mba'] = 30

        Pool(11).mba_set(11+1)
        Pool(33).mba_set(33+3)

        assert 11+1 == Pool.pools[11]['mba']
        assert 33+3 == Pool.pools[33]['mba']


    def test_pids_get(self):
        Pool.pools[1] = {}
        Pool.pools[1]['pids'] = [1, 10,1010]
        Pool.pools[30] = {}
        Pool.pools[30]['pids'] = [3, 30, 3030]

        assert Pool(1).pids_get() == [1, 10,1010]
        assert Pool(2).pids_get() == []
        assert Pool(30).pids_get() == [3, 30, 3030]


    def test_pids_set(self):
        Pool.pools[1] = {}
        Pool.pools[1]['pids'] = [1, 10,1010]
        Pool.pools[30] = {}
        Pool.pools[30]['pids'] = [3, 30, 3030]

        with mock.patch('appqos.cache_ops.set_affinity') as set_aff_mock:

            default_cores = [1, 44, 66]

            Pool(1).pids_set([1, 10], default_cores)

            set_aff_mock.assert_called_once_with([1010], [1, 44, 66])
            set_aff_mock.reset_mock()

            Pool(30).pids_set([30, 3030], default_cores)

            set_aff_mock.assert_called_once_with([3], [1, 44, 66])


    def test_cores_get(self):
        Pool.pools[12] = {}
        Pool.pools[12]['cores'] = [1, 10,11]
        Pool.pools[35] = {}
        Pool.pools[35]['cores'] = [3, 30, 33]

        assert Pool(12).cores_get() == [1, 10,11]
        assert Pool(20).cores_get() == []
        assert Pool(35).cores_get() == [3, 30, 33]


    @mock.patch('appqos.pqos_api.PQOS_API.alloc_assoc_set')
    @mock.patch('appqos.pqos_api.PQOS_API.release')
    @mock.patch('appqos.caps.caps_get', mock.MagicMock(return_value=[]))
    def test_cores_set(self, mock_release, mock_alloc_assoc_set):
        Pool.pools[1] = {}
        Pool.pools[1]['cores'] = []
        Pool.pools[2] = {}
        Pool.pools[2]['cores'] = [4, 5, 6]

        Pool(1).cores_set([1, 2])
        assert Pool.pools[1]['cores'] == [1, 2]
        mock_alloc_assoc_set.assert_called_once_with([1, 2], 1)

        Pool(1).cores_set([1, 3])
        assert Pool.pools[1]['cores'] == [1, 3]
        mock_alloc_assoc_set.assert_any_call([1, 3], 1)
        mock_release.assert_called_once_with([2])


    @mock.patch('appqos.pqos_api.PQOS_API.l3ca_set')
    @mock.patch('appqos.pqos_api.PQOS_API.alloc_assoc_set')
    def test_apply_not_configured(self, mock_l3ca_set, mock_alloc_assoc_set):
        result = Pool.apply(1)

        assert result == -1

        mock_l3ca_set.assert_not_called()
        mock_alloc_assoc_set.assert_not_called()


    @mock.patch('appqos.pqos_api.PQOS_API.mba_set')
    @mock.patch('appqos.pqos_api.PQOS_API.l2ca_set')
    @mock.patch('appqos.pqos_api.PQOS_API.l3ca_set')
    @mock.patch('appqos.pqos_api.PQOS_API.alloc_assoc_set')
    @mock.patch('appqos.pqos_api.PQOS_API.get_sockets')
    @mock.patch('appqos.pqos_api.PQOS_API.get_l2ids')
    @mock.patch('appqos.config.Config.get_mba_ctrl_enabled')
    def test_apply(self, mock_get_mba_ctrl_enabled, mock_get_l2ids, mock_get_socket, \
        mock_alloc_assoc_set, mock_l3ca_set, mock_l2ca_set, mock_mba_set):
        Pool.pools[2] = {}
        Pool.pools[2]['cores'] = [1]
        Pool.pools[2]['l3cbm'] = 0xc00
        Pool.pools[2]['l2cbm'] = 0xf
        Pool.pools[2]['mba'] = 99

        Pool.pools[1] = {}
        Pool.pools[1]['cores'] = [2, 3]
        Pool.pools[1]['l3cbm'] = 0x300
        Pool.pools[1]['l2cbm'] = 0xff
        Pool.pools[1]['mba'] = 11

        mock_alloc_assoc_set.return_value = 0
        mock_mba_set.return_value = 0
        mock_l3ca_set.return_value = 0
        mock_l2ca_set.return_value = 0
        mock_get_socket.return_value = [0, 2]
        mock_get_l2ids.return_value = [0, 1]
        mock_get_mba_ctrl_enabled.return_value = False

        result = Pool.apply(2)
        assert result == 0
        result = Pool.apply(1)
        assert result == 0

        mock_mba_set.assert_any_call([0, 2], 2, 99, False)
        mock_mba_set.assert_any_call([0, 2], 1, 11, False)
        mock_l3ca_set.assert_any_call([0, 2], 2, mask=0xc00)
        mock_l3ca_set.assert_any_call([0, 2], 1, mask=0x300)
        mock_l2ca_set.assert_any_call([0, 1], 1, mask=0xff)
        mock_l2ca_set.assert_any_call([0, 1], 2, mask=0xf)
        mock_alloc_assoc_set.assert_any_call([1], 2)
        mock_alloc_assoc_set.assert_any_call([2, 3], 1)

        # libpqos fails
        mock_get_socket.return_value = None
        result = Pool.apply(2)
        assert result != 0
        result = Pool.apply(1)
        assert result != 0

        mock_get_socket.return_value = [0,2]
        mock_alloc_assoc_set.return_value = -1
        mock_l3ca_set.return_value = 0
        result = Pool.apply(2)
        assert result != 0
        result = Pool.apply(1)
        assert result != 0

        mock_alloc_assoc_set.return_value = 0
        mock_l3ca_set.return_value = -1
        result = Pool.apply(2)
        assert result != 0
        result = Pool.apply(1)
        assert result != 0

        mock_l3ca_set.return_value = 0
        mock_mba_set.return_value = -1
        result = Pool.apply(2)
        assert result != 0
        result = Pool.apply(1)
        assert result != 0

        mock_l2ca_set.return_value = -1
        mock_mba_set.return_value = 0
        result = Pool.apply(2)
        assert result != 0
        result = Pool.apply(1)
        assert result != 0

        mock_l2ca_set.return_value = 0
        mock_alloc_assoc_set.return_value = -1
        mock_l3ca_set.return_value = -1
        result = Pool.apply(2)
        assert result != 0
        result = Pool.apply(1)
        assert result != 0


    def test_reset(self):
        Pool.pools[2] = {}
        Pool.pools[2]['cores'] = [1]
        Pool.pools[2]['cbm'] = 0xc00

        Pool.reset()
        assert 2 not in Pool.pools


class TestApps(object):

    def test_apps_configure(self):

        CONFIG = {"apps": [
            {
                "cores": [1],
                "id": 1,
                "pids": [1]
            },
            {
                "cores": [2],
                "id": 2,
                "pids": [2, 22]
            },
            {
                "cores": [10],
                "id": 10,
                "pids": [10]
            },
            {
                "cores": [10],
                "id": 11
            }
        ]}

        def get_pool_attr(cls, attr, pool_id):
            if attr == 'cores':
                return [1, 2, 3, 4]
            else:
                return None

        with mock.patch('appqos.config.Config.app_to_pool') as atp_mock,\
             mock.patch('appqos.config.Config.get_pool_attr') as gpa_mock,\
             mock.patch('appqos.cache_ops.set_affinity') as sa_mock:

                cfg = Config({})
                Apps.configure(cfg)

                atp_mock.assert_not_called()
                gpa_mock.assert_not_called()
                sa_mock.assert_not_called()


        with mock.patch('appqos.config.Config.app_to_pool', return_value=1),\
             mock.patch('appqos.config.Config.get_pool_attr', new=get_pool_attr),\
             mock.patch('appqos.cache_ops.set_affinity') as set_aff_mock:

                cfg = Config(CONFIG)
                Apps.configure(cfg)

                set_aff_mock.assert_any_call([1], [1])
                set_aff_mock.assert_any_call([2, 22], [2])
                set_aff_mock.assert_any_call([10], [1, 2, 3, 4])

