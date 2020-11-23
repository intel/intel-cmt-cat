################################################################################
# BSD LICENSE
#
# Copyright(c) 2020 Intel Corporation. All rights reserved.
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
RDT related requests.
"""

from flask_restful import Resource, request

import jsonschema

import caps
import common

from rest.rest_exceptions import BadRequest, InternalError
from rest.rest_auth import auth

from config import ConfigStore

class CapsMba(Resource):
    """
    Handles /caps/mba requests
    """

    @staticmethod
    @auth.login_required
    def get():
        """
        Handles GET /caps/mba request.

        Returns:
            response, status code
        """
        res = {
            'mba_enabled': not caps.mba_bw_enabled(),
            'mba_bw_enabled': caps.mba_bw_enabled()
        }
        return res, 200


class CapsMbaCtrl(Resource):
    """
    Handles /caps/mba_ctrl HTTP requests
    """

    @staticmethod
    @auth.login_required
    def get():
        """
        Handles HTTP /caps/mba_ctrl request.
        Retrieve MBA CTRL capability and current state details

        Returns:
            response, status code
        """
        res = {
            'supported': caps.mba_bw_supported(),
            'enabled': caps.mba_bw_enabled()
        }
        return res, 200


    @staticmethod
    @auth.login_required
    def put():
        """
        Handles PUT /caps/mba_ctrl request.
        Raises BadRequest, InternalError

        Returns:
            response, status code
        """
        json_data = request.get_json()

        # validate request
        try:
            schema, resolver = ConfigStore.load_json_schema('modify_mba_ctrl.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except jsonschema.ValidationError as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        if not caps.mba_bw_supported():
            return {'message': "MBA CTRL not supported!"}, 409

        if common.CONFIG_STORE.is_any_pool_defined():
            return {'message': "Please remove all Pools first!"}, 409

        return {'message': "Not Implemented"}, 501


class CapsRdtIface(Resource):
    """
    Handles /caps/rdt_iface HTTP requests
    """

    @staticmethod
    @auth.login_required
    def get():
        """
        Handles HTTP /caps/rdt_iface request.
        Retrieve RDT current and supported interface types

        Returns:
            response, status code
        """
        res = {
            'interface': common.PQOS_API.current_iface(),
            'interface_supported': common.PQOS_API.supported_iface()
        }
        return res, 200


    @staticmethod
    @auth.login_required
    def put():
        """
        Handles PUT /caps/rdt_iface request.
        Raises BadRequest, InternalError

        Returns:
            response, status code
        """
        json_data = request.get_json()

        # validate request
        try:
            schema, resolver = ConfigStore.load_json_schema('modify_rdt_iface.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except jsonschema.ValidationError as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        if not json_data['interface'] in common.PQOS_API.supported_iface():
            raise BadRequest("RDT interface '%s' not supported!" % (json_data['interface']))

        if common.CONFIG_STORE.is_any_pool_defined():
            return {'message': "Please remove all Pools first!"}, 409

        if not common.PQOS_API.init(json_data['interface']) == 0:
            raise InternalError("Failed to configure RDT interface.")

        common.CONFIG_STORE.recreate_default_pool()

        res = {'message': "RDT Interface modified"}
        return res, 200

