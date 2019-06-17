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

import mock
import pytest

import sstbf
import os

BF_PATH = "/sys/devices/system/cpu/cpu{}/cpufreq/base_frequency"


@pytest.mark.parametrize("isfile_res, which_res, result",
    [(True, "/usr/bin/sst_bf.py", True),
     (False, "/usr/bin/sst_bf.py", False),
     (True, None, False),
     (False, None, False)])
def test_is_sstbf_supported(isfile_res, which_res, result):
    with mock.patch("os.path.isfile", return_value=isfile_res) as mock_func_isfile,\
         mock.patch("shutil.which", return_value=which_res) as mock_func_which:

        assert sstbf.is_sstbf_supported() == result

        mock_func_isfile.assert_called_once_with(BF_PATH.format(0))

        if isfile_res:
            mock_func_which.assert_called_once()


def test_is_sstbf_enabled():
    with mock.patch("sstbf.is_sstbf_supported", return_value=True) as mock_supp,\
         mock.patch("sstbf._get_all_cores", return_value=[0,5,10,15]) as mock_all_cores,\
         mock.patch("sstbf._get_core_freqs", return_value=(2000,2000,2000)) as mock_get_freq:

        assert sstbf.is_sstbf_enabled()
        mock_supp.assert_called_once()
        mock_all_cores.assert_called_once()

        calls = [mock.call(0), mock.call(5), mock.call(10), mock.call(15)]
        mock_get_freq.assert_has_calls(calls, any_order=True)

        mock_supp.return_value=False
        assert not sstbf.is_sstbf_enabled()

        mock_supp.return_value=True

        mock_all_cores.return_value=[]
        assert not sstbf.is_sstbf_enabled()

        mock_all_cores.return_value=None
        assert not sstbf.is_sstbf_enabled()

        mock_all_cores.return_value=[0,5,10,15]

        mock_get_freq.return_value=(2700,2300,2300)
        assert not sstbf.is_sstbf_enabled()

        mock_get_freq.return_value=(2000,2000,2000)

        assert sstbf.is_sstbf_enabled()


def test_enable_sstbf():

    class A:
        def __init__(self):
            self.stdout = "Test"

    with mock.patch("sstbf._run_sstbf", return_value=A()) as mock_run:
        assert 0 == sstbf.enable_sstbf(True)
        mock_run.assert_called_once_with('-s')

        mock_run.reset_mock()

        assert 0 == sstbf.enable_sstbf(False)
        mock_run.assert_called_once_with('-m')

        mock_run.reset_mock()

        mock_run.return_value = None
        assert 0 != sstbf.enable_sstbf(False)
        assert 0 != sstbf.enable_sstbf(True)


def test_get_hp_cores():
    with mock.patch("sstbf._sstbf_get_hp_cores", return_value=[3, 33, 333]) as mock_hp_cores:
        assert [3, 33, 333] == sstbf.get_hp_cores()
        mock_hp_cores.assert_called_once()


def test_get_std_cores():
    with mock.patch("sstbf.get_hp_cores", return_value=[0,3,6]) as mock_hp_cores,\
         mock.patch("sstbf._get_all_cores", return_value=[0,1,2,3,4,5,6]) as mock_all_cores:
        assert [1,2,4,5] == sstbf.get_std_cores()


def test__get_all_cores():
    NUM_CORES = 10
    with mock.patch("common.PQOS_API.get_num_cores", return_value=NUM_CORES) as mock_num_cores:
        assert list(range(NUM_CORES)) == sstbf._get_all_cores()


@pytest.mark.parametrize("sstbf_l_out, expected",
    [("1,6,7,8,9,16,21,26,27,28,29,30\n0x7c2103c2", [1,6,7,8,9,16,21,26,27,28,29,30]),
     ("1,6,7,8,INV,16,21,26,27,28,29,30\n0x7c2103c2", None),
     ("", None),
     ("0x7c2103c2", None)])
def test__sstbf_get_hp_cores(sstbf_l_out, expected):

    run_result_mock = mock.MagicMock()
    type(run_result_mock).stdout = mock.PropertyMock(return_value=sstbf_l_out)

    with mock.patch("sstbf._run_sstbf", return_value=run_result_mock) as mock_run:
        assert expected == sstbf._sstbf_get_hp_cores()
        mock_run.assert_called_once_with('-l')

def test__get_core_freqs():
    CORE = 99
    BF_PATH = "/sys/devices/system/cpu/cpu{}/cpufreq/base_frequency"
    MAX_PATH = "/sys/devices/system/cpu/cpu{}/cpufreq/scaling_max_freq"
    MIN_PATH = "/sys/devices/system/cpu/cpu{}/cpufreq/scaling_min_freq"
    def read_int(fname):
        if fname == BF_PATH.format(CORE):
            return 1000
        elif fname == MIN_PATH.format(CORE):
            return 2000
        elif fname == MAX_PATH.format(CORE):
            return 3000
        else:
            return None

    with mock.patch("sstbf._read_int_from_file", side_effect=read_int) as mock_read_int:
        assert (1000, 2000, 3000) == sstbf._get_core_freqs(CORE)
        mock_read_int.assert_any_call(BF_PATH.format(CORE))
        mock_read_int.assert_any_call(MIN_PATH.format(CORE))
        mock_read_int.assert_any_call(MAX_PATH.format(CORE))
