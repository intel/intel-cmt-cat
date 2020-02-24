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
Common module which defines functions used in other modules.
"""

from __future__ import absolute_import, division, print_function
import ctypes

from pqos.error import ERRORS, PqosError


def pqos_handle_error(func_name, retval, expected=0):
    """
    Handles errors from PQoS library, raises a relevant exception
    if the returned error code is different than the expected one.
    """

    if retval == expected:
        return

    pqos_error_cls = ERRORS.get(retval, PqosError)
    err = pqos_error_cls(u'%s returned %d' % (func_name, retval), retval)
    raise err


def get_mask_int(mask):
    "Returns a bitmask as an integer."

    if isinstance(mask, type(u'')):
        if mask.lower().startswith('0x'):
            return int(mask.lower(), 16)

        return int(mask)

    if isinstance(mask, int):
        return mask

    if mask is None:
        return 0

    raise ValueError(u'Please specify mask as either a string,' \
                     u' an integer or None')


class COSBase(object):
    "Cache allocation class of service configuration"

    def __init__(self, class_id, mask=None, code_mask=None, data_mask=None):
        """
        Initializes cache allocation COS configuration object.

        Parameters:
            class_id: a class of service
            mask: a bitmask (an integer or a string starting from '0x')
                  representing used cache ways or None, if None is given,
                  then code_mask and data_mask must be set (default None)
            code_mask: a bitmask (an integer or a string starting from '0x')
                       representing used cache ways for code or None,
                       if None is given, then mask must be set
                       (default None)
            data_mask: a bitmask (an integer or a string starting from '0x')
                       representing used cache ways for data or None,
                       if None is given, then mask must be set
                       (default None)
        """
        if not mask and (not code_mask or not data_mask):
            raise ValueError(u'Please specify mask or code mask'
                             u' and data mask')

        self.class_id = class_id
        self.mask = get_mask_int(mask)
        self.code_mask = get_mask_int(code_mask)
        self.data_mask = get_mask_int(data_mask)
        self.cdp = bool(code_mask is not None or \
                   data_mask is not None)

    def __repr__(self):
        params = (self.class_id, repr(self.mask), repr(self.code_mask),
                  repr(self.data_mask))
        return u'COS(class_id=%s, mask=%s, code_mask=%s,' \
               u' data_mask=%s)' % params


def convert_from_cos(cos, cls):
    "Creates ctypes COS object from COS object."

    ctypes_cos = cls(class_id=cos.class_id, cdp=int(cos.cdp))

    if cos.cdp:
        ctypes_cos.u.s.data_mask = cos.data_mask
        ctypes_cos.u.s.code_mask = cos.code_mask
    else:
        ctypes_cos.u.ways_mask = cos.mask

    return ctypes_cos


def convert_to_cos(ctypes_cos, cls):
    "Creates COS object from ctypes COS object."

    mask = None
    code_mask = None
    data_mask = None

    if ctypes_cos.cdp:
        code_mask = ctypes_cos.u.s.code_mask
        data_mask = ctypes_cos.u.s.data_mask
    else:
        mask = ctypes_cos.u.ways_mask

    class_id = ctypes_cos.class_id

    return cls(class_id, mask, code_mask, data_mask)

def free_memory(ptr):
    "Releases memory allocated by the library."

    libc_path = ctypes.util.find_library(u'c')

    if not libc_path:
        raise Exception(u'Cannot find libc')

    libc = ctypes.cdll.LoadLibrary(libc_path)
    libc.free(ptr)
