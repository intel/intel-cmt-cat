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
System capabilities module
"""

import cache_ops

import common
import log


# System capabilities are detected during the runtime
SYSTEM_CAPS = {}

def caps_init():
    """
    Runs supported capabilities detection and logs to console
    """
    global SYSTEM_CAPS

    if SYSTEM_CAPS:
        SYSTEM_CAPS.clear()

    SYSTEM_CAPS = detect_supported_caps()
    log.debug("Supported capabilities:")
    log.debug(SYSTEM_CAPS)

    if (cat_supported() or mba_supported()) and cache_ops.PQOS_API.is_multicore:
        return 0

    return -1


def cat_supported():
    """
    Returns CAT support status
    """
    return common.CAT_CAP in SYSTEM_CAPS


def mba_supported():
    """
    Returns MBA support status
    """
    return common.MBA_CAP in SYSTEM_CAPS


def detect_supported_caps():
    """
    Generates list of supported caps

    Returns
        list of supported caps
    """
    result = []
    # generate list of supported capabilities

    if cache_ops.PQOS_API.is_cat_supported():
        result.append(common.CAT_CAP)

    if cache_ops.PQOS_API.is_mba_supported():
        result.append(common.MBA_CAP)

    return result
