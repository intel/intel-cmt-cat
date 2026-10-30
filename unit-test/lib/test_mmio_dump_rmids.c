/*
 * BSD LICENSE
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mmio_dump_rmids.h"
#include "test.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* the last line the library logged and its level, and how many lines the
 * dump printed, so a case can assert the message, that it is an error the
 * default verbosity shows, and that nothing was dumped before it
 */
static char test_log[512];
static int test_log_type;
static int test_printed;

void
__wrap_log_printf(int type, const char *str, ...)
{
        va_list ap;

        test_log_type = type;
        va_start(ap, str);
        vsnprintf(test_log, sizeof(test_log), str, ap);
        va_end(ap);
}

int
__wrap_printf(const char *format __attribute__((unused)), ...)
{
        test_printed++;
        return 0;
}

int
__wrap_puts(const char *str __attribute__((unused)))
{
        test_printed++;
        return 0;
}

const struct pqos_erdt_info *
__wrap__pqos_get_erdt(void)
{
        return mock_ptr_type(const struct pqos_erdt_info *);
}

const struct pqos_mrrm_info *
__wrap__pqos_get_mrrm(void)
{
        return mock_ptr_type(const struct pqos_mrrm_info *);
}

uint8_t *
__wrap_pqos_mmap_read(uint64_t address, const uint64_t size)
{
        check_expected(address);
        check_expected(size);

        return mock_ptr_type(uint8_t *);
}

void
__wrap_pqos_munmap(void *mem, const uint64_t size)
{
        check_expected_ptr(mem);
        check_expected(size);
}

static void
test_uint64_to_binary(void **state __attribute__((unused)))
{
        char binary[65];
        unsigned i;

        memset(binary, 'x', sizeof(binary));
        uint64_to_binary(0x8000000000000001ULL, binary);

        assert_int_equal(strlen(binary), 64);
        assert_int_equal(binary[0], '1');
        for (i = 1; i < 63; i++)
                assert_int_equal(binary[i], '0');
        assert_int_equal(binary[63], '1');
        assert_int_equal(binary[64], '\0');
}

static void
test_mmio_dump_rmids_null(void **state __attribute__((unused)))
{
        int ret;

        ret = mmio_dump_rmids(NULL);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_mmio_dump_rmids_unknown_domain(void **state __attribute__((unused)))
{
        struct pqos_erdt_info erdt = {0};
        uint16_t domain_id = 1;
        pqos_rmid_t rmid = 1;
        struct pqos_mmio_dump_rmids dump = {.num_domain_ids = 1,
                                            .domain_ids = &domain_id,
                                            .num_rmids = 1,
                                            .rmids = &rmid,
                                            .rmid_type =
                                                MMIO_DUMP_RMID_TYPE_CMT};
        int ret;

        will_return(__wrap__pqos_get_erdt, &erdt);
        ret = mmio_dump_rmids(&dump);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_non_null(strstr(test_log, "Domain ID 1 is not a CPU domain"));
        assert_int_equal(test_log_type, LOG_OPT_ERROR);
}

/* CPU domains 1 and 3, the way a part with two CBBs numbers them, each with
 * an MMRC block of its own, and device domains 16 and 17, each with a CMRD
 * and an IBRD block of its own. Only the cases that are not refused read a
 * register, through the mmap wrapper.
 */
static struct pqos_cpu_agent_info cbb_cpu_agents[] = {
    {.rmdd = {.domain_id = 1},
     .mmrc = {.reg_block_base_addr = 0x1000, .reg_block_size = 1}},
    {.rmdd = {.domain_id = 3},
     .mmrc = {.reg_block_base_addr = 0x3000, .reg_block_size = 1}}};
static struct pqos_device_agent_info cbb_dev_agents[] = {
    {.rmdd = {.domain_id = 16},
     .cmrd = {.reg_base_addr = 0x16000, .reg_block_size = 1, .clump_size = 8},
     .ibrd = {.reg_base_addr = 0x16100000,
              .reg_block_size = 1,
              .miss_bw_reg_offset = 0x100,
              .bw_reg_clump_size = 8,
              .miss_reg_clump_size = 8}},
    {.rmdd = {.domain_id = 17},
     .cmrd = {.reg_base_addr = 0x17000, .reg_block_size = 1, .clump_size = 8},
     .ibrd = {.reg_base_addr = 0x17100000,
              .reg_block_size = 1,
              .miss_bw_reg_offset = 0x100,
              .bw_reg_clump_size = 8,
              .miss_reg_clump_size = 8}}};
static const struct pqos_erdt_info cbb_erdt = {.num_cpu_agents = 2,
                                               .cpu_agents = cbb_cpu_agents,
                                               .num_dev_agents = 2,
                                               .dev_agents = cbb_dev_agents};

/* the I/O RMID types, and for each the block of a device agent it reads */
static const struct {
        enum pqos_mmio_dump_rmid_type type;
        int ibrd;
} io_types[] = {{MMIO_DUMP_RMID_IO_L3, 0},
                {MMIO_DUMP_RMID_IO_TOTAL, 1},
                {MMIO_DUMP_RMID_IO_MISS, 1}};
static const struct pqos_mrrm_info one_region = {.max_memory_regions_supported =
                                                     1};

static void
test_mmio_dump_rmids_unknown_domain_dumps_none(void **state
                                               __attribute__((unused)))
{
        /* 0 is the domain a user tries first, and is not one here; 1, before
         * it, used to be dumped before the dump failed with nothing said
         */
        uint16_t domain_ids[] = {1, 0};
        pqos_rmid_t rmid = 0;
        struct pqos_mmio_dump_rmids dump = {.num_domain_ids = 2,
                                            .domain_ids = domain_ids,
                                            .num_rmids = 1,
                                            .rmids = &rmid,
                                            .rmid_type =
                                                MMIO_DUMP_RMID_TYPE_CMT};
        int ret;

        test_printed = 0;
        will_return(__wrap__pqos_get_erdt, &cbb_erdt);
        ret = mmio_dump_rmids(&dump);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(test_printed, 0);
        assert_non_null(strstr(test_log, "Domain ID 0 is not a CPU domain"));
        assert_int_equal(test_log_type, LOG_OPT_ERROR);
}

static void
test_mmio_dump_rmids_domain_of_the_other_type(void **state
                                              __attribute__((unused)))
{
        /* a CPU domain for I/O RMIDs, which are in the device domains */
        uint16_t domain_id = 3;
        pqos_rmid_t rmid = 0;
        struct pqos_mmio_dump_rmids dump = {.num_domain_ids = 1,
                                            .domain_ids = &domain_id,
                                            .num_rmids = 1,
                                            .rmids = &rmid,
                                            .rmid_type = MMIO_DUMP_RMID_IO_L3};
        int ret;

        test_printed = 0;
        will_return(__wrap__pqos_get_erdt, &cbb_erdt);
        ret = mmio_dump_rmids(&dump);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(test_printed, 0);
        assert_non_null(strstr(test_log, "Domain ID 3 is not a device domain"));
        assert_int_equal(test_log_type, LOG_OPT_ERROR);
}

static void
test_mmio_dump_rmids_mbm_reads_each_cpu_domain(void **state
                                               __attribute__((unused)))
{
        /* MBM RMIDs are in the CPU domains, and each domain is read from its
         * own MMRC block, in the order given
         */
        static uint8_t block[PAGE_SIZE];
        uint16_t domain_ids[] = {1, 3};
        pqos_rmid_t rmid = 0;
        struct pqos_mmio_dump_rmids dump = {.num_domain_ids = DIM(domain_ids),
                                            .domain_ids = domain_ids,
                                            .num_mem_regions = 1,
                                            .region_num = {0},
                                            .num_rmids = 1,
                                            .rmids = &rmid,
                                            .rmid_type =
                                                MMIO_DUMP_RMID_TYPE_MBM};
        int ret;

        test_printed = 0;
        test_log[0] = '\0';
        will_return(__wrap__pqos_get_erdt, &cbb_erdt);
        will_return(__wrap__pqos_get_mrrm, &one_region);
        for (unsigned i = 0; i < DIM(cbb_cpu_agents); i++) {
                expect_value(__wrap_pqos_mmap_read, address,
                             cbb_cpu_agents[i].mmrc.reg_block_base_addr);
                expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
                will_return(__wrap_pqos_mmap_read, block);
                expect_value(__wrap_pqos_munmap, mem, block);
                expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);
        }

        ret = mmio_dump_rmids(&dump);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_true(test_printed > 0);
        assert_null(strstr(test_log, "is not a"));
}

static void
test_mmio_dump_rmids_mbm_of_a_device_domain(void **state
                                            __attribute__((unused)))
{
        uint16_t domain_id = 16;
        pqos_rmid_t rmid = 0;
        struct pqos_mmio_dump_rmids dump = {.num_domain_ids = 1,
                                            .domain_ids = &domain_id,
                                            .num_mem_regions = 1,
                                            .region_num = {0},
                                            .num_rmids = 1,
                                            .rmids = &rmid,
                                            .rmid_type =
                                                MMIO_DUMP_RMID_TYPE_MBM};
        int ret;

        test_printed = 0;
        will_return(__wrap__pqos_get_erdt, &cbb_erdt);
        will_return(__wrap__pqos_get_mrrm, &one_region);
        ret = mmio_dump_rmids(&dump);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(test_printed, 0);
        assert_non_null(strstr(test_log, "Domain ID 16 is not a CPU domain"));
        assert_int_equal(test_log_type, LOG_OPT_ERROR);
}

static void
test_mmio_dump_rmids_io_reads_each_device_domain(void **state
                                                 __attribute__((unused)))
{
        /* each I/O type of domains 16 and 17 is read from the block of each
         * domain's own agent, in the order given
         */
        static uint8_t block[PAGE_SIZE];
        uint16_t domain_ids[] = {16, 17};
        pqos_rmid_t rmid = 0;

        for (unsigned t = 0; t < DIM(io_types); t++) {
                struct pqos_mmio_dump_rmids dump = {
                    .num_domain_ids = DIM(domain_ids),
                    .domain_ids = domain_ids,
                    .num_rmids = 1,
                    .rmids = &rmid,
                    .rmid_type = io_types[t].type};
                int ret;

                test_printed = 0;
                test_log[0] = '\0';
                will_return(__wrap__pqos_get_erdt, &cbb_erdt);
                for (unsigned i = 0; i < DIM(cbb_dev_agents); i++) {
                        const struct pqos_device_agent_info *agent =
                            &cbb_dev_agents[i];

                        expect_value(__wrap_pqos_mmap_read, address,
                                     io_types[t].ibrd
                                         ? agent->ibrd.reg_base_addr
                                         : agent->cmrd.reg_base_addr);
                        expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
                        will_return(__wrap_pqos_mmap_read, block);
                        expect_value(__wrap_pqos_munmap, mem, block);
                        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);
                }

                ret = mmio_dump_rmids(&dump);

                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_true(test_printed > 0);
                assert_null(strstr(test_log, "is not a"));
        }
}

static void
test_mmio_dump_rmids_io_unknown_domain_dumps_none(void **state
                                                  __attribute__((unused)))
{
        /* 16 is a device domain and 0 is not; neither is read, for any I/O
         * type, which the mmap wrapper would fail on
         */
        uint16_t domain_ids[] = {16, 0};
        pqos_rmid_t rmid = 0;

        for (unsigned t = 0; t < DIM(io_types); t++) {
                struct pqos_mmio_dump_rmids dump = {
                    .num_domain_ids = DIM(domain_ids),
                    .domain_ids = domain_ids,
                    .num_rmids = 1,
                    .rmids = &rmid,
                    .rmid_type = io_types[t].type};
                int ret;

                test_printed = 0;
                test_log[0] = '\0';
                will_return(__wrap__pqos_get_erdt, &cbb_erdt);
                ret = mmio_dump_rmids(&dump);

                assert_int_equal(ret, PQOS_RETVAL_ERROR);
                assert_int_equal(test_printed, 0);
                assert_non_null(
                    strstr(test_log, "Domain ID 0 is not a device domain"));
                assert_int_equal(test_log_type, LOG_OPT_ERROR);
        }
}

static void
test_mmio_dump_rmids_invalid(void **state __attribute__((unused)))
{
        struct pqos_erdt_info erdt = {0};
        struct pqos_mmio_dump_rmids dump = {0};
        int ret;

        will_return(__wrap__pqos_get_erdt, &erdt);
        ret = mmio_dump_rmids(&dump);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_uint64_to_binary),
            cmocka_unit_test(test_mmio_dump_rmids_null),
            cmocka_unit_test(test_mmio_dump_rmids_unknown_domain),
            cmocka_unit_test(test_mmio_dump_rmids_unknown_domain_dumps_none),
            cmocka_unit_test(test_mmio_dump_rmids_domain_of_the_other_type),
            cmocka_unit_test(test_mmio_dump_rmids_mbm_reads_each_cpu_domain),
            cmocka_unit_test(test_mmio_dump_rmids_mbm_of_a_device_domain),
            cmocka_unit_test(test_mmio_dump_rmids_io_reads_each_device_domain),
            cmocka_unit_test(test_mmio_dump_rmids_io_unknown_domain_dumps_none),
            cmocka_unit_test(test_mmio_dump_rmids_invalid)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
