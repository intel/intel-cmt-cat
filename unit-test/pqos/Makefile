###############################################################################
# Makefile script for PQoS library and sample application
#
# @par
# BSD LICENSE
#
# Copyright(c) 2022-2026 Intel Corporation. All rights reserved.
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
PQOS_DEPS = $(PQOS_SRCS:$(PQOS_DIR)/%.c=$(OBJ_DIR)/%.d)
APP_MOCK_DEPS = $(APP_MOCK_SRCS:$(APP_MOCK_DIR)/%.c=$(OBJ_APP_MOCK_DIR)/%.d)

TESTS_SRCS = $(sort $(wildcard test_*.c))
TESTS = $(TESTS_SRCS:%.c=$(BIN_DIR)/%)
TESTS_DEPS = $(TESTS_SRCS:%.c=$(OBJ_DIR)/%.d)

LDFLAGS = -L$(LIB_DIR) -L$(MOCK_DIR) -L$(GRAB_OUTPUT_DIR) -lpqos -lcmocka -lpqosmock -lgraboutput\
	-z noexecstack -z relro -z now
CFLAGS = -I$(PQOS_DIR) -I$(LIB_DIR) -I$(MOCK_DIR) -I$(GRAB_OUTPUT_DIR) -I$(APP_MOCK_DIR)\
	-W -Wall -Wextra -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector-strong \
	-Wunreachable-code -Wsign-compare -Wno-endif-labels


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

CFLAGS += -g -ggdb -O0

all: $(TESTS)

# What a binary or an object here is made of, and nothing else, decides whether
# it is out of date. Three things do, and none of them was tracked. The reasoning
# is unit-test/lib/Makefile's, and applies here word for word.
#
# This Makefile, for the wraps and the -D flags. The headers in this directory and
# in the mock beside it: the test .c files go straight to the link recipes and
# never become objects, so what they include is recorded against the binary rather
# than against an object - $(TESTS_DEPS) below does that - and these wildcards are
# the belt beside it. caps_gen.h holds the capability fixtures every case is built
# on, and an edit to it left every binary up to date and every run asserting
# against the previous fixtures. And the flags that do not come from this file,
# which flags-stamp.mk records.
#
# $(APP_MOCK_OBJS) among the targets, not only $(PQOS_OBJS): those objects are
# compiled with the $(CFLAGS) defined here and nowhere else, and test_profiles
# links them, so a flag change left it linked against a mock the previous flags
# built.
FLAGS_STAMP_DIR = $(OBJ_DIR)
include ../flags-stamp.mk

# and on the three libraries these binaries link - the product library, the mock
# and the output capture.
# With SHARED=y each is loaded at run time and a rebuilt one is picked up by
# itself; with SHARED=n they are archives whose contents are copied in, so a
# library rebuilt after a flag change left every binary running the code the
# previous flags produced - test_output_capture asserts through the capture
# library and nothing else, and every one of them is linked against libpqos
# itself. Named by wildcard because which of the two forms
# exists is those builds' choice, made on their own command lines, and as
# prerequisites only: the link lines below name both with -l and must not be
# handed a second copy.
# and on the headers these compilations reach through -I, which are in four
# directories and not two: $(PQOS_DIR) holds main.h and the utility's own, and
# ../../pqos/main.c includes both that and the library's pqos.h, so an edit to
# either left obj/main.o up to date. The objects have recorded dependencies as
# well, generated below and read through depfiles.mk; these cover the test .c
# files, which are handed straight to the link recipes and never become objects.
$(TESTS) $(PQOS_OBJS) $(APP_MOCK_OBJS): Makefile $(wildcard *.h) \
    $(wildcard $(APP_MOCK_DIR)/*.h) $(FLAGS_STAMP) \
    $(wildcard $(PQOS_DIR)/*.h) $(wildcard $(LIB_DIR)/*.h) \
    $(wildcard $(MOCK_DIR)/*.h) $(wildcard $(GRAB_OUTPUT_DIR)/*.h) \
    $(wildcard $(MOCK_DIR)/libpqosmock.*) \
    $(wildcard $(GRAB_OUTPUT_DIR)/libgraboutput.*) \
    $(wildcard $(LIB_DIR)/libpqos.*)

# the recorded dependencies follow the flags that produced them, as in
# unit-test/lib: a .d file lists what the -I set in force at the time reached.
$(PQOS_DEPS) $(APP_MOCK_DEPS) $(TESTS_DEPS): Makefile $(FLAGS_STAMP)

# and they are read - see depfiles.mk. After "all:" above, so that it is still
# the default goal.
DEPFILES_TO_READ = $(PQOS_DEPS) $(APP_MOCK_DEPS) $(TESTS_DEPS)
include ../depfiles.mk


$(OBJ_DIR)/main.o: $(PQOS_DIR)/main.c $(OBJ_DIR)/main.d
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ --redefine-sym main=appmain $@

$(OBJ_DIR)/%.o: $(PQOS_DIR)/%.c $(OBJ_DIR)/%.d
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_APP_MOCK_DIR)/%.o: $(APP_MOCK_DIR)/%.c $(OBJ_APP_MOCK_DIR)/%.d
	mkdir -p $(OBJ_APP_MOCK_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# -MT, because -MM writes the target as a bare "main.o" with no directory in it
# and the objects here are $(OBJ_DIR)/main.o: a .d file written without it names
# a target this Makefile never builds, so its prerequisites attach to nothing.
# The .d file is a target as well, so that a header which stops including
# another regenerates it - see unit-test/lib/Makefile.
$(PQOS_DEPS): $(OBJ_DIR)/%.d : $(PQOS_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) -MM -MP -MT $(@:.d=.o) -MT $@ -MF $@ $(CFLAGS) $<

$(APP_MOCK_DEPS): $(OBJ_APP_MOCK_DIR)/%.d : $(APP_MOCK_DIR)/%.c
	mkdir -p $(OBJ_APP_MOCK_DIR)
	$(CC) -MM -MP -MT $(@:.d=.o) -MT $@ -MF $@ $(CFLAGS) $<

# and the test sources, whose target is the binary: they are compiled inside the
# link recipes and never become objects. Two of them - test_monitor_utils.c and
# test_monitor_xml.c - #include a utility source rather than linking the object,
# and nothing said so: editing ../../pqos/monitor_utils.c left the binary that
# carries a copy of it up to date. These files list whatever a test source
# includes, source or header, and a header that is removed is named as a phony
# target by -MP, so what includes it rebuilds and the compile reports it.
$(TESTS_DEPS): $(OBJ_DIR)/%.d : %.c
	mkdir -p $(OBJ_DIR)
	$(CC) -MM -MP -MT $(BIN_DIR)/$* -MT $@ -MF $@ $(CFLAGS) $<

# each binary waits for its own, which the pattern above cannot say: these rules
# carry no recipe and only add a prerequisite.
$(TESTS): $(BIN_DIR)/% : $(OBJ_DIR)/%.d

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
		-Wl,--wrap=pqos_get_num_mem_regions \
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

$(BIN_DIR)/test_main: ./test_main.c $(PQOS_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=puts \
		-Wl,--wrap=printf \
		-Wl,--wrap=putchar \
		-Wl,--wrap=exit \
		-Wl,--start-group \
		$(LDFLAGS) $(PQOS_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_cap: ./test_cap.c $(PQOS_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=puts \
		-Wl,--wrap=printf \
		-Wl,--wrap=putchar \
		-Wl,--wrap=exit \
		-Wl,--wrap=pqos_inter_get \
		-Wl,--wrap=pqos_io_devs_get \
		-Wl,--wrap=pqos_l3ca_get_min_cbm_bits \
		-Wl,--start-group \
		$(LDFLAGS) $(PQOS_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_cmdline: ./test_cmdline.c $(PQOS_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=puts \
		-Wl,--wrap=printf \
		-Wl,--wrap=putchar \
		-Wl,--wrap=exit \
		-Wl,--start-group \
		$(LDFLAGS) $(PQOS_OBJS) $< -Wl,--end-group -o $@

# main.c is included by the test rather than linked, so its object is left out
$(BIN_DIR)/test_print_modes: ./test_print_modes.c $(PQOS_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=puts \
		-Wl,--wrap=printf \
		-Wl,--wrap=fprintf \
		-Wl,--wrap=putchar \
		-Wl,--wrap=exit \
		-Wl,--wrap=alloc_requested_option \
		-Wl,--wrap=pqos_init \
		-Wl,--wrap=pqos_fini \
		-Wl,--wrap=pqos_hybrid_discover \
		-Wl,--wrap=pqos_hybrid_free \
		-Wl,--start-group \
		$(LDFLAGS) $(filter-out ./obj/main.o,$(PQOS_OBJS)) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_common: ./test_common.c $(PQOS_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=puts \
		-Wl,--wrap=printf \
		-Wl,--wrap=putchar \
		-Wl,--wrap=exit \
		-Wl,--start-group \
		$(LDFLAGS) $(PQOS_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_dump: ./test_dump.c $(PQOS_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=puts \
		-Wl,--wrap=printf \
		-Wl,--wrap=putchar \
		-Wl,--wrap=exit \
		-Wl,--start-group \
		$(LDFLAGS) $(PQOS_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_iface_select: ./test_iface_select.c $(PQOS_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--start-group \
		$(LDFLAGS) $(PQOS_OBJS) $< -Wl,--end-group -o $@

# printf and fprintf are wrapped so the cases can read what the enumeration
# printed, which is the feature: a return value says the command ran, not that
# it reported the processor's capabilities.
$(BIN_DIR)/test_hybrid: ./test_hybrid.c $(OBJ_DIR)/hybrid.o
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=printf \
		-Wl,--wrap=fprintf \
		-Wl,--wrap=puts \
		-Wl,--wrap=putchar \
		-Wl,--start-group -L$(LIB_DIR) -L$(GRAB_OUTPUT_DIR) \
		-lpqos -lcmocka -lgraboutput \
		$(OBJ_DIR)/hybrid.o $< -Wl,--end-group -o $@

# The capture the other binaries assert through, tested on its own: it is
# linked without the wraps, because the cases call the wrappers by name to fill
# the buffer and what they are checking is the capture rather than the utility.
$(BIN_DIR)/test_output_capture: ./test_output_capture.c
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--start-group \
		-L$(GRAB_OUTPUT_DIR) -lcmocka -lgraboutput \
		$< -Wl,--end-group -o $@

$(BIN_DIR)/test_monitor_utils: ./test_monitor_utils.c
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--start-group \
		-lcmocka $< -Wl,--end-group -o $@


$(BIN_DIR)/test_monitor_xml: ./test_monitor_xml.c
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--start-group \
		-lcmocka $< -Wl,--end-group -o $@

.PHONY: run
run: $(TESTS)
	@echo "Running pqos Unit Tests"
# every binary is run before the result is reported, so one failure
# does not hide the ones after it, and the target fails if any of
# them did: a test suite whose failures cannot fail the build is a
# suite nothing is gated on.
#
# each binary is run directly rather than through a command string
# given to another shell, so that whatever its name contains is a
# path and not something for a shell to read, and the summary is
# printed with printf, whose handling of a leading dash or a
# backslash in what it is given does not vary between shells
	@failed=""; \
	for test in $(TESTS); do \
		LD_LIBRARY_PATH=$(MOCK_DIR):$(LIB_DIR):$(GRAB_OUTPUT_DIR) "$$test" || failed="$$failed $$test"; \
	done; \
	if [ -n "$$failed" ]; then \
		printf 'FAILED:%s\n' "$$failed"; \
		exit 1; \
	fi

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
	$(CODESPELL) . --skip $(OBJ_DIR) -q 2 -L clos

CHECKPATCH?=checkpatch.pl
.PHONY: checkpatch
checkpatch:
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore $(CHECKPATCH_IGNORE) \
	--typedefsfile ../../.checkpatch.types \
	--file *.[ch] $(APP_MOCK_DIR)/*.[ch]

.PHONY: style
style:
	$(MAKE) checkpatch
	$(MAKE) clang-format
	$(MAKE) codespell

clean:
	rm -rf $(BIN_DIR)
	rm -rf $(OBJ_DIR)
	rm -rf $(OBJ_APP_MOCK_DIR)
