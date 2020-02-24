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

import mock
import pytest

import common
import caps

def test_caps_init():
    with mock.patch('caps.detect_supported_caps', return_value=[common.CAT_CAP, common.MBA_CAP, common.SSTBF_CAP]) as mock_detect_caps,\
         mock.patch('common.PQOS_API.is_multicore', return_value = True) as mock_is_multi:

        assert not caps.caps_init()

        mock_detect_caps.assert_called_once()
        mock_is_multi.assert_called_once()

        caps.SYSTEM_CAPS = ["TEST"]
        assert not caps.caps_init()

        mock_is_multi.return_value = False
        assert caps.caps_init() == -1


@mock.patch("common.PQOS_API.is_l3_cat_supported", mock.MagicMock(return_value=True))
@mock.patch("common.PQOS_API.is_mba_supported", mock.MagicMock(return_value=False))
@mock.patch("sstbf.is_sstbf_enabled", mock.MagicMock(return_value=False))
def test_detect_cat_l3():
    assert common.CAT_CAP in caps.detect_supported_caps()
    assert not common.MBA_CAP in caps.detect_supported_caps()
    assert not common.SSTBF_CAP in caps.detect_supported_caps()


@mock.patch("common.PQOS_API.is_l3_cat_supported", mock.MagicMock(return_value=False))
@mock.patch("common.PQOS_API.is_mba_supported", mock.MagicMock(return_value=False))
@mock.patch("sstbf.is_sstbf_enabled", mock.MagicMock(return_value=False))
def test_detect_cat_l3_negative():
    assert common.CAT_CAP not in caps.detect_supported_caps()


@mock.patch("common.PQOS_API.is_l3_cat_supported", mock.MagicMock(return_value=False))
@mock.patch("common.PQOS_API.is_mba_supported", mock.MagicMock(return_value=True))
@mock.patch("sstbf.is_sstbf_enabled", mock.MagicMock(return_value=False))
def test_detect_mba():
    assert not common.CAT_CAP in caps.detect_supported_caps()
    assert common.MBA_CAP in caps.detect_supported_caps()
    assert not common.SSTBF_CAP in caps.detect_supported_caps()


@mock.patch("common.PQOS_API.is_l3_cat_supported", mock.MagicMock(return_value=False))
@mock.patch("common.PQOS_API.is_mba_supported", mock.MagicMock(return_value=False))
@mock.patch("sstbf.is_sstbf_enabled", mock.MagicMock(return_value=True))
def test_detect_mba_negative():
    assert not common.CAT_CAP in caps.detect_supported_caps()
    assert not common.MBA_CAP in caps.detect_supported_caps()
    assert common.SSTBF_CAP in caps.detect_supported_caps()
