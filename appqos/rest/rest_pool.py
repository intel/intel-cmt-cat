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
REST API module
POOLs
"""

from copy import deepcopy
from flask_restful import Resource, request

import jsonschema

import caps
import common
import sstbf

from rest.rest_exceptions import NotFound, BadRequest, InternalError
from rest.rest_auth import auth

from config import ConfigStore


class Pool(Resource):
    """
    Handles /pools/<pool_id> HTTP requests
    """


    @staticmethod
    @auth.login_required
    def get(pool_id):
        """
        Handles HTTP GET /pools/<pool_id> request.
        Retrieve single pool
        Raises NotFound, BadRequest

        Parameters:
            pool_id: Id of pool to retrieve

        Returns:
            response, status code
        """

        data = deepcopy(common.CONFIG_STORE.get_config())
        if 'pools' not in data:
            raise NotFound("No pools in config file")

        try:
            pool = common.CONFIG_STORE.get_pool(data, int(pool_id))
        except:
            raise NotFound("POOL " + str(pool_id) + " not found in config")

        return pool, 200


    @staticmethod
    @auth.login_required
    def delete(pool_id):
        """
        Handles HTTP DELETE /pool/<pull_id> request.
        Deletes single Pool
        Raises NotFound, BadRequest

        Parameters:
            pool_id: Id of pool to delete

        Returns:
            response, status code
        """

        data = deepcopy(common.CONFIG_STORE.get_config())
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

            res = {'message': "POOL " + str(pool_id) + " deleted"}
            return res, 200

        raise NotFound("POOL " + str(pool_id) + " not found in config")


    @staticmethod
    @auth.login_required
    def put(pool_id):
        # pylint: disable=too-many-branches
        """
        Handles HTTP PUT /pools/<pool_id> request.
        Modifies a Pool
        Raises NotFound, BadRequest

        Parameters:
            pool_id: Id of pool

        Returns:
            response, status code
        """
        def check_alloc_tech(pool_id, json_data):
            if 'cbm' in json_data:
                if not caps.cat_supported():
                    raise BadRequest("System does not support CAT!")
                if pool_id > common.PQOS_API.get_max_cos_id([common.CAT_CAP]):
                    raise BadRequest("Pool {} does not support CAT".format(pool_id))

            if 'mba' in json_data:
                if not caps.mba_supported():
                    raise BadRequest("System does not support MBA!")
                if pool_id > common.PQOS_API.get_max_cos_id([common.MBA_CAP]):
                    raise BadRequest("Pool {} does not support MBA".format(pool_id))

        json_data = request.get_json()

        # validate app schema
        try:
            schema, resolver = ConfigStore.load_json_schema('modify_pool.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except jsonschema.ValidationError as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        admission_control_check = json_data.pop('verify', True) and\
            ('cores' in json_data or 'power_profile' in json_data)

        data = deepcopy(common.CONFIG_STORE.get_config())
        if 'pools' not in data:
            raise NotFound("No pools in config file")

        for pool in data['pools']:
            if pool['id'] != int(pool_id):
                continue

            check_alloc_tech(int(pool_id), json_data)

            # set new cbm
            if 'cbm' in json_data:
                cbm = json_data['cbm']
                if not isinstance(cbm, int):
                    cbm = int(cbm, 16)

                pool['cbm'] = cbm

            # set new mba
            if 'mba' in json_data:
                pool['mba'] = json_data['mba']

            # set new cores
            if 'cores' in json_data:
                pool['cores'] = json_data['cores']

            if 'apps' in pool and pool['apps']:
                for app_id in pool['apps']:
                    for app in data['apps']:
                        if app['id'] != app_id or 'cores' not in app:
                            continue
                        if not set(app['cores']).issubset(pool['cores']):
                            app.pop('cores')

            # set new name
            if 'name' in json_data:
                pool['name'] = json_data['name']

            # set new power profile
            # ignore 'power_profile' if SST-BF is enabled
            if 'power_profile' in json_data and not sstbf.is_sstbf_configured():
                pool['power_profile'] = json_data['power_profile']

            try:
                common.CONFIG_STORE.validate(data, admission_control_check)
            except Exception as ex:
                raise BadRequest("POOL " + str(pool_id) + " not updated, " + str(ex))

            common.CONFIG_STORE.set_config(data)

            res = {'message': "POOL " + str(pool_id) + " updated"}
            return res, 200

        raise NotFound("POOL " + str(pool_id) + " not found in config")


class Pools(Resource):
    """
    Handles /pools HTTP requests
    """


    @staticmethod
    @auth.login_required
    def get():
        """
        Handles HTTP GET /pools request.
        Retrieve all pools
        Raises NotFound

        Returns:
            response, status code
        """
        data = common.CONFIG_STORE.get_config().copy()
        if 'pools' not in data:
            raise NotFound("No pools in config file")

        return data['pools'], 200


    @staticmethod
    @auth.login_required
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

        admission_control_check = json_data.pop('verify', True) and\
            ('cores' in json_data or 'power_profile' in json_data)

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

        # ignore 'power_profile' if SST-BF is enabled
        if sstbf.is_sstbf_configured():
            post_data.pop('power_profile', None)

        data = deepcopy(common.CONFIG_STORE.get_config())
        data['pools'].append(post_data)

        try:
            common.CONFIG_STORE.validate(data, admission_control_check)
        except Exception as ex:
            raise BadRequest("New POOL not added, " + str(ex))

        common.CONFIG_STORE.set_config(data)

        res = {
            'id': post_data['id'],
            'message': "New POOL {} added".format(post_data['id'])
        }
        return res, 201
