################################################################################
# BSD LICENSE
#
# Copyright(c) 2020-2022 Intel Corporation. All rights reserved.
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

from appqos import caps
from appqos.config_store import ConfigStore
from appqos.rest.rest_exceptions import BadRequest, NotFound

class MbaNotFound(NotFound):
    """
    MBA not supported exception
    """
    def __init__(self):
        NotFound.__init__(self, "MBA not supported!")

class L3CatNotFound(NotFound):
    """
    L3 CAT not supported exception
    """
    def __init__(self):
        NotFound.__init__(self, "L3 CAT not supported!")

class L2CatNotFound(NotFound):
    """
    L2 CAT not supported exception
    """
    def __init__(self):
        NotFound.__init__(self, "L2 CAT not supported!")

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
        if not caps.mba_supported(ConfigStore.get_config().get_rdt_iface()):
            raise MbaNotFound()

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
        if not caps.mba_supported(ConfigStore.get_config().get_rdt_iface()):
            raise MbaNotFound()

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
        if not caps.mba_supported(ConfigStore.get_config().get_rdt_iface()):
            raise MbaNotFound()

        json_data = request.get_json()

        # validate request
        try:
            schema, resolver = ConfigStore.load_json_schema('modify_mba_ctrl.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except (jsonschema.ValidationError, OverflowError) as error:
            raise BadRequest("Request validation failed") from error

        if not caps.mba_bw_supported():
            return {'message': "MBA CTRL not supported!"}, 409

        cfg = ConfigStore.get_config()

        if cfg.is_any_pool_defined():
            return {'message': "Please remove all Pools first!"}, 409

        if cfg.get_mba_ctrl_enabled() != json_data['enabled']:
            data = deepcopy(cfg)

            CapsMbaCtrl.set_mba_ctrl_enabled(data, json_data['enabled'])

            ConfigStore.set_config(data)

        return {'message': "MBA CTRL status changed."}, 200

    @staticmethod
    def set_mba_ctrl_enabled(data, enabled):
        """
        Sets mba_ctrl enabled in config

        Parameters:
            data: configuration dict
            enabled: mba_ctrl status
        """
        # remove default pool
        data.remove_default_pool()

        if 'mba_ctrl' not in data:
            data['mba_ctrl'] = {}

        data['mba_ctrl']['enabled'] = enabled

        # recreate default pool
        data.add_default_pool()


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
        cfg = ConfigStore.get_config()

        res = {
            'interface': cfg.get_rdt_iface(),
            'interface_supported': caps.caps_iface()
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
            schema, resolver = ConfigStore().load_json_schema('modify_rdt_iface.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except (jsonschema.ValidationError, OverflowError) as error:
            raise BadRequest("Request validation failed") from error

        if not json_data['interface'] in caps.caps_iface():
            raise BadRequest(f"RDT interface '{json_data['interface']}' not supported!")

        cfg = ConfigStore.get_config()

        if cfg.is_any_pool_defined():
            return {'message': "Please remove all Pools first!"}, 409

        if cfg.get_rdt_iface() != json_data['interface']:
            data = deepcopy(cfg)

            if 'rdt_iface' not in data:
                data['rdt_iface'] = {}
            data['rdt_iface']['interface'] = json_data['interface']

            CapsMbaCtrl.set_mba_ctrl_enabled(data, False)
            CapsL3ca.set_cdp_enabled(data, False)
            CapsL2ca.set_cdp_enabled(data, False)

            ConfigStore.set_config(data)

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
        config = ConfigStore.get_config()

        if not caps.cat_l3_supported(config.get_rdt_iface()):
            raise L3CatNotFound()

        l3ca_info = caps.l3ca_info()

        res = {
            'cache_size': l3ca_info['cache_size'],
            'cw_size': l3ca_info['cache_way_size'],
            'cw_num': l3ca_info['cache_ways_num'],
            'clos_num': l3ca_info['clos_num'],
            'cdp_supported': l3ca_info['cdp_supported'],
            'cdp_enabled': config.get_l3cdp_enabled()
        }
        return res, 200


    @staticmethod
    def put():
        """
        Handles PUT /caps/l3ca request.
        Raises BadRequest, InternalError

        Returns:
            response, status code
        """
        iface = ConfigStore.get_config().get_rdt_iface()

        if not caps.cat_l3_supported(iface):
            raise L3CatNotFound()

        json_data = request.get_json()

        # validate request
        try:
            schema, resolver = ConfigStore.load_json_schema('modify_cdp.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except (jsonschema.ValidationError, OverflowError) as error:
            raise BadRequest("Request validation failed") from error

        if not caps.cdp_l3_supported(iface):
            return {'message': "L3 CDP not supported!"}, 409

        cfg = ConfigStore.get_config()

        if cfg.is_any_pool_defined():
            return {'message': "Please remove all Pools first!"}, 409

        if cfg.get_l3cdp_enabled() != json_data['cdp_enabled']:
            data = deepcopy(cfg)

            CapsL3ca.set_cdp_enabled(data, json_data['cdp_enabled'])

            ConfigStore().set_config(data)

        return {'message': "L3 CDP status changed."}, 200


    @staticmethod
    def set_cdp_enabled(data, enabled):
        """
        Sets l3cdp enabled in config

        Parameters:
            data: configuration dict
            enabled: l3cdp status
        """
        # remove default pool
        data.remove_default_pool()

        if 'rdt' not in data:
            data['rdt'] = {}

        data['rdt']['l3cdp'] = enabled

        # recreate default pool
        data.add_default_pool()


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
        config = ConfigStore.get_config()

        if not caps.cat_l2_supported(config.get_rdt_iface()):
            raise L2CatNotFound()

        l2ca_info = caps.l2ca_info()

        res = {
            'cache_size': l2ca_info['cache_size'],
            'cw_size': l2ca_info['cache_way_size'],
            'cw_num': l2ca_info['cache_ways_num'],
            'clos_num': l2ca_info['clos_num'],
            'cdp_supported': l2ca_info['cdp_supported'],
            'cdp_enabled': config.get_l2cdp_enabled()
        }
        return res, 200


    @staticmethod
    def put():
        """
        Handles PUT /caps/l2ca request.
        Raises BadRequest, InternalError
        Returns:
            response, status code
        """
        if not caps.cat_l2_supported(ConfigStore.get_config().get_rdt_iface()):
            raise L2CatNotFound()

        json_data = request.get_json()

        # validate request
        try:
            schema, resolver = ConfigStore.load_json_schema('modify_cdp.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except (jsonschema.ValidationError, OverflowError) as error:
            raise BadRequest("Request validation failed") from error

        cfg = ConfigStore.get_config()

        if not caps.cdp_l2_supported(cfg.get_rdt_iface()):
            return {'message': "L2 CDP not supported!"}, 409

        if cfg.is_any_pool_defined():
            return {'message': "Please remove all Pools first!"}, 409

        if cfg.get_l2cdp_enabled() != json_data['cdp_enabled']:
            data = deepcopy(cfg)

            CapsL2ca.set_cdp_enabled(data, json_data['cdp_enabled'])

            ConfigStore().set_config(data)

        return {'message': "L2 CDP status changed."}, 200


    @staticmethod
    def set_cdp_enabled(data, enabled):
        """
        Sets l2cdp enabled in config
        Parameters:
            data: configuration dict
            enabled: l2cdp status
        """
        # remove default pool
        data.remove_default_pool()

        if 'rdt' not in data:
            data['rdt'] = {}

        data['rdt']['l2cdp'] = enabled

        # recreate default pool
        data.add_default_pool()
