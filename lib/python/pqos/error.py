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
This module defines errors that the library can return.
"""

from __future__ import absolute_import, division, print_function


class PqosError(Exception):
    """
    Generic error returned from PQoS library.
    Field 'code' is an error code.
    """

    CODE = 1  # Generic error code

    def __init__(self, message, *args, **kwargs):
        super().__init__(message, *args, **kwargs)
        code = args[0] if args else kwargs.get(u'code')
        self.code = code or self.CODE


class PqosErrorParam(PqosError):
    "Parameter error returned from PQoS library"


class PqosErrorResource(PqosError):
    "Resource error returned from PQoS library"


class PqosErrorInit(PqosError):
    "Initialization error returned from PQoS library"


class PqosErrorTransport(PqosError):
    "Transport error returned from PQoS library"


class PqosErrorPerfCtr(PqosError):
    "Perf error returned from PQoS library"


class PqosErrorBusy(PqosError):
    "Busy error returned from PQoS library"


class PqosErrorInter(PqosError):
    "Internal error returned from PQoS library"


ERRORS = {
    1: PqosError,
    2: PqosErrorParam,
    3: PqosErrorResource,
    4: PqosErrorInit,
    5: PqosErrorTransport,
    6: PqosErrorPerfCtr,
    7: PqosErrorBusy,
    8: PqosErrorInter
}
