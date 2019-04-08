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

from pqos import Pqos
from pqos.capability import PqosCap


# Initialize PQoS library
pqos = Pqos()
pqos.init('OS')

cap = PqosCap()

# Check if MBA is supported
mba_supported = False
try:
    cap.get_type('mba')
    mba_supported = True
except:
    pass

print('Is MBA supported? %s' % ('Yes' if mba_supported else 'No'))

if mba_supported:
    # Get number of MBA classes of service
    mba_cos_num = cap.get_mba_cos_num()
    print('MBA: available %d classes of service' % mba_cos_num)

# Check if L3 CAT is supported
l3ca_cap = None
try:
    l3ca_cap = cap.get_type('l3ca')
except:
    pass

print('Is L3 CAT supported? %s' % ('Yes' if l3ca_cap else 'No'))

if l3ca_cap:
    # Read L3 CAT capabilities
    cdp_support = 'yes' if l3ca_cap.cdp else 'no'
    print('L3 CAT: CDP support - %s' % cdp_support)
    print('L3 CAT: number of cache ways - %d' % l3ca_cap.num_ways)

# Finalize PQoS library
pqos.fini()
