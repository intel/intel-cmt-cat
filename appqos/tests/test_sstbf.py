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

import sstbf


def test_is_sstbf_enabled():
    class SYS:
        def __init__(self, enabled):
            self.sst_bf_enabled = enabled

    with mock.patch("pwr.get_system", return_value=SYS(True)) as mock_get_system:
        assert sstbf.is_sstbf_enabled()
        mock_get_system.assert_called_once()

    with mock.patch("pwr.get_system", return_value=SYS(False)) as mock_get_system:
        assert not sstbf.is_sstbf_enabled()
        mock_get_system.assert_called_once()

    with mock.patch("pwr.get_system", return_value=None) as mock_get_system:
        assert not sstbf.is_sstbf_enabled()
        mock_get_system.assert_called_once()

    with mock.patch("pwr.get_system", side_effect = IOError('Test')) as mock_get_system:
        assert not sstbf.is_sstbf_enabled()
        mock_get_system.assert_called_once()


def test_is_sstbf_configured():
    class SYS:
        def __init__(self, configured):
            self.sst_bf_configured = configured

        def refresh_stats(self):
            return

    with mock.patch("pwr.get_system", return_value=SYS(True)) as mock_get_system:
        assert sstbf.is_sstbf_configured()
        mock_get_system.assert_called_once()

    with mock.patch("pwr.get_system", return_value=SYS(False)) as mock_get_system:
        assert not sstbf.is_sstbf_configured()
        mock_get_system.assert_called_once()

    with mock.patch("pwr.get_system", return_value=None) as mock_get_system:
        assert not sstbf.is_sstbf_configured()
        mock_get_system.assert_called_once()

    with mock.patch("pwr.get_system", return_value=[]) as mock_get_system:
        assert not sstbf.is_sstbf_configured()
        mock_get_system.assert_called_once()

    with mock.patch("pwr.get_system", side_effect = IOError('Test')) as mock_get_system:
        assert not sstbf.is_sstbf_configured()
        mock_get_system.assert_called_once()


def test_configure_sstbf():

    sys = mock.MagicMock()
    sys.commit = mock.MagicMock()

    with mock.patch("pwr.get_system", return_value=sys) as mock_get_sys:
        assert 0 == sstbf.configure_sstbf(True)
        mock_get_sys.assert_called_once()
        sys.commit.assert_called_with(sstbf.PWR_CFG_SST_BF)

        assert 0 == sstbf.configure_sstbf(False)
        sys.commit.assert_called_with(sstbf.PWR_CFG_BASE)

    with mock.patch("pwr.get_system", return_value=None) as mock_get_sys:
        assert 0 != sstbf.configure_sstbf(True)
        mock_get_sys.assert_called_once()

    with mock.patch("pwr.get_system", side_effect = IOError('Test')) as mock_get_sys:
        assert 0 != sstbf.configure_sstbf(True)
        mock_get_sys.assert_called_once()


def test_get_hp_cores():
    cores = []
    for i in range(0, 11):
        core = mock.MagicMock()
        core.core_id = i
        core.high_priority = (i % 2 == 1)
        cores.append(core)

    sstbf.HP_CORES = None
    with mock.patch("pwr.get_cores", return_value=cores) as mock_get_cores:
        assert list(range(1, 11, 2)) == sstbf.get_hp_cores()
        mock_get_cores.assert_called_once()

    for ret_val in [None, []]:
        sstbf.HP_CORES = None
        with mock.patch("pwr.get_cores", return_value=ret_val) as mock_get_cores:
            assert sstbf.get_hp_cores() == None

    sstbf.HP_CORES = None
    with mock.patch("pwr.get_cores", side_effect = IOError('Test')) as mock_get_cores:
        assert sstbf.get_hp_cores() == None
        mock_get_cores.assert_called_once()


def test_get_std_cores():
    cores = []
    for i in range(0, 11):
        core = mock.MagicMock()
        core.core_id = i
        core.high_priority = (i % 2 == 1)
        cores.append(core)

    sstbf.STD_CORES = None
    with mock.patch("pwr.get_cores", return_value=cores) as mock_get_cores:
        assert list(range(0, 11, 2)) == sstbf.get_std_cores()
        mock_get_cores.assert_called_once()

    for ret_val in [None, []]:
        sstbf.STD_CORES = None
        with mock.patch("pwr.get_cores", return_value=ret_val) as mock_get_cores:
            assert sstbf.get_std_cores() == None

    sstbf.STD_CORES = None
    with mock.patch("pwr.get_cores", side_effect = IOError('Test')) as mock_get_cores:
        assert sstbf.get_std_cores() == None
        mock_get_cores.assert_called_once()


def test_init_sstbf():
    for cfgd_value in [True, False]:
        with mock.patch("common.CONFIG_STORE.get_config", return_value={'sstbf' : {'configured' : cfgd_value}}) as mock_get_cfg,\
             mock.patch("sstbf.configure_sstbf", return_value=0) as mock_cfg,\
             mock.patch("sstbf._populate_cores") as mock_populate:

             assert 0 == sstbf.init_sstbf()
             mock_get_cfg.assert_called_once()
             mock_cfg.assert_called_once_with(cfgd_value)
             mock_populate.assert_called_once()
