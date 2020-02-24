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
import re
import subprocess
import pexpect

RESCTRL_ROOT_PATH = "/sys/fs/resctrl"


class ResctrlSchemata:
    def __init__(self, path):
        self.path = path
        self.data = {}

        self.parse()

    def parse(self):
        # Open file and fill each line into a list
        with open(self.path, 'r') as schema_file:
            lines = schema_file.readlines()

        for line in lines:
            match = re.search(r"^\s*([A-Z23]*):(.*)$", line)
            if not match:
                continue

            label = match.group(1)
            data = match.group(2).split(";")

            self.data[label] = []

            for mask in data:
                match = re.search(r"^([0-9]*)=\s*([0-9a-f]*)$", mask)
                if not match:
                    continue

                if label == "MB":
                    value = int(match.group(2), 10)
                else:
                    value = int(match.group(2), 16)

                self.data[label].append(value)

    def get(self, label, resource_id):
        return self.data[label][resource_id]


class Resctrl:
    def __init__(self):
        self.root = RESCTRL_ROOT_PATH

    def run_cmd(self, command):
        child = subprocess.Popen(command.split(), stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        (stdout, stderr) = child.communicate()

        if stdout is not None:
            stdout = stdout.decode("utf-8")
        if stderr is not None:
            stderr = stderr.decode("utf-8")

        return stdout, stderr, child.returncode

    def is_mounted(self):
        command = "mount -t resctrl"
        output, _, _ = self.run_cmd(command)
        return "resctrl" in output and self.root in output and "rw" in output

    def mount(self, cdp=False, cdpl2=False, mba_mbps=False):
        options_str = ''
        options = []

        if cdp:
            options.append('cdp')
        if cdpl2:
            options.append('cdpl2')
        if mba_mbps:
            options.append('mba_MBps')

        if options:
            options_str = ' -o ' + ','.join(options)

        command = "sudo mount -t resctrl resctrl%s %s" % (options_str, self.root)
        self.run_cmd(command)
        return self.is_mounted()

    def umount(self):
        command = "sudo umount -a -t resctrl"
        self.run_cmd(command)
        return not self.is_mounted()

    def info_dir_exists(self, dir_name):
        dir_path = os.path.join(self.root, "info", dir_name)
        return os.path.exists(dir_path)

    def get_mon_features(self):
        mon_feat_path = os.path.join(self.root, "info", "L3_MON", "mon_features")
        if not os.path.exists(mon_feat_path):
            return []

        with open(mon_feat_path, 'r') as feat_file:
            lines = feat_file.readlines()

        return [line.strip() for line in lines]

    def get_schemata_path(self, cos=0):
        if cos == 0:
            return os.path.join(self.root, "schemata")

        return os.path.join(self.root, "COS%s" % str(cos), "schemata")

    def get_schemata(self, cos=0):
        path = self.get_schemata_path(cos)
        return ResctrlSchemata(path)


    @staticmethod
    def get_ctrl_group_count():
        ctrl_group_count = 0

        command = 'bash -c "ls %s"' % RESCTRL_ROOT_PATH
        output, exit_code = pexpect.run(command, withexitstatus=True)

        if exit_code == 0:
            ctrl_group_count = 1

            command = 'bash -c "ls %s | grep COS | wc -l"' % RESCTRL_ROOT_PATH
            output = pexpect.run(command)
            ctrl_group_count += int(output)

        return ctrl_group_count
