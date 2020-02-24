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

import pytest
import mock

from pid_ops import *

def test_is_pid_valid():
    with mock.patch('pid_ops.get_pid_status', return_value=('R', True, 'TestApp')) as get_pid_status_mock:
        assert True == is_pid_valid(4321)
        get_pid_status_mock.assert_called_with(4321)

    with mock.patch('pid_ops.get_pid_status', return_value=('Z', False, 'TestAppZombi')) as get_pid_status_mock:
        assert False == is_pid_valid(1234)
        get_pid_status_mock.assert_called_with(1234)


def test_get_pid_status():
    with mock.patch('pid_ops._read_pid_state', return_value=('R', 'TestApp')) as read_pid_state_mock:
        assert ('R', True, 'TestApp') == get_pid_status(2345)
        read_pid_state_mock.assert_called_with(2345)

        assert ('R', True, 'TestApp') == get_pid_status(0)
        read_pid_state_mock.assert_called_with("self")

    with mock.patch('pid_ops._read_pid_state', side_effect=OSError()):
        assert ('E', False, '') == get_pid_status(2345)
