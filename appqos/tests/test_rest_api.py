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
import json
import common
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


CONFIG =                               \
{                                      \
    "apps": [                          \
        {                              \
            "cores": [1],              \
            "id": 1,                   \
            "name": "app 1",           \
            "pids": [1]                \
        },                             \
        {                              \
            "cores": [3],              \
            "id": 2,                   \
            "name": "app 2",           \
            "pids": [2, 3]             \
        },                             \
        {                              \
            "cores": [3],              \
            "id": 3,                   \
            "name": "app 3",           \
            "pids": [4]                \
        }                              \
    ],                                 \
    "auth": {                          \
        "password": "secretsecret",    \
        "username": "superadmin"       \
    },                                 \
    "pools": [                         \
        {                              \
            "apps": [1],               \
            "cbm": 0xf0,               \
            "cores": [1],              \
            "id": 1,                   \
            "mba": 20,                 \
            "name": "cat&mba"          \
        },                             \
        {                              \
            "apps": [2, 3],            \
            "cbm": 0xf,                \
            "cores": [3],              \
            "id": 2,                   \
            "name": "cat"              \
        },                             \
        {                              \
            "id": 3,                   \
            "mba": 30,                 \
            "name": "mba"              \
        }                              \
    ]                                  \
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

    # assert 3 pools are returned
    # structure, types and required fields are validated using schema
    assert len(data) ==3
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


def test_pool_delete_empty(my_app):
    status, rawData = my_app.api_requests('DELETE', 'pools/3')
    assert status == 200

    status, rawData = my_app.api_requests('GET', 'pools/3')
    assert status == 404


def test_pool_delete_not_empty(my_app):
    status, rawData = my_app.api_requests('DELETE', 'pools/2')
    assert status == 400


def test_pool_add_cbm(my_app):
    status, rawData = my_app.api_requests('POST', 'pools', {"name":"hello", "cores":[6, 7], "cbm": "0xf"})
    assert status == 201

    data = json.loads(rawData)

    print data

    #validate add app response schema
    schema, resolver = my_app._load_json_schema('add_pool_response.json')
    validate(data, schema, resolver=resolver)

    # assert app is added
    # structure, types and required fields are validated using schema
    assert len(data) == 1 # 1 fields in dict
    assert 'id' in data


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