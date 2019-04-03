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

import pytest
import mock

import common

import pqos_api

class TestPqosApi(object):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self):
       self.Pqos = pqos_api.PqosApi()
       self.Pqos.cap = mock.MagicMock()
       self.Pqos.cap.get_l3ca_cos_num = mock.MagicMock()
       self.Pqos.cap.get_mba_cos_num = mock.MagicMock()
    ## @endcond


    def test_get_max_cos_id(self):
       self.Pqos.cap.get_l3ca_cos_num.return_value = 16
       self.Pqos.cap.get_mba_cos_num.return_value = 8

       assert 7 == self.Pqos.get_max_cos_id([common.CAT_CAP, common.MBA_CAP])
       assert 7 == self.Pqos.get_max_cos_id([common.MBA_CAP])
       assert 15 == self.Pqos.get_max_cos_id([common.CAT_CAP])
       assert None == self.Pqos.get_max_cos_id([])

       self.Pqos.cap.get_l3ca_cos_num.return_value = 0
       assert None == self.Pqos.get_max_cos_id([common.CAT_CAP, common.MBA_CAP])
       assert 7 == self.Pqos.get_max_cos_id([common.MBA_CAP])
       assert None == self.Pqos.get_max_cos_id([common.CAT_CAP])
       assert None == self.Pqos.get_max_cos_id([])

       self.Pqos.cap.get_mba_cos_num.return_value = 0
       assert None == self.Pqos.get_max_cos_id([common.CAT_CAP, common.MBA_CAP])
       assert None == self.Pqos.get_max_cos_id([common.MBA_CAP])
       assert None == self.Pqos.get_max_cos_id([common.CAT_CAP])
       assert None == self.Pqos.get_max_cos_id([])


    def test_get_l3ca_num_cos(self):
       self.Pqos.cap.get_l3ca_cos_num.return_value = 32

       assert 32 == self.Pqos.get_l3ca_num_cos()


    def test_get_mba_cos_num(self):
       self.Pqos.cap.get_mba_cos_num.return_value = 16

       assert 16 == self.Pqos.get_mba_num_cos()
