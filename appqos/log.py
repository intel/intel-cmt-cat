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
Logging module
"""

import syslog
import logging

LOGGER = logging.getLogger('')
LOGGER.setLevel(logging.INFO)

#log format
FORMATTER = logging.Formatter("%(asctime)s %(levelname)-5s %(message)s")
# enable logging to console
CONSOLE_HANDLER = logging.StreamHandler()
CONSOLE_HANDLER.setFormatter(FORMATTER)
LOGGER.addHandler(CONSOLE_HANDLER)


def sys(string, syslogprio=syslog.LOG_NOTICE):
    """
    Logs to syslog

    Parameters:
        string: message to be logged
        syslogprio: priority of logged message
    """
    syslog.syslog(syslogprio, string)


def debug(string):
    """
    Logs to console, DEBUG level

    Parameters:
        string: message to be logged
    """
    LOGGER.debug(string)


def info(string):
    """
    Logs to console, INFO level

    Parameters:
        string: message to be logged
    """
    LOGGER.info(string)


def error(string):
    """
    Logs to console, ERROR level

    Parameters:
        string: message to be logged
    """
    LOGGER.error(string)


def warn(string):
    """
    Logs to console, WARN level

    Parameters:
        string: message to be logged
    """
    LOGGER.warning(string)


def enable_verbose():
    """
    Enables verbose mode
    """
    LOGGER.setLevel(logging.DEBUG)
