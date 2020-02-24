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

from pqos import Pqos
from pqos.cpuinfo import PqosCpuInfo
from pqos.mba import PqosMba


def str_to_int(num_str):
    """
    Converts string into number.

    Parameters:
        num_str: a string to be converted into number

    Returns:
        numeric value of the string representing the number
    """

    if num_str.lower().startswith('0x'):
        return int(num_str[2:], 16)

    return int(num_str)

# /**
#  * @brief Verifies and translates definition of single
#  *        allocation class of service
#  *        from args into internal configuration.
#  *
#  * @param argc Number of arguments in input command
#  * @param argv Input arguments for COS allocation
#  */

def set_allocation_class(sockets, class_id, mb_max):
    """
    Sets up allocation classes of service on selected CPU sockets

    Parameters:
        sockets: array with socket IDs
        class_id: class of service ID
        mb_max: COS rate in percent
    """

    mba = PqosMba()
    cos = mba.COS(class_id, mb_max)

    for socket in sockets:
        try:
            actual = mba.set(socket, [cos])

            params = (socket, class_id, mb_max, actual[0].mb_max)
            print("SKT%u: MBA COS%u => %u%% requested, %u%% applied" % params)
        except:
            print("Setting up cache allocation class of service failed!")


def print_allocation_config(sockets):
    """
    Prints allocation configuration.

    Parameters:
        sockets: array with socket IDs
    """

    mba = PqosMba()

    for socket in sockets:
        try:
            coses = mba.get(socket)

            print("MBA COS definitions for Socket %u:" % socket)

            for cos in coses:
                cos_params = (cos.class_id, cos.mb_max)
                print("    MBA COS%u => %u%% available" % cos_params)
        except:
            print("Error")
            raise


def parse_args():
    """
    Parses command line arguments.

    Returns:
        an object with parsed command line arguments
    """

    description = 'PQoS Library Python wrapper - MBA allocation example'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('-I', dest='interface', action='store_const',
                        const='OS', default='MSR',
                        help='select library OS interface')
    parser.add_argument('class_id', type=int, help='COS ID')
    parser.add_argument('mb_max', type=int, help='MBA rate')

    args = parser.parse_args()
    return args


class PqosContextManager:
    """
    Helper class for using PQoS library Python wrapper as a context manager
    (in a with statement).
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


def main():
    args = parse_args()

    try:
        with PqosContextManager(args.interface):
            cpu = PqosCpuInfo()
            sockets = cpu.get_sockets()

            set_allocation_class(sockets, args.class_id, args.mb_max)
            print_allocation_config(sockets)
    except:
        print("Error!")
        raise


if __name__ == "__main__":
    main()
