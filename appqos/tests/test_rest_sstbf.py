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
Unit tests for rest module SSTBF
"""

import json
from jsonschema import validate
import mock
import pytest

import common
import caps

from rest_common import get_config, load_json_schema, Rest


class TestSstbf:

    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("sstbf.get_hp_cores", mock.MagicMock(return_value=[0,1,2,3]))
    @mock.patch("sstbf.get_std_cores", mock.MagicMock(return_value=[4,5,6,7]))
    @mock.patch("caps.sstbf_enabled", mock.MagicMock(return_value=True))
    def test_get_sstbf(self):
        for ret_val in [True, False]:
           with mock.patch("sstbf.is_sstbf_configured", return_value=ret_val):
               response = Rest().get("/caps/sstbf")

               assert response.status_code == 200

               data = json.loads(response.data.decode('utf-8'))

               # validate response schema
               schema, resolver = load_json_schema('get_sstbf_response.json')
               validate(data, schema, resolver=resolver)

               params = ['configured', 'hp_cores', 'std_cores']

               assert len(data) == len(params)

               for param in params:
                   assert param in data

               assert data['configured'] == ret_val
               assert data['hp_cores'] == [0,1,2,3]
               assert data['std_cores'] == [4,5,6,7]


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.sstbf_enabled", mock.MagicMock(return_value=False))
    def test_get_sstbf_unsupported(self):
        response = Rest().get("/caps/sstbf")
        assert response.status_code == 404

        response = Rest().put("/caps/sstbf", {"configured": True})
        assert response.status_code == 404


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("sstbf.get_hp_cores", mock.MagicMock(return_value=[0,1,2,3]))
    @mock.patch("sstbf.get_std_cores", mock.MagicMock(return_value=[4,5,6,7]))
    @mock.patch("caps.sstbf_enabled", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("configured_value", [
        True,
        False
    ])
    def test_put_sstbf(self, configured_value):
        with mock.patch("sstbf.configure_sstbf", return_value=0) as func_mock:
            response = Rest().put("/caps/sstbf", {"configured": configured_value})
            assert response.status_code == 200
            func_mock.called_once_with(configured_value)


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.sstbf_enabled", mock.MagicMock(return_value=True))
    @pytest.mark.parametrize("invalid_fields_json", [
        {"configured": 10},
        {"configured": -1},
        {"configured": 0},
        {"configured": "true"},
        {"Enabled": True},
        {"configured": True, "extra_field": 1},
        {"configured": True, "hp_cores": [1,2,3]},
        {"configured": False, "std_cores": [1,2,3]}
    ])
    def test_put_sstbf_invalid(self, invalid_fields_json):
        with mock.patch("sstbf.configure_sstbf", return_value=0) as func_mock:
            response = Rest().put("/caps/sstbf", invalid_fields_json)
            # expect 400 BAD REQUEST
            assert response.status_code == 400
            func_mock.assert_not_called()


    @mock.patch("common.CONFIG_STORE.get_config", new=get_config)
    @mock.patch("caps.sstbf_enabled", mock.MagicMock(return_value=True))
    def test_put_sstbf_enable_failed(self):
        with mock.patch("sstbf.configure_sstbf", return_value=-1) as func_mock:
            response = Rest().put("/caps/sstbf", {"configured": True})
            assert response.status_code == 500
            func_mock.assert_called()