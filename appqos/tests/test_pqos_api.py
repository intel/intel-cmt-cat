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

import common

import pqos_api

class TestPqosApi(object):

    ## @cond
    @pytest.fixture(autouse=True)
    def init(self):
        self.Pqos_api = pqos_api.PqosApi()

        self.Pqos_api.cap = mock.MagicMock()
        self.Pqos_api.cap.get_l3ca_cos_num = mock.MagicMock()
        self.Pqos_api.cap.get_mba_cos_num = mock.MagicMock()
        self.Pqos_api.cap.get_type = mock.MagicMock()

        self.Pqos_api.alloc = mock.MagicMock()
        self.Pqos_api.alloc.release = mock.MagicMock()
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
        self.Pqos_api.mba.COS.assert_called_once_with(1, 44)
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


    def test_init(self):
        with mock.patch('pqos.Pqos.init') as pqos_init_mock,\
             mock.patch('pqos.capability.PqosCap.__init__', return_value = None) as pqos_cap_init_mock,\
             mock.patch('pqos.l3ca.PqosCatL3.__init__', return_value = None) as pqos_cat_l3_init_mock,\
             mock.patch('pqos.mba.PqosMba.__init__', return_value = None) as pqos_mba_init_mock,\
             mock.patch('pqos.allocation.PqosAlloc.__init__', return_value = None) as pqos_alloc_init_mock,\
             mock.patch('pqos.cpuinfo.PqosCpuInfo.__init__', return_value = None) as pqos_cpu_info_init_mock:

            assert 0 == self.Pqos_api.init()

            pqos_init_mock.assert_called_once_with('MSR')
            pqos_cap_init_mock.assert_called_once()
            pqos_cat_l3_init_mock.assert_called_once()
            pqos_mba_init_mock.assert_called_once()
            pqos_alloc_init_mock.assert_called_once()
            pqos_cpu_info_init_mock.assert_called_once()

            pqos_cpu_info_init_mock.side_effect = Exception('Test')
            assert -1 == self.Pqos_api.init()


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

       assert 7 == self.Pqos_api.get_max_cos_id([common.CAT_CAP, common.MBA_CAP])
       assert 7 == self.Pqos_api.get_max_cos_id([common.MBA_CAP])
       assert 15 == self.Pqos_api.get_max_cos_id([common.CAT_CAP])
       assert None == self.Pqos_api.get_max_cos_id([])

       self.Pqos_api.cap.get_l3ca_cos_num.return_value = 0
       assert None == self.Pqos_api.get_max_cos_id([common.CAT_CAP, common.MBA_CAP])
       assert 7 == self.Pqos_api.get_max_cos_id([common.MBA_CAP])
       assert None == self.Pqos_api.get_max_cos_id([common.CAT_CAP])
       assert None == self.Pqos_api.get_max_cos_id([])

       self.Pqos_api.cap.get_mba_cos_num.return_value = 0
       assert None == self.Pqos_api.get_max_cos_id([common.CAT_CAP, common.MBA_CAP])
       assert None == self.Pqos_api.get_max_cos_id([common.MBA_CAP])
       assert None == self.Pqos_api.get_max_cos_id([common.CAT_CAP])
       assert None == self.Pqos_api.get_max_cos_id([])
