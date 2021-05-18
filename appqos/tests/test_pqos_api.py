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
from pqos.capability import PqosCapabilityL2Ca, PqosCapabilityL3Ca
from pqos.error import PqosErrorResource

import common

import pqos_api

class TestPqosApi(object):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self):
        self.Pqos_api = pqos_api.PqosApi()

        self.Pqos_api.cap = mock.MagicMock()
        self.Pqos_api.cap.get_l3ca_cos_num = mock.MagicMock()
        self.Pqos_api.cap.get_l2ca_cos_num = mock.MagicMock()
        self.Pqos_api.cap.get_mba_cos_num = mock.MagicMock()
        self.Pqos_api.cap.get_type = mock.MagicMock()

        self.Pqos_api.alloc = mock.MagicMock()
        self.Pqos_api.alloc.release = mock.MagicMock()
        self.Pqos_api.alloc.reset = mock.MagicMock()
        self.Pqos_api.alloc.assoc_set = mock.MagicMock()

        self.Pqos_api.l3ca = mock.MagicMock()
        self.Pqos_api.l3ca.COS = mock.MagicMock()
        self.Pqos_api.l3ca.set = mock.MagicMock()

        self.Pqos_api.mba = mock.MagicMock()
        self.Pqos_api.mba.COS = mock.MagicMock()
        self.Pqos_api.mba.set = mock.MagicMock()

        self.Pqos_api.cpuinfo = mock.MagicMock()
        self.Pqos_api.cpuinfo.get_sockets = mock.MagicMock()
        self.Pqos_api.cpuinfo.get_cores = mock.MagicMock()
        self.Pqos_api.cpuinfo.check_core = mock.MagicMock()
    ## @endcond


    def test_get_max_l3_cat_cbm(self):
        with mock.patch('pqos_api.PqosApi.is_l3_cat_supported', return_value = False):
            assert None == self.Pqos_api.get_max_l3_cat_cbm()

        with mock.patch('pqos_api.PqosApi.is_l3_cat_supported', return_value = True):

            class A:
                def __init__(self):
                    self.num_ways = 4

            self.Pqos_api.cap.get_type.return_value = A()

            assert 0xF == self.Pqos_api.get_max_l3_cat_cbm()
            self.Pqos_api.cap.get_type.assert_called_once_with("l3ca")

            self.Pqos_api.cap.get_type.side_effect = Exception('Test')
            assert None == self.Pqos_api.get_max_l3_cat_cbm()


    def test_get_mba_num_cos(self):
        self.Pqos_api.cap.get_mba_cos_num.return_value = 55
        assert 55 == self.Pqos_api.get_mba_num_cos()
        self.Pqos_api.cap.get_mba_cos_num.assert_called_once()

        self.Pqos_api.cap.get_mba_cos_num.side_effect = Exception('Test')
        assert None == self.Pqos_api.get_mba_num_cos()


    def test_get_l3ca_num_cos(self):
        self.Pqos_api.cap.get_l3ca_cos_num.return_value = 44
        assert 44 == self.Pqos_api.get_l3ca_num_cos()
        self.Pqos_api.cap.get_l3ca_cos_num.assert_called_once()

        self.Pqos_api.cap.get_l3ca_cos_num.side_effect = Exception('Test')
        assert None == self.Pqos_api.get_l3ca_num_cos()


    def test_get_sockets(self):
        self.Pqos_api.cpuinfo.get_sockets.return_value = [0, 1]
        assert [0, 1] == self.Pqos_api.get_sockets()
        self.Pqos_api.cpuinfo.get_sockets.assert_called_once()

        self.Pqos_api.cpuinfo.get_sockets.side_effect = Exception('Test')
        assert None == self.Pqos_api.get_sockets()


    def test_check_core(self):
        self.Pqos_api.cpuinfo.check_core.return_value = True
        assert True == self.Pqos_api.check_core(104)
        self.Pqos_api.cpuinfo.check_core.assert_called_once_with(104)

        self.Pqos_api.cpuinfo.check_core.return_value = False
        assert False == self.Pqos_api.check_core(105)
        self.Pqos_api.cpuinfo.check_core.assert_called_with(105)

        self.Pqos_api.cpuinfo.check_core.side_effect = Exception('Test')
        assert None == self.Pqos_api.check_core(106)


    def test_get_num_cores(self):
        self.Pqos_api.cpuinfo.get_sockets.return_value = [0, 1]
        self.Pqos_api.cpuinfo.get_cores.return_value = list(range(0, 10))

        assert 20 == self.Pqos_api.get_num_cores()

        self.Pqos_api.cpuinfo.get_sockets.assert_called_once()
        self.Pqos_api.cpuinfo.get_cores.assert_any_call(0)
        self.Pqos_api.cpuinfo.get_cores.assert_any_call(1)

        self.Pqos_api.cpuinfo.get_sockets.side_effect = Exception('Test')
        assert None == self.Pqos_api.get_num_cores()
        self.Pqos_api.cpuinfo.get_sockets.mock_reset(side_effect = True)

        self.Pqos_api.cpuinfo.get_cores.side_effect = Exception('Test')
        assert None == self.Pqos_api.get_num_cores()


    def test_is_multicore(self):
        with mock.patch('pqos_api.PqosApi.get_num_cores', return_value = 1):
            assert False == self.Pqos_api.is_multicore()

        with mock.patch('pqos_api.PqosApi.get_num_cores', return_value = 2):
            assert True == self.Pqos_api.is_multicore()


    def test_is_l3_cat_supported(self):
        with mock.patch('pqos_api.PqosApi.get_l3ca_num_cos', return_value = 0):
            assert False == self.Pqos_api.is_l3_cat_supported()

        with mock.patch('pqos_api.PqosApi.get_l3ca_num_cos', return_value = 8):
            assert True == self.Pqos_api.is_l3_cat_supported()

        with mock.patch('pqos_api.PqosApi.get_l3ca_num_cos', side_effect = Exception('Test')):
            assert 0 == self.Pqos_api.is_l3_cat_supported()


    def test_is_mba_supported(self):
        with mock.patch('pqos_api.PqosApi.get_mba_num_cos', return_value = 0):
            assert False == self.Pqos_api.is_mba_supported()

        with mock.patch('pqos_api.PqosApi.get_mba_num_cos', return_value = 8):
            assert True == self.Pqos_api.is_mba_supported()

        with mock.patch('pqos_api.PqosApi.get_mba_num_cos', side_effect = Exception('Test')):
            assert 0 == self.Pqos_api.is_mba_supported()


    def test_mba_set(self):
        self.Pqos_api.mba.COS.return_value = 0xDEADBEEF
        assert 0 == self.Pqos_api.mba_set([0], 1, 44)
        self.Pqos_api.mba.COS.assert_called_once_with(1, 44, False)
        self.Pqos_api.mba.set.assert_called_once_with(0, [0xDEADBEEF])

        self.Pqos_api.mba.set.mock_reset()
        self.Pqos_api.mba.COS.mock_reset()
        self.Pqos_api.mba.COS.return_value = 0xDEADBEEF
        assert 0 == self.Pqos_api.mba_set([0,1], 2, 44)
        self.Pqos_api.mba.set.assert_any_call(0, [0xDEADBEEF])
        self.Pqos_api.mba.set.assert_any_call(1, [0xDEADBEEF])

        # socket param not a list
        assert -1 == self.Pqos_api.mba_set(0, 1, 44)

        self.Pqos_api.mba.COS.side_effect = Exception('Test')
        assert -1 == self.Pqos_api.mba_set([0], 1, 44)
        self.Pqos_api.mba.COS.mock_reset()
        self.Pqos_api.mba.COS.return_value = 0xDEADBEEF

        self.Pqos_api.mba.set.side_effect = Exception('Test')
        assert -1 == self.Pqos_api.mba_set([0], 1, 44)


    def test_l3ca_set(self):
        self.Pqos_api.l3ca.COS.return_value = 0xDEADBEEF
        assert 0 == self.Pqos_api.l3ca_set([0], 1, 0xff)
        self.Pqos_api.l3ca.COS.assert_called_once_with(1, 0xff)
        self.Pqos_api.l3ca.set.assert_called_once_with(0, [0xDEADBEEF])

        self.Pqos_api.l3ca.set.mock_reset()
        assert 0 == self.Pqos_api.l3ca_set([0,1], 1, 0xff)
        self.Pqos_api.l3ca.set.assert_any_call(0, [0xDEADBEEF])
        self.Pqos_api.l3ca.set.assert_any_call(1, [0xDEADBEEF])

        # socket param not a list
        assert -1 == self.Pqos_api.l3ca_set(0, 1, 0xff)

        self.Pqos_api.l3ca.COS.side_effect = Exception('Test')
        assert -1 == self.Pqos_api.l3ca_set([0], 1, 0xff)
        self.Pqos_api.l3ca.COS.mock_reset(side_effect = True)

        self.Pqos_api.l3ca.set.side_effect = Exception('Test')
        assert -1 == self.Pqos_api.l3ca_set([0], 1, 0xff)


    @mock.patch("os.system", mock.MagicMock(return_value=0))
    @pytest.mark.parametrize("iface, supp_iface", [
        ("msr", ["msr"]),
        ("os", ["os"]),
        ("msr", ["msr", "os"]),
        ("os", ["msr", "os"]),
    ])
    def test_init(self, iface, supp_iface):
        with mock.patch('pqos.Pqos.init') as pqos_init_mock,\
             mock.patch('pqos.Pqos.fini', return_value = 0) as pqos_finit_mock,\
             mock.patch('pqos.capability.PqosCap.__init__', return_value = None) as pqos_cap_init_mock,\
             mock.patch('pqos.l3ca.PqosCatL3.__init__', return_value = None) as pqos_cat_l3_init_mock,\
             mock.patch('pqos.mba.PqosMba.__init__', return_value = None) as pqos_mba_init_mock,\
             mock.patch('pqos.allocation.PqosAlloc.__init__', return_value = None) as pqos_alloc_init_mock,\
             mock.patch('pqos.cpuinfo.PqosCpuInfo.__init__', return_value = None) as pqos_cpu_info_init_mock,\
             mock.patch('pqos_api.PqosApi.supported_iface', return_value = supp_iface):

            assert 0 == self.Pqos_api.init(iface)

            pqos_init_mock.assert_called_once_with(iface.upper())
            pqos_cap_init_mock.assert_called_once()
            pqos_cat_l3_init_mock.assert_called_once()
            pqos_mba_init_mock.assert_called_once()
            pqos_alloc_init_mock.assert_called_once()
            pqos_cpu_info_init_mock.assert_called_once()

            pqos_cpu_info_init_mock.side_effect = Exception('Test')
            assert -1 == self.Pqos_api.init(iface)


    @pytest.mark.parametrize("iface", ["invalid_iface", "resctrl"])
    def test_init_invalid_iface(self, iface):
        with mock.patch('pqos_api.PqosApi.supported_iface', return_value = ["msr", "os"]):
            assert -1 == self.Pqos_api.init(iface)


    @pytest.mark.parametrize("iface, supp_ifaces", [("os", ["msr"]), ("msr", ["os"])])
    def test_init_unsupported_iface(self, iface, supp_ifaces):
        with mock.patch('pqos_api.PqosApi.supported_iface', return_value = supp_ifaces):
            assert -1 == self.Pqos_api.init(iface)


    def test_fini(self):
        with mock.patch('pqos.Pqos.fini') as pqos_fini_mock:
            self.Pqos_api.fini()
            pqos_fini_mock.assert_called_once()


    def test_release(self):
        assert 0 == self.Pqos_api.release(None)
        self.Pqos_api.alloc.release.assert_not_called

        assert 0 == self.Pqos_api.release(0)
        self.Pqos_api.alloc.release.assert_called_once_with(0)

        self.Pqos_api.alloc.release.reset_mock()
        assert 0 == self.Pqos_api.release(5)
        self.Pqos_api.alloc.release.assert_called_once_with(5)

        self.Pqos_api.alloc.release.side_effect = Exception('Test')
        assert -1 == self.Pqos_api.release(5)


    def test_alloc_assoc_set(self):
        assert 0 == self.Pqos_api.alloc_assoc_set([], 0)
        self.Pqos_api.alloc.assoc_set.assert_not_called

        assert 0 == self.Pqos_api.alloc_assoc_set([1], 2)
        self.Pqos_api.alloc.assoc_set.assert_called_once_with(1, 2)

        self.Pqos_api.alloc.assoc_set.reset_mock()
        assert 0 == self.Pqos_api.alloc_assoc_set([2,3,4], 3)
        self.Pqos_api.alloc.assoc_set.assert_any_call(2, 3)
        self.Pqos_api.alloc.assoc_set.assert_any_call(3, 3)
        self.Pqos_api.alloc.assoc_set.assert_any_call(4, 3)

        self.Pqos_api.alloc.assoc_set.side_effect = Exception('Test')
        assert -1 == self.Pqos_api.alloc_assoc_set([0,1], 5)


    def test_get_max_cos_id(self):
       self.Pqos_api.cap.get_l3ca_cos_num.return_value = 16
       self.Pqos_api.cap.get_mba_cos_num.return_value = 8

       assert 7 == self.Pqos_api.get_max_cos_id([common.CAT_L3_CAP, common.MBA_CAP])
       assert 7 == self.Pqos_api.get_max_cos_id([common.MBA_CAP])
       assert 15 == self.Pqos_api.get_max_cos_id([common.CAT_L3_CAP])
       assert None == self.Pqos_api.get_max_cos_id([])

       self.Pqos_api.cap.get_l3ca_cos_num.return_value = 0
       assert None == self.Pqos_api.get_max_cos_id([common.CAT_L3_CAP, common.MBA_CAP])
       assert 7 == self.Pqos_api.get_max_cos_id([common.MBA_CAP])
       assert None == self.Pqos_api.get_max_cos_id([common.CAT_L3_CAP])
       assert None == self.Pqos_api.get_max_cos_id([])

       self.Pqos_api.cap.get_mba_cos_num.return_value = 0
       assert None == self.Pqos_api.get_max_cos_id([common.CAT_L3_CAP, common.MBA_CAP])
       assert None == self.Pqos_api.get_max_cos_id([common.MBA_CAP])
       assert None == self.Pqos_api.get_max_cos_id([common.CAT_L3_CAP])
       assert None == self.Pqos_api.get_max_cos_id([])


    @mock.patch("pqos_api.PqosApi.is_mba_bw_supported", mock.MagicMock(return_value=False))
    @mock.patch("pqos_api.PqosApi.fini", mock.MagicMock(return_value=0))
    @pytest.mark.parametrize("supp_iface", [
        (["msr"]),
        (["os"]),
        (["msr", "os"])
    ])
    def test_detect_supported_ifaces(self, supp_iface):
        def mock_init(self, iface, force):
            assert force
            return 0 if iface in supp_iface else -1

        with mock.patch('pqos_api.PqosApi.init', new=mock_init):
            self.Pqos_api.detect_supported_ifaces()
            assert self.Pqos_api.supported_iface() == supp_iface


    @pytest.mark.parametrize("mba_ctrl_supp", [True, False])
    @pytest.mark.parametrize("mba_ctrl_en", [True, False])
    def test_refresh_mba_bw_status(self, mba_ctrl_supp, mba_ctrl_en):
        self.Pqos_api.cap.is_mba_ctrl_enabled.return_value = (mba_ctrl_supp, mba_ctrl_en)

        self.Pqos_api.refresh_mba_bw_status()

        assert self.Pqos_api.shared_dict['mba_bw_supported'] == mba_ctrl_supp
        assert self.Pqos_api.is_mba_bw_supported() == mba_ctrl_supp

        assert self.Pqos_api.shared_dict['mba_bw_enabled'] == mba_ctrl_en
        assert self.Pqos_api.is_mba_bw_enabled() == mba_ctrl_en


    @mock.patch("pqos_api.PqosApi.refresh_mba_bw_status", mock.MagicMock(return_value=0))
    @pytest.mark.parametrize("enabled_mba_bw", [True, False])
    def test_enable_mba_bw(self, enabled_mba_bw):

        # All OK!
        assert 0 == self.Pqos_api.enable_mba_bw(enabled_mba_bw)
        self.Pqos_api.alloc.reset.assert_called_once_with("any", "any", "ctrl" if enabled_mba_bw else "default")

        # Alloc Reset fails
        self.Pqos_api.alloc.reset.side_effect = Exception("test")
        assert -1 == self.Pqos_api.enable_mba_bw(enabled_mba_bw)


    def test_is_l2_cat_supported(self):
        self.Pqos_api.cap.get_type.side_effect = PqosErrorResource('error', 3)
        assert False == self.Pqos_api.is_l2_cat_supported()
        self.Pqos_api.cap.get_type.assert_called_once_with('l2ca')

        self.Pqos_api.cap.get_type.reset_mock()
        self.Pqos_api.cap.get_type.side_effect = None
        self.Pqos_api.cap.get_type.return_value = 0
        assert True == self.Pqos_api.is_l2_cat_supported()
        self.Pqos_api.cap.get_type.assert_called_once_with('l2ca')

        self.Pqos_api.cap.get_type.reset_mock()
        self.Pqos_api.cap.get_type.side_effect = Exception('Test')
        assert False == self.Pqos_api.is_l2_cat_supported()
        self.Pqos_api.cap.get_type.assert_called_once_with('l2ca')


    def test_get_l2ca_num_cos(self):
        self.Pqos_api.cap.get_l2ca_cos_num.return_value = 28
        assert 28 == self.Pqos_api.get_l2ca_num_cos()
        self.Pqos_api.cap.get_l2ca_cos_num.assert_called_once()

        self.Pqos_api.cap.get_l2ca_cos_num.side_effect = Exception('Test')
        assert None == self.Pqos_api.get_l2ca_num_cos()

    def test_get_l3_cache_size(self):
        l3ca_caps = PqosCapabilityL3Ca()
        l3ca_caps.num_ways = 10
        l3ca_caps.way_size = 1024 * 1024
        self.Pqos_api.cap.get_type.return_value = l3ca_caps

        cache_size = self.Pqos_api.get_l3_cache_size()

        self.Pqos_api.cap.get_type.assert_called_once_with('l3ca')
        assert cache_size == 10 * 1024 * 1024
    
    def test_get_l3_cache_way_size(self):
        l3ca_caps = PqosCapabilityL3Ca()
        l3ca_caps.way_size = 8 * 1024 * 1024
        self.Pqos_api.cap.get_type.return_value = l3ca_caps

        cache_way_size = self.Pqos_api.get_l3_cache_way_size()

        self.Pqos_api.cap.get_type.assert_called_once_with('l3ca')
        assert cache_way_size == 8 * 1024 * 1024

    def test_get_l3_num_cache_ways(self):
        l3ca_caps = PqosCapabilityL3Ca()
        l3ca_caps.num_ways = 30
        self.Pqos_api.cap.get_type.return_value = l3ca_caps

        num_cache_ways = self.Pqos_api.get_l3_num_cache_ways()

        self.Pqos_api.cap.get_type.assert_called_once_with('l3ca')
        assert num_cache_ways == 30

    @pytest.mark.parametrize("cdp", [True, False, None])
    def test_is_l3_cdp_supported(self, cdp):
        l3ca_caps = PqosCapabilityL3Ca()
        l3ca_caps.cdp = cdp
        self.Pqos_api.cap.get_type.return_value = l3ca_caps

        cdp_supported = self.Pqos_api.is_l3_cdp_supported()

        self.Pqos_api.cap.get_type.assert_called_once_with('l3ca')
        assert cdp_supported == cdp

    @pytest.mark.parametrize("cdp_on", [True, False])
    def test_is_l3_cdp_enabled(self, cdp_on):
        l3ca_caps = PqosCapabilityL3Ca()
        l3ca_caps.cdp_on = cdp_on
        self.Pqos_api.cap.get_type.return_value = l3ca_caps

        cdp_enabled = self.Pqos_api.is_l3_cdp_enabled()

        self.Pqos_api.cap.get_type.assert_called_once_with('l3ca')
        assert cdp_enabled == cdp_on

    def test_l2_cache_size(self):
        l2ca_caps = PqosCapabilityL2Ca()
        l2ca_caps.num_ways = 20
        l2ca_caps.way_size = 16 * 1024
        self.Pqos_api.cap.get_type.return_value = l2ca_caps
        self.Pqos_api.is_l2_cat_supported = mock.MagicMock(return_value=True)

        cache_size = self.Pqos_api.get_l2_cache_size()

        self.Pqos_api.cap.get_type.assert_called_once_with('l2ca')
        assert cache_size == 20 * 16 * 1024

        self.Pqos_api.cap.get_type.reset_mock()
        self.Pqos_api.is_l2_cat_supported = mock.MagicMock(return_value=False)

        cache_size = self.Pqos_api.get_l2_cache_size()
        assert cache_size is None

        self.Pqos_api.cap.get_type.assert_not_called()

    def test_get_l2_cache_way_size(self):
        l2ca_caps = PqosCapabilityL2Ca()
        l2ca_caps.way_size = 128 * 1024
        self.Pqos_api.cap.get_type.return_value = l2ca_caps
        self.Pqos_api.is_l2_cat_supported = mock.MagicMock(return_value=True)

        cache_way_size = self.Pqos_api.get_l2_cache_way_size()

        self.Pqos_api.cap.get_type.assert_called_once_with('l2ca')
        assert cache_way_size == 128 * 1024

        self.Pqos_api.cap.get_type.reset_mock()
        self.Pqos_api.is_l2_cat_supported = mock.MagicMock(return_value=False)

        cache_way_size = self.Pqos_api.get_l2_cache_way_size()
        assert cache_way_size is None

        self.Pqos_api.cap.get_type.assert_not_called()

    def test_get_l2_num_cache_ways(self):
        l2ca_caps = PqosCapabilityL2Ca()
        l2ca_caps.num_ways = 22
        self.Pqos_api.cap.get_type.return_value = l2ca_caps
        self.Pqos_api.is_l2_cat_supported = mock.MagicMock(return_value=True)

        num_cache_ways = self.Pqos_api.get_l2_num_cache_ways()

        self.Pqos_api.cap.get_type.assert_called_once_with('l2ca')
        assert num_cache_ways == 22

        self.Pqos_api.cap.get_type.reset_mock()
        self.Pqos_api.is_l2_cat_supported = mock.MagicMock(return_value=False)

        num_cache_ways = self.Pqos_api.get_l2_num_cache_ways()
        assert num_cache_ways is None

        self.Pqos_api.cap.get_type.assert_not_called()

    @pytest.mark.parametrize("cdp", [True, False, None])
    def test_is_l2_cdp_supported(self, cdp):
        l2ca_caps = PqosCapabilityL2Ca()
        l2ca_caps.cdp = cdp
        self.Pqos_api.cap.get_type.return_value = l2ca_caps
        self.Pqos_api.is_l2_cat_supported = mock.MagicMock(return_value=True)

        cdp_supported = self.Pqos_api.is_l2_cdp_supported()

        self.Pqos_api.cap.get_type.assert_called_once_with('l2ca')
        assert cdp_supported == cdp

        self.Pqos_api.cap.get_type.reset_mock()
        self.Pqos_api.is_l2_cat_supported = mock.MagicMock(return_value=False)

        cdp_supported = self.Pqos_api.is_l2_cdp_supported()
        assert cdp_supported is None

        self.Pqos_api.cap.get_type.assert_not_called()

    @pytest.mark.parametrize("cdp_on", [True, False])
    def test_is_l2_cdp_enabled(self, cdp_on):
        l2ca_caps = PqosCapabilityL2Ca()
        l2ca_caps.cdp_on = cdp_on
        self.Pqos_api.cap.get_type.return_value = l2ca_caps
        self.Pqos_api.is_l2_cat_supported = mock.MagicMock(return_value=True)

        cdp_enabled = self.Pqos_api.is_l2_cdp_enabled()

        self.Pqos_api.cap.get_type.assert_called_once_with('l2ca')
        assert cdp_enabled == cdp_on

        self.Pqos_api.cap.get_type.reset_mock()
        self.Pqos_api.is_l2_cat_supported = mock.MagicMock(return_value=False)

        cdp_enabled = self.Pqos_api.is_l2_cdp_enabled()
        assert cdp_enabled is None

        self.Pqos_api.cap.get_type.assert_not_called()
