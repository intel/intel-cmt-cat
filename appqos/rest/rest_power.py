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
Power Profiles
"""
from functools import wraps

from copy import deepcopy
from flask_restful import Resource, request
import jsonschema

from power import AdmissionControlError

from rest.rest_exceptions import NotFound, BadRequest, MethodNotAllowed
from rest.rest_auth import auth

import sstbf

import common


def _get_power_profiles_expert_mode():
    return common.CONFIG_STORE.get_global_attr('power_profiles_expert_mode', False)


def check_allowed(f):
    @wraps(f)
    def func_wrapper(*args, **kwargs):
        if not _get_power_profiles_expert_mode():
            raise MethodNotAllowed("Please enable Expert Mode first.")

        if sstbf.is_sstbf_configured():
            raise MethodNotAllowed("SST-BF configured, please un-configure SST-BF first")

        return f(*args, **kwargs)
    return func_wrapper


class Power(Resource):
    """
    Handles /power_profiles/<profile_id> HTTP requests
    """

    method_decorators = {'delete': [check_allowed], 'put' : [check_allowed]}

    @staticmethod
    @auth.login_required
    def get(profile_id):
        """
        Handles HTTP GET /power_profiles/<profile_id> request.
        Retrieve single power profile
        Raises NotFound

        Parameters:
            profile_id: Id of power profile to retrieve

        Returns:
            response, status code
        """

        data = common.CONFIG_STORE.get_config()
        if 'power_profiles' not in data:
            raise NotFound("No power profiles in config file")

        for power in data['power_profiles']:
            if not power['id'] == int(profile_id):
                continue

            return power, 200

        raise NotFound("Power profile " + str(profile_id) + " not found in config")


    @staticmethod
    @auth.login_required
    def delete(profile_id):
        """
        Handles HTTP DELETE /power_profiles/<profile_id> request.
        Deletes single Power Profile
        Raises NotFound, BadRequest

        Parameters:
            profile_id: Id of power_profile to delete

        Returns:
            response, status code
        """

        data = deepcopy(common.CONFIG_STORE.get_config())

        if 'power_profiles' not in data:
            raise NotFound("No Power Profiles in config file")

        for profile in data['power_profiles']:
            if profile['id'] != int(profile_id):
                continue

            for pool in data['pools']:
                if 'power_profile' not in pool:
                    continue

                if pool['power_profile'] == int(profile_id):
                    raise BadRequest("POWER PROFILE {} is in use.".format(str(profile_id)))

            # remove profile
            data['power_profiles'].remove(profile)
            common.CONFIG_STORE.set_config(data)

            res = {'message': "POWER PROFILE " + str(profile_id) + " deleted"}
            return res, 200

        raise NotFound("POWER PROFILE " + str(profile_id) + " not found in config")


    @staticmethod
    @auth.login_required
    def put(profile_id):
        # pylint: disable=too-many-branches
        """
        Handles HTTP PUT /power_profiles/<profile_id> request.
        Modifies a Power Profile
        Raises NotFound, BadRequest

        Parameters:
            profile_id: Id of pool

        Returns:
            response, status code
        """

        json_data = request.get_json()

        # validate app schema
        try:
            schema, resolver = common.CONFIG_STORE.load_json_schema('modify_power.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except jsonschema.ValidationError as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        admission_control_check = json_data.pop('verify', True)

        data = deepcopy(common.CONFIG_STORE.get_config())
        if 'power_profiles' not in data:
            raise NotFound("No Power Profiles in config file")

        for profile in data['power_profiles']:
            if profile['id'] != int(profile_id):
                continue

            # set new values
            profile.update(json_data)

            try:
                common.CONFIG_STORE.validate(data, admission_control_check)
            except Exception as ex:
                raise BadRequest("POWER PROFILE " + str(profile_id) + " not updated, " + str(ex))

            common.CONFIG_STORE.set_config(data)

            res = {'message': "POWER PROFILE " + str(profile_id) + " updated"}
            return res, 200

        raise NotFound("POWER PROFILE" + str(profile_id) + " not found in config")


class Powers(Resource):
    """
    Handles /power_profiles HTTP requests
    """

    method_decorators = {'post': [check_allowed]}

    @staticmethod
    @auth.login_required
    def get():
        """
        Handles HTTP GET /power_profiles request.
        Retrieve all power profiles
        Raises NotFound

        Returns:
            response, status code
        """
        data = common.CONFIG_STORE.get_config()
        if 'power_profiles' not in data:
            raise NotFound("No power profiles in config file")

        return data['power_profiles'], 200


    @staticmethod
    @auth.login_required
    def post():
        """
        Handles HTTP POST /power_profiles request.
        Add a new Power Profile
        Raises BadRequest

        Returns:
            response, status code
        """

        # validate power profile schema
        json_data = request.get_json()

        try:
            schema, resolver = common.CONFIG_STORE.load_json_schema('add_power.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except jsonschema.ValidationError as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        json_data['id'] = common.CONFIG_STORE.get_new_power_profile_id()

        data = deepcopy(common.CONFIG_STORE.get_config())
        if 'power_profiles' not in data:
            data['power_profiles'] = []
        data['power_profiles'].append(json_data)

        try:
            common.CONFIG_STORE.validate(data, False)
        except Exception as ex:
            raise BadRequest("New POWER PROFILE not added, " + str(ex))

        common.CONFIG_STORE.set_config(data)

        res = {
            'id': json_data['id'],
            'message': "New POWER PROFILE {} added".format(json_data['id'])
        }

        return res, 201
