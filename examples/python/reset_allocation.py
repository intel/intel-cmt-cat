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
from pqos.allocation import PqosAlloc
from pqos.cpuinfo import PqosCpuInfo
from pqos.l3ca import PqosCatL3


def reset_allocation():
    """
    Resets allocation configuration.
    """

    alloc = PqosAlloc()

    try:
        alloc.reset('any', 'any', 'any')
        print("Allocation reset successful")
    except:
        print("Allocation reset failed!")


def print_allocation_config():
    """
    Prints allocation configuration.
    """

    l3ca = PqosCatL3()
    alloc = PqosAlloc()
    cpuinfo = PqosCpuInfo()
    sockets = cpuinfo.get_sockets()

    for socket in sockets:
        try:
            coses = l3ca.get(socket)

            print("L3CA COS definitions for Socket %u:" % socket)

            for cos in coses:
                cos_params = (cos.class_id, cos.mask)
                print("    L3CA COS%u => MASK 0x%x" % cos_params)
        except:
            print("Error")
            raise

    for socket in sockets:
        try:
            print("Core information for socket %u:" % socket)

            cores = cpuinfo.get_cores(socket)

            for core in cores:
                class_id = alloc.assoc_get(core)
                try:
                    print("    Core %u => COS%u" % (core, class_id))
                except:
                    print("    Core %u => ERROR" % core)
        except:
            print("Error")
            raise


def parse_args():
    """
    Parses command line arguments.

    Returns:
        an object with parsed command line arguments
    """

    description = 'PQoS Library Python wrapper - COS reset example'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('-I', dest='interface', action='store_const',
                        const='OS', default='MSR',
                        help='select library OS interface')

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
            reset_allocation()
            print_allocation_config()
    except:
        print("Error!")
        raise


if __name__ == "__main__":
    main()
