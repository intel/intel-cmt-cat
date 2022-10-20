#!/usr/bin/env python3

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
Main module.
Parses command line arguments, sets up logging, initialises libpqos,
starts REST API server and runs main loop (AppQoS)
"""

import argparse
import multiprocessing
import signal
import sys
import syslog
import time
from jsonschema import ValidationError

from appqos import cache_ops
from appqos import caps
from appqos import common
from appqos import log
from appqos import power
from appqos.rest import rest_server
from appqos import sstbf
from appqos.config_store import ConfigStore
from appqos.pqos_api import PQOS_API
from appqos.__version__ import __version__

class AppQoS:
    """
    Main logic.
    Loads config file.
    Applies initial Intel RDT and SST-BF configuration.
    Handles configuration changes.
    """


    def __init__(self):
        self.stop_event = multiprocessing.Event()

    def run(self):
        """
        Runs main loop.
        """

        # process/validate already loaded config file
        try:
            ConfigStore().process_config()
        except Exception as ex:
            log.error("Invalid config file... ")
            log.error(ex)
            return

        data = ConfigStore.get_config()

        log.debug(f"Cores controlled: {data.get_pool_attr('cores', None)}")

        for pool in data['pools']:
            log.debug(f"Pool: {pool.get('name')}/{pool.get('id')} " \
                      f"Cores: {pool.get('cores')}, Apps: {pool.get('apps')}")

        # set initial SST-BF configuration
        if caps.sstbf_enabled():
            result = sstbf.init_sstbf(data)
            if result != 0:
                log.error("Failed to apply initial SST-BF configuration, terminating...")
                return

            log.info("SST-BF enabled, " \
                     f"{'not ' if not sstbf.is_sstbf_configured() else ''}configured.")
            log.info(f"SST-BF HP cores: {sstbf.get_hp_cores()}")
            log.info(f"SST-BF STD cores: {sstbf.get_std_cores()}")
        else:
            log.info("SST-BF not enabled")

        # set initial SST-CP configuration if SST-BF is not configured
        if caps.sstcp_enabled():
            if sstbf.is_sstbf_configured():
                log.info("Power Profiles/SST-CP enabled, not configured, SST-BF is configured")
            else:
                log.info("Power Profiles/SST-CP enabled.")
                # set initial POWER configuration
                result = power.configure_power(data)
                if result != 0:
                    log.error("Failed to apply initial Power Profiles configuration,"\
                        " terminating...")
                    return
        else:
            log.info("Power Profiles/EPP not enabled")

        # set initial RDT configuration
        log.info("Configuring RDT")

        # Configure MBA CTRL
        if caps.mba_supported():
            result = PQOS_API.enable_mba_bw(data.get_mba_ctrl_enabled())
            if result != 0:
                log.error("libpqos MBA CTRL initialization failed, Terminating...")
                return
            log.info(f"RDT MBA CTRL {'en' if PQOS_API.is_mba_bw_enabled() else 'dis'}abled")

        result = cache_ops.configure_rdt(data)
        if result != 0:
            log.error("Failed to apply initial RDT configuration, terminating...")
            return

        # set CTRL+C sig handler
        signal.signal(signal.SIGINT, self.signal_handler)

        self.event_handler()

        log.info("Terminating...")


    def event_handler(self):
        """
        Handles config_changed event
        """

        # rate limiting
        last_cfg_change_ts = 0
        min_time_diff = 1 / common.RATE_LIMIT

        while not self.stop_event.is_set():
            if ConfigStore().is_config_changed():
                cfg = ConfigStore.get_config()

                time_diff = time.time() - last_cfg_change_ts
                if time_diff < min_time_diff:
                    log.info("Rate Limiter, " \
                             f"sleeping {round((min_time_diff - time_diff) * 1000)} ms...")
                    time.sleep(min_time_diff - time_diff)

                log.info("Configuration changed, processing new config...")
                result = cache_ops.configure_rdt(cfg)
                if result != 0:
                    log.error("Failed to apply RDT configuration!")
                    break

                if caps.sstcp_enabled() and not sstbf.is_sstbf_configured():
                    result = power.configure_power(cfg)
                    if result != 0:
                        log.error("Failed to apply Power Profiles configuration!")
                        break

                last_cfg_change_ts = time.time()
                log.info("New configuration processed")


    def signal_handler(self, _signum, _frame):
        """
        Handles CTR+C
        """

        print("CTRL+C...")
        self.stop_event.set()

def load_config(config_file):
    """
    Loads config file.

    Parameters:
        config_file: config file path
    """

    # load config file
    try:
        ConfigStore().from_file(config_file)
    except FileNotFoundError as ex:
        log.error("Configuration file not found... ")
        log.error(ex)
        return -1
    except IOError as ex:
        log.error(f"Error reading from config file {config_file}... ")
        log.error(ex)
        return -1
    except ValidationError as ex:
        log.error(f"Config file validation failed - {ex}")
        return -1
    except Exception as ex:
        log.error("Invalid config file... ")
        log.error(ex)
        return -1

    return 0

def main():
    """
    Main entry point
    """

    # parse command line arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--config', metavar="PATH", help="Configuration file path")
    parser.add_argument('--port', metavar=("PORT"), default=[5000], type=int, nargs=1,
                        help="REST API port")
    parser.add_argument('-V', '--verbose', action='store_true', help="Verbose mode")
    parser.add_argument('-a', '--address', metavar="INET_ADDRESS", default=common.DEFAULT_ADDRESS,
                        help="AppQoS inet address")
    parser.add_argument('--version', action='store_true',
                        help="Output version information and exit")
    cmd_args = parser.parse_args()

    # configure syslog output
    syslog.openlog("AppQoS")

    if cmd_args.version:
        print(f"appqos, version {__version__}")
        sys.exit(0)

    if cmd_args.verbose:
        log.enable_verbose()

    # detect supported RDT interfaces
    PQOS_API.detect_supported_ifaces()

    # Load config file
    if load_config(cmd_args.config):
        log.error("Failed to load config file, Terminating...")
        return

    # initialize libpqos/Intel RDT interface
    result = PQOS_API.init(ConfigStore.get_config().get_rdt_iface())
    if result != 0:
        log.error("libpqos initialization failed, Terminating...")
        return
    log.info(f"RDT initialized with '{PQOS_API.current_iface()}' interface")

    # initialize capabilities
    result = caps.caps_init()
    if result == 0:
        # initialize main logic
        app_qos = AppQoS()

        # start REST API server
        server = rest_server.Server()
        result = server.start(cmd_args.address, cmd_args.port[0], cmd_args.verbose)
        if result == 0:
            # run main logic
            app_qos.run()

            # stop REST API server
            server.terminate()
        else:
            log.error("Failed to start REST API server, Terminating...")
    else:
        log.error("Required capabilities not supported, Terminating...")

    # de-initialize libpqos
    PQOS_API.fini()


if __name__ == '__main__':
    main()
