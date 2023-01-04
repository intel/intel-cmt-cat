###############################################################################
# Makefile script for PQoS library and sample application
#
# @par
# BSD LICENSE
#
# Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
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

PQOS_DIR = ../../pqos
LIB_DIR = ../../lib
OBJ_DIR = ./obj
BIN_DIR = ./bin
MOCK_DIR = ../mock
APP_MOCK_DIR = ./mock
OBJ_APP_MOCK_DIR = $(APP_MOCK_DIR)/obj
GRAB_OUTPUT_DIR = ../output
PQOS_SRCS = $(sort $(wildcard $(PQOS_DIR)/*.c))
PQOS_OBJS = $(PQOS_SRCS:$(PQOS_DIR)/%.c=$(OBJ_DIR)/%.o)
APP_MOCK_SRCS = $(sort $(wildcard $(APP_MOCK_DIR)/*.c))
APP_MOCK_OBJS = $(APP_MOCK_SRCS:$(APP_MOCK_DIR)/%.c=$(OBJ_APP_MOCK_DIR)/%.o)

TESTS_SRCS = $(sort $(wildcard test_*.c))
TESTS = $(TESTS_SRCS:%.c=$(BIN_DIR)/%)

LDFLAGS = -L$(LIB_DIR) -L$(MOCK_DIR) -L$(GRAB_OUTPUT_DIR) -lpqos -lcmocka -lpqosmock -lgraboutput\
	-z noexecstack -z relro -z now
CFLAGS = -I$(LIB_DIR) -I$(PQOS_DIR) -I$(MOCK_DIR) -I$(GRAB_OUTPUT_DIR) -I$(APP_MOCK_DIR)\
	-W -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector \
	-Wunreachable-code -Wsign-compare -Wno-endif-labels

CFLAGS += -g -ggdb -O0

all: $(TESTS)

$(OBJ_DIR)/main.o: $(PQOS_DIR)/main.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ --redefine-sym main=appmain $@

$(OBJ_DIR)/%.o: $(PQOS_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_APP_MOCK_DIR)/%.o: $(APP_MOCK_DIR)/%.c
	mkdir -p $(OBJ_APP_MOCK_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/test_alloc: ./test_alloc.c $(PQOS_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=puts \
		-Wl,--wrap=printf \
		-Wl,--wrap=putchar \
		-Wl,--wrap=exit \
		-Wl,--wrap=pqos_l3ca_get \
		-Wl,--wrap=pqos_l2ca_get \
		-Wl,--wrap=pqos_mba_get \
		-Wl,--wrap=pqos_pid_get_pid_assoc \
		-Wl,--wrap=pqos_alloc_assoc_get \
		-Wl,--wrap=pqos_mon_assoc_get \
		-Wl,--wrap=pqos_inter_get \
		-Wl,--wrap=pqos_mba_set \
		-Wl,--wrap=pqos_l3ca_set \
		-Wl,--wrap=pqos_l2ca_set \
		-Wl,--wrap=pqos_alloc_assoc_set \
		-Wl,--wrap=pqos_alloc_assoc_set_pid \
		-Wl,--start-group \
		$(LDFLAGS) $(filter-out ./obj/alloc.o,$(PQOS_OBJS)) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_profiles: ./test_profiles.c $(PQOS_OBJS) $(APP_MOCK_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=puts \
		-Wl,--wrap=printf \
		-Wl,--wrap=putchar \
		-Wl,--wrap=exit \
		-Wl,--wrap=selfn_allocation_class \
		-Wl,--start-group \
		$(LDFLAGS) $(filter-out ./obj/profiles.o,$(PQOS_OBJS)) $(APP_MOCK_OBJS) $< -Wl,--end-group -o $@


.PHONY: run
run: $(TESTS)
	@echo "Running pqos Unit Tests"
	@for test in $(TESTS); do \
		bash -c "LD_LIBRARY_PATH=$(MOCK_DIR):$(LIB_DIR):$(GRAB_OUTPUT_DIR) ./$$test" || true; \
	done;

CLANGFORMAT?=clang-format
.PHONY: clang-format
clang-format:
	@for file in $(wildcard *.[ch]) $(wildcard $(APP_MOCK_DIR)/*.[ch]); do \
		echo "Checking style $$file"; \
		$(CLANGFORMAT) -style=file "$$file" | diff "$$file" - | tee /dev/stderr | [ $$(wc -c) -eq 0 ] || \
		{ echo "ERROR: $$file has style problems"; exit 1; } \
	done

CODESPELL?=codespell
.PHONY: codespell
codespell:
	$(CODESPELL) . --skip $(OBJ_DIR) -q 2

CHECKPATCH?=checkpatch.pl
.PHONY: checkpatch
checkpatch:
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore CODE_INDENT,INITIALISED_STATIC,LEADING_SPACE,SPLIT_STRING,\
	NEW_TYPEDEFS,UNSPECIFIED_INT,BLOCK_COMMENT_STYLE,\
	SPDX_LICENSE_TAG,ARRAY_SIZE,EMBEDDED_FUNCTION_NAME,\
	SYMBOLIC_PERMS,CONST_STRUCT \
	-f test_alloc.c \
	-f test_profiles.c \
	-f mock/mock_alloc.c \
	-f mock/mock_alloc.h \

.PHONY: style
style:
	$(MAKE) checkpatch
	$(MAKE) clang-format
	$(MAKE) codespell

clean:
	rm -rf $(BIN_DIR)
	rm -rf $(OBJ_DIR)
	rm -rf $(OBJ_APP_MOCK_DIR)