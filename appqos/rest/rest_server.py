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
REST server
"""

import json
import multiprocessing
import os
import signal
import ssl

from time import sleep
from flask import Flask
from flask_restful import Api
from werkzeug.exceptions import HTTPException

import caps
import common
import log

from rest.rest_power import Power, Powers
from rest.rest_app import App, Apps
from rest.rest_pool import Pool, Pools
from rest.rest_misc import Stats, Caps, Sstbf, Reset


TLS_CERT_FILE = 'appqos.crt'
TLS_KEY_FILE = 'appqos.key'


class Server:
    """
    REST API server
    """


    def __init__(self):
        self.process = None
        self.app = Flask(__name__)
        self.app.config['MAX_CONTENT_LENGTH'] = 2 * 1024
        self.api = Api(self.app)

        # initialize SSL context
        self.context = ssl.SSLContext(ssl.PROTOCOL_TLS)

        # allow TLS 1.2 and later
        self.context.options |= ssl.OP_NO_SSLv2
        self.context.options |= ssl.OP_NO_SSLv3
        self.context.options |= ssl.OP_NO_TLSv1
        self.context.options |= ssl.OP_NO_TLSv1_1

        self.api.add_resource(Apps, '/apps')
        self.api.add_resource(App, '/apps/<int:app_id>')
        self.api.add_resource(Pools, '/pools')
        self.api.add_resource(Pool, '/pools/<int:pool_id>')
        if caps.sstcp_enabled():
            self.api.add_resource(Powers, '/power_profiles')
            self.api.add_resource(Power, '/power_profiles/<int:profile_id>')
        self.api.add_resource(Stats, '/stats')
        self.api.add_resource(Caps, '/caps')
        if caps.sstbf_enabled():
            self.api.add_resource(Sstbf, '/caps/sstbf')
        self.api.add_resource(Reset, '/reset')

        self.app.register_error_handler(HTTPException, Server.error_handler)


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

        try:
            # check for file existence and type
            with open(TLS_CERT_FILE, opener=common.check_link):
                pass
            with open(TLS_KEY_FILE, opener=common.check_link):
                pass
            self.context.load_cert_chain(TLS_CERT_FILE, TLS_KEY_FILE)
        except (FileNotFoundError, PermissionError) as ex:
            log.error("SSL cert or key file, {}".format(str(ex)))
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
        common.STATS_STORE.general_stats_inc_num_err()
        response = {'message': error.description}
        return json.dumps(response), error.code
