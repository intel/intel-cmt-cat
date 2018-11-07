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
import common
import jsonschema
import mock

from config import *
from gen_config import create_sample_config


def test_config_to_and_from_file():
    # Test saving config to file and retrieving it from file

    # Config_store to save to file
    config_store_to_file = ConfigStore()
    config_store_str = create_sample_config()  # from config_gen module
    config_store_to_file.set_path("test_config.conf")

    # Config_store to load config from file
    config_store_from_file = ConfigStore()

        # Full config schema is validated in to to_file and from_file functions
    config_store_to_file.to_file(config_store_str)
    config_store_from_file.from_file("test_config.conf")

def test_config_to_file_empty():
    # Test saving config to file and retrieving it from file

    # Config_store to save to file
    config_store_to_file = ConfigStore()
    config_store_str = create_sample_config()  # from config_gen module
    config_store_to_file.set_path("test_config.conf")

    # Config_store to load config from file
    config_store_from_file = ConfigStore()

    with pytest.raises(jsonschema.ValidationError):
        # Full config schema is validated in to to_file and from_file functions
        config_store_to_file.to_file({}) # Fails because config cannot be empty

def test_config_get_attr_list_unknown():
    # Test get_attr_list function with "unknown" attribute
    # Should return empty because attribute "unknown" doesnt exist in config

    # Generate congig_store and update with config
    config_store = ConfigStore()
    config_store.set_config(create_sample_config())  # from config_gen module

    #Test all options: Dynamic (prod, pre prod, best effort) and Static
    p_unknown = config_store.get_attr_list('unknown',common.TYPE_DYNAMIC, common.PRODUCTION)

    pp_unknown = config_store.get_attr_list('unknown',common.TYPE_DYNAMIC, common.PRE_PRODUCTION)

    be_unknown = config_store.get_attr_list('unknown',common.TYPE_DYNAMIC, common.BEST_EFFORT)

    static_unknown = config_store.get_attr_list('unknown',common.TYPE_STATIC, None)

    assert p_unknown == []

    assert pp_unknown == []

    assert be_unknown == []

    assert static_unknown == []


def test_config_get_attr_list_cores():
    # Test get_attr_list function with "cores" attribute

    # Generate congig_store and update with config
    config_store = ConfigStore()
    config_store.set_config(create_sample_config())  # from config_gen module

    #Test all options: Dynamic (prod, pre prod, best effort) and Static
    p_cores = config_store.get_attr_list('cores',common.TYPE_DYNAMIC, common.PRODUCTION)

    pp_cores = config_store.get_attr_list('cores',common.TYPE_DYNAMIC, common.PRE_PRODUCTION)

    be_cores = config_store.get_attr_list('cores',common.TYPE_DYNAMIC, common.BEST_EFFORT)

    static_cores = config_store.get_attr_list('cores',common.TYPE_STATIC, None)

    assert p_cores == [4, 8]

    assert pp_cores == [1, 6]

    assert be_cores == [3, 7]

    assert static_cores == [1, 6]

def test_config_get_attr_list_pids():
    # Test get_attr_list function with "pids" attribute

    # Generate congig_store and update with config
    config_store = ConfigStore()
    config_store.set_config(create_sample_config())  # from config_gen module

    #Test all options: Dynamic (prod, pre prod, best effort) and Static
    p_pids = config_store.get_attr_list('pids', common.TYPE_DYNAMIC, common.PRODUCTION)

    pp_pids = config_store.get_attr_list('pids', common.TYPE_DYNAMIC, common.PRE_PRODUCTION)

    be_pids = config_store.get_attr_list('pids',common.TYPE_DYNAMIC, common.BEST_EFFORT)

    static_pids = config_store.get_attr_list('pids',common.TYPE_STATIC, None)

    assert p_pids == [1234, 213]

    assert pp_pids == [1896,1897]

    assert be_pids == [3456]

    assert static_pids == []


def test_config_get_attr_list_name():
    # Test get_attr_list function with "name" attribute

    # Generate congig_store and update with config
    config_store = ConfigStore()
    config_store.set_config(create_sample_config())  # from config_gen module

    #Test all options: Dynamic (prod, pre prod, best effort) and Static
    p_name = config_store.get_attr_list('name', common.TYPE_DYNAMIC, common.PRODUCTION)

    pp_name = config_store.get_attr_list('name', common.TYPE_DYNAMIC, common.PRE_PRODUCTION)

    be_name = config_store.get_attr_list('name',common.TYPE_DYNAMIC, common.BEST_EFFORT)

    static_name = config_store.get_attr_list('name',common.TYPE_STATIC, None)

    assert p_name == ['nDPI VM']

    assert pp_name == ['OS VM']

    assert be_name == ['IPSec VM']

    assert static_name == ['Noisy Neighbor VM']


def test_config_get_attr_list_min_cws():
    # Test get_attr_list function with "min_cws" attribute

    # Generate congig_store and update with config
    config_store = ConfigStore()
    config_store.set_config(create_sample_config())  # from config_gen module

    #Test all options: Dynamic (prod, pre prod, best effort) and Static
    p_min_cws = config_store.get_attr_list('min_cws', common.TYPE_DYNAMIC, common.PRODUCTION)

    pp_min_cws = config_store.get_attr_list('min_cws', common.TYPE_DYNAMIC, common.PRE_PRODUCTION)

    be_min_cws = config_store.get_attr_list('min_cws',common.TYPE_DYNAMIC, common.BEST_EFFORT)

    static_min_cws = config_store.get_attr_list('min_cws',common.TYPE_STATIC, None)

    assert p_min_cws == 5

    assert pp_min_cws == 1

    assert be_min_cws == 1

    assert static_min_cws == 2


CONFIG = {\
"groups": [{"id": 1, "type": "static", "pools": [1], "cbm": "0xc00"},\
           {"id": 2, "type": "dynamic", "pools": [2,3,4], "cbm": "0x3ff"}],\
"apps": [{"id": 1, "cores": [0], "name": "OS pseudoVM"},\
         {"id": 2, "cores": [8], "name": "Infrastructure"},\
         {"id": 3, "cores": [7, 15], "name": "PMDs pseudoVM"},\
         {"id": 4, "cores": [1, 6], "name": "NN VM", "pids": [2584, 2585]},\
         {"id": 5, "cores": [4, 5, 13], "name": "IPSec VM", "pids": [1764, 1765, 1766]},\
         {"id": 6, "cores": [2, 3, 11], "name": "nDPI VM", "pids": [1748, 1749, 1750]}],\
"pools": [{"id": 1, "priority": "prod", "name": "NFVI", "min_cws": 2, "apps": [2,3]},\
          {"id": 2, "priority": "prod", "name": "Production", "min_cws": 1, "apps": [5]},\
          {"id": 3, "priority": "preprod", "name": "PreProduction", "min_cws": 1, "apps": [6]},\
          {"id": 4, "priority": "besteff", "name": "Best Effort", "min_cws": 1, "apps": [1,4]}]\
}

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("pid, app", [
    (2584, 4),
    (2585, 4),
    (1764, 5),
    (1749, 6),
    (1234, None),
    (None, None)
])
def test_config_pid_to_app(mock_get_config, pid, app):

    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.pid_to_app(pid) == app

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("app, pool_id, prio", [
    (2, 1, 'prod'),
    (3, 1, 'prod'),
    (1, 4, 'besteff'),
    (6, 3, 'preprod'),
    (99, None, None),
    (None, None, None)
])
def test_config_app_to_pool(mock_get_config, app, pool_id, prio):
    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.app_to_pool(app) == (pool_id, prio)

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("pid, pool_id, prio", [
    (1766, 2, 'prod'),
    (1765, 2, 'prod'),
    (2584, 4, 'besteff'),
    (1750, 3, 'preprod'),
    (1234, None, None),
    (None, None, None)
])
def test_config_pid_to_pool(mock_get_config, pid, pool_id, prio):
    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.pid_to_pool(pid) == (pool_id, prio)

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("pool_id, prio", [
    (2, 'prod'),
    (3, 'preprod'),
    (4, 'besteff'),
    (1234, None),
    (None, None)
])
def test_config_pool_to_prio(mock_get_config, pool_id, prio):
    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.pool_to_prio(pool_id) == prio