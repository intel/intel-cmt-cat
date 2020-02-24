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
Unit tests for MBA module.
"""

from __future__ import absolute_import, division, print_function
import unittest

from unittest.mock import MagicMock

from pqos.test.mock_pqos import mock_pqos_lib
from pqos.test.helper import ctypes_ref_set_int

from pqos.mba import PqosMba


class TestPqosMba(unittest.TestCase):
    "Tests for PqosMba class."

    @mock_pqos_lib
    def test_set(self, lib):
        "Tests set() method."

        def pqos_mba_set_mock(socket, num_cos, cos_arr, actual_arr):
            "Mock pqos_mba_set()."

            self.assertEqual(socket, 0)
            self.assertEqual(num_cos, 1)
            self.assertEqual(len(cos_arr), num_cos)
            self.assertEqual(len(actual_arr), num_cos)
            self.assertEqual(cos_arr[0].class_id, 1)
            self.assertEqual(cos_arr[0].mb_max, 40)
            self.assertEqual(cos_arr[0].ctrl, 0)

            actual_arr[0].class_id = cos_arr[0].class_id
            actual_arr[0].mb_max = 50
            actual_arr[0].ctrl = cos_arr[0].ctrl

            return 0

        lib.pqos_mba_set = MagicMock(side_effect=pqos_mba_set_mock)

        mba = PqosMba()
        cos = mba.COS(1, 40)
        actual = mba.set(0, [cos])

        self.assertEqual(len(actual), 1)
        self.assertEqual(actual[0].class_id, 1)
        self.assertEqual(actual[0].mb_max, 50)
        self.assertEqual(actual[0].ctrl, 0)

        lib.pqos_mba_set.assert_called_once()

    @mock_pqos_lib
    def test_get(self, lib):
        "Tests get() method."

        def pqos_mba_cos_num_mock(_p_cap, num_cos_ref):
            "Mock pqos_mba_get_cos_num()."

            ctypes_ref_set_int(num_cos_ref, 2)
            return 0

        def pqos_mba_get_mock(socket, max_num_cos, num_cos_ref, cos_arr):
            "Mock pqos_mba_get()."

            self.assertEqual(socket, 1)
            self.assertEqual(max_num_cos, 2)
            self.assertEqual(len(cos_arr), max_num_cos)

            ctypes_ref_set_int(num_cos_ref, 2)

            cos_arr[0].class_id = 0
            cos_arr[0].mb_max = 4000
            cos_arr[0].ctrl = 1

            cos_arr[1].class_id = 1
            cos_arr[1].mb_max = 8000
            cos_arr[1].ctrl = 1

            return 0

        lib.pqos_cap_get = MagicMock(return_value=0)
        lib.pqos_mba_get_cos_num = MagicMock(side_effect=pqos_mba_cos_num_mock)
        lib.pqos_mba_get = MagicMock(side_effect=pqos_mba_get_mock)

        mba = PqosMba()
        coses = mba.get(1)

        self.assertEqual(len(coses), 2)

        self.assertEqual(coses[0].class_id, 0)
        self.assertEqual(coses[0].mb_max, 4000)
        self.assertTrue(coses[0].ctrl)

        self.assertEqual(coses[1].class_id, 1)
        self.assertEqual(coses[1].mb_max, 8000)
        self.assertTrue(coses[1].ctrl)

        lib.pqos_mba_get.assert_called_once()
        lib.pqos_mba_get_cos_num.assert_called_once()
