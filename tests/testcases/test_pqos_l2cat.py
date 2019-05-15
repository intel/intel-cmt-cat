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

import test
import re
import pytest
from testlib.env import Env
from priority import PRIORITY_MEDIUM, PRIORITY_HIGH

class TestPqosL2Cat(test.Test):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self, request):
        super(TestPqosL2Cat, self).init(request)
        yield
        super(TestPqosL2Cat, self).fini()
    ## @endcond

    ## PQOS - L2 CAT Detection - Negative
    #
    #  \b Priority: High
    #
    #  \b Objective:
    #  Verify L2 CAT capability is not detected on platform
    #
    #  \b Instruction:
    #  Run "pqos [-I] -s -v" to print platform RDT capabilities and configuration in verbose
    #  mode
    #
    #  \b Result:
    #  Observe "L2CA capability not detected" in output
    @PRIORITY_HIGH
    @pytest.mark.rdt_unsupported("cat_l2")
    def test_pqos_l2cat_detection_negative(self, iface):
        (stdout, _, exitstatus) = self.run_pqos(iface, "-s -v")
        assert exitstatus == 0
        assert "L2CA capability not detected" in stdout

