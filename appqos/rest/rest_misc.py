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
Stats, Caps, etc.
"""

from flask_restful import Resource, request

import jsonschema

import caps
import common
import power
import sstbf

from rest.rest_exceptions import BadRequest, InternalError
from rest.rest_auth import auth

from config import ConfigStore
from stats import StatsStore


class Stats(Resource):
    """
    Handles /stats HTTP requests
    """


    @staticmethod
    @auth.login_required
    def get():
        """
        Handles HTTP GET /stats request.
        Retrieve general stats

        Returns:
            response, status code
        """
        res = {
            'num_apps_moves': \
                common.STATS_STORE.general_stats_get(StatsStore.General.NUM_APPS_MOVES),
            'num_err': common.STATS_STORE.general_stats_get(StatsStore.General.NUM_ERR)
        }
        return res, 200


class Caps(Resource):
    """
    Handles /caps HTTP requests
    """


    @staticmethod
    @auth.login_required
    def get():
        """
        Handles HTTP GET /caps request.
        Retrieve capabilities

        Returns:
            response, status code
        """
        res = {'capabilities': caps.SYSTEM_CAPS}
        return res, 200


class Sstbf(Resource):
    """
    Handles /caps/sstbf HTTP requests
    """


    @staticmethod
    @auth.login_required
    def get():
        """
        Handles HTTP GET /caps/sstbf request.
        Retrieve SST-BF capabilities details

        Returns:
            response, status code
        """
        res = {
            'configured': sstbf.is_sstbf_configured(),
            'hp_cores': sstbf.get_hp_cores(),
            'std_cores': sstbf.get_std_cores()
        }
        return res, 200


    @staticmethod
    @auth.login_required
    def put():
        """
        Handles HTTP PUT /caps/sstbf request.
        Raises BadRequest, InternalError

        Returns:
            response, status code
        """
        json_data = request.get_json()

        # validate app schema
        try:
            schema, resolver = ConfigStore.load_json_schema('modify_sstbf.json')
            jsonschema.validate(json_data, schema, resolver=resolver)
        except jsonschema.ValidationError as error:
            raise BadRequest("Request validation failed - %s" % (str(error)))

        if not sstbf.configure_sstbf(json_data['configured']) == 0:
            raise InternalError("Failed to change SST-BF configured state.")

        # update power profile configuration
        power.configure_power()

        res = {'message': "SST-BF caps modified"}
        return res, 200


class Reset(Resource):
    """
    Handles /reset HTTP requests
    """


    @staticmethod
    @auth.login_required
    def post():
        """
        Handles HTTP POST /reset request.
        Resets configuration, reloads config file

        Returns:
            response, status code
        """

        common.CONFIG_STORE.reset()

        res = {'message': "Reset performed. Configuration reloaded."}
        return res, 200
