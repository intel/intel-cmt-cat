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
# The rule below cannot use it: a recipe's $(file ...) is evaluated when make
# expands the recipe, which is before the mkdir in it has run, so the write finds
# no directory. It hands the text to the shell through the environment instead -
# a target-specific export, so the recipe reads one variable and the quotes in it
# are never a shell's business either.
#
# A Makefile including this sets FLAGS_STAMP_DIR to the directory the stamp
# belongs in, and includes it *after* the last line that adds to the flags.

ifndef FLAGS_STAMP_DIR
$(error FLAGS_STAMP_DIR must be set before including flags-stamp.mk)
endif

FLAGS_STAMP = $(FLAGS_STAMP_DIR)/.build-flags

# every variable that decides what is produced here, in one line.
#
# AR as well as CC: with SHARED=n the mock and the capture library are archives,
# and which ar builds them is as much a part of what came out as which compiler
# did. CPPFLAGS because unit-test/mock and unit-test/output have no compilation
# recipe of their own and fall through to make's built-in one, which is
# "$(CC) $(CPPFLAGS) $(CFLAGS) -c" - so a macro defined there decides what is
# produced and nothing else here would notice it.
#
# One field per variable, named and bracketed, because a plain concatenation does
# not say where one ends and the next begins: CFLAGS="-DX=1 -pthread" with
# LDFLAGS="-lm" and CFLAGS="-DX=1" with LDFLAGS="-pthread -lm" are different
# builds and the same string. Moving a token from one variable to another now
# changes the text, which is what a stamp is for.
#
# The text is also compared as it stands and not through $(strip). strip collapses
# runs of whitespace wherever they are, including inside a quoted macro value, so
# -DX="a b" and -DX="a  b" recorded equal and the objects compiled with the first
# were kept for the second. The brackets make that safe at the ends too: whatever
# a variable leaves at either of its own ends is inside them.
FLAGS_STAMP_TEXT = CC=[$(CC)] AR=[$(AR)] CPPFLAGS=[$(CPPFLAGS)] \
CFLAGS=[$(CFLAGS)] LDFLAGS=[$(LDFLAGS)] WRAP=[$(WRAP)]

ifneq ($(FLAGS_STAMP_TEXT),$(shell cat $(FLAGS_STAMP) 2>/dev/null))
$(shell mkdir -p $(FLAGS_STAMP_DIR))
$(file >$(FLAGS_STAMP),$(FLAGS_STAMP_TEXT))
endif

# and a rule as well as the write above, for the invocation that removes it after
# it has been written: "make clean all" writes the stamp while the makefile is
# read, clean deletes it with the directory it is in, and all then wants a
# prerequisite that no longer exists - "No rule to make target
# 'obj/.build-flags'". The write above handles a change of flags; this handles the
# file not being there.
$(FLAGS_STAMP): export FLAGS_STAMP_TEXT := $(FLAGS_STAMP_TEXT)
$(FLAGS_STAMP):
	@mkdir -p $(@D)
	@printf '%s' "$$FLAGS_STAMP_TEXT" > $@

# "make clean all" asks for both in one invocation, and make does not serialize
# goals: under -j the clean deletes what the build is writing - obj/ while a link
# is reading it, or this stamp after it has been written - and which wins is
# timing, so an invocation has passed and failed on one tree without anything
# changing in it. Naming clean as a prerequisite of the other goal is not enough
# on its own: that orders it before that goal's recipe and not before its other
# prerequisites, which under -j start while the clean is still running. So the
# invocation is run serially as well, through a .NOTPARALLEL that only this
# condition reads. A plain build is parallel as before.
#
# Here rather than in each Makefile, because every one of them that records flags
# has both goals and the same race.
#
# Order-only - the "|" - and not an ordinary prerequisite: the library recipes in
# unit-test/mock and unit-test/output hand $^ to the compiler, so "clean" named
# the ordinary way arrives on the link line and ld goes looking for a file called
# clean. Order-only prerequisites are not in $^.
ifneq ($(filter clean,$(MAKECMDGOALS)),)
ifneq ($(filter-out clean,$(MAKECMDGOALS)),)
.NOTPARALLEL:
# and the edge only where clean was asked for first, which is the order the
# invocation wants: "make <library> clean" asks to build and then tidy up, and an
# edge there would delete the objects between them and hand the recipe a $^ whose
# files are gone. With .NOTPARALLEL above, make takes the goals in the order they
# were given, which is what the other order needs.
ifeq ($(firstword $(MAKECMDGOALS)),clean)
$(filter-out clean,$(MAKECMDGOALS)): | clean
endif
endif
endif

