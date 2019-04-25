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
from pqos.mba import PqosMba


# Initialize PQoS library
pqos = Pqos()
pqos.init('OS')

# Check if MBA is supported
cap = PqosCap()
mba_supported = False
try:
    cap.get_type('mba')
    mba_supported = True
except:
    pass

print('Is MBA supported? %s' % ('Yes' if mba_supported else 'No'))

if mba_supported:
    mba = PqosMba()

    # Get MBA configuration for socket 0
    coses = mba.get(0)

    print('MBA configuration for socket 0:')
    for cos in coses:
        print('COS: %d' % cos.class_id)
        print('MB max: %d' % cos.mb_max)
        print('CTRL: %s' % cos.ctrl)
        print('')

    # Configure MBA, 50% of available bandwidth for COS 0
    cos = mba.COS(0, 50)
    # Apply for socket 0
    actual = mba.set(0, [cos])

    print('Trying to set %d%%...' % cos.mb_max)
    print('Actually set: %d%%' % actual[0].mb_max)

# Finalize PQoS library
pqos.fini()
