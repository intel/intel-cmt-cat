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

import json
import common
import cache_ops
import config
import rest

from jsonschema  import validate, RefResolver
from os.path import join, dirname

import ssl
import os
import time
import multiprocessing as mp
import requests
from requests.auth import HTTPBasicAuth

# suppress warning from requests
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

MAX_L3CA_COS_ID = 15
MAX_MBA_COS_ID = 7

CONFIG = {
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
        "password": "secretsecret",
        "username": "superadmin"
    },
    "pools": [
        {
            "cores": [20,21,22],
            "id": 0,
            "mba": 100,
            "name": "Default"
        },
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
            "cores": [2, 3],
            "id": 2,
            "name": "cat"
        },
        {
            "id": 3,
            "mba": 30,
            "name": "mba",
            "cores": [4]
        },
        {
            "id": 4,
            "mba": 30,
            "cores": [5]
        }
    ]
}


class RESTAPI(object):
    def __init__(self):
        self.user = 'superadmin'
        self.password = 'secretsecret'
        self.tcp = "https"
        self.host = 'localhost'
        self.address = '0.0.0.0'
        self.port = 6000

    def start_flask(self):
        def get_max_cos_id(alloc_type):
            if 'mba' in alloc_type:
                return MAX_MBA_COS_ID
            else:
                return MAX_L3CA_COS_ID

        with mock.patch('cache_ops.PQOS_API.get_max_cos_id', new=get_max_cos_id):
            # start process to run flask in the background
            server = rest.Server()
            server.start(self.address, self.port, True)
            return server

    def api_requests(self, method, endpoint, data={}):

        url = '%s://%s:%d/%s' % (self.tcp, self.host, self.port, endpoint)

        r = requests.request(method, url, json=data, auth=(self.user, self.password), verify=False)

        return (r.status_code, r.content)

    def _load_json_schema(self, filename):
        """ Loads the given schema file """
        # find path to schema
        relative_path = join('schema', filename)
        absolute_path = join(dirname(dirname(__file__)), relative_path)
        # path to all schema files
        schema_path = 'file:' + str(join(dirname(dirname(__file__)), 'schema')) + '/'
        with open(absolute_path) as schema_file:
            # add resolver for python to find all schema files
            schema = json.loads(schema_file.read())
            return schema, RefResolver(schema_path, schema)

@pytest.fixture(scope="session")
def my_app():
    my_app = RESTAPI()
    p = my_app.start_flask()
    time.sleep(1)
    yield my_app
    p.terminate()

@pytest.fixture(autouse=True)
def set_config():
    #restore config
    common.CONFIG_STORE.set_config(CONFIG.copy())


def test_apps_get(my_app):
    status, rawData = my_app.api_requests('GET', 'apps')

    data = json.loads(rawData)

    # validate all apps response schema
    schema, resolver = my_app._load_json_schema('get_app_all_response.json')
    validate(data, schema, resolver=resolver)

    # assert 3 apps are returned
    # structure, types and required fields are validated using schema
    assert len(data) == 3
    assert status == 200

def test_app_get(my_app):
    status, rawData = my_app.api_requests('GET', 'apps/2')

    data = json.loads(rawData)

    #validate get single app response schema
    schema, resolver = my_app._load_json_schema('get_app_response.json')
    validate(data, schema, resolver=resolver)

    # assert 1 app is returned
    # structure, types and required fields are validated using schema
    assert data['id'] == 2
    assert status == 200

def test_pools_get(my_app):
    status, rawData = my_app.api_requests('GET', 'pools')

    data = json.loads(rawData)

    #validate get all pools response schema
    schema, resolver = my_app._load_json_schema('get_pool_all_response.json')
    validate(data, schema, resolver=resolver)

    # assert 5 pools are returned
    # structure, types and required fields are validated using schema
    assert len(data) == 5
    assert status == 200


def test_pool_get(my_app):
    status, rawData = my_app.api_requests('GET', 'pools/3')

    data = json.loads(rawData)

    #validate get 1 pool response schema
    schema, resolver = my_app._load_json_schema('get_pool_response.json')
    validate(data, schema, resolver=resolver)

    # assert 1 pool is returned
    # structure, types and required fields are validated using schema
    assert data['id'] == 3
    assert status == 200


@pytest.mark.parametrize("empty_pool_id", [
    3,
    4
])

def test_pool_delete_empty(my_app, empty_pool_id):
    status, rawData = my_app.api_requests('DELETE', 'pools/' + str(empty_pool_id))
    assert status == 200

    status, rawData = my_app.api_requests('GET', 'pools/' + str(empty_pool_id))
    assert status == 404


@pytest.mark.parametrize("not_empty_pool_id", [
    1,
    2
])
def test_pool_delete_not_empty(my_app, not_empty_pool_id):
    status, rawData = my_app.api_requests('DELETE', 'pools/' + str(not_empty_pool_id))
    assert status == 400


def test_pool_delete_default(my_app):
    status, rawData = my_app.api_requests('GET', 'pools/0')
    assert status == 200

    status, rawData = my_app.api_requests('DELETE', 'pools/0')
    assert status == 400


@pytest.mark.parametrize("no_req_fields_json", [
    {"mba": 10},                                # missing cores
    {"cbm": "0xf"},                             # missing cores
    {"mba": 10, "cbm": "0xf"},                  # missing cores
    {"name":"hello", "mba": 10, "cbm": "0xf"},  # missing cores
    {"name":"hello", "cbm": "0xf"},             # missing cores

    {"cores":[3, 10]},                  # missing at least one alloc technology
    {"name":"hello", "cores":[3, 10]},  # missing at least one alloc technology

    {"name":"hello"},                   # missing at least one alloc technology and cores

    {"cores":[3, 10], "cbm": "0xf", "apps":[1]} # extra property "apps"
])
def test_pool_add_no_req_fields(my_app, no_req_fields_json):
    status, rawData = my_app.api_requests('POST', 'pools', no_req_fields_json)
    assert status == 400


def test_pool_add_no_name(my_app):
    status, rawData = my_app.api_requests('POST', 'pools', {"cores":[1, 2], "cbm": "0xf"})
    assert status == 201


def test_pool_add_cbm(my_app):
    status, rawData = my_app.api_requests('POST', 'pools', {"name":"hello", "cores":[13, 17], "cbm": "0xf"})
    assert status == 201

    data = json.loads(rawData)

    #validate add pool response schema
    schema, resolver = my_app._load_json_schema('add_pool_response.json')
    validate(data, schema, resolver=resolver)

    # assert pool is added
    # structure, types and required fields are validated using schema
    assert len(data) == 1 # 1 fields in dict
    assert 'id' in data

    pool_id = data['id']

    status, rawData = my_app.api_requests('GET', 'pools/' + str(pool_id))
    assert status == 200

    data = json.loads(rawData)

    #validate get pool response schema
    schema, resolver = my_app._load_json_schema('get_pool_response.json')
    validate(data, schema, resolver=resolver)

    assert len(data) == 4

    assert data['id'] == pool_id
    assert data['name'] == "hello"
    assert data['cores'] == [13, 17]
    assert data['cbm'] == 0xf
    assert 'mba' not in data


def test_pool_add_mba(my_app):
    status, rawData = my_app.api_requests('POST', 'pools', {"name":"hello_mba", "cores":[1, 6, 7], "mba": 50})
    assert status == 201

    data = json.loads(rawData)

    #validate add pool response schema
    schema, resolver = my_app._load_json_schema('add_pool_response.json')
    validate(data, schema, resolver=resolver)

    # assert pool is added
    # structure, types and required fields are validated using schema
    assert len(data) == 1 # 1 fields in dict
    assert 'id' in data

    pool_id = data['id']

    status, rawData = my_app.api_requests('GET', 'pools/' + str(data['id']))
    assert status == 200

    data = json.loads(rawData)

    #validate get pool response schema
    schema, resolver = my_app._load_json_schema('get_pool_response.json')
    validate(data, schema, resolver=resolver)

    assert len(data) == 4

    assert data['id'] == pool_id
    assert data['name'] == "hello_mba"
    assert data['cores'] == [1, 6, 7]
    assert data['mba'] == 50
    assert 'cbm' not in data


def test_pool_add_mba_cbm(my_app):
    status, rawData = my_app.api_requests('POST', 'pools', {"name":"hello_mba_cbm", "cores":[1, 2, 6, 7], "mba": 50, "cbm": "0xf0"})
    assert status == 201

    data = json.loads(rawData)

    #validate add pool response schema
    schema, resolver = my_app._load_json_schema('add_pool_response.json')
    validate(data, schema, resolver=resolver)

    # assert pool is added
    # structure, types and required fields are validated using schema
    assert len(data) == 1 # 1 fields in dict
    assert 'id' in data

    pool_id = data['id']

    status, rawData = my_app.api_requests('GET', 'pools/' + str(pool_id))
    assert status == 200

    data = json.loads(rawData)

    #validate get pool response schema
    schema, resolver = my_app._load_json_schema('get_pool_response.json')
    validate(data, schema, resolver=resolver)

    assert len(data) == 5

    assert data['id'] == pool_id
    assert data['name'] == "hello_mba_cbm"
    assert data['cores'] == [1, 2, 6, 7]
    assert data['mba'] == 50
    assert data['cbm'] == 0xf0


def test_pool_add_multiple(my_app):
    # create new pools
    pools = [
        {"name":"pool_1", "cores":[6], "mba": 10, "cbm": "0x1"},
        {"cores":[7], "mba": 20, "cbm": "0x2"},
        {"name":"pool_3", "cores":[8], "cbm": "0x4"},
        {"name":"pool_4", "cores":[9], "cbm": "0x40"},
        {"cores":[10, 11], "cbm": "0x10"},
        {"name":"pool_6", "cores":[12, 13, 14], "cbm": "0x30"},
        {"name":"pool_7", "cores":[15], "cbm": "0x70"},
        {"name":"pool_10", "cores":[16], "mba": 70, "cbm": "0x37"},
        {"name":"pool_8", "cores":[17], "cbm": "0xf0"},
        {"name":"pool_9", "cores":[18], "cbm": "0x17"},
        {"name":"pool_11", "cores":[19], "cbm": "0x1f"}
    ]

    pool_ids = []

    for pool_data in pools:
        status, rawData = my_app.api_requests('POST', 'pools', pool_data)
        assert status == 201

        data = json.loads(rawData)

        #validate add pool response schema
        schema, resolver = my_app._load_json_schema('add_pool_response.json')
        validate(data, schema, resolver=resolver)

        # assert pool is added
        # structure, types and required fields are validated using schema
        assert len(data) == 1 # 1 fields in dict
        assert 'id' in data

        # check allocated pool_id/COS
        if 'mba' in pool_data:
            assert data['id'] <= MAX_MBA_COS_ID
        else:
            assert data['id'] <= MAX_L3CA_COS_ID

        pool_ids.append(data['id'])

        status, rawData = my_app.api_requests('GET', 'pools/' + str(pool_ids[-1]))
        assert status == 200

        data = json.loads(rawData)

        #validate add pool response schema
        schema, resolver = my_app._load_json_schema('get_pool_response.json')
        validate(data, schema, resolver=resolver)

        assert data['id'] == pool_ids[-1]

    # attempt add one pool over the limit
    status, rawData = my_app.api_requests('POST', 'pools', {"name":"pool_12", "cores":[25], "cbm": "0x17"})
    assert status == 500

    # no more pools than number of classes
    status, rawData = my_app.api_requests('GET', 'pools')
    data = json.loads(rawData)
    assert len(data) == max(MAX_L3CA_COS_ID, MAX_MBA_COS_ID) + 1

    # remove pools
    for pool_id in pool_ids:
        status, rawData = my_app.api_requests('DELETE', 'pools/' + str(pool_id))
        assert status == 200

        status, rawData = my_app.api_requests('GET', 'pools/' + str(pool_id))
        assert status == 404


def test_pool_set_cbm(my_app):
    status, rawData = my_app.api_requests('PUT', 'pools/2', {"cbm": "0xc"})
    assert status == 200

    status, rawData = my_app.api_requests('GET', 'pools/2')
    assert status == 200
    data = json.loads(rawData)

    #validate get pool response schema
    schema, resolver = my_app._load_json_schema('get_pool_response.json')
    validate(data, schema, resolver=resolver)

    assert data['cbm'] == 0xc

def test_pool_set_mba(my_app):
    status, rawData = my_app.api_requests('PUT', 'pools/2', {"mba": 30})
    assert status == 200

    status, rawData = my_app.api_requests('GET', 'pools/2')
    assert status == 200
    data = json.loads(rawData)

    #validate get pool response schema
    schema, resolver = my_app._load_json_schema('get_pool_response.json')
    validate(data, schema, resolver=resolver)

    assert data['mba'] == 30


def test_app_add_all(my_app):

    status, rawData = my_app.api_requests('POST', 'apps', {"pool_id": 2, "name":"hello","cores":[1,2],"pids":[1]})

    data = json.loads(rawData)

    print data

    #validate add app response schema
    schema, resolver = my_app._load_json_schema('add_app_response.json')
    validate(data, schema, resolver=resolver)

    # assert app is added
    # structure, types and required fields are validated using schema
    assert len(data) == 1 # 1 fields in dict
    assert 'id' in data
    assert status == 201

def test_app_add_cores_only(my_app):
    # cores only
    status, rawData = my_app.api_requests('POST', 'apps', {"cores":[7,9]})

    data = json.loads(rawData)
    # assert app is not added
    # structure, types and required fields are validated using schema
    assert "Request validation failed" in data["message"]
    assert status == 400

def test_app_add_unknown(my_app):
    # cores only
    status, rawData = my_app.api_requests('POST', 'apps', {"pool_id": 2, "name": "hello", "cores": [1,2], "pids": [1], "unknown": [7,9]})

    data = json.loads(rawData)
    # assert app is not added
    # structure, types and required fields are validated using schema
    assert status == 400
    assert "Request validation failed" in data["message"]

def test_app_add_no_pool_id(my_app):
    # no pool_id
    status, rawData = my_app.api_requests('POST', 'apps', {"name": "hello", "cores": [1,2], "pids": [1]})

    data = json.loads(rawData)
    # assert app is not added
    # structure, types and required fields are validated using schema
    assert "Request validation failed" in data["message"]
    assert status == 400

def test_app_add_no_name(my_app):
    # no name
    status, rawData = my_app.api_requests('POST', 'apps', {"pool_id": 2, "cores": [1,2], "pids": [1]})

    data = json.loads(rawData)
    # assert app is not added
    # structure, types and required fields are validated using schema
    assert "Request validation failed" in data["message"]
    assert status == 400

def test_app_add_no_cores(my_app):
    # no cores
    status, rawData = my_app.api_requests('POST', 'apps', {"pool_id": 2, "name": "hello", "pids": [1]})

    data = json.loads(rawData)
    # assert app is not added/no
    # structure, types and required fields are validated using schema
    assert "Request validation failed" in data["message"]
    assert status == 400

def test_app_add_invalid_cores(my_app):
    # invalid cores
    status, rawData = my_app.api_requests('POST', 'apps', {"pool_id": 2, "name": "hello", "cores": 1, "pids": [1]})

    data = json.loads(rawData)
    # assert app is not added/no
    # structure, types and required fields are validated using schema
    assert "Request validation failed" in data["message"]
    assert status == 400

    # invalid cores
    status, rawData = my_app.api_requests('POST', 'apps', {"pool_id": 2, "name": "hello", "cores": [], "pids": [1]})

    data = json.loads(rawData)
    # assert app is not added/no
    # structure, types and required fields are validated using schema
    assert "Request validation failed" in data["message"]
    assert status == 400

    # invalid cores
    status, rawData = my_app.api_requests('POST', 'apps', {"pool_id": 2, "name": "hello", "cores": [-1], "pids": [1]})

    data = json.loads(rawData)
    # assert app is not added/no
    # structure, types and required fields are validated using schema
    assert "Request validation failed" in data["message"]
    assert status == 400

def test_app_add_invalid_pids(my_app):
    # invalid pids
    status, rawData = my_app.api_requests('POST', 'apps', {"pool_id": 2, "name": "hello", "cores": [1,2], "pids": [1324,124545454545454]})

    data = json.loads(rawData)
    # assert app is not added
    # structure, types and required fields are validated using schema
    assert "please provide valid pid's" in data["message"]
    assert status == 400

def test_app_add_invalid(my_app):
    status, rawData = my_app.api_requests('POST', 'apps', 'invalid')

    assert status == 400


def test_app_move_to_pool(my_app):
    status, rawData = my_app.api_requests('PUT', 'apps/2', {"pool_id": 2})

    assert status == 200

    status, rawData = my_app.api_requests('GET', 'pools/2')

    data = json.loads(rawData)

    # assert 1 pool is returned with new app id
    # structure, types and required fields are validated using schema
    assert data['id'] == 2
    assert 2 in data['apps']
    assert status == 200

def test_app_move_no_pool_id(my_app):
    status, rawData = my_app.api_requests('PUT', 'apps/2')

    data = json.loads(rawData)

    assert status == 400
    assert "Request validation failed" in data["message"]


def test_app_delete(my_app):

    status, rawData = my_app.api_requests('DELETE', 'apps/2')
    assert status == 200

    status, rawData = my_app.api_requests('GET', 'apps/2')
    assert status == 404

    status, rawData = my_app.api_requests('GET', 'apps/3')
    assert status == 200

    status, rawData = my_app.api_requests('GET', 'pools/2')

    data = json.loads(rawData)

    # assert 1 app is returned
    # structure, types and required fields are validated using schema
    assert not 2 in data['apps']
    assert status == 200


def test_stats(my_app):
    status, rawData = my_app.api_requests('GET', 'stats')

    assert status == 200

    data = json.loads(rawData)
    assert 'num_apps_moves' in data
    assert 'num_err' in data