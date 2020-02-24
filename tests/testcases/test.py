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

import os
import logging
import subprocess
import psutil

## @cond
logging.basicConfig(level=logging.DEBUG)
## @endcond

class Test:
    log = logging.getLogger('rdt-test')
    membw = None
    rdtset = None
    pqos = None

    def init(self, request):
        self.membw = request.config.membw
        self.rdtset = request.config.rdtset
        self.pqos = request.config.pqos
        if os.path.isdir("/sys/fs/resctrl/info"):
            self.run(self.pqos + " -I -R l3cdp-off", True)
            self.run("umount /sys/fs/resctrl", True)
        self.run(self.pqos + " -R l3cdp-off", True)
        self.run(self.pqos + " -R l2cdp-off", True)
        self.run(self.pqos + " -r -t 0", True)


    def fini(self):
        self.run("killall -9 memtester", True)


    ## Runs command and adds output to log
    def run(self, command, quiet=False):
        child = subprocess.Popen(command.split(), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE)
        (stdout, stderr) = child.communicate()

        if stdout is not None:
            stdout = stdout.decode("utf-8")
        if stderr is not None:
            stderr = stderr.decode("utf-8")

        if not quiet:
            self.log.debug(command)
        if not quiet and stdout:
            self.log.debug("===> stdout")
            self.log.debug(stdout)
        if not quiet and stderr:
            self.log.debug("===> stderr")
            self.log.debug(stderr)

        return stdout, stderr, child.returncode


    @staticmethod
    def cmd(iface, app, params):
        command = app
        if iface == "OS":
            command = command + " -I"
        command = command + " " + params

        return command


    def cmd_pqos(self, iface, params):
        return self.cmd(iface, self.pqos, params)


    def cmd_rdtset(self, iface, params):
        return self.cmd(iface, self.rdtset, params)


    ## Runs pqos command
    def run_pqos(self, iface, params):
        command = self.cmd_pqos(iface, params)

        return self.run(command)


    ## Runs rdtset command
    def run_rdtset(self, iface, params):
        command = self.cmd_rdtset(iface, params)

        return self.run(command)


    @staticmethod
    def get_pid_children(pid):
        if int(pid) < 0:
            return []

        try:
            children = []
            process = psutil.Process(int(pid))
            for child in process.children(recursive=True):
                children.append(child.pid)
            return children
        except Exception:
            return []
