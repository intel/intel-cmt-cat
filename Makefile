###############################################################################
# Makefile script for PQoS sample application
#
# @par
# BSD LICENSE
#
# Copyright(c) 2014-2026 Intel Corporation. All rights reserved.
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
#
###############################################################################

include pre-build.mk

# XXX: modify as desired
PREFIX ?= /usr/local
export PREFIX

ifdef DEBUG
export DEBUG
endif

ifdef SHARED
export SHARED
endif

.PHONY: all clean TAGS install uninstall style style-modes cppcheck setup-dev

all:
	$(MAKE) -C lib
	$(MAKE) -C pqos
	$(MAKE) -C tools/membw
	$(MAKE) -C examples/c/CAT_MBA
	$(MAKE) -C examples/c/CMT_MBM
	$(MAKE) -C examples/c/PSEUDO_LOCK

setup-dev:
	$(MAKE) -C tests setup-dev

clean:
	$(MAKE) -C lib clean
	$(MAKE) -C pqos clean
	$(MAKE) -C tools/membw clean
	$(MAKE) -C examples/c/CAT_MBA clean
	$(MAKE) -C examples/c/CMT_MBM clean
	$(MAKE) -C examples/c/PSEUDO_LOCK clean
	$(MAKE) -C tests clean
	$(MAKE) -C unit-test clean

# Directories that hold C source, for the mode check below
STYLE_MODE_DIRS = lib pqos tools examples unit-test tests

# A C file is not a program, so the executable bit on one is a mistake rather
# than a build requirement. The bit is on the file itself, in a clone and in a
# tree unpacked from a release archive alike, so the filesystem is what to ask.
# The index is the wrong thing to ask: it does not see a chmod until it is
# staged, which is the moment the mistake is made, and a check that begins by
# asking git turns itself off wherever git is not installed, refuses the
# directory as dubiously owned, or has no .git to look at - this repository's own
# .dockerignore drops .git, so a container build would land there. Only the user
# bit is looked at, which is the one git itself records. Hidden directories are
# skipped, so the C sources of a virtual environment under tests/ are not
# reported.
style-modes:
	@files=$$(find $(STYLE_MODE_DIRS) -name '.*' -prune -o \
		-type f -name '*.[ch]' -perm -u+x -print | \
		sort | sed 's/^/  /'); \
	if [ -n "$$files" ]; then \
		echo "style-modes: the executable bit is set on:"; \
		echo "$$files"; \
		exit 1; \
	fi

style: style-modes
	$(MAKE) -C lib style
	$(MAKE) -C pqos style
	$(MAKE) -C tools/membw style
	$(MAKE) -C examples/c/CAT_MBA style
	$(MAKE) -C examples/c/CMT_MBM style
	$(MAKE) -C examples/c/PSEUDO_LOCK style
	$(MAKE) -C tests style
	$(MAKE) -C unit-test style

cppcheck:
	$(MAKE) -C lib cppcheck
	$(MAKE) -C pqos cppcheck
	$(MAKE) -C tools/membw cppcheck
	$(MAKE) -C examples/c/CAT_MBA cppcheck
	$(MAKE) -C examples/c/CMT_MBM cppcheck
	$(MAKE) -C examples/c/PSEUDO_LOCK cppcheck

install:
	$(MAKE) -C lib install
	$(MAKE) -C pqos install
	$(MAKE) -C tools/membw install

uninstall:
	$(MAKE) -C lib uninstall
	$(MAKE) -C pqos uninstall
	$(MAKE) -C tools/membw uninstall

TAGS:
	find ./ -name "*.[ch]" -print | etags -
