################################################################################
# BSD LICENSE
#
# Copyright(c) 2019 Intel Corporation. All rights reserved.
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

import json
from config import *

def create_sample_config():

    apps = []
    pools = []

    # IPSEC VM
    ipsec_vm = {}
    ipsec_vm['id'] = 2
    ipsec_vm['name'] = "IPSec VM"
    ipsec_vm['pids'] = [3456]
    ipsec_vm['cores'] = [3, 7]

    # nDPI VM
    ndpi_vm = {}
    ndpi_vm['id'] = 3
    ndpi_vm['name'] = "nDPI VM"
    ndpi_vm['cores'] = [4, 8]
    ndpi_vm['pids'] = [1234,213]

    # OS VM
    os_vm = {}
    os_vm['id'] = 4
    os_vm['name'] = "OS VM"
    os_vm['pids'] = [1896,1897]
    os_vm['cores'] = [1,6]

    apps.append(ipsec_vm)
    apps.append(ndpi_vm)
    apps.append(os_vm)

    # Production pool
    p_pool = {}
    p_pool['id'] = 2
    p_pool['min_cws'] = 5
    p_pool['name'] = 'Production'
    p_pool['priority'] = "prod"
    p_pool['apps'] = [3]

    # PreProduction pool
    pp_pool = {}
    pp_pool['id'] = 3
    pp_pool['min_cws'] = 1
    pp_pool['name'] = 'PreProduction'
    pp_pool['priority'] = 'preprod'
    pp_pool['apps'] = [4]

    # Best Effort pool
    be_pool = {}
    be_pool['id'] = 4
    be_pool['min_cws'] = 1
    be_pool['name'] = 'Best Effort'
    be_pool['priority'] = 'besteff'
    be_pool['apps'] = [2]

    pools.append(p_pool)
    pools.append(pp_pool)
    pools.append(be_pool)

    config_store = ConfigStore()
    cs_cfg = config_store.get_config()

    cs_cfg['pools'] = pools
    cs_cfg['apps'] = apps

    # api authentication of user
    cs_cfg['auth'] = {'username':'superadmin', 'password':'secretsecret'}

    f = open('gen_config.conf', 'w')
    f.write(str(cs_cfg))
    f.close
    return cs_cfg


def main():
    # my code here
    config_store = create_sample_config()

if __name__ == "__main__":
    main()
