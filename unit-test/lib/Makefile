
LIB_DIR = ../../lib
OBJ_DIR = ./obj
BIN_DIR = ./bin
LIB_SRCS = $(sort $(wildcard $(LIB_DIR)/*.c))
LIB_OBJS = $(LIB_SRCS:$(LIB_DIR)/%.c=$(OBJ_DIR)/%.o)
LIB_DEPS = $(LIB_SRCS:$(LIB_DIR)/%.c=$(OBJ_DIR)/%.d)

MOCK_DIR = ../mock

TESTS_SRCS = $(sort $(wildcard test_*.c))
TESTS = $(TESTS_SRCS:%.c=$(BIN_DIR)/%)


LDFLAGS = -L$(MOCK_DIR) -lpthread -lcmocka -lpqosmock -z noexecstack -z relro -z now
CFLAGS = -pthread -I$(LIB_DIR) -I$(MOCK_DIR) \
	-D_GNU_SOURCE \
	-W -Wall -Wextra -Wstrict-prototypes \
	-Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector \
	-Wunreachable-code -Wsign-compare -Wno-endif-labels \
	-D'PQOS_LOCAL=' \
	-D'PQOS_API=' \
	-D'PQOS_STATIC=__attribute__((weak))'


IS_GCC = $(shell $(CC) -v 2>&1 | grep -c "^gcc version ")
IS_CLANG = $(shell $(CC) -v 2>&1 | grep -c "^clang version ")

# GCC-only options
ifeq ($(IS_GCC),1)
CFLAGS += -fno-strict-overflow \
	-fno-delete-null-pointer-checks \
	-fwrapv \
	-fstack-protector-strong \
	-fcf-protection=full
endif

# common function wrap
WRAP = \
	-Wl,--wrap=_pqos_get_cap \
	-Wl,--wrap=_pqos_get_cpu \
	-Wl,--wrap=_pqos_get_dev \
	-Wl,--wrap=_pqos_get_inter

CFLAGS += -g -ggdb -O0

all: $(TESTS)

$(OBJ_DIR)/%.o: $(LIB_DIR)/%.c $(OBJ_DIR)/%.d
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/allocation.o: $(LIB_DIR)/allocation.c $(OBJ_DIR)/allocation.d
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=hw_alloc_assoc_write --weaken-symbol=hw_alloc_assoc_write \
		--globalize-symbol=hw_alloc_assoc_read --weaken-symbol=hw_alloc_assoc_read \
		--globalize-symbol=hw_alloc_reset_cos --weaken-symbol=hw_alloc_reset_cos \
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
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=pqos_fopen --weaken-symbol=pqos_fopen \
		--globalize-symbol=pqos_fclose --weaken-symbol=pqos_fclose \
		$@

$(OBJ_DIR)/cap.o: $(LIB_DIR)/cap.c $(OBJ_DIR)/cap.d
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=_pqos_check_init --weaken-symbol=_pqos_check_init \
		--globalize-symbol=discover_interface --weaken-symbol=discover_interface \
		--globalize-symbol=discover_capabilities --weaken-symbol=discover_capabilities \
		$@

$(OBJ_DIR)/hw_monitoring.o: $(LIB_DIR)/hw_monitoring.c $(OBJ_DIR)/hw_monitoring.d
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
		--globalize-symbol=hw_mon_reset_iordt --weaken-symbol=hw_mon_reset_iordt \
		$@

$(OBJ_DIR)/hw_cap.o: $(LIB_DIR)/hw_cap.c $(OBJ_DIR)/hw_cap.d
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=hw_cap_l3ca_cdp --weaken-symbol=hw_cap_l3ca_cdp \
		--globalize-symbol=hw_cap_l3ca_iordt --weaken-symbol=hw_cap_l3ca_iordt \
		--globalize-symbol=hw_cap_l2ca_cdp --weaken-symbol=hw_cap_l2ca_cdp \
		--globalize-symbol=hw_cap_mon_snc_state --weaken-symbol=hw_cap_mon_snc_state \
		$@

$(OBJ_DIR)/resctrl_alloc.o: $(LIB_DIR)/resctrl_alloc.c $(OBJ_DIR)/resctrl_alloc.d
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=resctrl_alloc_fopen --weaken-symbol=resctrl_alloc_fopen \
		--globalize-symbol=resctrl_alloc_fclose --weaken-symbol=resctrl_alloc_fclose \
		$@

$(OBJ_DIR)/os_allocation.o: $(LIB_DIR)/os_allocation.c $(OBJ_DIR)/os_allocation.d
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
	$(CC) $(CFLAGS) -c $< -o $@
	objcopy $@ \
		--globalize-symbol=os_cap_mon_resctrl_support --weaken-symbol=os_cap_mon_resctrl_support \
		--globalize-symbol=os_cap_mon_perf_support --weaken-symbol=os_cap_mon_perf_support \
		$@

$(OBJ_DIR)/resctrl_monitoring.o: $(LIB_DIR)/resctrl_monitoring.c $(OBJ_DIR)/resctrl_monitoring.d
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

$(LIB_DEPS): $(OBJ_DIR)/%.d : $(LIB_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) -MM -MP -MF $@ $(CFLAGS) $<

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

$(BIN_DIR)/test_hw_alloc_reset_cos: test_hw_alloc_reset_cos.c $(LIB_OBJS)
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

$(BIN_DIR)/test_hw_mon_reset_iordt: test_hw_mon_reset_iordt.c $(LIB_OBJS)
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
	@for test in $(TESTS); do \
		bash -c "LD_LIBRARY_PATH=$(MOCK_DIR) ./$$test" || true; \
	done;

CHECKPATCH?=checkpatch.pl
.PHONY: checkpatch
checkpatch:
	$(CHECKPATCH) --no-tree --no-signoff --terse \
	--ignore CODE_INDENT,INITIALISED_STATIC,LEADING_SPACE,SPLIT_STRING,\
	NEW_TYPEDEFS,UNSPECIFIED_INT,BLOCK_COMMENT_STYLE,\
	SPDX_LICENSE_TAG,ARRAY_SIZE,EMBEDDED_FUNCTION_NAME,\
	SYMBOLIC_PERMS,CONST_STRUCT,PREFER_DEFINED_ATTRIBUTE_MACRO \
	--typedefsfile ../../.checkpatch.types \
	--file *.[ch] \

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
