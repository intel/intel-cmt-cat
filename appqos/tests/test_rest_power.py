################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
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
Unit tests for rest module POWER PROFILES
"""

from copy import deepcopy

import json
from jsonschema import validate
import mock
import pytest
import time

import common
import caps
import rest.rest_power

from rest_common import get_config, load_json_schema, REST, Rest, CONFIG, CONFIG_EMPTY


class TestRestPowerProfiles:
    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG))
    @mock.patch("caps.SYSTEM_CAPS", ['cat', 'mba', 'power'])
    def test_get(self):
        response = REST.get("/caps")
        data = json.loads(response.data.decode('utf-8'))

        # validate response schema
        schema, resolver = load_json_schema('get_caps_response.json')
        validate(data, schema, resolver=resolver)

        assert response.status_code == 200
        assert 'cat' in data["capabilities"]
        assert 'mba' in data["capabilities"]
        assert 'power' in data["capabilities"]


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG))
    @mock.patch("rest.rest_power._get_power_profiles_expert_mode", mock.MagicMock(return_value=True))
    @mock.patch("caps.sstcp_enabled", mock.MagicMock(return_value=False))
    def test_epp_unsupported_get_post(self):
        response = Rest().get("/power_profiles")
        assert response.status_code == 404

        response = Rest().post("/power_profiles", {"max_freq": 2000, "min_freq": 1000, "epp": "power"})
        assert response.status_code == 404


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG))
    @mock.patch("rest.rest_power._get_power_profiles_expert_mode", mock.MagicMock(return_value=True))
    @mock.patch("caps.sstcp_enabled", mock.MagicMock(return_value=True))
    def test_existing_profiles_operations_get_del(self):
        response = Rest().get("/power_profiles")
        assert response.status_code == 200

        data = json.loads(response.data.decode('utf-8'))
        assert CONFIG['power_profiles'] == data

        used_profiles_ids = []
        unused_profiles_id = None
        for pool in CONFIG['pools']:
            if 'power_profile' in pool:
                used_profiles_ids.append(pool['power_profile'])

        for profile in CONFIG['power_profiles']:
            response = Rest().get("/power_profiles/{}".format(profile['id']))
            assert response.status_code == 200

            data = json.loads(response.data.decode('utf-8'))
            assert profile == data

            if profile['id'] not in used_profiles_ids:
                unused_profiles_id = profile['id']

        for id in used_profiles_ids:
            response = Rest().delete("/power_profiles/{}".format(id))
            assert response.status_code == 400

        def mock_set_config(config):
            for profile in config['power_profiles']:
                assert profile['id'] != unused_profiles_id

        with mock.patch("common.CONFIG_STORE.set_config", new=mock_set_config):
            response = Rest().delete("/power_profiles/{}".format(unused_profiles_id))
            assert response.status_code == 200


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG))
    @mock.patch("rest.rest_power._get_power_profiles_expert_mode", mock.MagicMock(return_value=True))
    @mock.patch("caps.sstcp_enabled", mock.MagicMock(return_value=True))
    def test_existing_profiles_operations_put(self):

        put_profile = CONFIG['power_profiles'][-1]

        def set_config(config):
            for profile in config['power_profiles']:
                if profile['id'] == put_profile['id']:
                    assert profile != put_profile
                    break

        with mock.patch("common.CONFIG_STORE.set_config", side_effect=set_config) as mock_set_config,\
            mock.patch("common.CONFIG_STORE.validate"):
            for key, value in put_profile.items():

                if key in ["min_freq", "max_freq"]:
                    value +=100
                elif key in ["epp", "name"]:
                    value = value + "_test"

                response = Rest().put("/power_profiles/{}".format(put_profile['id']), {key: value})
                if key == "id":
                    assert response.status_code == 400
                else:
                    assert response.status_code == 200
                    mock_set_config.assert_called()


        with mock.patch("common.CONFIG_STORE.validate", side_effect = Exception('Test Exception!')),\
            mock.patch("common.CONFIG_STORE.set_config") as mock_set_config:
            for key, value in put_profile.items():

                if key in ["min_freq", "max_freq"]:
                    value +=100
                elif key in ["epp", "name"]:
                    value = value + "_test"

                response = Rest().put("/power_profiles/{}".format(put_profile['id']), {key: value})
                assert response.status_code == 400
                mock_set_config.assert_not_called()


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG))
    @mock.patch("rest.rest_power._get_power_profiles_expert_mode", mock.MagicMock(return_value=True))
    @mock.patch("caps.sstcp_enabled", mock.MagicMock(return_value=True))
    def test_nonexisting_profile_operations_get_del_put(self):
        response = Rest().get("/power_profiles")
        assert response.status_code == 200

        data = json.loads(response.data.decode('utf-8'))
        assert CONFIG['power_profiles'] == data

        ids = []
        for profile in CONFIG['power_profiles']:
            ids.append(profile['id'])

        assert ids

        response = Rest().get("/power_profiles/{}".format(sorted(ids)[-1] + 1))
        assert response.status_code == 404

        response = Rest().delete("/power_profiles/{}".format(sorted(ids)[-1] + 1))
        assert response.status_code == 404

        response = Rest().put("/power_profiles/{}".format(sorted(ids)[-1] + 1), {"name": "Test"})
        assert response.status_code == 404


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG_EMPTY))
    @mock.patch("rest.rest_power._get_power_profiles_expert_mode", mock.MagicMock(return_value=True))
    @mock.patch("caps.sstcp_enabled", mock.MagicMock(return_value=True))
    def test_get_epp_no_profiles_configured(self):
        response = Rest().get("/power_profiles")
        assert response.status_code == 404

        for id in range(5):
            response = Rest().get("/power_profiles/{}".format(id))
            assert response.status_code == 404

            response = Rest().delete("/power_profiles/{}".format(id))
            assert response.status_code == 404

            response = Rest().put("/power_profiles/{}".format(id), {"name": "Test"})
            assert response.status_code == 404


    @mock.patch("caps.sstcp_enabled", mock.MagicMock(return_value=True))
    @mock.patch("rest.rest_power._get_power_profiles_expert_mode", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("invalid_id", ["", "a", "test", "-1"])
    def test_invalid_index(self, invalid_id):

        response = Rest().get("/power_profiles/{}".format(invalid_id))
        assert response.status_code == 404

        response = Rest().delete("/power_profiles/{}".format(invalid_id))
        assert response.status_code == 404

        response = Rest().put("/power_profiles/{}".format(invalid_id), {"name": "Test"})
        assert response.status_code == 404


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG))
    @mock.patch("rest.rest_power._get_power_profiles_expert_mode", mock.MagicMock(return_value=True))
    @mock.patch("caps.sstcp_enabled", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("invalid_request", [
            {"id": 0, "min_freq": 1000, "max_freq": 2200, "epp": "performance", "name": "default"},
            {"id": 0},
            {"test": 5},
            {"min_freq": 1000, "max_freq": -2200, "epp": "performance", "name": "default"},
            {"min_freq": -1000, "max_freq": 2200, "epp": "performance", "name": "default"}
    ])
    def test_invalid_request(self, invalid_request):

        response = Rest().get("/power_profiles")
        assert response.status_code == 200

        data = json.loads(response.data.decode('utf-8'))
        assert CONFIG['power_profiles'] == data

        valid_id = None
        for profile in CONFIG['power_profiles']:
            valid_id = profile['id']
            break

        assert valid_id is not None

        response = Rest().put("/power_profiles/{}".format(valid_id), invalid_request)
        assert response.status_code == 400

        response = Rest().post("/power_profiles", invalid_request)
        assert response.status_code == 400


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG))
    @mock.patch("rest.rest_power._get_power_profiles_expert_mode", mock.MagicMock(return_value=True))
    @mock.patch("caps.sstcp_enabled", mock.MagicMock(return_value=True))
    def test_operations_post(self):

        post_profile = {"min_freq": 1000, "max_freq": 2200, "epp": "performance", "name": "test_profile"}
        post_profile_id = 10

        def set_config(config):
            for profile in config['power_profiles']:
                if profile['id'] == post_profile_id:
                    temp_profile = deepcopy(post_profile)
                    temp_profile.update({"id": post_profile_id})
                    assert profile == temp_profile
                    break

        with mock.patch("common.CONFIG_STORE.set_config", side_effect=set_config) as mock_set_config,\
            mock.patch("common.CONFIG_STORE.get_new_power_profile_id", return_value=post_profile_id) as mock_get_id,\
            mock.patch("common.CONFIG_STORE.validate") as mock_validate:
                response = Rest().post("/power_profiles", post_profile)
                assert response.status_code == 201
                mock_set_config.assert_called()
                mock_validate.assert_called()
                mock_get_id.assert_called()

        with mock.patch("common.CONFIG_STORE.set_config") as mock_set_config,\
            mock.patch("common.CONFIG_STORE.get_new_power_profile_id", return_value=post_profile_id) as mock_get_id,\
            mock.patch("common.CONFIG_STORE.validate", side_effect = Exception('Test Exception!')) as mock_validate:
                response = Rest().post("/power_profiles", post_profile)
                assert response.status_code == 400
                mock_validate.assert_called()
                mock_set_config.assert_not_called()


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG))
    @mock.patch("rest.rest_power._get_power_profiles_expert_mode", mock.MagicMock(return_value=False))
    @mock.patch("caps.sstcp_enabled", mock.MagicMock(return_value=True))
    def test_operations_post_expert_mode_disable_post(self):

        post_profile = {"min_freq": 1000, "max_freq": 2200, "epp": "performance", "name": "test_profile"}
        post_profile_id = 10

        with mock.patch("common.CONFIG_STORE.set_config") as mock_set_config,\
            mock.patch("common.CONFIG_STORE.get_new_power_profile_id", return_value=post_profile_id) as mock_get_id,\
            mock.patch("common.CONFIG_STORE.validate") as mock_validate:
                response = Rest().post("/power_profiles", post_profile)
                assert response.status_code == 405
                mock_set_config.assert_not_called()
                mock_validate.assert_not_called()
                mock_get_id.assert_not_called()


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG))
    @mock.patch("rest.rest_power._get_power_profiles_expert_mode", mock.MagicMock(return_value=False))
    @mock.patch("caps.sstcp_enabled", mock.MagicMock(return_value=True))
    def test_operations_post_expert_mode_disable_put_del(self):
        response = Rest().get("/power_profiles")
        assert response.status_code == 200

        data = json.loads(response.data.decode('utf-8'))
        assert CONFIG['power_profiles'] == data

        profile_ids = []

        for profile in CONFIG['power_profiles']:
            response = Rest().get("/power_profiles/{}".format(profile['id']))
            assert response.status_code == 200

            data = json.loads(response.data.decode('utf-8'))
            assert profile == data

            profile_ids.append(profile['id'])

            with mock.patch("common.CONFIG_STORE.set_config") as mock_set_config,\
                mock.patch("common.CONFIG_STORE.validate") as mock_validate:
                    response = Rest().put("/power_profiles/{}".format(profile['id']), {"name": "Test_name"})
                    assert response.status_code == 405
                    mock_set_config.assert_not_called()
                    mock_validate.assert_not_called()

        for id in profile_ids:
            with mock.patch("common.CONFIG_STORE.set_config") as mock_set_config,\
                mock.patch("common.CONFIG_STORE.validate") as mock_validate:
                    response = Rest().delete("/power_profiles/{}".format(id))
                    assert response.status_code == 405
                    mock_set_config.assert_not_called()
                    mock_validate.assert_not_called()
