###############################################################################
# Makefile script for PQoS sample application
#
# @par
# BSD LICENSE
# 
# Copyright(c) 2014-2015 Intel Corporation. All rights reserved.
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
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.O
# 
###############################################################################

CC = gcc
LIBNAME = ./lib/libpqos.a
LDFLAGS = -L./lib -lpqos -lpthread -fPIE -z noexecstack -z relro -z now
CFLAGS = -I./lib \
	-W -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector -fPIE -D_FORTIFY_SOURCE=2 \
	-Wunreachable-code -Wmissing-noreturn -Wsign-compare -Wno-endif-labels
ifneq ($(EXTRA_CFLAGS),)
CFLAGS += $(EXTRA_CFLAGS)
endif

# ICC and GCC options
ifeq ($(CC),icc)
else
CFLAGS += -Wcast-align -Wnested-externs
endif

# DEBUG build
ifeq ($(DEBUG),y)
CFLAGS += -g -ggdb -O0 -DDEBUG
else
CFLAGS += -g -O3
endif 

# PID API build option
ifeq ($(NO_PID_API), y)
export NO_PID_API
endif

# Build targets and dependencies
APP = pqos

MAN = pqos.8

# XXX: modify as desired
PREFIX ?= /usr/local
BIN_DIR = $(PREFIX)/bin
MAN_DIR = $(PREFIX)/man/man8

all: $(APP)

$(APP): main.o profiles.o $(LIBNAME)
	$(CC) $^ $(LDFLAGS) -o $@

$(LIBNAME):
	make -C lib all

install: $(APP) $(MAN)
	install -D -s $(APP) $(BIN_DIR)/$(APP)
	install -m 0444 $(MAN) -D $(MAN_DIR)/$(MAN)

uninstall:
	-rm $(BIN_DIR)/$(APP)
	-rm $(MAN_DIR)/$(MAN)

.PHONY: clean rinse TAGS install uninstall

rinse:
	-rm -f $(APP) main.o profiles.o

clean:
	-rm -f $(APP) main.o profiles.o $(DEPFILE) ./*~
	-make -C lib clean

TAGS:
	etags ./*.[ch] ./lib/*.[ch]

CHECKPATCH?=checkpatch.pl
.PHONY: style
style:
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore CODE_INDENT,INITIALISED_STATIC,LEADING_SPACE,SPLIT_STRING \
	 -f main.c -f profiles.c -f profiles.h

CPPCHECK?=cppcheck
.PHONY: cppcheck
cppcheck:
	$(CPPCHECK) --enable=warning,portability,performance,unusedFunction,missingInclude \
	--std=c99 -I./lib --template=gcc \
	main.c profiles.c profiles.h
