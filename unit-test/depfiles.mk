###############################################################################
# BSD LICENSE
#
# Copyright(c) 2026 Intel Corporation. All rights reserved.
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
###############################################################################

# Reads the dependencies the compiler recorded, which is what makes generating
# them worth anything: a .d file that nothing includes lists the headers an
# object was compiled from and is then ignored, so an edit to a header outside
# the directory rebuilt nothing at all. The wildcards in each Makefile cover the
# headers beside it; these cover the ones it reaches through -I.
#
# Not on every invocation, though. An unconditional include makes "make clean"
# bring every missing or outdated .d file up to date first - the compiler run
# across every source, to delete the result - and on a tree where the compiler
# cannot run it fails before cleaning anything. The same is true of the style
# goals, which read the sources with tools of their own.
#
# So the include happens unless every goal is one of those: "make clean all"
# still reads them, because that invocation does build, and a clean tree compiles
# everything in any case.
#
# A Makefile including this sets DEPFILES_TO_READ, and includes it after its
# default goal so that the goal is still the first target make saw.

DEPFILES_SKIP_GOALS = clean style checkpatch clang-format codespell
DEPFILES_GOALS = $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)

ifneq ($(filter-out $(DEPFILES_SKIP_GOALS),$(DEPFILES_GOALS)),)
-include $(DEPFILES_TO_READ)
endif
