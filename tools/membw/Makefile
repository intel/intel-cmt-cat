###############################################################################
# Makefile script for Membw tool
#
# @par
# BSD LICENSE
#
# Copyright(c) 2018-2020 Intel Corporation. All rights reserved.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#	* Redistributions of source code must retain the above copyright
#	  notice, this list of conditions and the following disclaimer.
#	* Redistributions in binary form must reproduce the above copyright
#	  notice, this list of conditions and the following disclaimer in
#	  the documentation and/or other materials provided with the
#	  distribution.
#	* Neither the name of Intel Corporation nor the names of its
#	  contributors may be used to endorse or promote products derived
#	  from this software without specific prior written permission.
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

APP = membw

CFLAGS=-W -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector -fPIE \
	-Wunreachable-code -Wsign-compare -Wno-endif-labels \
	-Winline -msse4.2

ifeq ($(DEBUG),y)
CFLAGS += -O0 -g -DDEBUG
else
CFLAGS += -O3 -g -D_FORTIFY_SOURCE=2
endif

HAS_AVX512 := $(shell $(CC) -mavx512f -dM -E - < /dev/null 2> /dev/null | grep -c "AVX512F")
ifeq ($(HAS_AVX512),1)
CFLAGS += -mavx512f
endif

HAS_CLWB := $(shell $(CC) -mclwb -dM -E - < /dev/null 2> /dev/null | grep -c "CLWB")
ifeq ($(HAS_CLWB),1)
CFLAGS += -mclwb
endif

IS_GCC = $(shell $(CC) -v 2>&1 | grep -c "^gcc version ")
# GCC-only options
ifeq ($(IS_GCC),1)
CFLAGS += -fno-strict-overflow \
    -fno-delete-null-pointer-checks \
    -fwrapv \
    -fno-expensive-optimizations
endif

SRCS = $(sort $(wildcard *.c))
OBJS = $(SRCS:.c=.o)
DEPFILES = $(SRCS:.c=.d)

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c %.d

%.d: %.c
	$(CC) -MM -MP -MF $@ $(CFLAGS) $<
	cat $@ | sed 's/$(@:.d=.o)/$@/' >> $@

.PHONY: clean

clean:
	-rm -f $(APP) $(OBJS) $(DEPFILES) ./*~

CHECKPATCH?=checkpatch.pl
.PHONY: checkpatch
checkpatch:
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore CODE_INDENT,INITIALISED_STATIC,LEADING_SPACE \
	--ignore SPLIT_STRING,UNSPECIFIED_INT,ARRAY_SIZE,COMPLEX_MACRO \
	--ignore STORAGE_CLASS,SPDX_LICENSE_TAG,CONST_STRUCT \
	-f membw.c

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

CPPCHECK?=cppcheck
.PHONY: cppcheck
cppcheck:
	$(CPPCHECK) enable=warning,portability,performance,unusedFunction,missingInclude \
	--std=c99 --template=gcc membw.c


# if target not clean then make dependencies
ifneq ($(MAKECMDGOALS),clean)
-include $(DEPFILES)
endif

