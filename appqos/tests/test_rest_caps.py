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
Unit tests for rest module CAPS
"""

import json
from jsonschema import validate
import mock
import pytest

import common
import caps

from rest_common import get_config, get_config_empty, load_json_schema, REST, Rest, CONFIG_EMPTY


class TestCaps:
    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.SYSTEM_CAPS", ['cat', 'mba', 'sstbf'])
    def test_get(self):
        response = REST.get("/caps")
        data = json.loads(response.data.decode('utf-8'))

        # validate response schema
        schema, resolver = load_json_schema('get_caps_response.json')
        validate(data, schema, resolver=resolver)

        assert response.status_code == 200
        assert 'cat' in data["capabilities"]
        assert 'mba' in data["capabilities"]
        assert 'sstbf' in data["capabilities"]


    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=False))
    def test_caps_mba_mba_not_supported_get_put_post(self):
        response = Rest().get("/caps/mba")
        assert response.status_code == 404

        response = Rest().put("/caps/mba", {"mba_enabled": True})
        assert response.status_code == 404

        response = Rest().post("/caps/mba", {"mba_bw_enabled": True})
        assert response.status_code == 404


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("mba_bw_enabled", [True, False])
    def test_caps_mba_get(self, mba_bw_enabled):

        with mock.patch("caps.mba_bw_enabled", return_value=mba_bw_enabled):

            response = Rest().get("/caps/mba")
            assert response.status_code == 200

            data = json.loads(response.data.decode('utf-8'))

            # validate response schema
            schema, resolver = load_json_schema('get_caps_mba_response.json')
            validate(data, schema, resolver=resolver)

            params = ['mba_enabled', 'mba_bw_enabled']
            assert len(data) == len(params)
            for param in params:
                assert param in data

            assert not data['mba_enabled'] == mba_bw_enabled
            assert data['mba_bw_enabled'] == mba_bw_enabled


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("mba_bw_enabled", [True, False])
    def test_caps_mba_put_post(self, mba_bw_enabled):

        with mock.patch("caps.mba_bw_enabled", return_value=mba_bw_enabled):
            response = Rest().put("/caps/mba", {"mba_enabled": True})
            assert response.status_code == 405

            response = Rest().post("/caps/mba", {"mba_bw_enabled": True})
            assert response.status_code == 405


    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=False))
    def test_caps_mba_ctrl_mba_not_supported_get_put_post(self):
        response = Rest().get("/caps/mba_ctrl")
        assert response.status_code == 404

        response = Rest().put("/caps/mba_ctrl", {"enabled": True})
        assert response.status_code == 404

        response = Rest().post("/caps/mba_ctrl", {"enabled": True})
        assert response.status_code == 404


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("mba_bw_supported", [True, False])
    @pytest.mark.parametrize("mba_bw_enabled", [True, False])
    def test_caps_mba_ctrl_get(self, mba_bw_supported, mba_bw_enabled):

        with mock.patch("caps.mba_bw_supported", return_value=mba_bw_supported),\
                mock.patch("caps.mba_bw_enabled", return_value=mba_bw_enabled):

            response = Rest().get("/caps/mba_ctrl")
            assert response.status_code == 200

            data = json.loads(response.data.decode('utf-8'))

            # validate response schema
            schema, resolver = load_json_schema('get_caps_mba_ctrl_response.json')
            validate(data, schema, resolver=resolver)

            params = ['supported', 'enabled']
            assert len(data) == len(params)
            for param in params:
                assert param in data

            assert data['supported'] == mba_bw_supported
            assert data['enabled'] == mba_bw_enabled


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("invalid_request", [
            {},
            {"enabled": 1},
            {"enabled": "True"},
            {"enabled": {}},
            {"enabled": {"enabled": True}},
            {"enabled": True, "supported": False},
            {"enabled": False, "supported": True},
            {"supported": True}
    ])
    def test_caps_mba_ctrl_invalid_request_put(self, invalid_request):
        response = Rest().put("/caps/mba_ctrl", invalid_request)
        assert response.status_code == 400


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=False))
    @pytest.mark.parametrize("valid_request", [
            {"enabled": True},
            {"enabled": False}
    ])
    def test_caps_mba_ctrl_mba_bw_not_supported_put(self, valid_request):
        response = Rest().put("/caps/mba_ctrl", valid_request)
        # MBA BW/CTRL not supported
        assert response.status_code == 409


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("valid_request", [
            {"enabled": True},
            {"enabled": False}
    ])
    def test_caps_mba_ctrl_pools_configured_put(self, valid_request):
        response = Rest().put("/caps/mba_ctrl", valid_request)
        # MBA BW/CTRL supported but Pools configured
        assert response.status_code == 409


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config_empty)
    @mock.patch("caps.mba_supported", mock.MagicMock(return_value=True))
    @mock.patch("caps.mba_bw_supported", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("valid_request", [
            {"enabled": True},
            {"enabled": False}
    ])
    def test_caps_mba_ctrl_put(self, valid_request):
        response = Rest().put("/caps/mba_ctrl", valid_request)
        # All OK, MBA BW/CTRL supported and no pool configured
        # HTTP OK 200 Expected here, but getting 501 Not Implemented for now
        assert response.status_code == 501


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @pytest.mark.parametrize("rdt_iface", ["msr", "os"])
    @pytest.mark.parametrize("rdt_iface_supported", [["msr"], ["msr", "os"]])
    def test_caps_rdt_iface_get(self, rdt_iface, rdt_iface_supported):

        with mock.patch("caps.rdt_iface", return_value=rdt_iface),\
                mock.patch("caps.rdt_iface_supported", return_value=rdt_iface_supported):
            response = Rest().get("/caps/rdt_iface")
            assert response.status_code == 200

            data = json.loads(response.data.decode('utf-8'))

            # validate response schema
            schema, resolver = load_json_schema('get_caps_rdt_iface_response.json')
            validate(data, schema, resolver=resolver)

            params = ['interface', 'interface_supported']
            assert len(data) == len(params)
            for param in params:
                assert param in data

            assert data['interface'] == rdt_iface
            assert data['interface_supported'] == rdt_iface_supported


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.rdt_iface_supported", mock.MagicMock(return_value=["msr", "os"]))
    @pytest.mark.parametrize("valid_request", [
            {"interface": "msr"},
            {"interface": "os"}
    ])
    def test_caps_rdt_iface_pools_configured_put(self, valid_request):
        response = Rest().put("/caps/rdt_iface", valid_request)
        # Requested RDT interface supported but Pools configured
        assert response.status_code == 409


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config_empty)
    @mock.patch("caps.rdt_iface_supported", mock.MagicMock(return_value=["msr"]))
    def test_caps_rdt_iface_not_supported_put(self):
        response = Rest().put("/caps/rdt_iface", {"interface": "os"})
        # No pool configured, but requested RDT interface not supported
        assert response.status_code == 400


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config_empty)
    @mock.patch("caps.rdt_iface_supported", mock.MagicMock(return_value=["msr", "os"]))
    @pytest.mark.parametrize("valid_request", [
            {"interface": "msr"},
            {"interface": "os"}
    ])
    def test_caps_rdt_iface_put(self, valid_request):
        response = Rest().put("/caps/rdt_iface", valid_request)
        # All OK, Requested RDT interface supported and no pool configured
        # HTTP OK 200 Expected here, but getting 501 Not Implemented for now
        assert response.status_code == 501


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.rdt_iface_supported", mock.MagicMock(return_value=["msr", "os"]))
    @pytest.mark.parametrize("invalid_request", [
            {},
            {"interface": 1},
            {"interface": "resctrl"},
            {"interface": "True"},
            {"interface": {}},
            {"interface": {"interface": "msr"}},
            {"interface": "os", "interface_supported": ["msr", "os"]},
            {"interface": "msr", "interface_supported": {}},
            {"interface": ["msr"]}
    ])
    def test_caps_rdt_iface_invalid_request_put(self, invalid_request):
        response = Rest().put("/caps/rdt_iface", invalid_request)
        assert response.status_code == 400

