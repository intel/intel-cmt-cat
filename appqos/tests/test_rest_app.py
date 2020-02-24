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
Unit tests for rest module APPs
"""

import json
from jsonschema import validate
import mock
import pytest

import common
import caps
import pid_ops

from rest_common import get_config, load_json_schema, REST, CONFIG_EMPTY


class TestApps:
    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    def test_get(self):
        response = REST.get("/apps")
        data = json.loads(response.data.decode('utf-8'))

        assert response.status_code == 200

        # validate get all pools response schema
        schema, resolver = load_json_schema('get_app_all_response.json')
        validate(data, schema, resolver=resolver)

        # assert 3 apps are returned
        # structure, types and required fields are validated using schema
        assert len(data) == 3


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG_EMPTY))
    def test_get_empty(self):
        response = REST.get("/apps")
        data = json.loads(response.data.decode('utf-8'))

        # assert 0 apps are returned
        assert response.status_code == 404
        assert "No apps in config" in data["message"]


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("pid_ops.is_pid_valid", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("power.validate_power_profiles", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("app_config", [
            {"pool_id": 2, "name":"hello","cores":[2, 3],"pids":[11]},
            {"pool_id": 2, "name":"hello", "pids": [12]}                                    # no cores
        ])
    def test_post(self, app_config):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock,\
             mock.patch('pid_ops.is_pid_valid', return_value=True):
            response = REST.post("/apps", app_config)
            func_mock.assert_called_once()
        data = json.loads(response.data.decode('utf-8'))

        # validate response schema
        schema, resolver = load_json_schema('add_app_response.json')
        validate(data, schema, resolver=resolver)

        assert response.status_code == 201
        assert 'id' in data


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("pid_ops.is_pid_valid", mock.MagicMock(return_value=True))
    @mock.patch("power.validate_power_profiles", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("app_config", [
        {"cores":[7,9]},                                                                # cores only
        {"pool_id": 2, "name": "hello", "cores": [2], "pids": [1], "unknown": [7,9]}    # unknown
    ])
    def test_post_badrequest(self, app_config):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock:
            response = REST.post("/apps", app_config)
            func_mock.assert_not_called()

        assert response.status_code == 400

    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=False))
    @mock.patch("pid_ops.is_pid_valid", mock.MagicMock(return_value=True))
    @mock.patch("power.validate_power_profiles", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("app_config", [
        {"pool_id": 2, "name":"hello", "cores":2, "pids":[1]},                          # invalid core
        {"pool_id": 2, "name":"hello", "cores":[-9], "pids":[1]},                       # invalid core
        {"pool_id": 2, "name": "hello", "cores": [22], "pids": [1]}                     # core does not match pool id
    ])
    def test_post_invalid_core(self, app_config):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock:
            response = REST.post("/apps", app_config)
            func_mock.assert_not_called()

        assert response.status_code == 400

    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("pid_ops.is_pid_valid", mock.MagicMock(return_value=False))
    @mock.patch("power.validate_power_profiles", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("app_config", [
        {"pool_id": 2, "name":"hello", "cores":[2], "pids":[199099]},                  # invalid PID
        {"pool_id": 2, "pids": 1},                                                     # invalid "pids" format
        {"pool_id": 2, "pids": [-1]},                                                  # invalid PID
    ])
    def test_post_invalid_pid(self, app_config):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock:
            response = REST.post("/apps", app_config)
            func_mock.assert_not_called()

        assert response.status_code == 400

    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG_EMPTY))
    def test_post_empty_config(self):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock:
            response = REST.post("/apps", {"pool_id": 2, "name":"hello", "cores":[50], "pids":[1]})
            func_mock.assert_not_called()

        assert response.status_code == 404


class TestApp_2:
    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    def test_get(self):
        response = REST.get("/apps/2")
        data = json.loads(response.data.decode('utf-8'))

        assert response.status_code == 200

        # validate get all pools response schema
        schema, resolver = load_json_schema('get_app_response.json')
        validate(data, schema, resolver=resolver)

        # assert 1 app is returned
        # structure, types and required fields are validated using schema
        assert data['id'] == 2

    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG_EMPTY))
    def test_get_empty(self):
        response = REST.get("/apps/2")
        data = json.loads(response.data.decode('utf-8'))

        # assert 0 apps are returned
        assert response.status_code == 404
        assert "No apps in config" in data["message"]


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    def test_delete_invalid_id(self):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock:
            response = REST.delete("/apps/10")
            func_mock.assert_not_called()
        data = json.loads(response.data.decode('utf-8'))

        # assert 0 apps are returned
        assert response.status_code == 404
        assert "APP 10 not found in config" in data["message"]


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    def test_delete(self):
        def set_config(data):
            for app in data['apps']:
                assert app['id'] != 2
            for pool in data['pools']:
                assert ('apps' not in pool) or (2 not in pool['apps'])

        with mock.patch('common.CONFIG_STORE.set_config', side_effect=set_config) as func_mock:
            response = REST.delete("/apps/2")
            func_mock.assert_called_once()

        assert response.status_code == 200


    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG_EMPTY))
    def test_delete_empty_config(self):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock:
            response = REST.delete("/apps/2")
            func_mock.assert_not_called()

        assert response.status_code == 404


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("pid_ops.is_pid_valid", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("power.validate_power_profiles", mock.MagicMock(return_value=True))
    def test_put_pool(self):
        def set_config(data):
            for pool in data['pools']:
                if pool['id'] == 2:
                    assert 2 in pool['apps']
                else:
                    assert ('apps' not in pool) or (2 not in pool['apps'])

        with mock.patch('common.CONFIG_STORE.set_config', side_effect=set_config) as func_mock:
            response = REST.put("/apps/2", {"pool_id": 2})
            func_mock.assert_called_once()

        assert response.status_code == 200


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("pid_ops.is_pid_valid", mock.MagicMock(return_value=True))
    @mock.patch("caps.cat_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("power.validate_power_profiles", mock.MagicMock(return_value=True))
    def test_put_cores(self):
        def set_config(data):
            for app in data['apps']:
                if app['id'] == 2:
                    assert app['cores'] == [3]

        with mock.patch('common.CONFIG_STORE.set_config', side_effect=set_config) as func_mock:
            response = REST.put("/apps/2", {"pool_id": 2, "cores": [3]})
            func_mock.assert_called_once()

        assert response.status_code == 200


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("pid_ops.is_pid_valid", mock.MagicMock(return_value=True))
    def test_put_invalid_pool(self):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock:
            response = REST.put("/apps/2", {"pool_id": 20})
            func_mock.assert_not_called()

        assert response.status_code == 400


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("pid_ops.is_pid_valid", mock.MagicMock(return_value=True))
    def test_put_invalid_app(self):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock:
            response = REST.put("/apps/20", {"pool_id": 2})
            func_mock.assert_not_called()

        assert response.status_code == 404


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("common.PQOS_API.check_core", mock.MagicMock(return_value=True))
    @mock.patch("pid_ops.is_pid_valid", mock.MagicMock(return_value=True))
    def test_put_invalid(self):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock:
            response = REST.put("/apps/2", {"invalid": 20})
            func_mock.assert_not_called()

        assert response.status_code == 400

    @mock.patch("common.CONFIG_STORE.get_config", mock.MagicMock(return_value=CONFIG_EMPTY))
    def test_put_empty_config(self):
        with mock.patch('common.CONFIG_STORE.set_config') as func_mock:
            response = REST.put("/apps/20", {"pool_id": 2})
            func_mock.assert_not_called()

        assert response.status_code == 404
