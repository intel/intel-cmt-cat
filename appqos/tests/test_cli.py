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
import mock
import tempfile

from jsonschema import validate, RefResolver
from os.path import join, dirname

from cli import *


def load_json_schema(file_name):
    path = join(dirname(dirname(__file__)), 'schema', file_name)
    schema_path = 'file:' + str(join(dirname(dirname(__file__)), 'schema')) + '/'
    with open(path, "r") as fd:
        schema = json.loads(fd.read())
        return schema, RefResolver(schema_path, schema)


class Response(object):
    def __init__(self, status_code, content):
        self.status_code = status_code
        self.content = content


@pytest.fixture(scope="class")
def set_config():
    Config.host = "localhost"
    Config.port = 5000
    Config.user = "admin"
    Config.password = "passsword"
    yield
    Config.host = None
    Config.port = None
    Config.user = None
    Config.password = None


@pytest.mark.usefixtures("set_config")
class TestApps(object):
    @mock.patch('requests.request')
    def test_show_id(self, mock_request, capsys):
        mock_request.return_value = Response(200, '{"id": 1, "cores": [0], "name": "test"}')

        ShellMain().onecmd("app show 1")
        out, err = capsys.readouterr()

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'GET'
        assert args[1] == 'https://localhost:5000/apps/1'
        assert 'app: 1 (test)' in out
        assert 'cores' in out


    @mock.patch('requests.request')
    def test_show_all(self, mock_request, capsys):
        mock_request.return_value = Response(200, '[{"id": 1, "cores": [0], "name": "test"}, {"id": 2, "cores": [0], "name": "test2"}]')

        ShellMain().onecmd("app show --all")
        out, err = capsys.readouterr()

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'GET'
        assert args[1] == 'https://localhost:5000/apps'
        assert 'app: 1 (test)' in out
        assert 'app: 2 (test2)' in out


    @mock.patch('requests.request')
    def test_show_non_existent(self, mock_request, capsys):
        mock_request.return_value = Response(404, "APP 20 not found in config")

        ShellMain().onecmd("app show 20")
        out, err = capsys.readouterr()

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'GET'
        assert args[1] == 'https://localhost:5000/apps/20'

        assert 'Request failed' in out


    @mock.patch('requests.request')
    def test_show_invalid(self, mock_request, capsys):
        ShellMain().onecmd("app show invalid")
        out, err = capsys.readouterr()

        mock_request.assert_not_called()
        assert 'invalid int value' in err


    @mock.patch('requests.request')
    def test_move(self, mock_request, capsys):
        mock_request.return_value = Response(200, '{}')

        ShellMain().onecmd("app move 1 2")

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'PUT'
        assert args[1] == 'https://localhost:5000/apps/1'
        assert kwargs['json'] == {"pool_id": 2}


    @mock.patch('requests.request')
    def test_remove(self, mock_request, capsys):
        mock_request.return_value = Response(200, '{}')

        ShellMain().onecmd("app remove 2")

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'DELETE'
        assert args[1] == 'https://localhost:5000/apps/2'


    @mock.patch('requests.request')
    def test_add_simple(self, mock_request, capsys):
        mock_request.return_value = Response(200, '{}')

        ShellMain().onecmd('app add 1 test 2 3 --pids 5 2 3')

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'POST'
        assert args[1] == 'https://localhost:5000/apps'
        assert kwargs['json'] == {
            "pool_id": 1,
            "name": "test",
            "cores": [2, 3],
            "pids": [5, 2, 3],
        }

        schema, resolver = load_json_schema("add_app.json")
        validate(kwargs['json'], schema, resolver=resolver)


    @mock.patch('requests.request')
    def test_add_complex(self, mock_request, capsys):
        mock_request.return_value = Response(200, '{}')

        ShellMain().onecmd('app add 1 "test4" 2 3 --pids 5 2 3')

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'POST'
        assert args[1] == 'https://localhost:5000/apps'
        assert "pool_id" in kwargs['json']
        assert "name" in kwargs['json']
        assert "cores" in kwargs['json']
        assert "pids" in kwargs['json']

        schema, resolver = load_json_schema("add_app.json")
        validate(kwargs['json'], schema, resolver=resolver)


    @mock.patch('requests.request')
    def test_show_invalid_argument(self, mock_request, capsys):
        ShellMain().onecmd("app show -a --invalid")

        out, err = capsys.readouterr()
        mock_request.assert_not_called()
        assert "unrecognized arguments" in err


    def test_help(self, capsys):
        ShellMain().onecmd("help app")
        out, err = capsys.readouterr()
        assert "show" in out
        assert "add" in out
        assert "remove" in out
        assert "move" in out


@pytest.mark.usefixtures("set_config")
class TestPools(object):
    @mock.patch('requests.request')
    def test_show_id(self, mock_request, capsys):
        mock_request.return_value = Response(200, '{"id": 1, "priority": "prod", "name": "NFVI", "min_cws": 2, "apps": [2,3]}')

        ShellMain().onecmd("pool show 1")
        out, err = capsys.readouterr()

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'GET'
        assert args[1] == 'https://localhost:5000/pools/1'
        assert 'pool: 1 (NFVI)' in out
        assert 'min_cws: 2' in out
        assert 'apps:' in out
        assert 'priority: prod' in out


    @mock.patch('requests.request')
    def test_show_all(self, mock_request, capsys):
        mock_request.return_value = Response(200, '[{"id": 1, "priority": "prod", "name": "NFVI", "min_cws": 2, "apps": [2,3]}, {"id": 2, "priority": "prod", "name": "Production", "min_cws": 1, "apps": [5,6]}]')

        ShellMain().onecmd("pool show --all")
        out, err = capsys.readouterr()

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'GET'
        assert args[1] == 'https://localhost:5000/pools'
        assert 'pool: 1 (NFVI)' in out
        assert 'pool: 2 (Production)' in out
        assert 'min_cws' in out
        assert 'apps' in out
        assert 'priority' in out


    @mock.patch('requests.request')
    def test_show_non_existent(self, mock_request, capsys):
        mock_request.return_value = Response(400, "Pool 20 not found in config")

        ShellMain().onecmd("pool show 20")
        out, err = capsys.readouterr()

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'GET'
        assert args[1] == 'https://localhost:5000/pools/20'

        assert 'Request failed' in out


    @mock.patch('requests.request')
    def test_show_invalid(self, mock_request, capsys):
        ShellMain().onecmd("pool show invalid")
        out, err = capsys.readouterr()

        mock_request.assert_not_called()
        assert 'invalid int value' in err


    @mock.patch('requests.request')
    def test_show_invalid_argument(self, mock_request, capsys):
        ShellMain().onecmd("pool show -a --invalid")

        out, err = capsys.readouterr()
        mock_request.assert_not_called()
        assert "unrecognized arguments" in err


    def test_help(self, capsys):
        ShellMain().onecmd("help pool")
        out, err = capsys.readouterr()
        assert "show" in out


@pytest.mark.usefixtures("set_config")
class TestGrpups(object):
    @mock.patch('requests.request')
    def test_show_id(self, mock_request, capsys):
        mock_request.return_value = Response(200, '{"id": 1, "type": "static", "pools": [1], "cbm": "0xc00"}')

        ShellMain().onecmd("group show 1")
        out, err = capsys.readouterr()

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'GET'
        assert args[1] == 'https://localhost:5000/groups/1'
        assert 'group: 1 (static)' in out
        assert 'cbm: 0xc00' in out
        assert 'pools: [1]' in out


    @mock.patch('requests.request')
    def test_show_all(self, mock_request, capsys):
        mock_request.return_value = Response(200, '[{"id": 1, "type": "static", "pools": [1], "cbm": "0xc00"}, {"id": 2, "type": "dynamic", "pools": [2,3,4], "cbm": "0x3ff"}]')

        ShellMain().onecmd("group show --all")
        out, err = capsys.readouterr()

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'GET'
        assert args[1] == 'https://localhost:5000/groups'
        assert 'group: 1 (static)' in out
        assert 'group: 2 (dynamic)' in out


    @mock.patch('requests.request')
    def test_show_non_existent(self, mock_request, capsys):
        mock_request.return_value = Response(400, "Group 20 not found in config")

        ShellMain().onecmd("group show 20")
        out, err = capsys.readouterr()

        mock_request.assert_called_once()
        args, kwargs = mock_request.call_args
        assert args[0] == 'GET'
        assert args[1] == 'https://localhost:5000/groups/20'

        assert 'Request failed' in out


    @mock.patch('requests.request')
    def test_show_invalid(self, mock_request, capsys):
        ShellMain().onecmd("group show invalid")
        out, err = capsys.readouterr()

        mock_request.assert_not_called()
        assert 'invalid int value' in err


    @mock.patch('requests.request')
    def test_show_invalid_argument(self, mock_request, capsys):
        ShellMain().onecmd("group show -a --invalid")

        out, err = capsys.readouterr()
        mock_request.assert_not_called()
        assert "unrecognized arguments" in err


    def test_help(self, capsys):
        ShellMain().onecmd("help group")
        out, err = capsys.readouterr()
        assert "show" in out


@pytest.mark.usefixtures("set_config")
class TestStats(object):
    @mock.patch('requests.request')
    def test_show(self, mock_request, capsys):
        mock_request.return_value = Response(200, '{"num_flushes": 1, "num_apps_moves": 2, "num_err": 3}')

        ShellMain().onecmd("stats show")
        out, err = capsys.readouterr()

        mock_request.assert_called_once()
        assert "Number of cache flushes: 1" in out
        assert "Number of APPs moves: 2" in out
        assert "Number of errors: 3" in out


    @mock.patch('requests.request')
    def test_show_invalid_argument(self, mock_request, capsys):
        ShellMain().onecmd("stats show --invalid")

        out, err = capsys.readouterr()
        mock_request.assert_not_called()
        assert "unrecognized arguments" in err


    def test_help(self, capsys):
        ShellMain().onecmd("help config")
        out, err = capsys.readouterr()
        assert "set" in out
        assert "load" in out
        assert "save" in out


class TestConfig(object):
    def test_load_invalid_path(self, capsys):
        ShellMain().onecmd("config load /tmp/non_existent")
        out, err = capsys.readouterr()
        assert 'No such file or directory' in out


    def test_load_invalid(self, capsys):
        fd, path = tempfile.mkstemp()
        os.write(fd, "invalid")
        os.close(fd)

        ShellMain().onecmd("config load %s" % (path))
        out, err = capsys.readouterr()
        assert "No JSON object could be decoded" in out

        os.remove(path)


    def test_load(self):
        fd, path = tempfile.mkstemp()
        os.write(fd, '{"auth":{"username":"user","password":"password"},"server":{"host":"localhost","port":5000}}')
        os.close(fd)

        ShellMain().onecmd("config load %s" % (path))

        assert Config.host == "localhost"
        assert Config.port == 5000
        assert Config.user == "user"
        assert Config.password == "password"

        os.remove(path)


    def test_save(self):
        assert Config.host == "localhost"
        assert Config.port == 5000
        assert Config.user == "user"
        assert Config.password == "password"

        path = tempfile.mktemp()

        ShellMain().onecmd("config save %s" % (path))

        assert os.path.isfile(path)

        with open(path, 'r') as fd:
            raw_data = fd.read()
            try:
                data = json.loads(raw_data.replace('\r\n', '\\r\\n'))
                schema, resolver = load_json_schema("cli.json")
                validate(data, schema, resolver=resolver)

                assert "auth" in data
                assert "server" in data
                assert "username" in data["auth"]
                assert "password" not in data["auth"]

            except ValueError:
                pytest.fail("Invalid file format")

        os.remove(path)


    def test_save_password(self):
        assert Config.host == "localhost"
        assert Config.port == 5000
        assert Config.user == "user"
        assert Config.password == "password"

        path = tempfile.mktemp()

        ShellMain().onecmd("config save %s --save-password" % (path))

        assert os.path.isfile(path)

        with open(path, 'r') as fd:
            raw_data = fd.read()
            try:
                data = json.loads(raw_data.replace('\r\n', '\\r\\n'))
                schema, resolver = load_json_schema("cli.json")
                validate(data, schema, resolver=resolver)

                assert "auth" in data
                assert "server" in data
                assert "username" in data["auth"]
                assert "password" in data["auth"]

            except ValueError:
                pytest.fail("Invalid file format")

        os.remove(path)


    def test_set(self):
        Config.host = None
        Config.port = None
        Config.user = None
        Config.password = None

        ShellMain().onecmd("config set --host localhost --port 5000 --user user --password password")

        assert Config.host == "localhost"
        assert Config.port == 5000
        assert Config.user == "user"
        assert Config.password == "password"
