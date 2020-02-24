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
This module defines classes and functions used to mock Pqos class
and the underlying libpqos library's APIs.
"""

from __future__ import absolute_import, division, print_function

from pqos import Pqos


class PqosMock(object):
    "Pqos class mock."

    def __init__(self, lib):
        self.lib = lib

    def init(self, *args, **kwargs):
        "PQoS library initialization stub."

    def fini(self):
        "PQoS library finalization stub."


class CustomMock(object):  # pylint: disable=too-few-public-methods
    "Custom object that has methods that can be mocked later."


def mock_pqos_lib(func):
    "Mocks pqos library (ctypes DLL) in Pqos class."

    def wrapper(self):
        "Configures Pqos class to use mock library object."
        lib = CustomMock()
        instance_mock = PqosMock(lib)
        instance = Pqos()
        Pqos.set_instance(instance_mock)
        result = func(self, lib)
        Pqos.set_instance(instance)
        return result

    wrapper.__doc__ = func.__doc__
    return wrapper
