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

"""
REST API module
"""

import json
import multiprocessing
import os
import signal

from time import sleep
from flask import Flask
from flask_httpauth import HTTPBasicAuth
from flask_restful import Api, Resource, request
from werkzeug.exceptions import HTTPException

import jsonschema

import caps
import common
import log
import pid_ops

from config import ConfigStore
from stats import StatsStore


class RestError(HTTPException):
    """
    RestError exception base class
    """


    def __init__(self, code, description):
        HTTPException.__init__(self)
        self.code = code
        self.description = description


class NotFound(RestError):
    """
    NotFound exception
    """


    def __init__(self, description="Not Found"):
        RestError.__init__(self, 404, description)


class BadRequest(RestError):
    """
    BadRequest exception
    """


    def __init__(self, description="BadRequest"):
        RestError.__init__(self, 400, description)


class InternalError(RestError):
    """
    InternalError exception
    """


    def __init__(self, description="Internal Server Error"):
        RestError.__init__(self, 500, description)


class Server(object):
    """
    REST API server
    """
    auth = HTTPBasicAuth()


    def __init__(self):
        self.process = None
        self.app = Flask(__name__)
        self.api = Api(self.app)

        # initialize SSL context
        self.context = ('appqos.crt', 'appqos.key')

        self.api.add_resource(Apps, '/apps')
        self.api.add_resource(App, '/apps/<app_id>')
        self.api.add_resource(Pools, '/pools')
        self.api.add_resource(Pool, '/pools/<pool_id>')
        self.api.add_resource(Stats, '/stats')
        self.api.add_resource(Caps, '/caps')

        self.app.register_error_handler(RestError, Server.error_handler)


    def start(self, host, port, debug=False):
        """
        Start REST server

        Parameters:
            host: address to bind to
            port: port to bind to
            debug(bool): Debug flag

        Returns:
            0 on success
        """

        for ssl_ctx_file in self.context:
            if not os.path.isfile(ssl_ctx_file):
                log.error("SSL cert or key file missing.")
                return -1

        self.process = multiprocessing.Process(target=self.app.run,
                                               kwargs={'host': host,
                                                       'port': port,
                                                       'ssl_context': self.context,
                                                       'debug': debug,
                                                       'use_reloader': False,
                                                       'processes': 1})
        self.process.start()
        return 0


    def terminate(self):
        """
        Terminates server
        """
        os.kill(self.process.pid, signal.SIGINT)
        sleep(1)
        if self.process.is_alive():
            self.process.terminate()
        self.process.join()


    @staticmethod
    def error_handler(error):
        """
        Error handler

        Parameters:
            error: error
        """
        response = {"message": error.message}
        return json.dumps(response), error.code


    @staticmethod
    @auth.verify_password
    def verify(username, password):
        """
        Authenticate user, HTTP Basic Auth

        Parameters:
            username: Username
            password: Password

        Returns:
            Authentication result (bool)
        """
        if not (username and password):
            return False
        if 'auth' in common.CONFIG_STORE.get_config():
            if username == common.CONFIG_STORE.get_config()['auth']['username'] and \
                password == common.CONFIG_STORE.get_config()['auth']['password']:
                return True
        return False


class App(Resource):
    """
    Handle /apps/<app_id> HTTP requests
    """


    @staticmethod
    @Server.auth.login_required
    def get(app_id):
        """
        Handles HTTP GET /apps/<app_id> request.
        Retrieve single app
        Raises NotFound

        Parameters:
            app_id: Id of app to retrieve

        Returns:
            response, status code
        """
        data = common.CONFIG_STORE.get_config()
        if 'apps' not in data:
            raise NotFound("No apps in config file")

        for app in data['apps']:
            if not app['id'] == int(app_id):
                continue

            return app, 200

        raise NotFound("APP " + str(app_id) + " not found in config")


    @staticmethod
    @Server.auth.login_required
    def delete(app_id):
        """
        Handles HTTP DELETE /apps/<app_id> request.
        Deletes single App
        Raises NotFound, InternalError

        Parameters:
            app_id: Id of app to delete

        Returns:
            response, status code
        """
        data = common.CONFIG_STORE.get_config().copy()
        if 'apps' not in data or 'pools' not in data:
            raise NotFound("No apps or pools in config file")

        for app in data['apps']:
            if app['id'] != int(app_id):
                continue

            # remove app id from pool
            for pool in data['pools']:
                if 'apps' not in pool:
                    continue

                if app['id'] in pool['apps']:
                    pool['apps'].remove(app['id'])
                    break

            # remove app
            data['apps'].remove(app)
            common.CONFIG_STORE.set_config(data)
            return "APP " + str(app_id) + " deleted", 200

        raise NotFound("APP " + str(app_id) + " not found in config")


    @staticmethod
    @Server.auth.login_required
    def put(app_id):
        """
        Handles HTTP PUT /apps/<app_id> request.
        Modifies an App (e.g.: moves to different pool)
        Raises NotFound, InternalError

        Parameters:
            app_id: Id of app to modify

        Returns:
            response, status code
        """
        json_data = request.get_json()

        # validate app schema
        try:
            schema, resolver = ConfigStore.load_json_schema('move_app.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except jsonschema.ValidationError as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        pool_id = json_data["pool_id"]

        data = common.CONFIG_STORE.get_config().copy()
        if 'apps' not in data or 'pools' not in data:
            raise NotFound("No apps or pools in config file")

        for app in data['apps']:
            if app['id'] != int(app_id):
                continue

            # remove app id from pool
            for pool in data['pools']:
                if 'apps' in pool:
                    if app['id'] in pool['apps']:
                        pool['apps'].remove(app['id'])
                        break
            # add app id to new pool
            for pool in data['pools']:
                if pool['id'] == int(pool_id):
                    if 'apps' in pool:
                        pool['apps'].append(app['id'])
                        break
                    else:
                        pool['apps'] = [app['id']]
                        break
            # set new cores
            if 'cores' in json_data:
                app['cores'] = json_data['cores']
            elif 'cores' in app:
                app.pop('cores')

            common.CONFIG_STORE.set_config(data)
            common.STATS_STORE.general_stats_inc_apps_moves()
            return "APP " + str(app_id) + " moved to new pool", 200

        raise NotFound("APP " + str(app_id) + " not found in config")


class Apps(Resource):
    """
    Handles /apps HTTP requests
    """


    @staticmethod
    @Server.auth.login_required
    def get():
        """
        Handles HTTP GET /apps request.
        Get all Apps
        Raises NotFound

        Returns:
            response, status code
        """
        data = common.CONFIG_STORE.get_config()
        if 'apps' not in data:
            raise NotFound("No apps in config file")

        return (data['apps']), 200


    @staticmethod
    @Server.auth.login_required
    def post():
        """
        Handles HTTP POST /apps request.
        Add a new App
        Raises NotFound, BadRequest, InternalError

        Returns:
            response, status code
        """

        def get_app_id():
            """
            Get ID for new App

            Returns:
                ID for new App
            """
            # put all ids into list
            app_ids = []
            for app in data['apps']:
                app_ids.append(app['id'])
            app_ids = sorted(app_ids)
            # no app found in config
            if not app_ids:
                return 1

            # add new app to apps
            # find an id
            new_ids = list(set(range(1, app_ids[-1])) - set(app_ids))
            if new_ids:
                return new_ids[0]

            return app_ids[-1] + 1

        json_data = request.get_json()

        # validate app schema
        try:
            schema, resolver = ConfigStore.load_json_schema('add_app.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except jsonschema.ValidationError as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        data = common.CONFIG_STORE.get_config()

        if 'pools' not in data:
            raise NotFound("No pools in config file")

        json_data['id'] = get_app_id()

        if 'cores' in json_data:
            for core in json_data['cores']:
                if not common.is_core_valid(core):
                    raise BadRequest("New APP not added, please provide valid cores")

        if 'pids' in json_data:
            # validate pids
            for pid in json_data['pids']:
                valid = pid_ops.is_pid_valid(pid)
                if not valid:
                    raise BadRequest("New APP not added, please provide valid pid's")

        app_added = False
        for i in range(len(data['pools'])):
            if data['pools'][i]['id'] == json_data['pool_id']:

                json_data.pop('pool_id')
                data['apps'].append(json_data)

                data['pools'][i]['apps'].append(json_data['id'])
                app_added = True
                break

        if not app_added:
            raise NotFound("New APP not added, pool " + json_data['pool_id'] + " doesn't exist")

        res = {'id': json_data['id']}
        return res, 201


class Pool(Resource):
    """
    Handles /pools/<pool_id> HTTP requests
    """


    @staticmethod
    @Server.auth.login_required
    def get(pool_id):
        """
        Handles HTTP GET /pools/<pool_id> request.
        Retrieve single pool
        Raises NotFound

        Parameters:
            pool_id: Id of pool to retrieve

        Returns:
            response, status code
        """

        data = common.CONFIG_STORE.get_config()
        if 'pools' not in data:
            raise NotFound("No pools in config file")

        for pool in data['pools']:
            if not pool['id'] == int(pool_id):
                continue

            return pool, 200

        raise NotFound("Pool " + str(pool_id) + " not found in config")


    @staticmethod
    @Server.auth.login_required
    def delete(pool_id):
        """
        Handles HTTP DELETE /pool/<pull_id> request.
        Deletes single Pool
        Raises NotFound, InternalError

        Parameters:
            pool_id: Id of pool to delete

        Returns:
            response, status code
        """
        data = common.CONFIG_STORE.get_config().copy()
        if 'pools' not in data:
            raise NotFound("No pools in config file")

        if int(pool_id) == 0:
            raise BadRequest("POOL " + str(pool_id) + " is Default, cannot delete")

        for pool in data['pools']:
            if pool['id'] != int(pool_id):
                continue

            if 'apps' in pool and pool['apps']:
                raise BadRequest("POOL " + str(pool_id) + " is not empty")

            # remove app
            data['pools'].remove(pool)
            common.CONFIG_STORE.set_config(data)
            return "POOL " + str(pool_id) + " deleted", 200

        raise NotFound("POOL " + str(pool_id) + " not found in config")


    @staticmethod
    @Server.auth.login_required
    def put(pool_id):
        """
        Handles HTTP PUT /pools/<pool_id> request.
        Modifies a Pool
        Raises NotFound, BadRequest, InternalError

        Parameters:
            pool_id: Id of pool

        Returns:
            response, status code
        """
        json_data = request.get_json()

        # validate app schema
        try:
            schema, resolver = ConfigStore.load_json_schema('modify_pool.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except jsonschema.ValidationError as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        data = common.CONFIG_STORE.get_config().copy()
        if 'pools' not in data:
            raise NotFound("No pools in config file")

        for pool in data['pools']:
            if pool['id'] != int(pool_id):
                continue

            # set new cbm
            if 'cbm' in json_data:
                cbm = json_data['cbm']
                if not isinstance(cbm, int):
                    cbm = int(cbm, 16)

                pool['cbm'] = cbm

            # set new mba
            if 'mba' in json_data:
                pool['mba'] = json_data['mba']

            common.CONFIG_STORE.set_config(data)
            return "POOL " + str(pool_id) + " updated", 200

        raise NotFound("POOL " + str(pool_id) + " not found in config")

class Pools(Resource):
    """
    Handles /pools HTTP requests
    """


    @staticmethod
    @Server.auth.login_required
    def get():
        """
        Handles HTTP GET /pools request.
        Retrieve all pools
        Raises NotFound

        Returns:
            response, status code
        """
        data = common.CONFIG_STORE.get_config()
        if 'pools' not in data:
            raise NotFound("No pools in config file")

        return (data['pools']), 200


    @staticmethod
    @Server.auth.login_required
    def post():
        """
        Handles HTTP POST /pools request.
        Add a new Pool
        Raises NotFound, BadRequest, InternalError

        Returns:
            response, status code
        """
        json_data = request.get_json()

        # validate pool schema
        try:
            schema, resolver = ConfigStore.load_json_schema('add_pool.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except jsonschema.ValidationError as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        post_data = json_data.copy()
        post_data['id'] = common.CONFIG_STORE.get_new_pool_id(post_data)
        if post_data['id'] is None:
            raise InternalError("New POOL not added, maximum number of POOLS"\
                " reached for requested allocation combination")

        # convert cbm from string to int
        if 'cbm' in post_data:
            cbm = post_data['cbm']
            if not isinstance(cbm, int):
                cbm = int(cbm, 16)

            post_data['cbm'] = cbm

        if 'cores' in post_data:
            for core in post_data['cores']:
                if not common.is_core_valid(core):
                    raise BadRequest("New POOL not added, please provide valid cores")

        data = common.CONFIG_STORE.get_config().copy()
        data['pools'].append(post_data)
        common.CONFIG_STORE.set_config(data)

        res = {'id': post_data['id']}
        return res, 201


class Stats(Resource):
    """
    Handles /stats HTTP requests
    """


    @staticmethod
    @Server.auth.login_required
    def get():
        """
        Handles HTTP GET /stats request.
        Retrieve general stats

        Returns:
            response, status code
        """
        data = {}
        data["num_apps_moves"] = \
            common.STATS_STORE.general_stats_get(StatsStore.General.NUM_APPS_MOVES)
        data["num_err"] = common.STATS_STORE.general_stats_get(StatsStore.General.NUM_ERR)

        return data, 200


class Caps(Resource):
    """
    Handles /caps HTTP requests
    """


    @staticmethod
    @Server.auth.login_required
    def get():
        """
        Handles HTTP GET /caps request.
        Retrieve capabilities

        Returns:
            response, status code
        """
        data = {}
        data["capabilities"] = caps.SYSTEM_CAPS

        return data, 200
