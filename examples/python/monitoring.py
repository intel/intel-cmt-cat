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

import argparse
import time

from pqos import Pqos
from pqos.capability import PqosCap, CPqosMonitor
from pqos.cpuinfo import PqosCpuInfo
from pqos.monitoring import PqosMon


def get_event_name(event_type):
    """
    Converts a monitoring event type to a string label required by libpqos
    Python wrapper.

    Parameters:
        event_type: monitoring event type

    Returns:
        a string label
    """

    event_map = {
        CPqosMonitor.PQOS_MON_EVENT_L3_OCCUP: 'l3_occup',
        CPqosMonitor.PQOS_MON_EVENT_LMEM_BW: 'lmem_bw',
        CPqosMonitor.PQOS_MON_EVENT_TMEM_BW: 'tmem_bw',
        CPqosMonitor.PQOS_MON_EVENT_RMEM_BW: 'rmem_bw',
        CPqosMonitor.PQOS_PERF_EVENT_LLC_MISS: 'perf_llc_miss',
        CPqosMonitor.PQOS_PERF_EVENT_IPC: 'perf_ipc'
    }

    return event_map.get(event_type)


def get_supported_events():
    """
    Returns a list of supported monitoring events.

    Returns:
        a list of supported monitoring events
    """

    cap = PqosCap()
    mon_cap = cap.get_type('mon')

    events = [get_event_name(event.type) for event in mon_cap.events]

    # Filter out perf events
    events = list(filter(lambda event: 'perf' not in event, events))

    return events


def get_all_cores():
    """
    Returns all available cores.

    Returns:
        a list of available cores
    """

    cores = []
    cpu = PqosCpuInfo()
    sockets = cpu.get_sockets()

    for socket in sockets:
        cores += cpu.get_cores(socket)

    return cores


def bytes_to_kb(num_bytes):
    """
    Converts bytes to kilobytes.

    Parameters:
        num_bytes number of bytes

    Returns:
        number of kilobytes
    """

    return num_bytes / 1024.0


def bytes_to_mb(num_bytes):
    """
    Converts bytes to megabytes.

    Parameters:
        num_bytes: number of bytes

    Returns:
        number of megabytes
    """

    return num_bytes / (1024.0 * 1024.0)


class Monitoring:
    "Generic class for monitoring"

    def __init__(self):
        self.mon = PqosMon()
        self.groups = []

    def setup_groups(self):
        "Sets up monitoring groups. Needs to be implemented by a derived class."

        return []

    def setup(self):
        "Resets monitoring and configures (starts) monitoring groups."

        self.mon.reset()
        self.groups = self.setup_groups()

    def update(self):
        "Updates values for monitored events."

        self.mon.poll(self.groups)

    def print_data(self):
        """Prints current values for monitored events. Needs to be implemented
        by a derived class."""

        pass

    def stop(self):
        "Stops monitoring."

        for group in self.groups:
            group.stop()


class MonitoringCore(Monitoring):
    "Monitoring per core"

    def __init__(self, cores, events):
        """
        Initializes object of this class with cores and events to monitor.

        Parameters:
            cores: a list of cores to monitor
            events: a list of monitoring events
        """

        super(MonitoringCore, self).__init__()
        self.cores = cores or get_all_cores()
        self.events = events

    def setup_groups(self):
        """
        Starts monitoring for each core using separate monitoring groups for
        each core.

        Returns:
            created monitoring groups
        """

        groups = []

        for core in self.cores:
            group = self.mon.start([core], self.events)
            groups.append(group)

        return groups

    def print_data(self):
        "Prints current values for monitored events."

        print("    CORE     RMID    LLC[KB]    MBL[MB]    MBR[MB]")

        for group in self.groups:
            core = group.cores[0]
            rmid = group.poll_ctx[0].rmid if group.poll_ctx else 'N/A'
            llc = bytes_to_kb(group.values.llc)
            mbl = bytes_to_mb(group.values.mbm_local_delta)
            mbr = bytes_to_mb(group.values.mbm_remote_delta)
            print("%8u %8s %10.1f %10.1f %10.1f" % (core, rmid, llc, mbl, mbr))


class MonitoringPid(Monitoring):
    "Monitoring per PID (OS interface only)"

    def __init__(self, pids, events):
        """
        Initializes object of this class with PIDs and events to monitor.

        Parameters:
            pids: a list of PIDs to monitor
            events: a list of monitoring events
        """

        super(MonitoringPid, self).__init__()
        self.pids = pids
        self.events = events

    def setup_groups(self):
        """
        Starts monitoring for each PID using separate monitoring groups for
        each PID.

        Returns:
            created monitoring groups
        """

        groups = []

        for pid in self.pids:
            group = self.mon.start_pids([pid], self.events)
            groups.append(group)

        return groups

    def print_data(self):
        "Prints current values for monitored events."

        print("   PID    LLC[KB]    MBL[MB]    MBR[MB]")

        for group in self.groups:
            pid = group.pids[0]
            llc = bytes_to_kb(group.values.llc)
            mbl = bytes_to_mb(group.values.mbm_local_delta)
            mbr = bytes_to_mb(group.values.mbm_remote_delta)
            print("%6d %10.1f %10.1f %10.1f" % (pid, llc, mbl, mbr))


class PqosContextManager:
    """
    Helper class for using PQoS library Python wrapper as a context manager
    (in with statement).
    """

    def __init__(self, *args, **kwargs):
        self.args = args
        self.kwargs = kwargs
        self.pqos = Pqos()

    def __enter__(self):
        "Initializes PQoS library."

        self.pqos.init(*self.args, **self.kwargs)
        return self.pqos

    def __exit__(self, *args, **kwargs):
        "Finalizes PQoS library."

        self.pqos.fini()
        return None


def parse_args():
    """
    Parses command line arguments.

    Returns:
        an object with parsed command line arguments
    """

    description = 'PQoS Library Python wrapper - monitoring example'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('-I', dest='interface', action='store_const',
                        const='OS', default='MSR',
                        help='select library OS interface')
    parser.add_argument('-p', '--pid', action='store_true',
                        help='select PID monitoring')
    parser.add_argument('cores_pids', metavar='CORE/PID', type=int, nargs='+',
                        help='a core or PID to be monitored')

    args = parser.parse_args()
    return args


def validate(params):
    """
    Validates command line arguments.

    Parameters:
        params: an object with parsed command line arguments
    """

    if params.pid and params.interface != 'OS':
        print('PID monitoring requires OS interface')
        return False

    return True


def main():
    "Main function that runs the example."

    args = parse_args()

    if not validate(args):
        return

    with PqosContextManager(args.interface):
        events = get_supported_events()

        if args.pid:
            monitoring = MonitoringPid(args.cores_pids, events)
        else:
            monitoring = MonitoringCore(args.cores_pids, events)

        monitoring.setup()

        while True:
            try:
                monitoring.update()
                monitoring.print_data()

                time.sleep(1.0)
            except KeyboardInterrupt:
                break

        monitoring.stop()


if __name__ == "__main__":
    main()
