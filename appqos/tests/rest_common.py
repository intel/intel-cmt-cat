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
Unit tests for rest module
"""

import json
import logging
from os.path import join, dirname
from copy import deepcopy
import mock
import pytest
from requests.auth import _basic_auth_str
from jsonschema import validate, RefResolver

from rest import rest_server
import common
import caps
import pid_ops
from stats import StatsStore

logging.basicConfig(level=logging.DEBUG)

LOG = logging.getLogger('rest')

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
        "password": "password",
        "username": "user"
    },
    "pools": [
        {
            "cores": [20, 21, 22],
            "id": 0,
            "mba": 100,
            "name": "Default",
            "power_profile": 0
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
            "id": 15,
            "mba": 30,
            "name": "mba",
            "cores": [5],
            "power_profile": 1
        }
    ],
    "power_profiles": [
        {
            "id": 0,
            "min_freq": 1000,
            "max_freq": 2200,
            "epp": "performance",
            "name": "default"
        },
        {
            "id": 1,
            "min_freq": 1000,
            "max_freq": 1000,
            "epp": "power",
            "name": "low_priority"
        },
        {
            "id": 2,
            "min_freq": 2300,
            "max_freq": 3500,
            "epp": "performance",
            "name": "high_priority"
        }
    ]
}


CONFIG_EMPTY = {
    "auth": {
        "password": "password",
        "username": "user"
    }
}


def get_config():
    return deepcopy(CONFIG)

def get_max_cos_id(alloc_tech):
    if 'mba' in alloc_tech:
        return 8

    return 16


def load_json_schema(filename):
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


class Rest:
    def __init__(self):
        self.server = rest_server.Server()
        self.client = self.server.app.test_client()

    def get(self, url):
        response = self.client.get(url, headers={'Authorization': _basic_auth_str("user", "password")})

        LOG.info((">>> response code {}").format(response.status_code))
        LOG.info((">>> response data {}").format(response.data))

        return response

    def delete(self, url):
        response =  self.client.delete(url, headers={'Authorization': _basic_auth_str("user", "password")})

        LOG.info((">>> response code {}").format(response.status_code))
        LOG.info((">>> response data {}").format(response.data))

        return response

    def post(self, url, data=None):
        response = self.client.post(url, data=json.dumps(data), headers={
            'Authorization': _basic_auth_str("user", "password"),
            'Content-Type': 'application/json'
        })

        LOG.info((">>> response code {}").format(response.status_code))
        LOG.info((">>> response data {}").format(response.data))

        return response

    def put(self, url, data=None):
        response = self.client.put(url, data=json.dumps(data), headers={
            'Authorization': _basic_auth_str("user", "password"),
            'Content-Type': 'application/json'
        })

        LOG.info((">>> response code {}").format(response.status_code))
        LOG.info((">>> response data {}").format(response.data))

        return response

REST = Rest()
