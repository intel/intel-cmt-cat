###############################################################################
# Makefile script for PQoS library and sample application
#
# @par
# BSD LICENSE
# 
# Copyright(c) 2014-2016 Intel Corporation. All rights reserved.
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

LIBSONAME_ = libpqos.so
LIBSONAME = $(LIBSONAME_).1
LIBANAME = libpqos.a
ifeq ($(SHARED),y)
LIBNAME = $(LIBSONAME)
else
LIBNAME = $(LIBANAME)
endif 

LDFLAGS = -L. -lpthread -fPIE -z noexecstack -z relro -z now
CFLAGS = -pthread -I./ \
	-W -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector -D_FORTIFY_SOURCE=2\
	-Wunreachable-code -Wmissing-noreturn -Wsign-compare -Wno-endif-labels
ifneq ($(EXTRA_CFLAGS),)
CFLAGS += $(EXTRA_CFLAGS)
endif
DOXY_DIRS = doc_api doc_lib

# ICC and GCC options
ifeq ($(CC),icc)
else
CFLAGS += -Wcast-align -Wnested-externs
endif

# so or static build
ifeq ($(SHARED),y)
CFLAGS += -fPIC
else
CFLAGS += -fPIE
endif 

# DEBUG build
ifeq ($(DEBUG),y)
CFLAGS += -g -ggdb -O0 -DDEBUG
else
CFLAGS += -g -O2
endif 

# Build targets and dependencies
OBJS = cpuinfo.o machine.o host_cap.o host_allocation.o host_monitoring.o utils.o log.o

# On FreeBSD build with NO_PID_API option
ifeq ($(shell uname), FreeBSD)
NO_PID_API=y
endif

# PID API build option
ifeq ($(NO_PID_API), y)
CFLAGS += -DPQOS_NO_PID_API
else
OBJS += host_pidapi.o perf.o
endif

DEPFILE = $(LIBANAME).dep

all: $(LIBNAME)

$(LIBNAME): $(OBJS)
ifeq ($(SHARED),y)
	$(CC) -shared -Wl,-soname,$(LIBNAME) -o $(LIBNAME).0.1 $^ -l c
	-ln -s $(LIBNAME).0.1 $(LIBNAME)
	-ln -s $(LIBNAME).0.1 $(LIBSONAME_)
else
	$(AR) crvs -D $@ $^
endif

.PHONY: dep
dep: $(DEPFILE)

$(DEPFILE): $(subst .o,.c,$(OBJS))
	$(CC) -MM $(CFLAGS) $^ > $@


.PHONY: clean rinse doxy help

help:
	@echo "PQoS library make targets:"
	@echo "    make all          - build static library"
	@echo "    make all SHARED=y - build shared library"
	@echo "    make all DEBUG=y  - build static library for debugging"
	@echo "    make rinse        - remove build files"
	@echo "    make clean        - remove even more files than rinse"
	@echo "    make doxy         - make doxygen documentation" 
	@echo "    make style        - coding style check" 

doxy:
	doxygen api_doxygen.cfg
	doxygen lib_doxygen.cfg

rinse:
	-rm -f $(OBJS) $(LIBNAME)* $(DEPFILE)

clean:
	-rm -f $(OBJS) $(LIBANAME) $(LIBSONAME_) $(LIBSONAME)* $(DEPFILE) ./*~
	-rm -rf $(DOXY_DIRS)
	-$(MAKE) clean -C examples/CAT
	-$(MAKE) clean -C examples/CMT_MBM

CHECKPATCH?=checkpatch.pl
.PHONY: style
style:
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore CODE_INDENT,INITIALISED_STATIC,LEADING_SPACE,SPLIT_STRING \
	-f host_cap.c -f host_cap.h -f host_allocation.c -f perf.c -f perf.h \
	-f host_allocation.h -f host_monitoring.c -f host_monitoring.h \
	-f log.c -f log.h -f list.h -f types.h -f machine.c -f machine.h \
	-f utils.c -f cpuinfo.c -f cpuinfo.h -f host_pidapi.c -f host_pidapi.h
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore CODE_INDENT,INITIALISED_STATIC,LEADING_SPACE,SPLIT_STRING,\
	NEW_TYPEDEFS \
	-f pqos.h

CPPCHECK?=cppcheck
.PHONY: cppcheck
cppcheck:
	$(CPPCHECK) \
	--enable=warning,portability,performance,missingInclude \
	--std=c99 --template=gcc \
	host_cap.c host_cap.h host_allocation.c perf.c perf.h \
	host_allocation.h host_monitoring.c host_monitoring.h \
	log.c log.h list.h types.h machine.c machine.h \
	utils.c cpuinfo.c cpuinfo.h host_pidapi.c host_pidapi.h

# dependecies file
-include $(DEPFILE)  
