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
import common
import jsonschema
import mock

from config import *

CONFIG = {
    "apps": [
        {
            "cores": [1],
            "id": 1,
            "name": "app 1",
            "pids": [1]
        },
        {
            "cores": [3],
            "id": 2,
            "name": "app 2",
            "pids": [2, 3]
        },
        {
            "cores": [3],
            "id": 3,
            "name": "app 3",
            "pids": [4]
        }
    ],
    "auth": {
        "password": "password",
        "username": "admin"
    },
    "pools": [
        {
            "apps": [1],
            "cbm": 0xf0,
            "cores": [1],
            "id": 1,
            "mba": 20,
            "name": "cat&mba"
        },
        {
            "apps": [2, 3],
            "cbm": 0xf,
            "cores": [3],
            "id": 2,
            "name": "cat"
        },
        {
            "id": 3,
            "mba": 30,
            "name": "mba",
            "cores": [4]
        }
    ]
}

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("pid, app", [
    (1, 1),
    (2, 2),
    (3, 2),
    (4, 3),
    (5, None),
    (None, None)
])
def test_config_pid_to_app(mock_get_config, pid, app):

    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.pid_to_app(pid) == app

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("app, pool_id", [
    (1, 1),
    (2, 2),
    (3, 2),
    (99, None),
    (None, None)
])
def test_config_app_to_pool(mock_get_config, app, pool_id):
    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.app_to_pool(app) == pool_id

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("pid, pool_id", [
    (1, 1),
    (2, 2),
    (3, 2),
    (4, 2),
    (1234, None),
    (None, None)
])
def test_config_pid_to_pool(mock_get_config, pid, pool_id):
    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.pid_to_pool(pid) == pool_id


@mock.patch('cache_ops.PQOS_API.get_num_cores')
def test_config_default_pool(mock_get_num_cores):
    mock_get_num_cores.return_value = 16
    config_store = ConfigStore()
    config = CONFIG.copy()

    # just in case, remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config_store.is_default_pool_defined(config)

    # add default pool to config
    config_store.add_default_pool(config)
    assert config_store.is_default_pool_defined(config)

    # test that config now contains all cores (cores configured + default pool cores)
    all_cores = range(cache_ops.PQOS_API.get_num_cores())
    for pool in config['pools']:
        all_cores = [core for core in all_cores if core not in pool['cores']]
    assert not all_cores

    # remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config_store.is_default_pool_defined(config)


CONFIG_POOLS = {
    "pools": [
        {
            "apps": [1],
            "cbm": 0xf0,
            "cores": [1],
            "id": 0,
            "mba": 20,
            "name": "cat&mba_0"
        },
        {
            "apps": [2, 3],
            "cbm": 0xf,
            "cores": [3],
            "id": 9,
            "name": "cat_9"
        },
        {
            "id": 31,
            "cbm": "0xf0",
            "name": "cat_31",
            "cores": [4],
        }
    ]
}


@mock.patch('config.ConfigStore.get_config')
def test_config_get_new_pool_id(mock_get_config):

    def get_max_cos_id(alloc_type):
        if 'mba' in alloc_type:
            return 9
        else:
            return 31


    with mock.patch('cache_ops.PQOS_API.get_max_cos_id', new=get_max_cos_id):
        config_store = ConfigStore()

        mock_get_config.return_value = CONFIG
        assert 9 == config_store.get_new_pool_id({"mba":10})
        assert 9 == config_store.get_new_pool_id({"mba":20, "cbm":"0xf0"})
        assert 31 == config_store.get_new_pool_id({"cbm":"0xff"})

        mock_get_config.return_value = CONFIG_POOLS
        assert 8 == config_store.get_new_pool_id({"mba":10})
        assert 8 == config_store.get_new_pool_id({"mba":20, "cbm":"0xf0"})
        assert 30 == config_store.get_new_pool_id({"cbm":"0xff"})


@mock.patch('cache_ops.PQOS_API.get_num_cores')
@mock.patch('config.ConfigStore.load')
def test_config_reset(mock_load, mock_get_num_cores):
        from copy import deepcopy

        # mock load to avoid reading config from external file
        mock_load.return_value = deepcopy(CONFIG)
        mock_get_num_cores.return_value = 8

        config_store = ConfigStore()
        config_store.from_file("/tmp/appqos_test.config")

        assert len(config_store.get_pool_attr('cores', None)) == 8

        # reset mock and change return values
        # more cores this time (8 vs. 16)
        mock_get_num_cores.return_value = 16
        mock_get_num_cores.reset_mock()

        # get fresh copy of CONFIG
        mock_load.return_value = deepcopy(CONFIG)
        mock_load.reset_mock()

        # verify that reset reloads config from file and Default pool is
        # recreated with different set of cores
        # (get_num_cores mocked to return different values)
        config_store.reset()

        mock_load.assert_called_once_with("/tmp/appqos_test.config")
        mock_get_num_cores.assert_called_once()

        assert len(config_store.get_pool_attr('cores', None)) == 16

