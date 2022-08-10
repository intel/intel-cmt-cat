################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2022 Intel Corporation. All rights reserved.
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
import sys

from time import sleep
from flask import Flask
from flask_restful import Api
from gevent.pywsgi import WSGIServer
from werkzeug.exceptions import HTTPException

import caps
import common
import log

from rest.rest_power import Power, Powers
from rest.rest_app import App, Apps
from rest.rest_pool import Pool, Pools
from rest.rest_misc import Stats, Caps, Sstbf, Reset
from rest.rest_rdt import CapsRdtIface, CapsMba, CapsMbaCtrl, CapsL3ca, CapsL2ca

TLS_CERT_FILE = 'ca/appqos.crt'
TLS_KEY_FILE = 'ca/appqos.key'
TLS_CA_CERT_FILE = 'ca/ca.crt'

TLS_CIPHERS = [
    'AES128-GCM-SHA256',
    'AES256-GCM-SHA384',
    'ECDHE-RSA-AES128-GCM-SHA256',
]

class Server:
    """
    REST API server
    """


    def __init__(self):
        self.process = None
        self.app = Flask(__name__)
        self.app.config['MAX_CONTENT_LENGTH'] = 2 * 1024
        self.app.url_map.strict_slashes = False
        self.api = Api(self.app)

        self.http_server = None

        # initialize SSL context
        self.context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
        self.context.verify_mode = ssl.CERT_REQUIRED
        self.context.set_ciphers(':'.join(TLS_CIPHERS))

        # allow TLS 1.2 and later
        if sys.version_info < (3, 7, 0):
            self.context.options |= ssl.OP_NO_SSLv2
            self.context.options |= ssl.OP_NO_SSLv3
            self.context.options |= ssl.OP_NO_TLSv1
            self.context.options |= ssl.OP_NO_TLSv1_1
        else:
            self.context.minimum_version = ssl.TLSVersion.TLSv1_2

        # Apps and Pools API
        self.api.add_resource(Apps, '/apps')
        self.api.add_resource(App, '/apps/<int:app_id>')
        self.api.add_resource(Pools, '/pools')
        self.api.add_resource(Pool, '/pools/<int:pool_id>')

        # SST-CP API
        if caps.sstcp_enabled():
            self.api.add_resource(Powers, '/power_profiles')
            self.api.add_resource(Power, '/power_profiles/<int:profile_id>')

        # Stats and Capabilities API
        self.api.add_resource(Stats, '/stats')
        self.api.add_resource(Caps, '/caps')

        # SST-BF API
        if caps.sstbf_enabled():
            self.api.add_resource(Sstbf, '/caps/sstbf')

        # RDT interface API
        self.api.add_resource(CapsRdtIface, '/caps/rdt_iface')

        # MBA API
        if caps.mba_supported():
            self.api.add_resource(CapsMba, '/caps/mba')
            self.api.add_resource(CapsMbaCtrl, '/caps/mba_ctrl')

        # L3 CAT API
        if caps.cat_l3_supported():
            self.api.add_resource(CapsL3ca, '/caps/' + common.CAT_L3_CAP)

        # L2 CAT API
        if caps.cat_l2_supported():
            self.api.add_resource(CapsL2ca, '/caps/' + common.CAT_L2_CAP)

        # Reset API
        self.api.add_resource(Reset, '/reset')

        self.app.register_error_handler(HTTPException, Server.error_handler)


    def start(self, host, port, _debug=False):
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
            with open(TLS_CERT_FILE, opener=common.check_link, encoding='utf-8'):
                pass
            with open(TLS_KEY_FILE, opener=common.check_link, encoding='utf-8'):
                pass
            self.context.load_cert_chain(TLS_CERT_FILE, TLS_KEY_FILE)
        except (FileNotFoundError, PermissionError) as ex:
            log.error(f"SSL cert or key file, {str(ex)}")
            return -1

        # loading CA crt file - it is needed for mTLS verification of client certificates
        try:
            with open(TLS_CA_CERT_FILE, opener=common.check_link, encoding='utf-8'):
                pass
            self.context.load_verify_locations(cafile=TLS_CA_CERT_FILE)

        except (FileNotFoundError, PermissionError) as ex:
            log.error(f"CA certificate file, {str(ex)}")
            return -1

        self.http_server = WSGIServer((host, port), self.app, ssl_context=self.context, spawn=1)
        def handle_gevent_stop(_signum, _frame):
            log.info("Stopping gevent server loop")
            self.http_server.stop()
            self.http_server.close()

        # dedicated handler for gevent server is needed to stop it gracefully
        # before process will be terminated
        signal.signal(signal.SIGINT, handle_gevent_stop)

        self.process = multiprocessing.Process(target=self.http_server.serve_forever)
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
