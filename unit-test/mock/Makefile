###############################################################################
# Makefile script for PQoS library and sample application
#
# @par
# BSD LICENSE
#
# Copyright(c) 2020-2026 Intel Corporation. All rights reserved.
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


include ../../pre-build.mk

LIB_DIR = ../../lib

LIB = libpqosmock
SHARED ?= y
LDFLAGS = -L. -lpthread -z noexecstack -z relro -z now
CFLAGS = -pthread -I$(LIB_DIR) -D_GNU_SOURCE \
	-W -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector-strong \
	-Wunreachable-code -Wsign-compare -Wno-endif-labels
ifneq ($(EXTRA_CFLAGS),)
CFLAGS += $(EXTRA_CFLAGS)
endif
ifneq ($(EXTRA_LDFLAGS),)
LDFLAGS += $(EXTRA_LDFLAGS)
endif

# ICC and GCC/CLANG options
ifeq ($(CC),icc)
else
# GCC and clang
CFLAGS += -Wcast-align \
    -Wnested-externs \
    -Wmissing-noreturn
endif

IS_GCC = $(shell $(CC) -v 2>&1 | grep -c "^gcc version ")
# GCC-only options
ifeq ($(IS_GCC),1)
CFLAGS += -fno-strict-overflow \
    -fno-delete-null-pointer-checks \
    -fwrapv \
    -fstack-clash-protection \
    -fcf-protection=full
endif

# so or static build
ifeq ($(SHARED),y)
CFLAGS += -fPIC
LIBNAME = $(LIB).so
LIBPERM = 0755
else
CFLAGS += -fPIE
LIBNAME = $(LIB).a
LIBPERM = 0644
endif

# DEBUG build
ifeq ($(DEBUG),y)
CFLAGS += -g -ggdb -O0 -DDEBUG
else
CFLAGS += -g -O2 -D_FORTIFY_SOURCE=2
endif

# Build targets and dependencies
SRCS = $(sort $(wildcard *.c))
OBJS = $(SRCS:.c=.o)
CLEAN_OBJS = $(SRCS:.c=.o)
DEPFILE = $(LIB).dep

$(LIBNAME): $(OBJS)
ifeq ($(SHARED),y)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(LIB).so -o $(LIBNAME) $^ -lc
else
	$(AR) crvsD $@ $^
endif

# The objects here are described by this Makefile - the mock's own flags are
# written nowhere else - so a flag change has to rebuild them, or the tests link
# against the library the previous flags built. SHARED and EXTRA_CFLAGS arrive on
# the command line rather than from the file, and flags-stamp.mk is what records
# those.
#
# The objects and not the library: the link recipe above passes $^ to the
# compiler, so a Makefile named as a prerequisite of the library would be handed
# to the linker as an object file. Rebuilding an object relinks the library.
FLAGS_STAMP_DIR = .
include ../flags-stamp.mk

DEPFILES = $(SRCS:.c=.d)

$(OBJS) $(DEPFILES): Makefile $(wildcard *.h) $(FLAGS_STAMP)

%.o: %.c %.d

%.d: %.c
	$(CC) -MM -MP -MF $@ $(CFLAGS) $<
	cat $@ | sed 's/$(@:.d=.o)/$@/' >> $@

CHECKPATCH?=checkpatch.pl
.PHONY: checkpatch
checkpatch:
	$(CHECKPATCH) --no-tree --no-signoff --terse \
	--ignore $(CHECKPATCH_IGNORE) \
	--typedefsfile ../../.checkpatch.types \
	--file *.[ch]

CLANGFORMAT?=clang-format
.PHONY: clang-format
clang-format:
	@for file in $(wildcard *.[ch]); do \
		echo "Checking style $$file"; \
		$(CLANGFORMAT) -style=file "$$file" | diff "$$file" - | tee /dev/stderr | [ $$(wc -c) -eq 0 ] || \
		{ echo "ERROR: $$file has style problems"; exit 1; } \
	done

CODESPELL?=codespell
.PHONY: codespell
codespell:
	$(CODESPELL) . -q 2

.PHONY: style
style:
	$(MAKE) checkpatch
	$(MAKE) clang-format
	$(MAKE) codespell

.PHONY: clean
clean:
	-rm -f $(CLEAN_OBJS) $(LIB)* $(DEPFILES) $(FLAGS_STAMP)
