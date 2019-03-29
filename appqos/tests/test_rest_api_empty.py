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
import json
from config import *
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

class Empty_RESTAPI(object):

    # Test REST API with empty config
    def __init__(self):
        self.user = 'superadmin'
        self.password = 'secretsecret'
        self.tcp = "https"
        self.host = 'localhost'
        self.address = '0.0.0.0'
        self.port = 5001

    def start_flask(self):
        common.CONFIG_STORE.set_config({"auth": {"username": self.user, "password":self.password}})
        # start process to run flask in the background
        server = rest.Server()
        server.start(self.address, self.port, True)
        return server

    def api_requests(self, method, endpoint, data={}):

        url = '%s://%s:%d/%s' % (self.tcp, self.host, self.port, endpoint)

        r = requests.request(method, url, json=data, auth=(self.user, self.password), verify=False)

        return (r.status_code, r.content.decode('utf-8'))

@pytest.fixture(scope="session")
def my_app():
    my_app = Empty_RESTAPI()
    p = my_app.start_flask()
    time.sleep(1)
    yield my_app
    p.terminate()

def test_get_apps_empty(my_app):
    status, rawData = my_app.api_requests('GET','apps')

    data = json.loads(rawData)

    # assert 0 apps are returned
    # structure, types and required fields are validated using schema
    assert "No apps in config file" in data["message"]
    assert status == 404

def test_get_app_empty(my_app):

    status, rawData = my_app.api_requests('GET','apps/1')

    data = json.loads(rawData)

    # assert no app is returned
    # structure, types and required fields are validated using schema
    assert "No apps in config file" in data["message"]
    assert status == 404

def test_get_pools_empty(my_app):

    status, rawData = my_app.api_requests('GET','pools')

    data = json.loads(rawData)

    # assert 0 pools are returned
    # structure, types and required fields are validated using schema
    assert "No pools in config file" in data["message"]
    assert status == 404


def test_get_pool_empty(my_app):

    status, rawData = my_app.api_requests('GET','pools/3')

    data = json.loads(rawData)

    # assert no pool is returned
    # structure, types and required fields are validated using schema
    assert "No pools in config file" in data["message"]
    assert status == 404


def test_add_app(my_app):

    status, rawData = my_app.api_requests('POST', 'apps', {"pool_id": 2, "name": "hello", "cores": [1,2], "pids": [1324,124]})

    data = json.loads(rawData)
    print(data)

    assert status == 404

def test_move_app_to_pool(my_app):

    status, rawData = my_app.api_requests('PUT', 'apps/1', {"pool_id": 2})

    assert status == 404

def test_delete_app(my_app):

    status, rawData = my_app.api_requests('DELETE', 'apps/1')

    assert status == 404


def test_authentication(my_app):
    my_app.user = "good"
    my_app.password = "morning"
    status, rawData = my_app.api_requests('GET', 'apps/2')

    assert rawData == "Unauthorized Access"
