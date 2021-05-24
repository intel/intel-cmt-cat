################################################################################
# BSD LICENSE
#
# Copyright(c) 2020-2021 Intel Corporation. All rights reserved.
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
from copy import deepcopy
from flask_restful import Resource, request

import jsonschema

import caps
import common

from rest.rest_exceptions import BadRequest, InternalError

from config import ConfigStore

class CapsMba(Resource):
    """
    Handles /caps/mba requests
    """

    @staticmethod
    def get():
        """
        Handles GET /caps/mba request.

        Returns:
            response, status code
        """

        mba_info = caps.mba_info()

        res = {
            'clos_num': mba_info['clos_num'],
            'mba_enabled': mba_info['enabled'],
            'mba_bw_enabled': mba_info['ctrl_enabled']
        }
        return res, 200


class CapsMbaCtrl(Resource):
    """
    Handles /caps/mba_ctrl HTTP requests
    """

    @staticmethod
    def get():
        """
        Handles HTTP /caps/mba_ctrl request.
        Retrieve MBA CTRL capability and current state details

        Returns:
            response, status code
        """

        mba_ctrl_info = caps.mba_ctrl_info()

        res = {
            'supported': mba_ctrl_info['supported'],
            'enabled': mba_ctrl_info['enabled']
        }
        return res, 200


    @staticmethod
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
        except (jsonschema.ValidationError, OverflowError) as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        if not caps.mba_bw_supported():
            return {'message': "MBA CTRL not supported!"}, 409

        if common.CONFIG_STORE.is_any_pool_defined():
            return {'message': "Please remove all Pools first!"}, 409

        data = deepcopy(common.CONFIG_STORE.get_config())

        CapsMbaCtrl.set_mba_ctrl_enabled(data, json_data['enabled'])

        common.CONFIG_STORE.set_config(data)

        return {'message': "MBA CTRL status changed."}, 200

    @staticmethod
    def set_mba_ctrl_enabled(data, enabled):
        if 'mba_ctrl' not in data:
            data['mba_ctrl'] = {}

        data['mba_ctrl']['enabled'] = enabled


class CapsRdtIface(Resource):
    """
    Handles /caps/rdt_iface HTTP requests
    """

    @staticmethod
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
        except (jsonschema.ValidationError, OverflowError) as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        if not json_data['interface'] in common.PQOS_API.supported_iface():
            raise BadRequest("RDT interface '%s' not supported!" % (json_data['interface']))

        if common.CONFIG_STORE.is_any_pool_defined():
            return {'message': "Please remove all Pools first!"}, 409

        data = deepcopy(common.CONFIG_STORE.get_config())

        if 'rdt_iface' not in data:
            data['rdt_iface'] = {}

        data['rdt_iface']['interface'] = json_data['interface']
        CapsMbaCtrl.set_mba_ctrl_enabled(data, False)

        common.CONFIG_STORE.set_config(data)

        res = {'message': "RDT Interface modified"}
        return res, 200


class CapsL3ca(Resource):
    """
    Handles /caps/l3ca requests
    """

    @staticmethod
    def get():
        """
        Handles GET /caps/l3ca request.

        Returns:
            response, status code
        """

        l3ca_info = caps.l3ca_info()

        res = {
            'cache_size': l3ca_info['cache_size'],
            'cw_size': l3ca_info['cache_way_size'],
            'cw_num': l3ca_info['cache_ways_num'],
            'clos_num': l3ca_info['clos_num'],
            'cdp_supported': l3ca_info['cdp_supported'],
            'cdp_enabled': l3ca_info['cdp_enabled']
        }
        return res, 200


class CapsL2ca(Resource):
    """
    Handles /caps/l2ca requests
    """

    @staticmethod
    def get():
        """
        Handles GET /caps/l2ca request.

        Returns:
            response, status code
        """

        l2ca_info = caps.l2ca_info()

        res = {
            'cache_size': l2ca_info['cache_size'],
            'cw_size': l2ca_info['cache_way_size'],
            'cw_num': l2ca_info['cache_ways_num'],
            'clos_num': l2ca_info['clos_num'],
            'cdp_supported': l2ca_info['cdp_supported'],
            'cdp_enabled': l2ca_info['cdp_enabled']
        }
        return res, 200
