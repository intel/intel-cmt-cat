
LIB_DIR = ../../lib
OBJ_DIR = obj
LIB_SRCS = $(sort $(wildcard $(LIB_DIR)/*.c))
LIB_OBJS = $(LIB_SRCS:$(LIB_DIR)/%.c=$(OBJ_DIR)/%.o)
LIB_DEPS = $(LIB_SRCS:$(LIB_DIR)/%.c=$(OBJ_DIR)/%.d)
MOCK_DIR = ../mock
MOCK_SRCS = $(sort $(wildcard $(MOCK_DIR)*.c))

TESTS_SRCS = $(sort $(wildcard test_*.c))
TESTS = $(TESTS_SRCS:.c=)

LDFLAGS = -L$(MOCK_DIR) -lpthread -lcmocka -lpqosmock -z noexecstack -z relro -z now
CFLAGS = -pthread -I$(LIB_DIR) -I$(MOCK_DIR) \
	-D_GNU_SOURCE \
	-W -Wall -Wextra -Wstrict-prototypes \
	-Wold-style-definition -Wpointer-arith \
	-Wcast-qual -Wundef -Wwrite-strings \
	-Wformat -Wformat-security -fstack-protector \
	-Wunreachable-code -Wsign-compare -Wno-endif-labels

all: $(TESTS)

$(LIB_OBJS): $(OBJ_DIR)/%.o : $(LIB_DIR)/%.c $(OBJ_DIR)/%.d
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB_DEPS): $(OBJ_DIR)/%.d : $(LIB_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) -MM -MP -MF $@ $(CFLAGS) $<

%.o: %.c %.d

%.d: %.c
	$(CC) -MM -MP -MF $@ $(CFLAGS) $<

test_api: test_api.c $(LIB_OBJS)
	$(CC) $(CFLAGS) \
		-Wl,--start-group \
		-Wl,--wrap=_pqos_check_init \
		-Wl,--wrap=_pqos_api_lock \
		-Wl,--wrap=_pqos_api_unlock \
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
		-Wl,--wrap=hw_mon_assoc_get \
		-Wl,--wrap=hw_mon_start \
		-Wl,--wrap=os_mon_start \
		-Wl,--wrap=hw_mon_stop \
		-Wl,--wrap=os_mon_stop \
		-Wl,--wrap=pqos_mon_poll_events \
		-Wl,--wrap=os_mon_start_pids \
		-Wl,--wrap=os_mon_add_pids \
		-Wl,--wrap=os_mon_remove_pids \
		-Wl,--wrap=_pqos_cap_get \
		$(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

test_hw_allocation: test_hw_allocation.c $(LIB_OBJS)
	$(CC) $(CFLAGS) \
		-Wl,--wrap=_pqos_cap_get \
		-Wl,--wrap=msr_write \
		-Wl,--wrap=msr_read \
		-Wl,--start-group $(LDFLAGS) $(LIB_OBJS) $< -Wl,--end-group -o $@

.PHONY: run
run: $(TESTS)
	@echo "Running Library Unit Tests"
	@for test in $(TESTS); do \
		bash -c "LD_LIBRARY_PATH=$(MOCK_DIR) ./$$test" || true; \
	done;

CHECKPATCH?=checkpatch.pl
.PHONY: checkpatch
checkpatch:
	$(CHECKPATCH) --no-tree --no-signoff --emacs \
	--ignore CODE_INDENT,INITIALISED_STATIC,LEADING_SPACE,SPLIT_STRING,\
	NEW_TYPEDEFS,UNSPECIFIED_INT,BLOCK_COMMENT_STYLE,\
	SPDX_LICENSE_TAG,ARRAY_SIZE,EMBEDDED_FUNCTION_NAME,\
	SYMBOLIC_PERMS,CONST_STRUCT \
	-f test_api.c \
	-f test_hw_allocation.c

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
	$(CODESPELL) . --skip $(OBJ_DIR) -q 2

.PHONY: style
style:
	$(MAKE) checkpatch
	$(MAKE) clang-format
	$(MAKE) codespell

clean:
	rm -f $(TESTS)
	rm -rf $(OBJ_DIR)