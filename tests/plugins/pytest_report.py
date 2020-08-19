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

import re
import warnings
import pytest

def pytest_addoption(parser):
    parser.addoption("--report", action="store_true", dest='report', help='Print Test Report')


def pytest_configure(config):
    if config.getoption("--report"):
        config.report = TestReport(config)
        config.pluginmanager.register(config.report)


def pytest_unconfigure(config):
    report = getattr(config, 'report', None)
    if report:
        del config.report
        config.pluginmanager.unregister(report)


class Result:
    def __init__(self, nodeid, outcome, priority):
        self.nodeid = nodeid
        self.result = self.__get_result(outcome)
        self.priority = priority


    def update(self, outcome):
        result = self.__get_result(outcome)

        if self.result != result:
            if result == "FAIL":
                self.result = "FAIL"

            elif result == "PASS" and self.result != "FAIL":
                self.result = "PASS"


    @staticmethod
    def __get_result(outcome):
        if outcome in ['passed', 'xpassed']:
            return 'PASS'
        if outcome == 'failed':
            return 'FAIL'
        if outcome == 'skipped':
            return 'SKIP'
        if outcome == 'xfailed':
            return 'XFAIL'
        return ''


class TestReport:
    def __init__(self, config):
        self.results = {}
        self.config = config

        # priority os processed test
        self.current_priority = None


    def report(self, terminalreporter):
        terminalreporter.write_sep('=', 'Test Report')
        terminalreporter.write_line('%-45s | Priority | Result    | Comment/Defect ID' %
                                    ('Test Case Identifier'))
        terminalreporter.write_sep('=')

        for test in sorted(self.results):
            if self.results[test].result == 'PASS':
                markup = {'green': True}
            elif self.results[test].result == 'FAIL' or self.results[test].result == 'XFAIL':
                markup = {'red': True}
            else:
                markup = {'yellow': True}

            terminalreporter.write('%-45s | %-8s | ' \
                % (self.results[test].nodeid,
                   self.results[test].priority if self.results[test].priority else ""))
            terminalreporter.write('%-10s' % (self.results[test].result), **markup)
            terminalreporter.write('|')
            terminalreporter.write_line("")

        terminalreporter.write_sep('-')


    def pytest_runtest_setup(self, item):
        priority = item.get_closest_marker("priority")
        if priority:
            self.current_priority = priority.args[0]
        else:
            msg = "Priority missing for " + item.nodeid
            warnings.warn(pytest.PytestWarning(msg))
            self.current_priority = None


    def pytest_runtest_logreport(self, report):
        if report.when != "call":
            return

        # We are interested only on executed tests
        if report.outcome != "passed" and report.outcome != "failed":
            return

        nodeid = report.nodeid
        nodeid = nodeid.rsplit("::", 1)[1]
        nodeid = re.sub(r"\[.*\]$", "", nodeid)

        if nodeid not in self.results:
            self.results[nodeid] = Result(nodeid, report.outcome, self.current_priority)

        else:
            self.results[nodeid].update(report.outcome)


    def pytest_terminal_summary(self, terminalreporter):
        if self.config.getoption("--report"):
            self.report(terminalreporter)
