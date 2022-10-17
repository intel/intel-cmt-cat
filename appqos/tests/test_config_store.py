################################################################################
# BSD LICENSE
#
# Copyright(c) 2022 Intel Corporation. All rights reserved.
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
import logging
import common
import jsonschema
import mock

from config_store import ConfigStore
from config import Config
import caps

from copy import deepcopy
from test_config import CONFIG, CONFIG_POOLS

logging.basicConfig(level=logging.DEBUG)

LOG = logging.getLogger('config_store')


CONFIG_NO_MBA = {
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
            "cores": [1],
            "id": 1,
            "name": "cat"
        },
        {
            "apps": [2, 3],
            "cbm": 0xf,
            "cores": [3],
            "id": 2,
            "name": "cat"
        }
    ]
}

@pytest.mark.parametrize("def_pool_def", [True, False])
def test_config_recreate_default_pool(def_pool_def):
    config_store = ConfigStore()

    with mock.patch('config.Config.is_default_pool_defined', mock.MagicMock(return_value=def_pool_def)) as mock_is_def_pool_def,\
         mock.patch('config.Config.remove_default_pool') as mock_rm_def_pool,\
         mock.patch('config.Config.add_default_pool') as mock_add_def_pool:

        config_store.recreate_default_pool()

        if def_pool_def:
            mock_rm_def_pool.assert_called_once()
        else:
            mock_rm_def_pool.assert_not_called()

        mock_add_def_pool.assert_called_once()


@mock.patch('config_store.ConfigStore.get_config')
def test_config_get_new_pool_id(mock_get_config):

    def get_max_cos_id(alloc_type):
        if 'mba' in alloc_type:
            return 9
        else:
            return 31


    with mock.patch('common.PQOS_API.get_max_cos_id', new=get_max_cos_id):
        config_store = ConfigStore()

        mock_get_config.return_value = Config(CONFIG)
        assert 9 == config_store.get_new_pool_id({"mba":10})
        assert 9 == config_store.get_new_pool_id({"mba":20, "cbm":"0xf0"})
        assert 31 == config_store.get_new_pool_id({"cbm":"0xff"})
        assert 31 == config_store.get_new_pool_id({"l2cbm":"0xff"})
        assert 31 == config_store.get_new_pool_id({"l2cbm":"0xff"})
        assert 31 == config_store.get_new_pool_id({"l2cbm":"0xff", "cbm":"0xf0"})

        mock_get_config.return_value = Config(CONFIG_POOLS)
        assert 8 == config_store.get_new_pool_id({"mba":10})
        assert 8 == config_store.get_new_pool_id({"mba":20, "cbm":"0xf0"})
        assert 30 == config_store.get_new_pool_id({"cbm":"0xff"})
        assert 30 == config_store.get_new_pool_id({"l2cbm":"0xff"})
        assert 30 == config_store.get_new_pool_id({"l2cbm":"0xff", "cbm":"0xf0"})


def test_config_reset():
    from copy import deepcopy

    with mock.patch('common.PQOS_API.get_cores') as mock_get_cores,\
         mock.patch('config_store.ConfigStore.load') as mock_load,\
         mock.patch('caps.mba_supported', return_value = True) as mock_mba,\
         mock.patch('caps.cat_l3_supported', return_value = True),\
         mock.patch('caps.cat_l2_supported', return_value = True),\
         mock.patch('common.PQOS_API.get_max_l3_cat_cbm', return_value = 0xFFF),\
         mock.patch('common.PQOS_API.get_max_l2_cat_cbm', return_value = 0xFF),\
         mock.patch('common.PQOS_API.check_core', return_value = True),\
         mock.patch('pid_ops.is_pid_valid', return_value = True):

        mock_load.return_value = Config(deepcopy(CONFIG))
        mock_get_cores.return_value = range(8)

        config_store = ConfigStore()
        config_store.from_file("/tmp/appqos_test.config")
        config_store.process_config()

        assert len(config_store.get_config().get_pool_attr('cores', None)) == 8
        assert config_store.get_config().get_pool_attr('l3cbm', 0) == 0xFFF
        assert config_store.get_config().get_pool_attr('l2cbm', 0) == 0xFF
        assert config_store.get_config().get_pool_attr('mba', 0) == 100

        # test get_pool_attr
        assert config_store.get_config().get_pool_attr('non_exisiting_key', None) == None

        # reset mock and change return values
        # more cores this time (8 vs. 16)
        mock_get_cores.return_value = range(16)
        mock_get_cores.reset_mock()

        # use CONFIG_NO_MBA this time, as MBA is reported as not supported
        mock_load.return_value = Config(deepcopy(CONFIG_NO_MBA))
        mock_load.reset_mock()
        mock_mba.return_value = False

        # verify that reset reloads config from file and Default pool is
        # recreated with different set of cores
        # (get_num_cores mocked to return different values)
        config_store.reset()

        mock_load.assert_called_once_with("/tmp/appqos_test.config")
        mock_get_cores.assert_called_once()

        assert len(config_store.get_config().get_pool_attr('cores', None)) == 16
        assert config_store.get_config().get_pool_attr('l3cbm', 0) == 0xFFF
        assert config_store.get_config().get_pool_attr('mba', 0) is None


class TestConfigValidate:

    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l2_supported", mock.MagicMock(return_value=False))
    def test_pool_invalid_core(self):
        def check_core(core):
            return core != 3

        data = Config({
            "pools": [
                {
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "valid"
                },
                {
                    "cbm": 0xf,
                    "cores": [3],
                    "id": 8,
                    "name": "invalid"
                }
            ]
        })

        with mock.patch('common.PQOS_API.check_core', new=check_core):
            with pytest.raises(ValueError, match="Invalid core 3"):
                ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_pool_duplicate_core(self):
        data = Config({
            "pools": [
                {
                    "cbm": 0xf0,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                },
                {
                    "cbm": 0xf,
                    "id": 10,
                    "cores": [3],
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(ValueError, match="already assigned to another pool"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_pool_same_ids(self):
        data = Config({
            "pools": [
                {
                    "cbm": 0xf0,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                },
                {
                    "cbm": 0xf,
                    "id": 1,
                    "cores": [3],
                    "name": "pool 2"
                }
            ]
        })

        with pytest.raises(ValueError, match="Pool 1, multiple pools with same id"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_pool_invalid_app(self):
        data = Config({
            "pools": [
                {
                    "apps": [1, 3],
                    "cbm": 0xf0,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ],
            "apps": [
                {
                    "cores": [3],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                }
            ]
        })

        with pytest.raises(KeyError, match="does not exist"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_pool_invalid_l3cbm(self):
        data = Config({
            "pools": [
                {
                    "apps": [],
                    "l3cbm": 0x5,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(ValueError, match="not contiguous"):
            ConfigStore().validate(data)

        data['pools'][0]['l3cbm'] = 0
        with pytest.raises(ValueError, match="not contiguous"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l2_supported", mock.MagicMock(return_value=True))
    def test_pool_invalid_l2cbm(self):
        data = Config({
            "pools": [
                {
                    "apps": [],
                    "l2cbm": 0x5,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(ValueError, match="not contiguous"):
            ConfigStore().validate(data)

        data['pools'][0]['l2cbm'] = 0
        with pytest.raises(ValueError, match="not contiguous"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=False))
    def test_pool_cat_not_supported(self):
        data = Config({
            "pools": [
                {
                    "apps": [],
                    "l3cbm": 0x4,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(ValueError, match="CAT is not supported"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l2_supported", mock.MagicMock(return_value=False))
    def test_pool_l2cat_not_supported(self):
        data = Config({
            "pools": [
                {
                    "apps": [],
                    "l2cbm": 0x4,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(ValueError, match="L2 CAT is not supported"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=False))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    def test_pool_cat_not_supported_mba(self):
        data = Config({
            "pools": [
                {
                    "apps": [],
                    "l3cbm": 0x4,
                    "mba": 100,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(ValueError, match="CAT is not supported"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    def test_pool_invalid_mba(self):
        data = Config({
            "pools": [
                {
                    "mba": 101,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(jsonschema.exceptions.ValidationError, match="Failed validating 'maximum' in schema"):
            ConfigStore().validate(data)

        data['pools'][0]['mba'] = 0
        with pytest.raises(jsonschema.exceptions.ValidationError, match="Failed validating 'minimum' in schema"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=False))
    def test_pool_mba_not_supported(self):
        data = Config({
            "pools": [
                {
                    "mba": 50,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(ValueError, match="MBA is not supported"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=False))
    def test_pool_mba_bw_not_supported(self):
        data = Config({
            "pools": [
                {
                    "mba_bw": 5000,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(ValueError, match="MBA BW is not enabled/supported"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=False))
    def test_pool_mba_not_supported_cat(self):
        data = Config({
            "pools": [
                {
                    "cbm": 0xf,
                    "mba": 50,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(ValueError, match="MBA is not supported"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=False))
    def test_pool_mba_bw_not_supported_cat(self):
        data = Config({
            "pools": [
                {
                    "cbm": 0xf,
                    "mba_bw": 5000,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                }
            ]
        })

        with pytest.raises(ValueError, match="MBA BW is not enabled/supported"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=True))
    def test_pool_mba_mba_bw_enabled(self):
        data = Config({
            "rdt_iface": {"interface": "os"},
            "mba_ctrl": {"enabled": True},
            "pools": [
                {
                    "cbm": 0xf,
                    "mba": 50,
                    "cores": [1, 3],
                    "id": 1,
                    "name": "pool 1"
                },
                {
                    "cbm": 0xf,
                    "mba": 70,
                    "cores": [2],
                    "id": 2,
                    "name": "pool 2"
                }
            ]
        })

        with pytest.raises(ValueError, match="MBA % is not enabled. Disable MBA BW and try again"):
            ConfigStore().validate(data)


    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_invalid_core(self):
        def check_core(core):
            return core != 3

        data = Config({
            "pools": [
                {
                    "apps": [1],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                }
            ],
            "apps": [
                {
                    "cores": [3],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                }
            ]
        })

        with mock.patch('common.PQOS_API.check_core', new=check_core):
            with pytest.raises(ValueError, match="Invalid core 3"):
                ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_core_does_not_match_pool(self):
        data = Config({
            "pools": [
                {
                    "apps": [1],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                }
            ],
            "apps": [
                {
                    "cores": [3,4,5],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                }
            ]
        })

        with pytest.raises(ValueError, match="App 1, cores {3, 4, 5} does not match Pool 1"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_without_pool(self):

        data = Config({
            "pools": [
                {
                    "apps": [1],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                },
                {
                    "cbm": 0xf0,
                    "cores": [2],
                    "id": 2,
                    "name": "pool 2"
                }
            ],
            "apps": [
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                },
                {
                    "cores": [1],
                    "id": 2,
                    "name": "app 2",
                    "pids": [2]
                }
            ]
        })

        with pytest.raises(ValueError, match="not assigned to any pool"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_without_pool(self):
        data = Config({
            "pools": [
                {
                    "apps": [1],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                },
                {
                    "apps": [1, 2],
                    "cbm": 0xf0,
                    "cores": [2],
                    "id": 2,
                    "name": "pool 2"
                }
            ],
            "apps": [
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                },
                {
                    "cores": [2],
                    "id": 2,
                    "name": "app 2",
                    "pids": [2]
                }
            ]
        })

        with pytest.raises(ValueError, match="App 1, Assigned to more than one pool"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_same_ids(self):
        data = Config({
            "pools": [
                {
                    "apps": [1],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                },
            ],
            "apps": [
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                },
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 2",
                    "pids": [1]
                }
            ]
        })

        with pytest.raises(ValueError, match="App 1, multiple apps with same id"):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_same_pid(self):
        data = Config({
            "pools": [
                {
                    "apps": [1, 2],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                },
            ],
            "apps": [
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                },
                {
                    "cores": [1],
                    "id": 2,
                    "name": "app 2",
                    "pids": [1]
                }
            ]
        })

        with pytest.raises(ValueError, match=r"App 2, PIDs \{1} already assigned to another App."):
            ConfigStore().validate(data)


    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_l3_supported", mock.MagicMock(return_value=True))
    def test_app_invalid_pid(self):
        data = Config({
            "pools": [
                {
                    "apps": [1, 2],
                    "cbm": 0xf0,
                    "cores": [1],
                    "id": 1,
                    "name": "pool 1"
                },
            ],
            "apps": [
                {
                    "cores": [1],
                    "id": 1,
                    "name": "app 1",
                    "pids": [1]
                },
                {
                    "cores": [1],
                    "id": 2,
                    "name": "app 2",
                    "pids": [99999]
                }
            ]
        })

        with pytest.raises(ValueError, match="App 2, PID 99999 is not valid"):
            ConfigStore().validate(data)


    def test_power_profile_expert_mode_invalid(self):
        data = Config({
            "pools": [],
            "apps": [],
            "power_profiles_expert_mode": None
        })

        with pytest.raises(jsonschema.exceptions.ValidationError, match="None is not of type 'boolean'"):
            ConfigStore().validate(data)

        data['power_profiles_expert_mode'] = 1
        with pytest.raises(jsonschema.exceptions.ValidationError, match="1 is not of type 'boolean'"):
            ConfigStore().validate(data)

        data['power_profiles_expert_mode'] = []
        with pytest.raises(jsonschema.exceptions.ValidationError, match="\\[\\] is not of type 'boolean'"):
            ConfigStore().validate(data)


    def test_power_profile_verify_invalid(self):
        data = Config({
            "pools": [],
            "apps": [],
            "power_profiles_verify": None
        })

        with pytest.raises(jsonschema.exceptions.ValidationError, match="None is not of type 'boolean'"):
            ConfigStore().validate(data)

        data['power_profiles_verify'] = 1
        with pytest.raises(jsonschema.exceptions.ValidationError, match="1 is not of type 'boolean'"):
            ConfigStore().validate(data)

        data['power_profiles_verify'] = []
        with pytest.raises(jsonschema.exceptions.ValidationError, match="\\[\\] is not of type 'boolean'"):
            ConfigStore().validate(data)


    def test_rdt_iface_invalid(self):
        data = Config({
            "pools": [],
            "apps": [],
            "rdt_iface": "os"
        })

        with pytest.raises(jsonschema.exceptions.ValidationError, match="'os' is not of type 'object'"):
            ConfigStore().validate(data)

        data['rdt_iface'] = {}
        with pytest.raises(jsonschema.exceptions.ValidationError, match="'interface' is a required property"):
            ConfigStore().validate(data)

        data['rdt_iface']['interface'] = None
        with pytest.raises(jsonschema.exceptions.ValidationError, match="None is not of type 'string'"):
            ConfigStore().validate(data)

        data['rdt_iface']['interface'] = 2
        with pytest.raises(jsonschema.exceptions.ValidationError, match="2 is not of type 'string'"):
            ConfigStore().validate(data)

        data['rdt_iface']['interface'] = "test_string"
        with pytest.raises(jsonschema.exceptions.ValidationError, match="'test_string' is not one of \\['msr', 'os'\\]"):
            ConfigStore().validate(data)


    def test_mba_ctrl_invalid(self):
        data = Config({
            "pools": [],
            "apps": [],
            "mba_ctrl": True
        })

        with pytest.raises(jsonschema.exceptions.ValidationError, match="True is not of type 'object'"):
            ConfigStore().validate(data)

        data['mba_ctrl'] = {}
        with pytest.raises(jsonschema.exceptions.ValidationError, match="'enabled' is a required property"):
            ConfigStore().validate(data)

        data['mba_ctrl']['enabled'] = None
        with pytest.raises(jsonschema.exceptions.ValidationError, match="None is not of type 'boolean'"):
            ConfigStore().validate(data)

        data['mba_ctrl']['enabled'] = 2
        with pytest.raises(jsonschema.exceptions.ValidationError, match="2 is not of type 'boolean'"):
            ConfigStore().validate(data)

        data['mba_ctrl']['enabled'] = "test_string"
        with pytest.raises(jsonschema.exceptions.ValidationError, match="'test_string' is not of type 'boolean'"):
            ConfigStore().validate(data)

        data['mba_ctrl']['enabled'] = True
        with pytest.raises(ValueError, match="MBA CTRL requires RDT OS interface"):
            ConfigStore().validate(data)

        data['rdt_iface'] = {"interface": "msr"}
        with pytest.raises(ValueError, match="MBA CTRL requires RDT OS interface"):
            ConfigStore().validate(data)