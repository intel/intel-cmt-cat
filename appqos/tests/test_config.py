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
Unit tests for appqos.config module
"""

import pytest
import logging
import jsonschema
import mock

from appqos import common
from appqos.config import Config
import appqos.caps

from copy import deepcopy

logging.basicConfig(level=logging.DEBUG)

LOG = logging.getLogger('config')

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
    "pools": [
        {
            "apps": [1],
            "cbm": 0xf0,
            "l2cbm": 0xf,
            "cores": [1],
            "id": 1,
            "mba": 20,
            "name": "cat&mba"
        },
        {
            "apps": [2, 3],
            "cbm": 0xf,
            "l2cbm": 0xf0,
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


@pytest.mark.parametrize("pid, app", [
    (1, 1),
    (2, 2),
    (3, 2),
    (4, 3),
    (5, None),
    (None, None)
])
def test_config_pid_to_app(pid, app):
    config = Config(CONFIG)

    assert config.pid_to_app(pid) == app


@pytest.mark.parametrize("app, pool_id", [
    (1, 1),
    (2, 2),
    (3, 2),
    (99, None),
    (None, None)
])
def test_config_app_to_pool(app, pool_id):
    config = Config(CONFIG)

    assert config.app_to_pool(app) == pool_id


@pytest.mark.parametrize("pid, pool_id", [
    (1, 1),
    (2, 2),
    (3, 2),
    (4, 2),
    (1234, None),
    (None, None)
])
def test_config_pid_to_pool(pid, pool_id):
    config = Config(CONFIG)

    assert config.pid_to_pool(pid) == pool_id


@mock.patch('appqos.pqos_api.PQOS_API.get_cores')
def test_config_default_pool(mock_get_cores):
    mock_get_cores.return_value = range(16)
    config = Config(deepcopy(CONFIG))

    # just in case, remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config.is_default_pool_defined()

    # add default pool to config
    config.add_default_pool()
    assert config.is_default_pool_defined()

    # test that config now contains all cores (cores configured + default pool cores)
    all_cores = range(16)
    for pool in config['pools']:
        all_cores = [core for core in all_cores if core not in pool['cores']]
    assert not all_cores

    # remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config.is_default_pool_defined()


@mock.patch('appqos.pqos_api.PQOS_API.get_cores', mock.MagicMock(return_value=range(8)))
@mock.patch('appqos.pqos_api.PQOS_API.get_max_l3_cat_cbm', mock.MagicMock(return_value=0xDEADBEEF))
@mock.patch("appqos.caps.cat_l3_supported", mock.MagicMock(return_value=True))
@mock.patch("appqos.caps.cat_l2_supported", mock.MagicMock(return_value=False))
@mock.patch("appqos.caps.mba_supported", mock.MagicMock(return_value=False))
def test_config_default_pool_cat():
    config = Config(deepcopy(CONFIG))

    # just in case, remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config.is_default_pool_defined()

    # add default pool to config
    config.add_default_pool()
    assert config.is_default_pool_defined()

    pool_cbm = None

    for pool in config['pools']:
        if pool['id'] == 0:
            assert 'l3cbm' in pool
            assert not 'mba' in pool
            assert not 'mba_bw' in pool
            pool_cbm = pool['l3cbm']
            break

    assert pool_cbm == 0xDEADBEEF


@mock.patch('appqos.pqos_api.PQOS_API.get_cores', mock.MagicMock(return_value=range(8)))
@mock.patch('appqos.pqos_api.PQOS_API.get_max_l2_cat_cbm', mock.MagicMock(return_value=0xDEADBEEF))
@mock.patch("appqos.caps.cat_l3_supported", mock.MagicMock(return_value=False))
@mock.patch("appqos.caps.cat_l2_supported", mock.MagicMock(return_value=True))
@mock.patch("appqos.caps.mba_supported", mock.MagicMock(return_value=False))
def test_config_default_pool_l2cat():
    config = Config(deepcopy(CONFIG))

    # just in case, remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config.is_default_pool_defined()

    # add default pool to config
    config.add_default_pool()
    assert config.is_default_pool_defined()

    pool_l2cbm = None

    for pool in config['pools']:
        if pool['id'] == 0:
            assert 'l2cbm' in pool
            assert not 'cbm' in pool
            assert not 'l3cbm' in pool
            assert not 'mba' in pool
            assert not 'mba_bw' in pool
            pool_l2cbm = pool['l2cbm']
            break

    assert pool_l2cbm == 0xDEADBEEF


@mock.patch('appqos.pqos_api.PQOS_API.get_cores', mock.MagicMock(return_value=range(8)))
@mock.patch("appqos.caps.mba_supported", mock.MagicMock(return_value=True))
@mock.patch("appqos.caps.cat_l3_supported", mock.MagicMock(return_value=False))
@mock.patch("appqos.caps.cat_l2_supported", mock.MagicMock(return_value=False))
@mock.patch("appqos.caps.mba_bw_enabled", mock.MagicMock(return_value=False))
def test_config_default_pool_mba():
    config = Config(deepcopy(CONFIG))

    # just in case, remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config.is_default_pool_defined()

    # add default pool to config
    config.add_default_pool()
    assert config.is_default_pool_defined()

    pool_mba = None

    for pool in config['pools']:
        if pool['id'] == 0:
            assert not 'cat' in pool
            assert not 'mba_bw' in pool
            pool_mba = pool['mba']
            break

    assert pool_mba == 100


@mock.patch('appqos.pqos_api.PQOS_API.get_cores', mock.MagicMock(return_value=range(8)))
@mock.patch("appqos.caps.mba_supported", mock.MagicMock(return_value=True))
@mock.patch("appqos.caps.mba_bw_enabled", mock.MagicMock(return_value=True))
@mock.patch("appqos.caps.cat_l3_supported", mock.MagicMock(return_value=False))
@mock.patch("appqos.config.Config.get_mba_ctrl_enabled", mock.MagicMock(return_value=True))
def test_config_default_pool_mba_bw():
    config = Config(deepcopy(CONFIG))

    # just in case, remove default pool from config
    for pool in config['pools']:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # no default pool in config
    assert not config.is_default_pool_defined()

    # add default pool to config
    config.add_default_pool()
    assert config.is_default_pool_defined()

    pool_mba_bw = None

    for pool in config['pools']:
        if pool['id'] == 0:
            assert not 'mba' in pool
            assert not 'cbm' in pool
            assert not 'l3cbm' in pool
            pool_mba_bw = pool['mba_bw']
            break

    assert pool_mba_bw == 2**32 - 1


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


def test_config_is_default_pool_defined():
    config = Config(deepcopy(CONFIG_POOLS))

    # FUT, default pool in config
    assert config.is_default_pool_defined() == True

    # remove default pool from config
    for pool in config['pools'][:]:
        if pool['id'] == 0:
            config['pools'].remove(pool)
            break

    # FUT, no default pool in config
    assert not config.is_default_pool_defined()


def test_config_remove_default_pool():
    config = Config(deepcopy(CONFIG_POOLS))

    # default pool in config
    assert config.is_default_pool_defined() == True

    # FUT
    config.remove_default_pool()

    # no default pool in config
    assert not config.is_default_pool_defined()


def test_config_is_any_pool_defined():
    config = Config(deepcopy(CONFIG_POOLS))

    assert config.is_any_pool_defined() == True

    for pool in config['pools'][:]:
        print(pool)
        if not pool['id'] == 0:
            config['pools'].remove(pool)

    print(config)

    assert not config.is_any_pool_defined()


@pytest.mark.parametrize("cfg, default, result", [
    ({}, True, True),
    ({}, False, False),
    ({"test": True}, True, True),
    ({"test": True}, False, False),
    ({"power_profiles_expert_mode": False}, False, False),
    ({"power_profiles_expert_mode": False}, True, False),
    ({"power_profiles_expert_mode": True}, False, True),
    ({"power_profiles_expert_mode": True}, True, True)
])
def test_get_global_attr_power_profiles_expert_mode(cfg, default, result):
    config = Config(cfg)

    assert config.get_global_attr('power_profiles_expert_mode', default) == result


@pytest.mark.parametrize("cfg, default, result", [
    ({}, True, True),
    ({}, False, False),
    ({"test": True}, True, True),
    ({"test": True}, False, False),
    ({"power_profiles_verify": False}, False, False),
    ({"power_profiles_verify": False}, True, False),
    ({"power_profiles_verify": True}, False, True),
    ({"power_profiles_verify": True}, True, True)
])
def test_get_global_attr_power_profiles_verify(cfg, default, result):
    config = Config(cfg)

    assert config.get_global_attr('power_profiles_verify', default) == result


@pytest.mark.parametrize("cfg, result", [
    ({}, "msr"),
    ({"rdt_iface": {"interface": "msr"}}, "msr"),
    ({"rdt_iface": {"interface": "msr_test"}}, "msr_test"),
    ({"rdt_iface": {"interface": "os_test"}}, "os_test"),
    ({"rdt_iface": {"interface": "os"}}, "os")
])
def test_get_rdt_iface(cfg, result):
    config = Config(cfg)

    assert config.get_rdt_iface() == result


@pytest.mark.parametrize("cfg, result", [
    ({}, False),
    ({"mba_ctrl": {"enabled": True}}, True),
    ({"mba_ctrl": {"enabled": False}}, False)
])
def test_get_mba_ctrl_enabled(cfg, result):
    config = Config(cfg)

    assert config.get_mba_ctrl_enabled() == result
