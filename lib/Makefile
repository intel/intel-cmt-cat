###############################################################################
# Makefile script for PQoS library and sample application
#
# @par
# BSD LICENSE
#
# Copyright(c) 2014-2020 Intel Corporation. All rights reserved.
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
###############################################################################

LIB = libpqos
VERSION = 4.0.0
SO_VERSION = 4
SHARED ?= y
LDFLAGS = -L. -lpthread -z noexecstack -z relro -z now
CFLAGS = -pthread -I./ -D_GNU_SOURCE \
	-W -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector \
	-Wunreachable-code -Wsign-compare -Wno-endif-labels
ifneq ($(EXTRA_CFLAGS),)
CFLAGS += $(EXTRA_CFLAGS)
endif
ifneq ($(EXTRA_LDFLAGS),)
LDFLAGS += $(EXTRA_LDFLAGS)
endif

DOXY_DIRS = doc_api doc_lib

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
    -fwrapv
endif

# so or static build
ifeq ($(SHARED),y)
CFLAGS += -fPIC
LIBNAME = $(LIB).so.$(VERSION)
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

# RMID CUSTOM
ifeq ($(RMID_CUSTOM),y)
CFLAGS += -DPQOS_RMID_CUSTOM
endif

# Build targets and dependencies
SRCS = $(sort $(wildcard *.c))
OBJS = $(SRCS:.c=.o)
CLEAN_OBJS = $(SRCS:.c=.o)

# On FreeBSD build with no OS support
ifeq ($(shell uname), FreeBSD)
OBJS := $(filter-out perf.o \
	os_allocation.o \
	os_cap.o \
	os_monitoring.o \
	resctrl.o \
	resctrl_alloc.o \
	resctrl_monitoring.o \
	resctrl_schemata.o \
	resctrl_utils.o \
	perf_monitoring.o,$(OBJS))
endif

HDR = pqos.h
PREFIX ?= /usr/local
LIB_INSTALL_DIR ?= $(PREFIX)/lib
HDR_DIR ?= $(PREFIX)/include
DEPFILE = $(LIB).dep
NOLDCONFIG ?= n

all: $(LIBNAME)

$(LIBNAME): $(OBJS)
ifeq ($(SHARED),y)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(LIB).so.$(SO_VERSION) -o $(LIBNAME) $^ -lc
	ln -f -s $(LIBNAME) $(LIB).so.$(SO_VERSION)
	ln -f -s $(LIB).so.$(SO_VERSION) $(LIB).so
else
	$(AR) crvsD $@ $^
endif

install: $(LIBNAME)
# Install on FreeBSD
ifeq ($(shell uname), FreeBSD)
	install -d $(LIB_INSTALL_DIR)
	install -m $(LIBPERM) $(LIBNAME) $(LIB_INSTALL_DIR)
	install -m 0644 $(HDR) $(HDR_DIR)
# Install on Linux
else
	install -m $(LIBPERM) $(LIBNAME) -D $(LIB_INSTALL_DIR)/$(LIBNAME)
	install -m 0644 $(HDR) -D $(HDR_DIR)/$(HDR)
endif

# Create symlinks to DSO
ifeq ($(SHARED),y)
	cd $(LIB_INSTALL_DIR); \
		ln -f -s $(LIB).so.$(VERSION) $(LIB).so.$(SO_VERSION); \
		ln -f -s $(LIB).so.$(SO_VERSION) $(LIB).so
ifneq ($(NOLDCONFIG),y)
	ldconfig
endif
endif

uninstall:
	-rm $(LIB_INSTALL_DIR)/libpqos*
ifeq ($(SHARED),y)
ifneq ($(NOLDCONFIG),y)
	ldconfig
endif
endif

DEPFILES = $(SRCS:.c=.d)

%.o: %.c %.d

%.d: %.c
	$(CC) -MM -MP -MF $@ $(CFLAGS) $<
	cat $@ | sed 's/$(@:.d=.o)/$@/' >> $@

.PHONY: clean clobber doxy help

help:
	@echo "PQoS library make targets:"
	@echo "    make              - build shared library"
	@echo "    make SHARED=n     - build static library"
	@echo "    make DEBUG=y      - build shared library for debugging"
	@echo "    make install      - install library (accepts PREFIX=/some/where)"
	@echo "    make uninstall    - uninstall library (accepts PREFIX=/some/where)"
	@echo "    make clean        - remove files produced normally by make"
	@echo "    make clobber      - clean and remove documentation"
	@echo "    make doxy         - make doxygen documentation"
	@echo "    make style        - coding style check (accepts CHECKPATCH=/some/where/checkpatch.pl)"
	@echo "    make cppcheck     - static code analysis (accepts CPPCHECK=/some/where/cppcheck)"

doxy:
	doxygen api_doxygen.cfg
	doxygen lib_doxygen.cfg

clean:
	-rm -f $(CLEAN_OBJS) $(LIB)* $(DEPFILES)
	$(MAKE) -C python clean

clobber:
	-rm -f $(CLEAN_OBJS) $(LIB)* $(DEPFILES)
	-rm -rf $(DOXY_DIRS)

CHECKPATCH?=checkpatch.pl
.PHONY: checkpatch
checkpatch:
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore CODE_INDENT,INITIALISED_STATIC,LEADING_SPACE,SPLIT_STRING,\
	NEW_TYPEDEFS,UNSPECIFIED_INT,BLOCK_COMMENT_STYLE,\
	SPDX_LICENSE_TAG,ARRAY_SIZE,EMBEDDED_FUNCTION_NAME,\
	SYMBOLIC_PERMS,CONST_STRUCT \
	-f pqos.h \
	-f allocation.h -f allocation.c \
	-f api.h -f api.c \
	-f cap.h -f cap.c \
	-f common.h -f common.c \
	-f cpuinfo.h -f cpuinfo.c \
	-f hw_cap.h -f hw_cap.c \
	-f hw_monitoring.h hw_monitoring.c \
	-f log.h -f log.c \
	-f machine.h -f machine.c \
	-f monitoring.h -f monitoring.c \
	-f os_allocation.h -f os_allocation.c \
	-f os_cap.h -f os_cap.c \
	-f os_monitoring.h os_monitoring.c \
	-f perf.h -f perf.c \
	-f perf_monitoring.h -f perf_monitoring.c \
	-f resctrl.h -f resctrl.c \
	-f resctrl_monitoring.h -f resctrl_monitoring.c \
	-f resctrl_alloc.h -f resctrl_alloc.c \
	-f resctrl_schemata.h -f resctrl_schemata.c \
	-f resctrl_utils.h -f resctrl_utils.c \
	-f types.h \
	-f utils.h -f utils.c

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
	$(CODESPELL) . --skip python -q 2

.PHONY: style
style:
	$(MAKE) checkpatch
	$(MAKE) clang-format
	$(MAKE) codespell
	$(MAKE) -C python style

CPPCHECK?=cppcheck
.PHONY: cppcheck
cppcheck:
	$(CPPCHECK) \
	--enable=warning,portability,performance,missingInclude \
	--std=c99 --template=gcc \
	api.c api.h cap.c cap.h common.h common.c allocation.c perf.c perf.h \
	allocation.h monitoring.c monitoring.h \
	log.c log.h types.h machine.c machine.h \
	utils.c utils.h \
	cpuinfo.c cpuinfo.h os_allocation.h os_allocation.c \
	hw_cap.h hw_cap.c \
	hw_monitoring.h hw_monitoring.c \
	os_cap.h os_cap.c \
	os_monitoring.h os_monitoring.c \
	perf_monitoring.h perf_monitoring.c \
	resctrl_alloc.h resctrl_alloc.c \
	resctrl.h resctrl.c \
	resctrl_monitoring.h resctrl_monitoring.c \
	resctrl_schemata.h resctrl_schemata.c \
	resctrl_utils.h resctrl_utils.c

# if target not clean or rinse then make dependencies
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),clobber)
-include $(DEPFILES)
endif
endif
