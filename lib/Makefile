###############################################################################
# Makefile script for PQoS library and sample application
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
###############################################################################

include ../pre-build.mk

LIB = libpqos
VERSION = 7.1.0
SO_VERSION = 7
SHARED ?= y
LDFLAGS = -L. -lpthread -z noexecstack -z relro -z now
CFLAGS = -pthread -I./ -D_GNU_SOURCE \
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
IS_CLANG = $(shell $(CC) -v 2>&1 | grep -c "^clang version ")

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
LIBNAME = $(LIB).so.$(VERSION)
LIBPERM = 0755
else
CFLAGS += -fPIE
LIBNAME = $(LIB).a
LIBPERM = 0644
endif

# DEBUG build
ifeq ($(DEBUG),y)
CFLAGS += -g -ggdb -Og -DDEBUG
else
CFLAGS += -g -O2 -D_FORTIFY_SOURCE=2
endif

# RMID CUSTOM
ifeq ($(RMID_CUSTOM),y)
CFLAGS += -DPQOS_RMID_CUSTOM
endif

# UNLOCK CUSTOM (disable API locking)
ifeq ($(UNLOCK_CUSTOM),y)
CFLAGS += -DUNLOCK_CUSTOM
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
	os_cpuinfo.o \
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
# Registers LIB_INSTALL_DIR with the dynamic linker when it is not searched
# by default (e.g. /usr/local/lib on Fedora/RHEL). The file is changed only
# when its first line is LDCONF_MARK, so one written by anyone else is kept.
LDCONF_DIR ?= /etc/ld.so.conf.d
LDCONF_FILE = $(LDCONF_DIR)/$(LIB).conf
LDCONF_MARK = \# Added by $(LIB) make install
LDCONF_OURS = head -n 1 $(LDCONF_FILE) 2>/dev/null | \
	grep -qxF '$(LDCONF_MARK)'
# A shell function that writes its arguments, one per line, as the whole of
# LDCONF_FILE: into a file made beside it, 0644 once complete, then renamed
# into place, so the loader's configuration is replaced in one step and never
# seen half written or left truncated by a write that failed. ldconfig reads
# *.conf only, so the file being made is not read by one that runs meanwhile.
LDCONF_WRITE = ldconf_write() { tmp=$$(mktemp $(LDCONF_FILE).XXXXXX) && \
	{ printf '%s\n' "$$@" > "$$tmp" && chmod 0644 "$$tmp" && \
	  mv -f "$$tmp" $(LDCONF_FILE) || { rm -f "$$tmp"; false; }; }; }
# The cache names a library by the directory ldconfig scanned (/lib64 for
# /usr/lib64 on Fedora), so an entry is matched by file, not by path
LDCACHE_HAS_LIB = ldconfig -p | \
	sed -n 's|.* => \(.*/$(LIB)\.so\.$(SO_VERSION)\)$$|\1|p' | \
	while read -r so; do \
		[ "$$so" -ef "$(LIB_INSTALL_DIR)/$(LIB).so.$(SO_VERSION)" ] && \
			echo "$$so"; \
	done | grep -q .
# LIB_INSTALL_DIR with symbolic links resolved. This is what gets
# registered, so no link above it can later point the loader elsewhere.
LIBDIR_REAL = cd "$(LIB_INSTALL_DIR)" 2>/dev/null && pwd -P
# True when "dir" and every directory above it are owned by root and
# writable by no one else. Otherwise another user could replace what
# privileged programs load from it, so it is not registered.
LIBDIR_TRUSTED = ( d="$$dir"; \
	while [ -n "$$(find "$$d" -maxdepth 0 -user 0 ! -perm /022)" ]; do \
		[ "$$d" = / ] && exit 0; \
		d=$$(dirname "$$d"); \
	done; exit 1 )

all: unselected-form

$(LIBNAME): $(OBJS)
ifeq ($(SHARED),y)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(LIB).so.$(SO_VERSION) -o $(LIBNAME) $^ -lc
	ln -f -s $(LIBNAME) $(LIB).so.$(SO_VERSION)
	ln -f -s $(LIB).so.$(SO_VERSION) $(LIB).so
else
	$(AR) crvsD $@ $^
endif

# The form that was not built goes once this one is there. Consumers link with
# -lpqos, and the linker prefers a .so where both are there, so a "make
# SHARED=n" that left the shared library from an earlier build in place had
# every consumer link that one instead of the archive just built. Phony, so it
# runs on every build and not only one that relinks - a library already up to
# date still has the other form beside it. After the library and not before
# it, so that a build which fails leaves what was working where it was.
.PHONY: unselected-form
unselected-form: $(LIBNAME)
ifeq ($(SHARED),y)
	rm -f $(LIB).a
else
	rm -f $(LIB).so $(LIB).so.$(SO_VERSION) $(LIB).so.$(VERSION)
endif

install: unselected-form

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
ifneq ($(shell uname), FreeBSD)
	@$(LDCONF_WRITE); dir=$$($(LIBDIR_REAL)); \
	if ! $(LDCACHE_HAS_LIB) && [ -d $(LDCONF_DIR) ]; then \
		if ! $(LIBDIR_TRUSTED); then \
			echo "Not adding $$dir to the dynamic linker search" \
				"path: it or a directory above it is not owned" \
				"by root or is writable by others"; \
		else \
			[ -e $(LDCONF_FILE) ] || [ -L $(LDCONF_FILE) ] || \
				ldconf_write '$(LDCONF_MARK)'; \
			if [ ! -e $(LDCONF_FILE) ] && [ ! -L $(LDCONF_FILE) ]; then \
				echo "Could not create $(LDCONF_FILE)" >&2; exit 1; \
			elif ! $(LDCONF_OURS); then \
				echo "$(LDCONF_FILE) was not written by $(LIB)" \
					"make install, leaving it unchanged"; \
			elif ! grep -qxF "$$dir" $(LDCONF_FILE); then \
				echo "Adding $$dir to the dynamic linker" \
					"search path in $(LDCONF_FILE)"; \
				keep=$$(tail -n +2 $(LDCONF_FILE)) || exit 1; \
				ldconf_write '$(LDCONF_MARK)' $${keep:+"$$keep"} \
					"$$dir" && ldconfig || exit 1; \
			fi; \
		fi; \
	fi
	@$(LDCACHE_HAS_LIB) || echo "WARNING: $(LIB_INSTALL_DIR) is not in" \
		"the dynamic linker search path, add it to a file in" \
		"$(LDCONF_DIR) or to LD_LIBRARY_PATH"
endif
endif
endif

uninstall:
	-rm $(LIB_INSTALL_DIR)/libpqos*
# on Linux whatever SHARED is: the rm above took the shared library too, if
# it was there, so the loader's configuration and cache are brought up to date
ifneq ($(NOLDCONFIG),y)
ifeq ($(shell uname), FreeBSD)
ifeq ($(SHARED),y)
	ldconfig
endif
else
# Every line of ours names a directory install put the library in, so one whose
# directory holds no libpqos shared object of any version any more, anywhere
# below it - the loader searches a directory's hwcaps subdirectories as well -
# is stale and goes: this uninstall's own directory, just emptied above, and
# any emptied or removed since it was registered - under whatever name, or
# through whatever link, it is reached by today. A line whose library is still
# there stays, which is what a prefix that still holds an install needs; of any
# version, so that a library of another SONAME installed there by another
# release of this file keeps its line, and a regular file that is there, or a
# link that resolves to one, not a link left dangling by one removed. The tree
# is walked as it lies on disk, no link to a directory followed: nothing vets
# the tree again here, and a link added since the install could lead anywhere.
# A directory that cannot be seen at the time, on a filesystem not mounted,
# reads as empty and goes too; the next install there puts it back.
	@$(LDCONF_WRITE); if $(LDCONF_OURS); then \
		lines=$$(tail -n +2 $(LDCONF_FILE)) || exit 1; \
		keep=$$(printf '%s\n' "$$lines" | while IFS= read -r d; do \
			[ -n "$$d" ] || continue; \
			if find "$$d" -name '$(LIB).so.[0-9]*' \( -type f -o -type l \) \
				-exec test -f {} \; -print -quit 2>/dev/null | grep -q .; then \
				printf '%s\n' "$$d"; \
			else \
				echo "Removing $$d from $(LDCONF_FILE)" >&2; \
			fi; \
		done); \
		if [ -z "$$keep" ]; then \
			rm -f $(LDCONF_FILE) || exit 1; \
		else \
			ldconf_write '$(LDCONF_MARK)' "$$keep" || exit 1; \
		fi; \
	fi
	ldconfig
endif
endif

DEPFILES = $(SRCS:.c=.d)

%.o: %.c %.d

%.d: %.c
	set -e; rm -f $@; \
	$(CC) -MM -MP -MF $@ $(CFLAGS) $< > $@.$$$$; \
	sed 's/$(@:.d=.o)/$@/' < $@.$$$$ > $@; \
	rm -f $@.$$$$

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

clobber:
	-rm -f $(CLEAN_OBJS) $(LIB)* $(DEPFILES)
	-rm -rf $(DOXY_DIRS)

CHECKPATCH?=checkpatch.pl
.PHONY: checkpatch
checkpatch:
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore $(CHECKPATCH_IGNORE) \
	--typedefsfile ../.checkpatch.types \
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
	$(CODESPELL) . -q 2 -L clos,mmapped,uint,te,circularly

.PHONY: style
style:
	$(MAKE) checkpatch
	$(MAKE) clang-format
	$(MAKE) codespell

CPPCHECK?=cppcheck
.PHONY: cppcheck
cppcheck:
	$(CPPCHECK) \
	--enable=warning,portability,performance,missingInclude \
	--std=c99 --template=gcc \
	--suppress=missingIncludeSystem \
	api.c api.h cap.c cap.h common.h common.c allocation.c perf.c perf.h \
	allocation.h allocation_common.c allocation_common.h monitoring.c monitoring.h \
	log.c log.h types.h machine.c machine.h \
	utils.c utils.h \
	cpuinfo.c cpuinfo.h os_allocation.h os_allocation.c \
	hw_cap.h hw_cap.c \
	hybrid.h hybrid.c \
	hw_monitoring.h hw_monitoring.c \
	lock.h lock.c \
	os_cap.h os_cap.c \
	os_common.h \
	os_monitoring.h os_monitoring.c \
	perf_monitoring.h perf_monitoring.c \
	resctrl_alloc.h resctrl_alloc.c \
	resctrl.h resctrl.c \
	resctrl_monitoring.h resctrl_monitoring.c \
	resctrl_schemata.h resctrl_schemata.c \
	resctrl_utils.h resctrl_utils.c \
	acpi.h acpi.c acpi_table.h \
	iordt.h iordt.c \
	uncore_monitoring.h uncore_monitoring.c

# if target not clean or rinse then make dependencies
ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),clobber)
ifneq ($(MAKECMDGOALS),style)
ifneq ($(MAKECMDGOALS),clang-format)
ifneq ($(MAKECMDGOALS),checkpatch)
ifneq ($(MAKECMDGOALS),codespell)
-include $(DEPFILES)
endif
endif
endif
endif
endif
endif
