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
The main module.
It defines Pqos class that initializes/finalizes PQoS library and allows
to call APIs from PQoS library.
"""

from __future__ import absolute_import, division, print_function
import ctypes
import sys

from pqos.common import pqos_handle_error


class CPqosConfig(ctypes.Structure):
    "pqos_config structure"
    # pylint: disable=too-few-public-methods

    PQOS_INTER_MSR = 0
    PQOS_INTER_OS = 1
    PQOS_INTER_OS_RESCTRL_MON = 2

    LOG_VER_SILENT = -1
    LOG_VER_DEFAULT = 0
    LOG_VER_VERBOSE = 1
    LOG_VER_SUPER_VERBOSE = 2

    LOG_CALLBACK = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_size_t,
                                    ctypes.c_char_p)

    _fields_ = [
        (u"fd_log", ctypes.c_int),
        (u"callback_log", LOG_CALLBACK),
        (u"context_log", ctypes.c_void_p),
        (u"verbose", ctypes.c_int),
        (u"interface", ctypes.c_int),
        (u"reserved", ctypes.c_int),
    ]


class Pqos(object):
    """
    The main class that is responsible for PQoS library initialization
    and finalization. It implements singleton pattern.
    """

    LOG_VER_SILENT = -1
    LOG_VER_DEFAULT = 0
    LOG_VER_VERBOSE = 1
    LOG_VER_SUPER_VERBOSE = 2

    _instance = None

    @classmethod
    def set_instance(cls, instance):
        "Sets an instance of this class."

        cls._instance = instance

    @classmethod
    def get_instance(cls):
        "Gets an instance of this class."

        return cls._instance

    def __new__(cls):
        """
        Returns an object of this class if already created
        or constructs a new one.
        """

        instance = cls.get_instance()

        if instance is None:
            instance = object.__new__(cls)
            cls.set_instance(instance)

        return cls.get_instance()

    def __init__(self):
        "Finds PQoS library and constructs a new object."

        self.lib = ctypes.cdll.LoadLibrary(u'libpqos.so.4')

    def init(self, interface, log_file=None, log_callback=None,
             log_context=None, verbose=u'default'):
        """Initializes PQoS library.

        Parameters:
            interface: an interface to be used by PQoS library, Available
                       options: MSR, OS, OS_RESCTRL_MON
            log_file: a file object where logs will be written to or None,
                      if None is given, then sys.stdout is used (default None)
            log_callback: a callback invoked for each log message (default None)
            log_context: an additional information given to a log callback
                         (default None)
            verbose: log verbosity level, available options: silent,
                     default (or None), verbose and super (default 'default')
        """

        if interface.upper() == u'MSR':
            cfg_interface = CPqosConfig.PQOS_INTER_MSR
        elif interface.upper() == u'OS':
            cfg_interface = CPqosConfig.PQOS_INTER_OS
        elif interface.upper() == u'OS_RESCTRL_MON':
            cfg_interface = CPqosConfig.PQOS_INTER_OS_RESCTRL_MON
        else:
            raise ValueError(u'Unknown interface selected: %s.'
                             u' Available options: MSR, OS,'
                             u' OS_RESCTRL_MON' % interface)

        if not log_file and not log_callback:
            log_file = sys.stdout

        if log_file:
            cfg_fd_log = log_file.fileno()
        else:
            cfg_fd_log = None

        if log_callback:
            def pqos_log_callback_wrapper(callback):
                """
                Wraps Python's callback into PQoS log callback-compatible
                function.
                """
                def cpqos_log_callback(context, _size, message):
                    "Calls Python's log callback."
                    return callback(message, context)

                return cpqos_log_callback

            wrapped_callback = pqos_log_callback_wrapper(log_callback)
            cfg_callback_log = CPqosConfig.LOG_CALLBACK(wrapped_callback)
        else:
            cfg_callback_log = CPqosConfig.LOG_CALLBACK(0)

        if verbose is None or verbose.lower() == u'default':
            cfg_verbose = CPqosConfig.LOG_VER_DEFAULT
        elif verbose.lower() == u'silent':
            cfg_verbose = CPqosConfig.LOG_VER_SILENT
        elif verbose.lower() == u'verbose':
            cfg_verbose = CPqosConfig.LOG_VER_VERBOSE
        elif verbose.lower() == u'super':
            cfg_verbose = CPqosConfig.LOG_VER_SUPER_VERBOSE
        else:
            raise ValueError(u'Unknown verbosity level selected: %s.'
                             u' Available options: silent, default (or None),'
                             u' verbose, super' % interface)

        config = CPqosConfig(interface=cfg_interface, fd_log=cfg_fd_log,
                             callback_log=cfg_callback_log,
                             verbose=cfg_verbose, context_log=log_context,
                             reserved=0)

        ret = self.lib.pqos_init(ctypes.byref(config))
        pqos_handle_error(u'pqos_init', ret)

    def fini(self):
        "Finalizes PQoS library."

        ret = self.lib.pqos_fini()
        pqos_handle_error(u'pqos_fini', ret)
