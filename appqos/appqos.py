#!/usr/bin/env python2.7

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
Main module.
Parses command line arguments, sets up logging, initialises perf interface,
REST API server and runs main logic (AppQoS)
"""

import argparse
import multiprocessing
import signal
import syslog

from time import sleep
from jsonschema import ValidationError

import cache_ops
import caps
import common
import log
import perf
import pid_ops
import rest

from collectorperf import CollectingProcessPerf
from flusher import FlusherProcess
from processor import ProcessingProcess
from timer import TimerProcess


class AppQoS(object):
    """
    Main logic.
    Starts and stops all processes.
    Handles configuration changes.
    """


    def __init__(self):
        self.cprocesses = []
        self.pprocesses = []
        self.mprocesses = []
        self.stop_event = multiprocessing.Event()
        self.trigger_event = multiprocessing.Event()
        self.stats_queue = multiprocessing.Queue()
        self.flush_done_event = multiprocessing.Event()


    def start_collector_perf(self, pids):
        """
        Create perf collecting processes, one process per PID.

        Parameters:
            pids (list): a list of PIDs to start perf collecting process for

        Returns:
            num_valid_pids: number of valid PIDs processed
        """
        num_valid_pids = 0
        for pid in pids:
            # Validate PID
            if not pid_ops.is_pid_valid(pid):
                log.warn("PID {} is not valid".format(pid))
                continue

            log.info('Creating PERF collecting process for PID:{}'.format(pid))
            process = CollectingProcessPerf(self.stop_event, self.trigger_event, self.stats_queue,\
                                            pid, perf.get_supported_events())
            process.start()
            self.cprocesses.append(process)
            num_valid_pids += 1
        return num_valid_pids


    def run(self, args):
        """
        Runs main loop.

        Parameters:
            args: command line arguments
        """

        try:
            common.CONFIG_STORE.from_file(args.config)
        except IOError as ex:
            log.error("Error reading from config file {}... ".format(args.config))
            log.error(ex)
            return
        except (ValueError, ValidationError) as ex:
            log.error("Could not parse config file {}... ".format(args.config))
            log.error(ex)
            return

        # get SYS, HP, BE PIDs and cores
        p_pids = common.CONFIG_STORE.get_attr_list('pids', common.TYPE_DYNAMIC,\
                                                           common.PRODUCTION)
        p_cores = common.CONFIG_STORE.get_attr_list('cores', common.TYPE_DYNAMIC,\
                                                           common.PRODUCTION)

        pp_pids = common.CONFIG_STORE.get_attr_list('pids', common.TYPE_DYNAMIC,\
                                                           common.PRE_PRODUCTION)
        pp_cores = common.CONFIG_STORE.get_attr_list('cores', common.TYPE_DYNAMIC,\
                                                            common.PRE_PRODUCTION)

        be_pids = common.CONFIG_STORE.get_attr_list('pids', common.TYPE_DYNAMIC,\
                                                           common.BEST_EFFORT)
        be_cores = common.CONFIG_STORE.get_attr_list('cores', common.TYPE_DYNAMIC,\
                                                            common.BEST_EFFORT)

        static_pids = common.CONFIG_STORE.get_attr_list('pids', common.TYPE_STATIC, None)
        static_cores = common.CONFIG_STORE.get_attr_list('cores', common.TYPE_STATIC, None)

        log.debug("Dynamic Group, P Pool")
        log.debug("Cores: {}".format(p_cores))
        log.debug("PIDs: {}".format(p_pids))

        log.debug("Dynamic Group, PP Pool")
        log.debug("Cores: {}".format(pp_cores))
        log.debug("PIDs: {}".format(pp_pids))

        log.debug("Dynamic Group, BE Pool")
        log.debug("Cores: {}".format(be_cores))
        log.debug("PIDs: {}".format(be_pids))

        log.debug("Static Group, SYS Pool:")
        log.debug("Cores: {}".format(static_cores))
        log.debug("PIDs: {}".format(static_pids))

        # check for flusher module to be loaded
        if not FlusherProcess.pid_flush_module_loaded():
            log.error("PID flusher module not loaded, "\
                      "please load PID flusher module, terminating...")
            return

        # set initial, static CAT configuration
        log.info("Configuring CAT")
        result = cache_ops.configure_cat()
        if result != 0:
            log.error("Failed to apply initial CAT configuration, terminating...")
            return

        dyn_pids = common.CONFIG_STORE.get_attr_list('pids', common.TYPE_DYNAMIC, None)
        num_valid_pids = self.start_collector_perf(dyn_pids)

        if num_valid_pids != len(dyn_pids):
            log.error("Invalid PIDs found in config file, terminating...")
            self.stop_processes()
            return

        # Only one data processing thread
        log.info("Creating data processing process...")
        pprocess = ProcessingProcess(self.stop_event, self.trigger_event, self.stats_queue,\
                                     self.flush_done_event, common.ACQUISITION_MAX_TIME)
        self.pprocesses.append(pprocess)

        # create timer process
        log.info("Creating timer process...")
        tprocess = TimerProcess(self.stop_event, self.trigger_event, self.stats_queue,\
                                common.SAMPLING_INTERVAL)
        self.mprocesses.append(tprocess)

        # create flusher process
        log.info("Creating flusher process...")
        fprocess = FlusherProcess(self.stop_event, self.flush_done_event)
        self.mprocesses.append(fprocess)

        log.info("Starting all processes...")
        processes_array = [self.pprocesses, self.mprocesses]
        for processes_list in processes_array:
            for proc in processes_list:
                proc.start()

        # set CTRL+C sig handler
        signal.signal(signal.SIGINT, self.signal_handler)

        log.info("Waiting for threads... CTRL+C to terminate...")

        # cleanup finished processes
        while not self.stop_event.is_set() or self.cprocesses:
            log.debug("POOL stats: {}".format(common.STATS_STORE.get_pool_stats()))
            log.debug("APP stats: {}".format(common.STATS_STORE.get_app_stats()))
            log.debug("GENERAL stats: {}".format(common.STATS_STORE.general_stats_get()))

            for proc in self.cprocesses[:]:
                if not proc.is_alive() or self.stop_event.is_set():
                    proc.join()
                    log.info('PID: {}/{}, Joined...'.format(proc.pid, proc.get_mon_pid()))
                    dyn_pids.remove(proc.get_mon_pid())
                    self.cprocesses.remove(proc)
                    # check subsequent threads
                    continue

            if common.CONFIG_STORE.is_config_changed():
                log.info("Configuration changed")
                cache_ops.configure_cat()

                # start perf monitoring for new pids
                pids = common.CONFIG_STORE.get_attr_list('pids', common.TYPE_DYNAMIC, None)
                new_pids = [pid for pid in pids if pid not in dyn_pids]
                if new_pids:
                    self.start_collector_perf(new_pids)
                    dyn_pids.extend(new_pids)

            sleep(common.SAMPLING_INTERVAL)

        # do the clean-up
        log.info("Terminating...")
        self.stop_processes()

        return


    def stop_processes(self):
        """
        Stops all processes, via "stop event"
        """

        log.info("Sent stop signal to all processes...")
        self.stop_event.set()

        log.info("Waiting for all processes to end...")
        sleep(1)

        processes_array = [self.pprocesses, self.cprocesses, self.mprocesses]
        for processes_list in processes_array:
            for proc in processes_list[:]:
                proc.join(1)
                if not proc.is_alive():
                    log.info('PID: {}, Joined...'.format(proc.pid))
                    processes_list.remove(proc)
                else:
                    log.warn('PID: {} {}, failed to join, terminating...'.\
                             format(proc.pid, proc))
                    proc.terminate()
                    if not proc.is_alive():
                        log.info('PID: {}, Terminated...'.format(proc.pid))
                        processes_list.remove(proc)
                    else:
                        log.error('PID: {} {}, failed to terminate...'.\
                                  format(proc.pid, proc))

        log.info("Processes ended...")


    def signal_handler(self, _signum, _frame):
        """
        Handles CTR+C
        """

        print "CTRL+C..."
        self.stop_processes()


def main():
    """
    Main entry point
    """

    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--config', metavar="PATH", default=common.CONFIG_FILENAME,
                        help="Configuration file path")
    parser.add_argument('--port', metavar=("PORT"), default=[5000], type=int, nargs=1,
                        help="REST API port")
    parser.add_argument('-V', '--verbose', action='store_true', help="Verbose mode")
    cmd_args = parser.parse_args()

    # configure syslog output
    syslog.openlog("App QoS")

    if cmd_args.verbose:
        log.enable_verbose()

    # initialize perf
    result = perf.perf_event_init()
    if result != 0:
        log.error("Required perf events not supported, Terminating...")
        return

    result = cache_ops.PQOS_API.init()
    if result != 0:
        log.error("libpqos initialization failed, Terminating...")
        return

    # initialize capabilities
    result = caps.caps_init()
    if result == 0:
        # initialize main logic
        app_qos = AppQoS()

        # start REST API server
        server = rest.Server()
        result = server.start("0.0.0.0", cmd_args.port[0], cmd_args.verbose)
        if result == 0:
            # run main logic
            app_qos.run(cmd_args)

            # stop REST API server
            server.terminate()
        else:
            log.error("Failed to start REST API server, Terminating...")
    else:
        log.error("Required capabilities not supported, Terminating...")

    # de-initialize libpqos
    cache_ops.PQOS_API.fini()


if __name__ == '__main__':
    main()
