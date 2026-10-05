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

# A file recording the flags the last build here used, for the objects and
# binaries to depend on.
#
# Naming the Makefile as a prerequisite covers the flags written *in* the
# Makefile. It cannot cover the ones that arrive on the command line - DEBUG in
# unit-test/lib and unit-test/pqos, SHARED and EXTRA_CFLAGS in unit-test/mock and
# unit-test/output - because nothing about the file changes when the command line
# does. So `make DEBUG=y` in a tree that has already been built found everything
# up to date and produced no debug build at all: ASSERT stayed compiled out, and
# a case that only reaches its subject under ASSERT passed without testing it,
# while telling the reader in a comment to build this way.
#
# The flags themselves are the dependency, so this records them and is rewritten
# only when they differ from what it holds. A stamp rewritten every run would
# rebuild everything every run.
#
# Written with $(file ...) rather than a shell redirection because these flag
# sets contain both kinds of quote - -D'PQOS_LOCAL=' beside
# -DLOCKFILE=\"/tmp/...\" - and there is no way to pass that through a shell
# unharmed. $(file ...) needs no shell.
#
# A Makefile including this sets FLAGS_STAMP_DIR to the directory the stamp
# belongs in, and includes it *after* the last line that adds to the flags.

ifndef FLAGS_STAMP_DIR
$(error FLAGS_STAMP_DIR must be set before including flags-stamp.mk)
endif

FLAGS_STAMP = $(FLAGS_STAMP_DIR)/.build-flags

# every variable that decides what is produced here, in one line. AR as well as
# CC: with SHARED=n the mock and the capture library are archives, and which ar
# builds them is as much a part of what came out as which compiler did.
FLAGS_STAMP_TEXT = $(strip $(CC) $(AR) $(CFLAGS) $(LDFLAGS) $(WRAP))

ifneq ($(FLAGS_STAMP_TEXT),$(strip $(shell cat $(FLAGS_STAMP) 2>/dev/null)))
$(shell mkdir -p $(FLAGS_STAMP_DIR))
$(file >$(FLAGS_STAMP),$(FLAGS_STAMP_TEXT))
endif
