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
from testlib.env import Env
from testlib.features_rdt_msr import FeaturesRdtMsr
from testlib.features_rdt_os import FeaturesRdtOs


def pytest_addoption(parser):
    parser.addoption("--iface-msr", action="store_true", dest="iface_msr",
                     help="MSR interface")
    parser.addoption("-I", "--iface-os", action="store_true", dest="iface_os",
                     help="OS interface")
    parser.addoption("--env", action="store", dest="env", required=True,
                     help="Path to environment file")


def pytest_configure(config):
    config.iface_msr = config.getoption("--iface-msr")
    config.iface_os = config.getoption("--iface-os")

    env_path = config.getoption("--env")
    # load env file
    if env_path:
        Env().load(env_path)

    if not config.iface_os and not config.iface_msr:
        config.iface_msr = True
        config.iface_os = True

    config.features = {}

    if config.iface_msr:
        msr_features = FeaturesRdtMsr()
        config.features['MSR'] = msr_features.get_features()

    if config.iface_os:
        os_features = FeaturesRdtOs()
        config.features['OS'] = os_features.get_features()


def pytest_generate_tests(metafunc):
    ## get required interface for test case
    def get_req_iface():
        req_iface = []
        if hasattr(metafunc, "definition"):
            if metafunc.definition.get_closest_marker("iface_os"):
                req_iface.append("OS")
            if metafunc.definition.get_closest_marker("iface_msr"):
                req_iface.append("MSR")
        if hasattr(metafunc.function, "iface_os"):
            req_iface.append("OS")
        if hasattr(metafunc.function, "iface_msr"):
            req_iface.append("MSR")
        if not req_iface:
            return ["MSR", "OS"]
        return req_iface

    if 'iface' in metafunc.fixturenames:
        req_iface = get_req_iface()

        iface = []
        if metafunc.config.iface_os and "OS" in req_iface:
            iface.append("OS")

        if metafunc.config.iface_msr and "MSR" in req_iface:
            iface.append("MSR")

        metafunc.parametrize("iface", iface)


def pytest_pyfunc_call(pyfuncitem):
    iface = "MSR"
    reqs_supported = []
    reqs_unsupported = []

    # get selected interface
    if "iface" in pyfuncitem.funcargs:
        iface = pyfuncitem.funcargs["iface"]

    # get features for selected interface
    features = pyfuncitem.config.features.get(iface, [])

    # get test requirements
    # supported features
    for req in pyfuncitem.iter_markers("rdt_supported"):
        reqs_supported += list(req.args)

    # unsupported features
    for req in pyfuncitem.iter_markers("rdt_unsupported"):
        reqs_unsupported += list(req.args)

    # skip tests based on required features
    # skip if feature requested but not supported
    for req in reqs_supported:
        if req not in features:
            pytest.skip("%s requirement not met" % req)

    # skip if feature requested to be unsupported but it is supported
    for req in reqs_unsupported:
        if req in features:
            pytest.skip("%s requirement met" % req)
