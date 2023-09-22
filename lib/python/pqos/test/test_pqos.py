################################################################################
# BSD LICENSE
#
# Copyright(c) 2019-2023 Intel Corporation. All rights reserved.
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
Unit tests for main (pqos) module.
"""

from __future__ import absolute_import, division, print_function
import ctypes
import unittest
from unittest.mock import MagicMock, patch

from pqos import Pqos, CPqosConfig
from pqos.native_struct import (
    CPqosSysconfig, CPqosCap, CPqosCpuInfo, CPqosDevinfo
)


class TestPqos(unittest.TestCase):
    "Tests for Pqos class."

    @patch('ctypes.cdll.LoadLibrary')
    def test_singleton(self, _load_lib):
        "Tests if the same object is constructed each time Pqos() is invoked."

        pqos = Pqos()
        pqos2 = Pqos()

        self.assertIs(pqos, pqos2)

    @patch('ctypes.cdll.LoadLibrary')
    def test_init(self, _load_lib):
        "Tests library initialization."
        # pylint: disable=no-self-use

        def pqos_init_mock(_cfg_ref):
            "Mock pqos_init()."

            return 0

        pqos = Pqos()

        pqos.lib.pqos_init = MagicMock(side_effect=pqos_init_mock)

        pqos.init('MSR')

        pqos.lib.pqos_init.assert_called_once()

    @patch('ctypes.cdll.LoadLibrary')
    def test_fini(self, _load_lib):
        "Tests library finalization."
        # pylint: disable=no-self-use

        def pqos_init_mock(_cfg_ref):
            "Mock pqos_init()."

            return 0

        def pqos_fini_mock():
            "Mock pqos_fini()."

            return 0

        pqos = Pqos()

        pqos.lib.pqos_init = MagicMock(side_effect=pqos_init_mock)
        pqos.lib.pqos_fini = MagicMock(side_effect=pqos_fini_mock)

        pqos.init('OS')
        pqos.fini()

        pqos.lib.pqos_init.assert_called_once()
        pqos.lib.pqos_fini.assert_called_once()

    def _test_init_verbose(self, verbose, expected_verbose):
        """
        Tests if verbosity level is correctly validated during library
        initialization.
        """

        def pqos_init_mock(cfg_ref):
            "Mock pqos_init()."
            p_cfg = ctypes.cast(cfg_ref, ctypes.POINTER(CPqosConfig))
            self.assertEqual(p_cfg.contents.verbose, expected_verbose)

            return 0

        pqos = Pqos()

        pqos.lib.pqos_init = MagicMock(side_effect=pqos_init_mock)

        pqos.init('OS_RESCTRL_MON', verbose=verbose)

        pqos.lib.pqos_init.assert_called_once()

    @patch('ctypes.cdll.LoadLibrary')
    def test_init_verbose_silent(self, _load_lib):
        "Tests if 'silent' verbosity level is properly handled."
        self._test_init_verbose('silent', CPqosConfig.LOG_VER_SILENT)

    @patch('ctypes.cdll.LoadLibrary')
    def test_init_verbose_default(self, _load_lib):
        "Tests if 'default' verbosity level is properly handled."
        self._test_init_verbose('default', CPqosConfig.LOG_VER_DEFAULT)

    @patch('ctypes.cdll.LoadLibrary')
    def test_init_verbose_none(self, _load_lib):
        "Tests if None as verbosity level is properly handled."
        self._test_init_verbose(None, CPqosConfig.LOG_VER_DEFAULT)

    @patch('ctypes.cdll.LoadLibrary')
    def test_init_verbose_verbose(self, _load_lib):
        "Tests if 'verbose' verbosity level is properly handled."
        self._test_init_verbose('verbose', CPqosConfig.LOG_VER_VERBOSE)

    @patch('ctypes.cdll.LoadLibrary')
    def test_init_verbose_super(self, _load_lib):
        "Tests if 'super' verbosity level is properly handled."
        self._test_init_verbose('super', CPqosConfig.LOG_VER_SUPER_VERBOSE)

    @patch('ctypes.cdll.LoadLibrary')
    def test_get_sysconfig(self, _load_lib):
        "Tests get_sysconfig() method."

        def pqos_init_mock(_cfg_ref):
            "Mock pqos_init()."

            return 0

        def pqos_fini_mock():
            "Mock pqos_fini()."

            return 0

        # Build stub system configuration
        cap = CPqosCap(mem_size=5, version=2, num_cap=0)
        cpu = CPqosCpuInfo(mem_size=7, vendor=3, num_cores=0)
        dev = CPqosDevinfo(num_channels=3, num_devs=1)
        cfg = CPqosSysconfig(cap=ctypes.pointer(cap), cpu=ctypes.pointer(cpu),
                             dev=ctypes.pointer(dev))

        def pqos_sysconfig_get_mock(cfg_ref):
            "Mock pqos_sysconfig_get()."

            cfg_ptr = ctypes.pointer(cfg)
            addr = ctypes.addressof(cfg_ptr)
            size = ctypes.sizeof(cfg_ptr)
            ctypes.memmove(cfg_ref, addr, size)

            return 0

        pqos = Pqos()

        # Setup mock functions
        pqos.lib.pqos_init = MagicMock(side_effect=pqos_init_mock)
        pqos.lib.pqos_sysconfig_get = MagicMock(side_effect=pqos_sysconfig_get_mock)
        pqos.lib.pqos_fini = MagicMock(side_effect=pqos_fini_mock)

        # Get system configuration
        pqos.init('OS')
        syscfg = pqos.get_sysconfig()
        pqos.fini()

        # Ensure mock function has been called
        pqos.lib.pqos_sysconfig_get.assert_called_once()

        # Verify capabilities
        self.assertEqual(syscfg.cap.contents.mem_size, 5)
        self.assertEqual(syscfg.cap.contents.version, 2)
        self.assertEqual(syscfg.cap.contents.num_cap, 0)

        # Verify cpuinfo
        self.assertEqual(syscfg.cpu.contents.mem_size, 7)
        self.assertEqual(syscfg.cpu.contents.vendor, 3)
        self.assertEqual(syscfg.cpu.contents.num_cores, 0)

        # Verify devinfo
        self.assertEqual(syscfg.dev.contents.num_channels, 3)
        self.assertEqual(syscfg.dev.contents.num_devs, 1)
