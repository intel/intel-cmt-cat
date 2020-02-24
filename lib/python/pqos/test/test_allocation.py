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

"""
Unit tests for allocation module.
"""

from __future__ import absolute_import, division, print_function
import ctypes
import unittest

from unittest.mock import MagicMock, patch

from pqos.test.mock_pqos import mock_pqos_lib
from pqos.test.helper import ctypes_ref_set_uint, ctypes_build_array

from pqos.allocation import PqosAlloc
from pqos.capability import CPqosCapability


class TestPqosAlloc(unittest.TestCase):
    "Tests for PqosAlloc class."

    @mock_pqos_lib
    def test_assoc_set(self, lib):
        "Tests assoc_set() method."
        # pylint: disable=no-self-use

        lib.pqos_alloc_assoc_set = MagicMock(return_value=0)

        alloc = PqosAlloc()
        alloc.assoc_set(3, 7)

        lib.pqos_alloc_assoc_set.assert_called_once_with(3, 7)

    @mock_pqos_lib
    def test_assoc_get(self, lib):
        "Tests assoc_get() method."

        def pqos_allc_assoc_get_m(core, class_id_ref):
            "Mock pqos_alloc_assoc_get()."

            self.assertEqual(core, 2)
            ctypes_ref_set_uint(class_id_ref, 5)
            return 0

        lib.pqos_alloc_assoc_get = MagicMock(side_effect=pqos_allc_assoc_get_m)

        alloc = PqosAlloc()
        class_id = alloc.assoc_get(2)

        lib.pqos_alloc_assoc_get.assert_called_once()
        self.assertEqual(class_id, 5)

    @mock_pqos_lib
    def test_assoc_set_pid(self, lib):
        "Tests assoc_set_pid() method."
        # pylint: disable=no-self-use

        lib.pqos_alloc_assoc_set_pid = MagicMock(return_value=0)

        alloc = PqosAlloc()
        alloc.assoc_set_pid(2, 1)

        lib.pqos_alloc_assoc_set_pid.assert_called_once_with(2, 1)

    @mock_pqos_lib
    def test_assoc_get_pid(self, lib):
        "Tests assoc_get_pid() method."

        def pqos_allc_assoc_get_pid_m(pid, class_id_ref):
            "Mock pqos_alloc_assoc_get_pid()."

            self.assertEqual(pid, 200)
            ctypes_ref_set_uint(class_id_ref, 1)
            return 0

        func_mock = MagicMock(side_effect=pqos_allc_assoc_get_pid_m)
        lib.pqos_alloc_assoc_get_pid = func_mock

        alloc = PqosAlloc()
        class_id = alloc.assoc_get_pid(200)

        lib.pqos_alloc_assoc_get_pid.assert_called_once()
        self.assertEqual(class_id, 1)

    @mock_pqos_lib
    def test_assign(self, lib):
        "Tests assign() method."

        def pqos_alloc_assign_m(mask, core_array, core_array_len, class_id_ref):
            "Mock pqos_alloc_assign()."

            mba_mask = 1 << CPqosCapability.PQOS_CAP_TYPE_MBA
            l3ca_mask = 1 << CPqosCapability.PQOS_CAP_TYPE_L3CA
            expected_mask = mba_mask | l3ca_mask
            self.assertEqual(mask, expected_mask)
            self.assertEqual(core_array_len, 4)
            self.assertEqual(core_array[0], 1)
            self.assertEqual(core_array[1], 2)
            self.assertEqual(core_array[2], 4)
            self.assertEqual(core_array[3], 7)

            ctypes_ref_set_uint(class_id_ref, 3)
            return 0

        func_mock = MagicMock(side_effect=pqos_alloc_assign_m)
        lib.pqos_alloc_assign = func_mock

        alloc = PqosAlloc()
        class_id = alloc.assign(['mba', 'l3ca'], [1, 2, 4, 7])

        lib.pqos_alloc_assign.assert_called_once()
        self.assertEqual(class_id, 3)

    @mock_pqos_lib
    def test_release(self, lib):
        "Tests release() method."

        def pqos_alloc_release_m(core_array, core_array_len):
            "Mock pqos_alloc_release()."

            self.assertEqual(core_array_len, 3)
            self.assertEqual(core_array[0], 2)
            self.assertEqual(core_array[1], 3)
            self.assertEqual(core_array[2], 5)

            return 0

        func_mock = MagicMock(side_effect=pqos_alloc_release_m)
        lib.pqos_alloc_release = func_mock

        alloc = PqosAlloc()
        alloc.release([2, 3, 5])

        lib.pqos_alloc_release.assert_called_once()

    @mock_pqos_lib
    def test_assign_pid(self, lib):
        "Tests assign_pid() method."

        def pqos_alloc_assign_pid_m(mask, pid_array, pid_array_len,
                                    class_id_ref):
            "Mock pqos_alloc_assign_pid()."

            l2ca_mask = 1 << CPqosCapability.PQOS_CAP_TYPE_L2CA
            l3ca_mask = 1 << CPqosCapability.PQOS_CAP_TYPE_L3CA
            expected_mask = l2ca_mask | l3ca_mask
            self.assertEqual(mask, expected_mask)
            self.assertEqual(pid_array_len, 4)
            self.assertEqual(pid_array[0], 1000)
            self.assertEqual(pid_array[1], 1200)
            self.assertEqual(pid_array[2], 2300)
            self.assertEqual(pid_array[3], 5000)

            ctypes_ref_set_uint(class_id_ref, 3)
            return 0

        func_mock = MagicMock(side_effect=pqos_alloc_assign_pid_m)
        lib.pqos_alloc_assign_pid = func_mock

        alloc = PqosAlloc()
        class_id = alloc.assign_pid(['l2ca', 'l3ca'], [1000, 1200, 2300, 5000])

        lib.pqos_alloc_assign_pid.assert_called_once()
        self.assertEqual(class_id, 3)

    @mock_pqos_lib
    def test_release_pid(self, lib):
        "Tests release_pid() method."

        def pqos_alloc_release_pid_m(pid_array, pid_array_len):
            "Mock pqos_alloc_release_pid()."

            self.assertEqual(pid_array_len, 4)
            self.assertEqual(pid_array[0], 1234)
            self.assertEqual(pid_array[1], 5432)
            self.assertEqual(pid_array[2], 7568)
            self.assertEqual(pid_array[3], 4545)

            return 0

        func_mock = MagicMock(side_effect=pqos_alloc_release_pid_m)
        lib.pqos_alloc_release_pid = func_mock

        alloc = PqosAlloc()
        alloc.release_pid([1234, 5432, 7568, 4545])

        lib.pqos_alloc_release_pid.assert_called_once()

    @mock_pqos_lib
    def test_get_pids(self, lib):
        "Tests get_pids() method."

        pids_uint = [ctypes.c_uint(pid) for pid in [1000, 1500, 3000, 5600]]
        pid_array = ctypes_build_array(pids_uint)

        def pqos_pid_get_pid_assoc_m(class_id, count_ref):
            "Mock pqos_pid_get_pid_assoc()."

            self.assertEqual(class_id, 7)

            ctypes_ref_set_uint(count_ref, len(pid_array))
            return ctypes.cast(pid_array, ctypes.POINTER(ctypes.c_uint))

        func_mock = MagicMock(side_effect=pqos_pid_get_pid_assoc_m)
        lib.pqos_pid_get_pid_assoc = func_mock

        alloc = PqosAlloc()

        with patch('pqos.allocation.free_memory'):
            pids = alloc.get_pids(7)

        lib.pqos_pid_get_pid_assoc.assert_called_once()

        self.assertEqual(len(pids), 4)
        self.assertEqual(pids[0], 1000)
        self.assertEqual(pids[1], 1500)
        self.assertEqual(pids[2], 3000)
        self.assertEqual(pids[3], 5600)

    @mock_pqos_lib
    def test_reset(self, lib):
        "Tests reset() method."

        def pqos_alloc_reset_m(l3_cdp_cfg, l2_cdp_cfg, mba_cfg):
            "Mock pqos_alloc_reset()."

            self.assertEqual(l3_cdp_cfg, 1)
            self.assertEqual(l2_cdp_cfg, 2)
            self.assertEqual(mba_cfg, 2)

            return 0

        func_mock = MagicMock(side_effect=pqos_alloc_reset_m)
        lib.pqos_alloc_reset = func_mock

        alloc = PqosAlloc()
        alloc.reset('on', 'any', 'ctrl')

        lib.pqos_alloc_reset.assert_called_once()
