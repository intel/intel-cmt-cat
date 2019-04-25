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
from pqos.l3ca import PqosCatL3


# Initialize PQoS library
pqos = Pqos()
pqos.init('MSR')

# Check if L3 CAT is supported
cap = PqosCap()
l3ca_supported = False
try:
    cap.get_type('l3ca')
    l3ca_supported = True
except:
    pass

print('Is L3 CAT supported? %s' % ('Yes' if l3ca_supported else 'No'))

if l3ca_supported:
    l3ca = PqosCatL3()

    # Read L3 CAT configuration for socket 0
    coses = l3ca.get(0)
    for cos in coses:
        print(cos)

    # Configure L3 CAT
    # Build configuration for COS 0 and COS 1
    cos1 = l3ca.COS(0, 0x0007)
    cos2 = l3ca.COS(1, 0x00f8)

    # Set L3 CAT configuration for socket 0
    l3ca.set(0, [cos1, cos2])

# Finalize PQoS library
pqos.fini()
