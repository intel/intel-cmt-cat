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
import common
import jsonschema
import mock

from config import *

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
        "password": "password",        \
        "username": "admin"            \
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

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("pid, app", [
    (1, 1),
    (2, 2),
    (3, 2),
    (4, 3),
    (5, None),
    (None, None)
])
def test_config_pid_to_app(mock_get_config, pid, app):

    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.pid_to_app(pid) == app

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("app, pool_id", [
    (1, 1),
    (2, 2),
    (3, 2),
    (99, None),
    (None, None)
])
def test_config_app_to_pool(mock_get_config, app, pool_id):
    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.app_to_pool(app) == pool_id

@mock.patch('config.ConfigStore.get_config')
@pytest.mark.parametrize("pid, pool_id", [
    (1, 1),
    (2, 2),
    (3, 2),
    (4, 2),
    (1234, None),
    (None, None)
])
def test_config_pid_to_pool(mock_get_config, pid, pool_id):
    mock_get_config.return_value = CONFIG
    config_store = ConfigStore()

    assert config_store.pid_to_pool(pid) == pool_id
