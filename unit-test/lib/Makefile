include ../../pre-build.mk

LIB_DIR = ../../lib
OBJ_DIR = ./obj
BIN_DIR = ./bin
LIB_SRCS = $(sort $(wildcard $(LIB_DIR)/*.c))
LIB_OBJS = $(LIB_SRCS:$(LIB_DIR)/%.c=$(OBJ_DIR)/%.o)
LIB_DEPS = $(LIB_SRCS:$(LIB_DIR)/%.c=$(OBJ_DIR)/%.d)

MOCK_DIR = ../mock

TESTS_SRCS = $(sort $(wildcard test_*.c))
TESTS = $(TESTS_SRCS:%.c=$(BIN_DIR)/%)
TESTS_DEPS = $(TESTS_SRCS:%.c=$(OBJ_DIR)/%.d)


LDFLAGS = -L$(MOCK_DIR) -lpthread -lcmocka -lpqosmock -z noexecstack -z relro -z now
CFLAGS = -pthread -I$(LIB_DIR) -I$(MOCK_DIR) \
	-D_GNU_SOURCE \
	-W -Wall -Wextra -Wstrict-prototypes \
	-Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector-strong \
	-Wunreachable-code -Wsign-compare -Wno-endif-labels \
	-D'PQOS_LOCAL=' \
	-D'PQOS_API=' \
	-D'PQOS_STATIC=__attribute__((weak))'

CFLAGS += -DLOCKFILE=\"/tmp/libpqos_test_lockfile\"
# and no test reads the machine's own CXL bus: the cases that mock the file
# helpers would otherwise walk whatever /sys/bus/cxl the host happens to have,
# and one answering "every file exists" would be asked for a region's targets
# for as long as it kept answering. A path nothing can create is the whole
# isolation; the cases for the reader itself point this at a fixture of their own
CFLAGS += -DCXL_BUS_PATH=\"/nonexistent/pqos-unit-test/bus/cxl\"

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

# common function wrap
WRAP = \
	-Wl,--wrap=_pqos_get_cap \
	-Wl,--wrap=_pqos_get_cpu \
	-Wl,--wrap=_pqos_get_dev \
	-Wl,--wrap=_pqos_get_inter

CFLAGS += -g -ggdb -O0

# DEBUG=y turns the library's ASSERT into assert(), which is how the cases that
# are about a debug build's behaviour reach it: log_printf() asserts that nothing
# logs before log_init(), and test_common_refuses_a_symlink_before_the_log_exists
# only catches a regression of that when the assertion is compiled in.
DEBUG ?= n
ifeq ($(DEBUG),y)
CFLAGS += -DDEBUG
endif

all: $(TESTS)

# What a binary or an object here is made of, and nothing else, decides whether
# it is out of date. Three things do, and none of them was tracked.
#
# This Makefile: the -Wl,--wrap= list a test links with, and the -D flags an
# object is compiled with, are written nowhere else. Without this prerequisite
# make finds them up to date after such a change and the run exercises the
# previous build - a wrap that is no longer linked, or a path that no longer
# points where the recipe says. What that looks like is cmocka's "No entries for
# symbol", or a real function running where a mock was expected, or ten fixture
# cases failing against a tree the reader no longer reads: a failure that looks
# like the code under test rather than like the build.
#
# The headers in this directory. The test .c files are handed straight to the link
# recipes and never become objects, so what they include is recorded against the
# binary rather than against an object - see $(TESTS_DEPS) below, which is what
# does that now. This wildcard is the belt beside it: test.h is included by every
# case, and test_acpi.h, test_cap.h, test_iordt.h and test_lock.h carry the
# fixtures their own cases are built on, so a header added later is covered by the
# next invocation without anybody remembering either line.
#
# And the flags that do not come from this file at all - DEBUG arrives on the
# command line - which is what flags-stamp.mk records.
#
# The cost is that editing any of the three rebuilds everything here, which takes
# seconds and is the answer to a question nobody should have to ask.
FLAGS_STAMP_DIR = $(OBJ_DIR)
include ../flags-stamp.mk

# and on the mock library they link. With SHARED=y that library is loaded at run
# time and a rebuilt one is picked up by itself; with SHARED=n it is an archive
# whose contents are copied into each binary, so a mock rebuilt after a flag
# change left every binary running the code the previous flags produced. Named
# by wildcard because which of the two exists is the mock build's choice, made
# on its own command line - and as a prerequisite only: the link lines below name
# the library with -lpqosmock and must not be handed a second copy of it.
# and the headers the test .c files reach through -I. Those files have recorded
# dependencies of their own now - $(TESTS_DEPS) - so this is a second belt: by
# directory rather than by name, for the reason the wildcard above is one, and it
# covers a test source whose .d file has not been written yet.
$(TESTS) $(LIB_OBJS): Makefile $(wildcard *.h) $(FLAGS_STAMP) \
    $(wildcard $(LIB_DIR)/*.h) $(wildcard $(MOCK_DIR)/*.h) \
    $(wildcard $(MOCK_DIR)/libpqosmock.*)

# the recorded dependencies follow the flags that produced them: a .d file lists
# the headers the -I set in force at the time reached, so a change to that set
# has to regenerate them - or the objects rebuild once and the next edit to a
# header only the new search path finds rebuilds nothing.
$(LIB_DEPS) $(TESTS_DEPS): Makefile $(FLAGS_STAMP)

# and they are read - see depfiles.mk, which is where the conditions on that are
# written down. Included here rather than at the top so that the default goal is
# still "all".
DEPFILES_TO_READ = $(LIB_DEPS) $(TESTS_DEPS) $(OBJ_DIR)/cxl_fixture.d
include ../depfiles.mk


# mkdir in every one of these, as the dependency rule below has one: make reads
# the directory listing once and a directory that clean removed in the same
# invocation still looks present to it, so an order-only prerequisite on the
# directory is not made and "make clean all" reached the first object recipe with
# nowhere to put it - "Fatal error: can't create obj/acpi.o".
$(OBJ_DIR)/%.o: $(LIB_DIR)/%.c $(OBJ_DIR)/%.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/allocation.o: $(LIB_DIR)/allocation.c $(OBJ_DIR)/allocation.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=hw_alloc_assoc_write --weaken-symbol=hw_alloc_assoc_write \
		--globalize-symbol=hw_alloc_assoc_read --weaken-symbol=hw_alloc_assoc_read \
		--globalize-symbol=hw_alloc_reset_clos --weaken-symbol=hw_alloc_reset_clos \
		--globalize-symbol=hw_alloc_reset_l2cdp --weaken-symbol=hw_alloc_reset_l2cdp \
		--globalize-symbol=hw_alloc_reset_l3cdp --weaken-symbol=hw_alloc_reset_l3cdp \
		--globalize-symbol=hw_alloc_reset_l3iordt --weaken-symbol=hw_alloc_reset_l3iordt \
		--globalize-symbol=hw_alloc_reset_assoc --weaken-symbol=hw_alloc_reset_assoc \
		--globalize-symbol=hw_alloc_reset_assoc_cores --weaken-symbol=hw_alloc_reset_assoc_cores \
		--globalize-symbol=hw_alloc_reset_assoc_channels --weaken-symbol=hw_alloc_reset_assoc_channels \
		--globalize-symbol=hw_alloc_assoc_unused --weaken-symbol=hw_alloc_assoc_unused \
		--globalize-symbol=hw_l3ca_set --weaken-symbol=hw_l3ca_set \
		--globalize-symbol=hw_l3ca_set --weaken-symbol=hw_l3ca_set \
		--globalize-symbol=hw_l3ca_get --weaken-symbol=hw_l3ca_get \
		--globalize-symbol=hw_l2ca_set --weaken-symbol=hw_l2ca_set \
		--globalize-symbol=hw_l2ca_get --weaken-symbol=hw_l2ca_get \
		--globalize-symbol=hw_alloc_assoc_set_channel --weaken-symbol=hw_alloc_assoc_set_channel \
		--globalize-symbol=hw_alloc_assoc_get_channel --weaken-symbol=hw_alloc_assoc_get_channel \
		$@

$(OBJ_DIR)/common.o: $(LIB_DIR)/common.c $(OBJ_DIR)/common.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=pqos_fopen --weaken-symbol=pqos_fopen \
		--globalize-symbol=pqos_fclose --weaken-symbol=pqos_fclose \
		$@

$(OBJ_DIR)/cap.o: $(LIB_DIR)/cap.c $(OBJ_DIR)/cap.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=_pqos_check_init --weaken-symbol=_pqos_check_init \
		--globalize-symbol=discover_interface --weaken-symbol=discover_interface \
		--globalize-symbol=discover_capabilities --weaken-symbol=discover_capabilities \
		$@

$(OBJ_DIR)/hw_monitoring.o: $(LIB_DIR)/hw_monitoring.c $(OBJ_DIR)/hw_monitoring.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=hw_mon_assoc_write --weaken-symbol=hw_mon_assoc_write \
		--globalize-symbol=hw_mon_assoc_read --weaken-symbol=hw_mon_assoc_read \
		--globalize-symbol=hw_mon_assoc_unused --weaken-symbol=hw_mon_assoc_unused \
		--globalize-symbol=hw_mon_read --weaken-symbol=hw_mon_read \
		--globalize-symbol=hw_mon_start_perf --weaken-symbol=hw_mon_start_perf \
		--globalize-symbol=hw_mon_stop_perf --weaken-symbol=hw_mon_stop_perf \
		--globalize-symbol=hw_mon_start_counter --weaken-symbol=hw_mon_start_counter \
		--globalize-symbol=hw_mon_read_counter --weaken-symbol=hw_mon_read_counter \
		--globalize-symbol=mon_reset_iordt --weaken-symbol=mon_reset_iordt \
		$@

$(OBJ_DIR)/common_monitoring.o: $(LIB_DIR)/common_monitoring.c $(OBJ_DIR)/common_monitoring.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=mon_reset_iordt --weaken-symbol=mon_reset_iordt \
		$@

$(OBJ_DIR)/hw_cap.o: $(LIB_DIR)/hw_cap.c $(OBJ_DIR)/hw_cap.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=hw_cap_l3ca_cdp --weaken-symbol=hw_cap_l3ca_cdp \
		--globalize-symbol=hw_cap_l3ca_iordt --weaken-symbol=hw_cap_l3ca_iordt \
		--globalize-symbol=hw_cap_l2ca_cdp --weaken-symbol=hw_cap_l2ca_cdp \
		--globalize-symbol=hw_cap_mon_snc_state --weaken-symbol=hw_cap_mon_snc_state \
		$@

$(OBJ_DIR)/resctrl_alloc.o: $(LIB_DIR)/resctrl_alloc.c $(OBJ_DIR)/resctrl_alloc.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=resctrl_alloc_fopen --weaken-symbol=resctrl_alloc_fopen \
		--globalize-symbol=resctrl_alloc_fclose --weaken-symbol=resctrl_alloc_fclose \
		$@

$(OBJ_DIR)/os_allocation.o: $(LIB_DIR)/os_allocation.c $(OBJ_DIR)/os_allocation.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=os_alloc_assoc_set --weaken-symbol=os_alloc_assoc_set \
		--globalize-symbol=os_alloc_assoc_get --weaken-symbol=os_alloc_assoc_get \
		--globalize-symbol=os_alloc_assoc_set_pid --weaken-symbol=os_alloc_assoc_set_pid \
		--globalize-symbol=os_alloc_assoc_get_pid --weaken-symbol=os_alloc_assoc_get_pid \
		--globalize-symbol=os_alloc_reset_cores --weaken-symbol=os_alloc_reset_cores \
		--globalize-symbol=os_alloc_reset_schematas --weaken-symbol=os_alloc_reset_schematas \
		--globalize-symbol=os_alloc_reset_tasks --weaken-symbol=os_alloc_reset_tasks \
		--globalize-symbol=os_alloc_mount --weaken-symbol=os_alloc_mount \
		$@

$(OBJ_DIR)/os_cap.o: $(LIB_DIR)/os_cap.c $(OBJ_DIR)/os_cap.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=os_cap_mon_resctrl_support --weaken-symbol=os_cap_mon_resctrl_support \
		--globalize-symbol=os_cap_mon_perf_support --weaken-symbol=os_cap_mon_perf_support \
		$@

$(OBJ_DIR)/resctrl_monitoring.o: $(LIB_DIR)/resctrl_monitoring.c $(OBJ_DIR)/resctrl_monitoring.d
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=resctrl_mon_is_supported --weaken-symbol=resctrl_mon_is_supported \
		--globalize-symbol=resctrl_mon_is_event_supported --weaken-symbol=resctrl_mon_is_event_supported \
		--globalize-symbol=resctrl_mon_cpumask_read --weaken-symbol=resctrl_mon_cpumask_read \
		--globalize-symbol=resctrl_mon_cpumask_write --weaken-symbol=resctrl_mon_cpumask_write \
		--globalize-symbol=resctrl_mon_assoc_set --weaken-symbol=resctrl_mon_assoc_set \
		--globalize-symbol=resctrl_mon_assoc_set_pid --weaken-symbol=resctrl_mon_assoc_set_pid \
		--globalize-symbol=resctrl_mon_new_group --weaken-symbol=resctrl_mon_new_group \
		--globalize-symbol=resctrl_mon_mkdir --weaken-symbol=resctrl_mon_mkdir \
		--globalize-symbol=resctrl_mon_rmdir --weaken-symbol=resctrl_mon_rmdir \
		$@

# -MT, because -MM writes the target as a bare "log.o" with no directory in it,
# and the objects here are $(OBJ_DIR)/log.o: the rules in a .d file written
# without it name a target this Makefile never builds, so the prerequisites in
# them attach to nothing.
#
# And the .d file is a target too, of the same headers: otherwise a header that
# stops including another rebuilds the object but not this file, which goes on
# naming the other - and once that is deleted, -MP's empty rule for it is
# always out of date, so everything listing it rebuilds on every invocation.
$(LIB_DEPS): $(OBJ_DIR)/%.d : $(LIB_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) -MM -MP -MT $(@:.d=.o) -MT $@ -MF $@ $(CFLAGS) $<

# and the same for the test sources, whose target is the binary: these are
# compiled inside the link recipes and never become objects, so what the
# recorded dependencies belong to is bin/test_lock and not an obj/test_lock.o
# that does not exist.
#
# The wildcard above covers the headers beside them while those headers are
# there - and a header that is *removed* leaves it, so nothing said the binaries
# that include it were out of date and they kept running. With -MP the .d file
# names the removed header as a phony target, which make remakes and so rebuilds
# what depends on it - a compile that then reports the missing include, which is
# the answer the reader wants.
$(TESTS_DEPS): $(OBJ_DIR)/%.d : %.c
	mkdir -p $(OBJ_DIR)
	$(CC) -MM -MP -MT $(BIN_DIR)/$* -MT $@ -MF $@ $(CFLAGS) $<

# the fixture object's, with the flags it is compiled with: the same source
# reaches a different path through -DCXL_BUS_PATH, so what it includes is its own
# question
$(OBJ_DIR)/cxl_fixture.d: $(LIB_DIR)/cxl.c Makefile $(FLAGS_STAMP)
	mkdir -p $(OBJ_DIR)
	$(CC) -MM -MP -MT $(@:.d=.o) -MT $@ -MF $@ $(CFLAGS) \
		-UCXL_BUS_PATH -DCXL_BUS_PATH=\"bus/cxl\" $<

# each binary waits for its own, which the pattern above cannot say: these rules
# carry no recipe and only add a prerequisite.
$(TESTS): $(BIN_DIR)/% : $(OBJ_DIR)/%.d

%.o: %.c %.d

%.d: %.c
	$(CC) -MM -MP -MF $@ $(CFLAGS) $<

$(BIN_DIR)/test_api: test_api.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=_pqos_check_init \
		-Wl,--wrap=lock_get \
		-Wl,--wrap=lock_release \
		-Wl,--wrap=hw_alloc_assoc_set \
		-Wl,--wrap=os_alloc_assoc_set \
		-Wl,--wrap=hw_alloc_assoc_get \
		-Wl,--wrap=os_alloc_assoc_get \
		-Wl,--wrap=os_alloc_assoc_set_pid \
		-Wl,--wrap=os_alloc_assoc_get_pid \
		-Wl,--wrap=hw_alloc_assoc_set \
		-Wl,--wrap=os_alloc_assoc_set \
		-Wl,--wrap=hw_alloc_assign \
		-Wl,--wrap=os_alloc_assign \
		-Wl,--wrap=hw_alloc_release \
		-Wl,--wrap=os_alloc_release \
		-Wl,--wrap=os_alloc_assign_pid \
		-Wl,--wrap=os_alloc_release_pid \
		-Wl,--wrap=hw_alloc_reset \
		-Wl,--wrap=os_alloc_reset \
		-Wl,--wrap=os_pid_get_pid_assoc \
		-Wl,--wrap=hw_l3ca_set \
		-Wl,--wrap=os_l3ca_set \
		-Wl,--wrap=hw_l3ca_get \
		-Wl,--wrap=os_l3ca_get \
		-Wl,--wrap=hw_l3ca_get_min_cbm_bits \
		-Wl,--wrap=os_l3ca_get_min_cbm_bits \
		-Wl,--wrap=hw_l2ca_set \
		-Wl,--wrap=os_l2ca_set \
		-Wl,--wrap=hw_l2ca_get \
		-Wl,--wrap=os_l2ca_get \
		-Wl,--wrap=hw_l2ca_get_min_cbm_bits \
		-Wl,--wrap=os_l2ca_get_min_cbm_bits \
		-Wl,--wrap=hw_mba_set \
		-Wl,--wrap=os_mba_set \
		-Wl,--wrap=hw_mba_get \
		-Wl,--wrap=os_mba_get \
		-Wl,--wrap=cpuinfo_get_config \
		-Wl,--wrap=hw_mon_reset \
		-Wl,--wrap=os_mon_reset \
		-Wl,--wrap=hw_mon_assoc_get_core \
		-Wl,--wrap=hw_mon_assoc_get_channel \
		-Wl,--wrap=hw_mon_start_cores \
		-Wl,--wrap=os_mon_start_cores \
		-Wl,--wrap=hw_mon_start_channels \
		-Wl,--wrap=hw_mon_stop \
		-Wl,--wrap=os_mon_stop \
		-Wl,--wrap=pqos_mon_poll_events \
		-Wl,--wrap=os_mon_start_pids \
		-Wl,--wrap=os_mon_add_pids \
		-Wl,--wrap=os_mon_remove_pids \
		-Wl,--wrap=hw_mon_start_uncore \
		-Wl,--wrap=hw_alloc_assoc_get_channel \
		-Wl,--wrap=hw_alloc_assoc_get_dev \
		-Wl,--wrap=hw_alloc_assoc_set_channel \
		-Wl,--wrap=hw_alloc_assoc_set_dev \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_allocation: test_allocation.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=os_alloc_init \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_allocation: test_hw_allocation.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--wrap=cpuinfo_get_config \
		-Wl,--wrap=_pqos_cap_l3cdp_change \
		-Wl,--wrap=_pqos_cap_l2cdp_change \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_assoc_write: test_hw_alloc_assoc_write.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_assoc_read: test_hw_alloc_assoc_read.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_reset_l3cdp: test_hw_alloc_reset_l3cdp.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_reset_l3iordt: test_hw_alloc_reset_l3iordt.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_reset_l2cdp: test_hw_alloc_reset_l2cdp.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_reset_assoc: test_hw_alloc_reset_assoc.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_reset_assoc_cores_channels: test_hw_alloc_reset_assoc_cores_channels.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=_pqos_get_sysconfig \
		-Wl,--wrap=pqos_l3ca_iordt_enabled \
		-Wl,--wrap=iordt_assoc_reset \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_assoc_unused: test_hw_alloc_assoc_unused.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_l3ca_get_min_cbm_bits: test_hw_l3ca_get_min_cbm_bits.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_l2ca_get_min_cbm_bits: test_hw_l2ca_get_min_cbm_bits.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_reset_clos: test_hw_alloc_reset_clos.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_write \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_assign: test_hw_alloc_assign.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_mon_reset_iordt: test_mon_reset_iordt.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--wrap=_pqos_get_cap \
		-Wl,--wrap=_pqos_get_cpu \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_mon_assoc_write: test_hw_mon_assoc_write.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--wrap=perf_mon_init \
		-Wl,--wrap=perf_mon_fini \
		-Wl,--wrap=uncore_mon_discover \
		-Wl,--wrap=uncore_mon_init \
		-Wl,--wrap=uncore_mon_fini \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_mon_assoc_read: test_hw_mon_assoc_read.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--wrap=perf_mon_init \
		-Wl,--wrap=perf_mon_fini \
		-Wl,--wrap=uncore_mon_discover \
		-Wl,--wrap=uncore_mon_init \
		-Wl,--wrap=uncore_mon_fini \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_mon_assoc_unused: test_hw_mon_assoc_unused.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=perf_mon_init \
		-Wl,--wrap=perf_mon_fini \
		-Wl,--wrap=uncore_mon_discover \
		-Wl,--wrap=uncore_mon_init \
		-Wl,--wrap=uncore_mon_fini \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_mon_read: test_hw_mon_read.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_mon_read_counter: test_hw_mon_read_counter.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=perf_mon_init \
		-Wl,--wrap=perf_mon_fini \
		-Wl,--wrap=uncore_mon_discover \
		-Wl,--wrap=uncore_mon_init \
		-Wl,--wrap=uncore_mon_fini \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_mon_start_perf: test_hw_mon_start_perf.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=perf_mon_init \
		-Wl,--wrap=perf_mon_fini \
		-Wl,--wrap=perf_mon_is_event_supported \
		-Wl,--wrap=perf_mon_start \
		-Wl,--wrap=perf_mon_stop \
		-Wl,--wrap=uncore_mon_discover \
		-Wl,--wrap=uncore_mon_init \
		-Wl,--wrap=uncore_mon_fini \
		-Wl,--wrap=uncore_mon_is_event_supported \
		-Wl,--wrap=uncore_mon_start \
		-Wl,--wrap=uncore_mon_stop \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_mon_start_counter: test_hw_mon_start_counter.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=perf_mon_init \
		-Wl,--wrap=perf_mon_fini \
		-Wl,--wrap=uncore_mon_discover \
		-Wl,--wrap=uncore_mon_init \
		-Wl,--wrap=uncore_mon_fini \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_monitoring: test_hw_monitoring.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=perf_mon_init \
		-Wl,--wrap=perf_mon_fini \
		-Wl,--wrap=perf_mon_is_event_supported \
		-Wl,--wrap=perf_mon_start \
		-Wl,--wrap=perf_mon_stop \
		-Wl,--wrap=iordt_mon_assoc_write \
		-Wl,--wrap=iordt_mon_assoc_read \
		-Wl,--wrap=iordt_get_numa \
		-Wl,--wrap=iordt_mon_assoc_reset \
		-Wl,--wrap=uncore_mon_discover \
		-Wl,--wrap=uncore_mon_init \
		-Wl,--wrap=uncore_mon_fini \
		-Wl,--wrap=uncore_mon_is_event_supported \
		-Wl,--wrap=uncore_mon_start \
		-Wl,--wrap=uncore_mon_stop \
		-Wl,--wrap=mon_assoc_get_core \
		-Wl,--wrap=mon_reset \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_cap: test_hw_cap.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=lcpuid \
		-Wl,--wrap=msr_read \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=uncore_mon_discover \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hybrid: test_hybrid.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=sched_getaffinity \
		-Wl,--wrap=sched_setaffinity \
		-Wl,--wrap=__sched_cpualloc \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hybrid_discover: test_hybrid_discover.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=lcpuid \
		-Wl,--wrap=os_cpuinfo_topology \
		-Wl,--wrap=sched_getaffinity \
		-Wl,--wrap=sched_setaffinity \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_cap_l3ca_cdp: test_hw_cap_l3ca_cdp.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_read \
		-Wl,--wrap=msr_write \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_cap_l3ca_iordt: test_hw_cap_l3ca_iordt.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=msr_read \
		-Wl,--wrap=msr_write \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_cap_l2ca_cdp: test_hw_cap_l2ca_cdp.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=msr_read \
		-Wl,--wrap=msr_write \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_resctrl: test_resctrl.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=mount \
		-Wl,--wrap=umount2 \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_resctrl_schemata: test_resctrl_schemata.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_resctrl_alloc: test_resctrl_alloc.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=setvbuf \
		-Wl,--wrap=kill \
		-Wl,--wrap=free \
		-Wl,--wrap=scandir \
		-Wl,--wrap=pqos_file_exists \
		-Wl,--wrap=pqos_fread_uint64 \
		-Wl,--wrap=resctrl_cpumask_write \
		-Wl,--wrap=resctrl_cpumask_read \
		-Wl,--wrap=resctrl_cpumask_set \
		-Wl,--wrap=resctrl_schemata_read \
		-Wl,--wrap=resctrl_schemata_l3ca_write \
		-Wl,--wrap=resctrl_schemata_l2ca_write \
		-Wl,--wrap=resctrl_schemata_mba_write \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_resctrl_mon_init: test_resctrl_mon_init.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=pqos_fopen \
		-Wl,--wrap=pqos_fclose \
		-Wl,--wrap=pqos_dir_exists \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_resctrl_mon_cpumask: test_resctrl_mon_cpumask.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=pqos_fopen \
		-Wl,--wrap=pqos_fclose \
		-Wl,--wrap=resctrl_cpumask_write \
		-Wl,--wrap=resctrl_cpumask_read \
		-Wl,--wrap=resctrl_cpumask_set \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_resctrl_mon_assoc: test_resctrl_mon_assoc.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=resctrl_alloc_get_grps_num \
		-Wl,--wrap=resctrl_alloc_assoc_get \
		-Wl,--wrap=resctrl_alloc_assoc_get_pid \
		-Wl,--wrap=resctrl_cpumask_get \
		-Wl,--wrap=resctrl_cpumask_set \
		-Wl,--wrap=scandir \
		-Wl,--wrap=pqos_fopen \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_resctrl_mon_start: test_resctrl_mon_start.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=scandir \
		-Wl,--wrap=pqos_fopen \
		-Wl,--wrap=pqos_fclose \
		-Wl,--wrap=pqos_dir_exists \
		-Wl,--wrap=resctrl_mon_cpumask_write \
		-Wl,--wrap=resctrl_mon_cpumask_read \
		-Wl,--wrap=resctrl_cpumask_unset \
		-Wl,--wrap=resctrl_cpumask_set \
		-Wl,--wrap=resctrl_cpumask_get \
		-Wl,--wrap=resctrl_alloc_task_validate \
		-Wl,--wrap=resctrl_alloc_get_grps_num \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_resctrl_mon: test_resctrl_mon.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=mkdir \
		-Wl,--wrap=rmdir \
		-Wl,--wrap=pqos_fopen \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_aet_telemetry: test_aet_telemetry.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=_pqos_check_init \
		-Wl,--wrap=lock_get \
		-Wl,--wrap=lock_release \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_resctrl_aet_init: test_resctrl_aet_init.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=pqos_dir_exists \
		-Wl,--wrap=pqos_fopen \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_os_alloc_mount: test_os_alloc_mount.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=resctrl_mount \
		-Wl,--wrap=resctrl_umount \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_os_allocation: test_os_allocation.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=resctrl_mount \
		-Wl,--wrap=resctrl_umount \
		-Wl,--wrap=resctrl_lock_exclusive \
		-Wl,--wrap=resctrl_lock_release \
		-Wl,--wrap=resctrl_lock_shared \
		-Wl,--wrap=resctrl_alloc_init \
		-Wl,--wrap=resctrl_alloc_fini \
		-Wl,--wrap=resctrl_alloc_assoc_set \
		-Wl,--wrap=resctrl_alloc_assoc_get \
		-Wl,--wrap=resctrl_alloc_assoc_get_pid \
		-Wl,--wrap=resctrl_alloc_get_unused_group \
		-Wl,--wrap=resctrl_alloc_cpumask_read \
		-Wl,--wrap=resctrl_alloc_cpumask_write \
		-Wl,--wrap=resctrl_alloc_schemata_read \
		-Wl,--wrap=resctrl_alloc_schemata_write \
		-Wl,--wrap=resctrl_alloc_task_write \
		-Wl,--wrap=resctrl_cpumask_set \
		-Wl,--wrap=resctrl_schemata_l3ca_set \
		-Wl,--wrap=resctrl_schemata_l3ca_get \
		-Wl,--wrap=resctrl_schemata_l2ca_set \
		-Wl,--wrap=resctrl_schemata_l2ca_get \
		-Wl,--wrap=resctrl_schemata_mba_set \
		-Wl,--wrap=resctrl_schemata_mba_get \
		-Wl,--wrap=_pqos_cap_l3cdp_change \
		-Wl,--wrap=_pqos_cap_l2cdp_change \
		-Wl,--wrap=_pqos_cap_mba_change \
		-Wl,--wrap=mkdir \
		-Wl,--wrap=pqos_file_exists \
		-Wl,--wrap=pqos_dir_exists \
		-Wl,--wrap=resctrl_mon_active \
		-Wl,--wrap=pqos_fread_uint64 \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_os_alloc_assoc_set: test_os_alloc_assoc_set.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=resctrl_lock_exclusive \
		-Wl,--wrap=resctrl_lock_release \
		-Wl,--wrap=resctrl_lock_shared \
		-Wl,--wrap=resctrl_alloc_assoc_set \
		-Wl,--wrap=resctrl_mon_assoc_set \
		-Wl,--wrap=resctrl_mon_assoc_get \
		-Wl,--wrap=resctrl_mon_assoc_get_pid \
		-Wl,--wrap=resctrl_mon_assoc_set_pid \
		-Wl,--wrap=resctrl_alloc_assoc_set_pid \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_os_alloc_reset: test_os_alloc_reset.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=resctrl_mount \
		-Wl,--wrap=resctrl_umount \
		-Wl,--wrap=resctrl_lock_exclusive \
		-Wl,--wrap=resctrl_lock_release \
		-Wl,--wrap=resctrl_lock_shared \
		-Wl,--wrap=resctrl_cpumask_set \
		-Wl,--wrap=resctrl_alloc_cpumask_read \
		-Wl,--wrap=resctrl_alloc_cpumask_write \
		-Wl,--wrap=resctrl_alloc_schemata_write \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_os_cap: test_os_cap.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=resctrl_mount \
		-Wl,--wrap=resctrl_umount \
		-Wl,--wrap=resctrl_lock_exclusive \
		-Wl,--wrap=resctrl_lock_release \
		-Wl,--wrap=resctrl_lock_shared \
		-Wl,--wrap=pqos_dir_exists \
		-Wl,--wrap=pqos_file_exists \
		-Wl,--wrap=pqos_file_contains \
		-Wl,--wrap=pqos_fread_uint64 \
		-Wl,--wrap=resctrl_alloc_get_num_closids \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_os_cap_mon: test_os_cap_mon.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=pqos_dir_exists \
		-Wl,--wrap=pqos_file_exists \
		-Wl,--wrap=pqos_file_contains \
		-Wl,--wrap=pqos_fopen \
		-Wl,--wrap=pqos_fread_uint64 \
		-Wl,--wrap=resctrl_mount \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_iordt: test_iordt.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=acpi_init \
		-Wl,--wrap=acpi_fini \
		-Wl,--wrap=acpi_free \
		-Wl,--wrap=acpi_get_sig \
		-Wl,--wrap=acpi_print \
		-Wl,--wrap=pci_init \
		-Wl,--wrap=pci_fini \
		-Wl,--wrap=pci_dev_get \
		-Wl,--wrap=pci_dev_release \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_mrrm: test_mrrm.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=acpi_init -Wl,--wrap=acpi_fini \
		-Wl,--wrap=acpi_free -Wl,--wrap=acpi_get_sig \
		-Wl,--wrap=acpi_print \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

# The reader's bus path is a compile time constant, so this binary gets a copy of
# the module compiled against the fixture's tree instead of the path every other
# binary is built with. The shared object is filtered out of the link for that
# reason - two definitions of cxl_devices_read() would not resolve. The path is
# relative: test_cxl.c makes a directory of its own with mkdtemp() and changes
# into it, so two runs of this binary cannot touch each other's fixture.
#
# This Makefile is a prerequisite because the path above is only written here:
# nothing under $(LIB_DIR) changes when it changes, so without this an existing
# object stays up to date and the cases keep running against the tree the old
# path names. That is not hypothetical - it is what happened when the path
# became relative, and the ten fixture cases failed until the object was
# deleted by hand.
# Its own .d file and not cxl.o's: a .d file names the object its prerequisites
# belong to, so depending on cxl.d makes this object wait for that file to be
# written and inherits nothing from inside it - an edit to cxl.h rebuilt cxl.o and
# left this one, which test_cxl then linked.
$(OBJ_DIR)/cxl_fixture.o: $(LIB_DIR)/cxl.c $(OBJ_DIR)/cxl_fixture.d Makefile
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -UCXL_BUS_PATH -DCXL_BUS_PATH=\"bus/cxl\" \
		-c $< -o $@

$(BIN_DIR)/test_cxl: test_cxl.c $(filter-out $(OBJ_DIR)/cxl.o,$(LIB_OBJS)) \
		$(OBJ_DIR)/cxl_fixture.o
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--start-group \
		$(LDFLAGS) $(filter-out $(OBJ_DIR)/cxl.o,$(LIB_OBJS)) \
		$(OBJ_DIR)/cxl_fixture.o $< -Wl,--end-group -o $@

$(BIN_DIR)/test_mem_regions: test_mem_regions.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=acpi_free -Wl,--wrap=acpi_get_sig \
		-Wl,--wrap=pqos_file_exists -Wl,--wrap=pqos_dir_exists \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_erdt: test_erdt.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=acpi_init -Wl,--wrap=acpi_fini \
		-Wl,--wrap=acpi_free -Wl,--wrap=acpi_get_sig \
		-Wl,--wrap=acpi_print \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_iordt_assoc: test_iordt_assoc.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=acpi_init \
		-Wl,--wrap=acpi_fini \
		-Wl,--wrap=acpi_free \
		-Wl,--wrap=acpi_get_sig \
		-Wl,--wrap=pci_init \
		-Wl,--wrap=pci_fini \
		-Wl,--wrap=pci_dev_get \
		-Wl,--wrap=pci_dev_release \
		-Wl,--wrap=pqos_mmap_read \
		-Wl,--wrap=pqos_mmap_write \
		-Wl,--wrap=pqos_munmap \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_mmio: test_mmio.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=pqos_mmap_read \
		-Wl,--wrap=pqos_munmap \
		-Wl,--wrap=_pqos_get_erdt \
		-Wl,--wrap=log_printf \
		-Wl,--wrap=_pqos_get_mrrm \
		-Wl,--wrap=get_mba_optimal_bw_region_clos_v1 \
		-Wl,--wrap=get_mba_min_bw_region_clos_v1 \
		-Wl,--wrap=get_mba_max_bw_region_clos_v1 \
		-Wl,--wrap=_pqos_get_channels_domains \
		-Wl,--wrap=get_total_iol3_mbm_rmid_range_v1 \
		-Wl,--wrap=get_miss_iol3_mbm_rmid_range_v1 \
		-Wl,--wrap=set_mba_optimal_bw_region_clos_v1 \
		-Wl,--wrap=set_mba_min_bw_region_clos_v1 \
		-Wl,--wrap=set_mba_max_bw_region_clos_v1 \
		-Wl,--wrap=set_iol3_cbm_clos_v1 \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_mmio_dump: test_mmio_dump.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=pqos_mmap_write \
		-Wl,--wrap=pqos_munmap \
		-Wl,--wrap=_pqos_get_erdt \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_mmio_dump_rmids: test_mmio_dump_rmids.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=_pqos_get_erdt \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_acpi: test_acpi.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=munmap \
		-Wl,--wrap=free \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_hw_alloc_assoc_channel: test_hw_alloc_assoc_channel.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=_pqos_get_sysconfig \
		-Wl,--wrap=pqos_l3ca_iordt_enabled \
		-Wl,--wrap=iordt_assoc_read \
		-Wl,--wrap=iordt_assoc_write \
		-Wl,--start-group \
		-Wl,--wrap=_pqos_get_cap \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_cap: test_cap.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=os_cap_init \
		-Wl,--wrap=machine_init \
		-Wl,--wrap=machine_fini \
		-Wl,--wrap=cpuinfo_init \
		-Wl,--wrap=cpuinfo_fini \
		-Wl,--wrap=iordt_init \
		-Wl,--wrap=iordt_fini \
		-Wl,--wrap=hw_cap_mon_discover \
		-Wl,--wrap=os_cap_mon_discover \
		-Wl,--wrap=hybrid_cap_discover \
		-Wl,--wrap=hw_cap_l3ca_discover \
		-Wl,--wrap=os_cap_l3ca_discover \
		-Wl,--wrap=hw_cap_l2ca_discover \
		-Wl,--wrap=os_cap_l2ca_discover \
		-Wl,--wrap=hw_cap_mba_discover \
		-Wl,--wrap=os_cap_mba_discover \
		-Wl,--wrap=pqos_mon_init \
		-Wl,--wrap=pqos_mon_fini \
		-Wl,--wrap=pqos_alloc_init \
		-Wl,--wrap=pqos_alloc_fini \
		-Wl,--wrap=lock_init \
		-Wl,--wrap=lock_fini \
		-Wl,--wrap=lock_get \
		-Wl,--wrap=lock_release \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_cap_static: ./test_cap_static.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=getenv \
		-Wl,--wrap=access \
		-Wl,--wrap=malloc \
		-Wl,--wrap=hw_cap_l3ca_discover \
		-Wl,--wrap=os_cap_l3ca_discover \
		-Wl,--wrap=hw_cap_l2ca_discover \
		-Wl,--wrap=os_cap_l2ca_discover \
		-Wl,--wrap=hw_cap_mba_discover \
		-Wl,--wrap=amd_cap_mba_discover \
		-Wl,--wrap=os_cap_mba_discover \
		-Wl,--wrap=hw_cap_mon_discover \
		-Wl,--wrap=os_cap_mon_discover \
		-Wl,--wrap=hybrid_cap_discover \
		-Wl,--wrap=os_cap_get_mba_ctrl \
		-Wl,--wrap=pqos_cap_get_type \
		-Wl,--wrap=api_init \
		-Wl,--wrap=pqos_alloc_init \
		-Wl,--wrap=pqos_alloc_fini \
		-Wl,--wrap=pqos_utils_init \
		-Wl,--wrap=pqos_mon_init \
		-Wl,--wrap=pqos_mon_fini \
		-Wl,--wrap=os_cap_init \
		-Wl,--wrap=cpuinfo_init \
		-Wl,--wrap=cpuinfo_fini \
		-Wl,--wrap=log_init \
		-Wl,--wrap=log_fini \
		-Wl,--wrap=log_wait_quiescent \
		-Wl,--wrap=log_printf \
		-Wl,--wrap=resctrl_is_supported \
		-Wl,--wrap=machine_init \
		-Wl,--wrap=machine_fini \
		-Wl,--wrap=lock_init \
		-Wl,--wrap=lock_fini \
		-Wl,--wrap=lock_get \
		-Wl,--wrap=lock_release \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_lock: test_lock.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=open \
		-Wl,--wrap=close \
		-Wl,--wrap=lockf \
		-Wl,--wrap=fprintf \
		-Wl,--wrap=access \
		-Wl,--wrap=unlink \
		-Wl,--wrap=pthread_mutex_init \
		-Wl,--wrap=pthread_mutex_destroy \
		-Wl,--wrap=pthread_mutex_lock \
		-Wl,--wrap=pthread_mutex_unlock \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_pqos_inter_get: test_pqos_inter_get.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=lock_init \
		-Wl,--wrap=lock_fini \
		-Wl,--wrap=lock_get \
		-Wl,--wrap=lock_release \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_common: ./test_common.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=getline \
		-Wl,--wrap=log_printf \
		-Wl,--wrap=log_printf_if_init \
		-Wl,--wrap=lstat \
		-Wl,--wrap=open \
		-Wl,--wrap=fdopen \
		-Wl,--wrap=fopen \
		-Wl,--wrap=fgets \
		-Wl,--wrap=fread \
		-Wl,--wrap=fclose \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_common_fread_uint64: ./test_common_fread_uint64.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=pqos_fopen \
		-Wl,--wrap=pqos_close \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_log: ./test_log.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_mba_ids: ./test_utils_pqos_cpu_get_mba_ids.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_sockets: ./test_utils_pqos_cpu_get_sockets.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_l3cat_ids: ./test_utils_pqos_cpu_get_l3cat_ids.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_l2ids: ./test_utils_pqos_cpu_get_l2ids.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_cores: ./test_utils_pqos_cpu_get_cores.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_one_core: ./test_utils_pqos_cpu_get_one_core.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_socketid: ./test_utils_pqos_cpu_get_socketid.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_one_by_l2id: ./test_utils_pqos_cpu_get_one_by_l2id.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_one_by_l3cat_id: ./test_utils_pqos_cpu_get_one_by_l3cat_id.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@


$(BIN_DIR)/test_utils_pqos_cpu_get_one_by_mba_id: ./test_utils_pqos_cpu_get_one_by_mba_id.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_clusterid: ./test_utils_pqos_cpu_get_clusterid.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_get_core_info: ./test_utils_pqos_cpu_get_core_info.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils_pqos_cpu_check_core: ./test_utils_pqos_cpu_check_core.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_utils: ./test_utils.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_uncore_monitoring: ./test_uncore_monitoring.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=lcpuid \
		-Wl,--wrap=scandir \
		-Wl,--wrap=_pqos_get_cpu \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_perf_monitoring: ./test_perf_monitoring.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=perf_setup_counter \
		-Wl,--wrap=perf_shutdown_counter \
		-Wl,--wrap=perf_read_counter \
		-Wl,--wrap=perf_mon_get_fd \
		-Wl,--wrap=pqos_file_exists \
		-Wl,--wrap=pqos_fopen \
		-Wl,--wrap=pqos_fclose \
		-Wl,--wrap=scandir \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_os_cpuinfo_cpu: ./test_os_cpuinfo_cpu.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=pqos_fread_uint \
		-Wl,--wrap=scandir \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_os_cpuinfo: ./test_os_cpuinfo.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=scandir \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_os_monitoring: ./test_os_monitoring.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=perf_mon_init \
		-Wl,--wrap=perf_mon_fini \
		-Wl,--wrap=resctrl_mon_init \
		-Wl,--wrap=resctrl_mon_fini \
		-Wl,--wrap=resctrl_mon_reset \
		-Wl,--wrap=resctrl_mon_assoc_get \
		-Wl,--wrap=scandir \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

$(BIN_DIR)/test_os_monitoring_events: ./test_os_monitoring_events.c $(LIB_OBJS)
	mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(WRAP) \
		-Wl,--wrap=perf_mon_is_event_supported \
		-Wl,--wrap=perf_mon_start \
		-Wl,--wrap=perf_mon_stop \
		-Wl,--wrap=resctrl_mon_is_event_supported \
		-Wl,--wrap=resctrl_lock_exclusive \
		-Wl,--wrap=resctrl_mon_start \
		-Wl,--wrap=resctrl_mon_stop \
		-Wl,--wrap=resctrl_lock_release \
		-Wl,--start-group \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

.PHONY: run
run: $(TESTS)
	@echo "Running Library Unit Tests"
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
		LD_LIBRARY_PATH=$(MOCK_DIR) "$$test" || failed="$$failed $$test"; \
	done; \
	if [ -n "$$failed" ]; then \
		printf 'FAILED:%s\n' "$$failed"; \
		exit 1; \
	fi

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
	$(CODESPELL) . --skip $(OBJ_DIR) -q 2 -Lclos

.PHONY: style
style:
	$(MAKE) clang-format
	$(MAKE) checkpatch
	$(MAKE) codespell

clean:
	rm -rf $(BIN_DIR)
	rm -rf $(OBJ_DIR)
