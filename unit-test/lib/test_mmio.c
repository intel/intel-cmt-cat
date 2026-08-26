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

#include "mmio.h"
#include "mmio_allocation.h"
#include "mmio_monitoring.h"
#include "monitoring.h"
#include "test.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* the last line the library logged, so a test can assert the message and not
 * only the return value
 */
static char test_log[512];

void
__wrap_log_printf(int type __attribute__((unused)), const char *str, ...)
{
        va_list ap;

        va_start(ap, str);
        vsnprintf(test_log, sizeof(test_log), str, ap);
        va_end(ap);
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

const struct pqos_channels_domains *
__wrap__pqos_get_channels_domains(void)
{
        return mock_ptr_type(const struct pqos_channels_domains *);
}

int
__wrap_get_total_iol3_mbm_rmid_range_v1(const struct pqos_erdt_ibrd *ibrd,
                                        unsigned int rmid_first,
                                        unsigned int rmid_last,
                                        iol3_mbm_rmid_t *rmids_val)
{
        check_expected_ptr(ibrd);
        check_expected(rmid_first);
        check_expected(rmid_last);

        *rmids_val = TOTAL_IO_BW_RMID_O_MASK;
        return mock_type(int);
}

int
__wrap_get_miss_iol3_mbm_rmid_range_v1(const struct pqos_erdt_ibrd *ibrd,
                                       unsigned int rmid_first,
                                       unsigned int rmid_last,
                                       iol3_mbm_rmid_t *rmids_val)
{
        check_expected_ptr(ibrd);
        check_expected(rmid_first);
        check_expected(rmid_last);

        *rmids_val = TOTAL_IO_BW_RMID_O_MASK;
        return mock_type(int);
}

int
__wrap_get_mba_optimal_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                         int region_num,
                                         unsigned int clos_number,
                                         unsigned int *value)
{
        check_expected_ptr(marc);
        check_expected(region_num);
        check_expected(clos_number);

        *value = 0;
        return PQOS_RETVAL_OK;
}

int
__wrap_get_mba_min_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                     int region_num,
                                     unsigned int clos_number,
                                     unsigned int *value)
{
        check_expected_ptr(marc);
        check_expected(region_num);
        check_expected(clos_number);

        *value = 0;
        return PQOS_RETVAL_OK;
}

int
__wrap_get_mba_max_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                     int region_num,
                                     unsigned int clos_number,
                                     unsigned int *value)
{
        check_expected_ptr(marc);
        check_expected(region_num);
        check_expected(clos_number);

        *value = 0;
        return PQOS_RETVAL_OK;
}

int
__wrap_set_mba_optimal_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                         int region_num,
                                         unsigned int clos_number,
                                         unsigned int value)
{
        check_expected_ptr(marc);
        check_expected(region_num);
        check_expected(clos_number);
        check_expected(value);

        return mock_type(int);
}

int
__wrap_set_mba_min_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                     int region_num,
                                     unsigned int clos_number,
                                     unsigned int value)
{
        check_expected_ptr(marc);
        check_expected(region_num);
        check_expected(clos_number);
        check_expected(value);

        return mock_type(int);
}

int
__wrap_set_mba_max_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                     int region_num,
                                     unsigned int clos_number,
                                     unsigned int value)
{
        check_expected_ptr(marc);
        check_expected(region_num);
        check_expected(clos_number);
        check_expected(value);

        return mock_type(int);
}

int
__wrap_set_iol3_cbm_clos_v1(const struct pqos_erdt_card *card,
                            unsigned int clos_number,
                            uint64_t value)
{
        check_expected_ptr(card);
        check_expected(clos_number);
        check_expected(value);

        return mock_type(int);
}

static void
test_cpu_cmt_range_crosses_clump(void **state __attribute__((unused)))
{
        struct pqos_erdt_cmrc cmrc = {0};
        uint64_t registers[PAGE_SIZE / sizeof(uint64_t)] = {0};
        l3_cmt_rmid_t values[4] = {0};
        const uint64_t expected[] = {2, 3, 4, 5};
        int ret;

        cmrc.block_base_addr = 0x1000;
        cmrc.block_size = 1;
        cmrc.clump_size = 4;
        cmrc.clump_stride = 64;

        registers[2] = expected[0];
        registers[3] = expected[1];
        registers[8] = expected[2];
        registers[9] = expected[3];

        expect_value(__wrap_pqos_mmap_read, address, cmrc.block_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);

        ret = get_l3_cmt_rmid_range_v1(&cmrc, 2, 5, values);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_memory_equal(values, expected, sizeof(expected));
}

static void
test_io_cmt_range_crosses_page(void **state __attribute__((unused)))
{
        struct pqos_erdt_cmrd cmrd = {0};
        uint64_t registers[2 * PAGE_SIZE / sizeof(uint64_t)] = {0};
        iol3_cmt_rmid_t values[3] = {0};
        const uint64_t expected[] = {1, 2, 3};
        int ret;

        cmrd.reg_base_addr = 0x2000;
        cmrd.reg_block_size = 2;
        cmrd.offset = 32;
        cmrd.clump_size = 2;

        registers[5] = expected[0];
        registers[(PAGE_SIZE + 32) / sizeof(uint64_t)] = expected[1];
        registers[(PAGE_SIZE + 40) / sizeof(uint64_t)] = expected[2];

        expect_value(__wrap_pqos_mmap_read, address, cmrd.reg_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, 2 * PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, 2 * PAGE_SIZE);

        ret = get_iol3_cmt_rmid_range_v1(&cmrd, 1, 3, values);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_memory_equal(values, expected, sizeof(expected));
}

static void
test_cmt_range_rejects_invalid_range(void **state __attribute__((unused)))
{
        struct pqos_erdt_cmrc cmrc = {0};
        uint64_t registers[PAGE_SIZE / sizeof(uint64_t)] = {0};
        l3_cmt_rmid_t value;
        int ret;

        cmrc.block_size = 1;
        cmrc.clump_size = 4;

        expect_value(__wrap_pqos_mmap_read, address, cmrc.block_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);

        ret = get_l3_cmt_rmid_range_v1(&cmrc, 2, 1, &value);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        cmrc.clump_stride = PAGE_SIZE;
        expect_value(__wrap_pqos_mmap_read, address, cmrc.block_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);

        ret = get_l3_cmt_rmid_range_v1(&cmrc, 4, 4, &value);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_mbm_range_rejects_invalid_range(void **state __attribute__((unused)))
{
        struct pqos_erdt_mmrc mmrc = {0};
        uint64_t registers[PAGE_SIZE / sizeof(uint64_t)] = {0};
        l3_mbm_rmid_t value;
        int ret;

        mmrc.reg_block_size = 1;

        ret = get_l3_mbm_region_rmid_range_v1(&mmrc, 0, 2, 1, &value);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        expect_value(__wrap_pqos_mmap_read, address, mmrc.reg_block_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);

        ret = get_l3_mbm_region_rmid_range_v1(&mmrc, 0, 0, UINT_MAX, &value);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/**
 * @brief Fills an MBA request that only sets the optimal bandwidth limit
 *
 * @param [out] requested MBA request to fill
 * @param [in] domain_id domain to target
 * @param [in] region_num memory region number
 * @param [in] bw optimal bandwidth limit value
 */
static void
mba_request_init(struct pqos_mba *requested,
                 const uint16_t domain_id,
                 const int region_num,
                 const int bw)
{
        memset(requested, 0, sizeof(*requested));

        requested->class_id = 1;
        requested->domain_id = domain_id;
        requested->num_mem_regions = 1;
        requested->mem_regions[0].region_num = region_num;
        requested->mem_regions[0].bw_ctrl_val[PQOS_BW_CTRL_TYPE_OPT_IDX] = bw;
        requested->mem_regions[0].bw_ctrl_val[PQOS_BW_CTRL_TYPE_MIN_IDX] = -1;
        requested->mem_regions[0].bw_ctrl_val[PQOS_BW_CTRL_TYPE_MAX_IDX] = -1;
}

/**
 * @brief Queues an MRRM mock advertising the given number of memory regions
 *
 * @param [out] mrrm MRRM information to fill and queue
 * @param [in] num_mem_regions number of memory regions to advertise
 */
static void
mrrm_will_return(struct pqos_mrrm_info *mrrm, const uint8_t num_mem_regions)
{
        memset(mrrm, 0, sizeof(*mrrm));
        mrrm->max_memory_regions_supported = num_mem_regions;
        will_return(__wrap__pqos_get_mrrm, mrrm);
}

static void
test_mba_set_resolves_domain_id(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agents[2] = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested = {0};
        int ret;

        erdt.max_clos = 4;
        erdt.num_cpu_agents = 2;
        erdt.cpu_agents = cpu_agents;
        cpu_agents[0].rmdd.domain_id = 10;
        cpu_agents[1].rmdd.domain_id = 20;

        requested.class_id = 1;
        requested.domain_id = 20;
        requested.num_mem_regions = 1;
        requested.mem_regions[0].region_num = 0;
        requested.mem_regions[0].bw_ctrl_val[PQOS_BW_CTRL_TYPE_OPT_IDX] = 100;
        requested.mem_regions[0].bw_ctrl_val[PQOS_BW_CTRL_TYPE_MIN_IDX] = -1;
        requested.mem_regions[0].bw_ctrl_val[PQOS_BW_CTRL_TYPE_MAX_IDX] = -1;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, marc,
                     &cpu_agents[1].marc);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, region_num, 0);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, clos_number, 1);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, value, 100);
        will_return(__wrap_set_mba_optimal_bw_region_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_mba_set_accepts_max_bw(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested;
        int ret;

        erdt.max_clos = 4;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent.rmdd.domain_id = 10;

        mba_request_init(&requested, cpu_agent.rmdd.domain_id, 0, MBA_MAX_BW);

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, marc,
                     &cpu_agent.marc);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, region_num, 0);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, clos_number, 1);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, value,
                     MBA_MAX_BW);
        will_return(__wrap_set_mba_optimal_bw_region_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/**
 * @brief Asserts that an MBA request is rejected without any register write
 *
 * No expect_*() call is registered for the setter wrapper, so cmocka fails the
 * test if the request reaches it.
 *
 * @param [in] num_mem_regions number of memory regions advertised by MRRM
 * @param [in] region_num memory region number
 * @param [in] bw optimal bandwidth limit value
 */
static void
assert_mba_set_rejects(const uint8_t num_mem_regions,
                       const int region_num,
                       const int bw)
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested;
        int ret;

        erdt.max_clos = 4;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent.rmdd.domain_id = 10;

        mba_request_init(&requested, cpu_agent.rmdd.domain_id, region_num, bw);

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, num_mem_regions);

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_mba_set_rejects_invalid_request(void **state __attribute__((unused)))
{
        /* bandwidth above the register field width */
        assert_mba_set_rejects(PQOS_MAX_MEM_REGIONS, 0, MBA_MAX_BW + 1);
        /* bandwidth that is neither a limit nor the -1 sentinel */
        assert_mba_set_rejects(PQOS_MAX_MEM_REGIONS, 0, -2);
        /* memory region outside the array bound */
        assert_mba_set_rejects(PQOS_MAX_MEM_REGIONS, PQOS_MAX_MEM_REGIONS,
                               MBA_MAX_BW);
}

static void
test_mba_set_rejects_unsupported_region(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested;
        int ret;

        /* the platform advertises fewer regions than the array can hold */
        assert_mba_set_rejects(2, 2, MBA_MAX_BW);
        assert_mba_set_rejects(2, PQOS_MAX_MEM_REGIONS - 1, MBA_MAX_BW);

        /* the last supported region is still accepted */
        erdt.max_clos = 4;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent.rmdd.domain_id = 10;

        mba_request_init(&requested, cpu_agent.rmdd.domain_id, 1, MBA_MAX_BW);

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 2);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, marc,
                     &cpu_agent.marc);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, region_num, 1);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, clos_number, 1);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, value,
                     MBA_MAX_BW);
        will_return(__wrap_set_mba_optimal_bw_region_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_mba_set_validates_before_write(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested[2];
        int ret;

        erdt.max_clos = 4;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent.rmdd.domain_id = 10;

        /* the first request is valid, the second one is not */
        mba_request_init(&requested[0], cpu_agent.rmdd.domain_id, 0,
                         MBA_MAX_BW);
        mba_request_init(&requested[1], cpu_agent.rmdd.domain_id, 0,
                         MBA_MAX_BW + 1);

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);

        /*
         * No setter expectation is registered, so the valid request must not
         * be written either.
         */
        ret = mmio_mba_set(0, 2, requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_mba_get_ignores_num_clos_input(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm = {0};
        struct pqos_mba mba_tab[2] = {0};
        unsigned num_clos = UINT32_MAX;
        const int num_reads = 2 * PQOS_MAX_MEM_REGIONS;
        int ret;

        erdt.max_clos = 2;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent.rmdd.domain_id = 10;
        mrrm.max_memory_regions_supported = PQOS_MAX_MEM_REGIONS + 1;
        mba_tab[0].domain_id = cpu_agent.rmdd.domain_id;

        will_return(__wrap__pqos_get_erdt, &erdt);
        will_return(__wrap__pqos_get_mrrm, &mrrm);
        expect_value_count(__wrap_get_mba_optimal_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_reads);
        expect_any_count(__wrap_get_mba_optimal_bw_region_clos_v1, region_num,
                         num_reads);
        expect_any_count(__wrap_get_mba_optimal_bw_region_clos_v1, clos_number,
                         num_reads);
        expect_value_count(__wrap_get_mba_min_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_reads);
        expect_any_count(__wrap_get_mba_min_bw_region_clos_v1, region_num,
                         num_reads);
        expect_any_count(__wrap_get_mba_min_bw_region_clos_v1, clos_number,
                         num_reads);
        expect_value_count(__wrap_get_mba_max_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_reads);
        expect_any_count(__wrap_get_mba_max_bw_region_clos_v1, region_num,
                         num_reads);
        expect_any_count(__wrap_get_mba_max_bw_region_clos_v1, clos_number,
                         num_reads);

        ret = mmio_mba_get(0, 2, &num_clos, mba_tab);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(num_clos, 2);
        assert_int_equal(mba_tab[1].domain_id, cpu_agent.rmdd.domain_id);
        assert_int_equal(mba_tab[0].class_id, 0);
        assert_int_equal(mba_tab[1].class_id, 1);
        assert_int_equal(mba_tab[0].num_mem_regions, PQOS_MAX_MEM_REGIONS);
        assert_int_equal(mba_tab[1].num_mem_regions, PQOS_MAX_MEM_REGIONS);
}

static void
test_mba_get_uses_mrrm_count(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba mba_tab[1] = {0};
        unsigned num_clos = 0;
        const uint8_t supported = 2;
        const int num_reads = supported;
        int ret;

        erdt.max_clos = 1;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent.rmdd.domain_id = 10;
        mba_tab[0].domain_id = cpu_agent.rmdd.domain_id;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, supported);
        expect_value_count(__wrap_get_mba_optimal_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_reads);
        expect_any_count(__wrap_get_mba_optimal_bw_region_clos_v1, region_num,
                         num_reads);
        expect_any_count(__wrap_get_mba_optimal_bw_region_clos_v1, clos_number,
                         num_reads);
        expect_value_count(__wrap_get_mba_min_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_reads);
        expect_any_count(__wrap_get_mba_min_bw_region_clos_v1, region_num,
                         num_reads);
        expect_any_count(__wrap_get_mba_min_bw_region_clos_v1, clos_number,
                         num_reads);
        expect_value_count(__wrap_get_mba_max_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_reads);
        expect_any_count(__wrap_get_mba_max_bw_region_clos_v1, region_num,
                         num_reads);
        expect_any_count(__wrap_get_mba_max_bw_region_clos_v1, clos_number,
                         num_reads);

        ret = mmio_mba_get(0, 1, &num_clos, mba_tab);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        /* only the regions the platform advertises are read back */
        assert_int_equal(mba_tab[0].num_mem_regions, supported);
}

static void
test_mba_get_rejects_unsupported_num_regions(void **state
                                             __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba mba_tab[1] = {0};
        unsigned num_clos = 0;
        int ret;

        erdt.max_clos = 1;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent.rmdd.domain_id = 10;
        mba_tab[0].domain_id = cpu_agent.rmdd.domain_id;
        /* more regions than the platform advertises */
        mba_tab[0].num_mem_regions = PQOS_MAX_MEM_REGIONS;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 2);

        ret = mmio_mba_get(0, 1, &num_clos, mba_tab);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_mba_reset_uses_mrrm_count(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        const uint8_t supported = 2;
        const int num_writes = 2 * supported; /* max_clos * regions */
        int ret;

        erdt.max_clos = 2;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent.rmdd.domain_id = 10;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, supported);

        expect_value_count(__wrap_set_mba_optimal_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_writes);
        expect_any_count(__wrap_set_mba_optimal_bw_region_clos_v1, region_num,
                         num_writes);
        expect_any_count(__wrap_set_mba_optimal_bw_region_clos_v1, clos_number,
                         num_writes);
        expect_value_count(__wrap_set_mba_optimal_bw_region_clos_v1, value,
                           MBA_MAX_BW, num_writes);
        expect_value_count(__wrap_set_mba_min_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_writes);
        expect_any_count(__wrap_set_mba_min_bw_region_clos_v1, region_num,
                         num_writes);
        expect_any_count(__wrap_set_mba_min_bw_region_clos_v1, clos_number,
                         num_writes);
        expect_value_count(__wrap_set_mba_min_bw_region_clos_v1, value,
                           MBA_MAX_BW, num_writes);
        expect_value_count(__wrap_set_mba_max_bw_region_clos_v1, marc,
                           &cpu_agent.marc, num_writes);
        expect_any_count(__wrap_set_mba_max_bw_region_clos_v1, region_num,
                         num_writes);
        expect_any_count(__wrap_set_mba_max_bw_region_clos_v1, clos_number,
                         num_writes);
        expect_value_count(__wrap_set_mba_max_bw_region_clos_v1, value,
                           MBA_MAX_BW, num_writes);

        for (int i = 0; i < num_writes; i++) {
                will_return(__wrap_set_mba_optimal_bw_region_clos_v1,
                            PQOS_RETVAL_OK);
                will_return(__wrap_set_mba_min_bw_region_clos_v1,
                            PQOS_RETVAL_OK);
                will_return(__wrap_set_mba_max_bw_region_clos_v1,
                            PQOS_RETVAL_OK);
        }

        ret = mmio_alloc_reset_mba();

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
assert_io_overflow_invalidates_baseline(const enum pqos_mon_event event)
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_channels_domains channels_domains = {0};
        struct pqos_mon_poll_ctx ctx = {0};
        struct pqos_mon_data_internal intl = {0};
        struct pqos_mon_data group = {0};
        pqos_channel_t channel_id = 1;
        uint16_t domain_id = 2;
        uint16_t domain_id_idx = 0;
        int ret;

        dev_agent.ibrd.reg_block_size = 1;
        dev_agent.ibrd.bw_reg_clump_size = 1;
        dev_agent.ibrd.miss_reg_clump_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        channels_domains.num_channel_ids = 1;
        channels_domains.channel_ids = &channel_id;
        channels_domains.domain_ids = &domain_id;
        channels_domains.domain_id_idxs = &domain_id_idx;
        ctx.channel_id = channel_id;
        intl.hw.ctx = &ctx;
        intl.hw.num_ctx = 1;
        intl.valid_io_total_read = 1;
        intl.valid_io_miss_read = 1;
        group.intl = &intl;

        will_return(__wrap__pqos_get_erdt, &erdt);
        will_return(__wrap__pqos_get_channels_domains, &channels_domains);
        if (event == PQOS_MON_EVENT_IO_TOTAL_MEM_BW) {
                expect_value(__wrap_get_total_iol3_mbm_rmid_range_v1, ibrd,
                             &dev_agent.ibrd);
                expect_value(__wrap_get_total_iol3_mbm_rmid_range_v1,
                             rmid_first, 0);
                expect_value(__wrap_get_total_iol3_mbm_rmid_range_v1, rmid_last,
                             0);
                will_return(__wrap_get_total_iol3_mbm_rmid_range_v1,
                            PQOS_RETVAL_OK);
        } else {
                expect_value(__wrap_get_miss_iol3_mbm_rmid_range_v1, ibrd,
                             &dev_agent.ibrd);
                expect_value(__wrap_get_miss_iol3_mbm_rmid_range_v1, rmid_first,
                             0);
                expect_value(__wrap_get_miss_iol3_mbm_rmid_range_v1, rmid_last,
                             0);
                will_return(__wrap_get_miss_iol3_mbm_rmid_range_v1,
                            PQOS_RETVAL_OK);
        }

        ret = mmio_mon_read_counter(&group, event);

        assert_int_equal(ret, PQOS_RETVAL_OVERFLOW);
        if (event == PQOS_MON_EVENT_IO_TOTAL_MEM_BW)
                assert_false(intl.valid_io_total_read);
        else
                assert_false(intl.valid_io_miss_read);
}

static void
test_io_overflow_invalidates_baseline(void **state __attribute__((unused)))
{
        assert_io_overflow_invalidates_baseline(PQOS_MON_EVENT_IO_TOTAL_MEM_BW);
        assert_io_overflow_invalidates_baseline(PQOS_MON_EVENT_IO_MISS_MEM_BW);
}

/* ======== mmio_l3ca_get / mmio_l3ca_set domain selection ======== */

static void
test_mmio_l3ca_get_rejects_domain_without_l3ca(void **state
                                               __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca[2] = {{0}};
        unsigned num_ca = 0;
        int ret;

        /* the platform has one I/O device domain, 0x10, whose CARD block gives
         * it the registers L3 CAT programs
         */
        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;

        /* a CPU domain has no CARD block, so it carries no L3 CAT */
        ca[0].domain_id = 0;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        ret = mmio_l3ca_get(0, DIM(ca), &num_ca, ca);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(num_ca, 0);
        /* the message says what the domain lacks, which domains have it, and
         * where to look them up
         */
        assert_non_null(strstr(test_log, "carries no L3 CAT registers"));
        assert_non_null(strstr(test_log, "0x10"));
        assert_non_null(strstr(test_log, "--print-io-devs"));
}

static void
test_mmio_l3ca_set_rejects_domain_without_l3ca(void **state
                                               __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca = {0};
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;

        ca.domain_id = 0;
        ca.class_id = 0;
        ca.u.ways_mask = 1;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        ret = mmio_l3ca_set(0, 1, &ca);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        /* the same message as the read path, which used to differ from it */
        assert_non_null(strstr(test_log, "carries no L3 CAT registers"));
        assert_non_null(strstr(test_log, "0x10"));
}

static void
test_mmio_l3ca_set_rejects_monitoring_only_domain(void **state
                                                  __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agents[2] = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca = {0};
        int ret;

        /* 0x10 is enumerated for monitoring alone, so its CARD block was never
         * populated, while 0x11 has one
         */
        dev_agents[0].rmdd.domain_id = 0x10;
        dev_agents[1].rmdd.domain_id = 0x11;
        dev_agents[1].card.reg_base_addr = 0xf0000000;
        dev_agents[1].card.reg_block_size = 1;
        erdt.num_dev_agents = DIM(dev_agents);
        erdt.dev_agents = dev_agents;
        erdt.max_clos = 1;

        ca.domain_id = 0x10;
        ca.class_id = 0;
        ca.u.ways_mask = 1;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        ret = mmio_l3ca_set(0, 1, &ca);

        /* a device domain, and still no registers to program, so it is rejected
         * rather than left to fail on a zeroed register base
         */
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_non_null(strstr(test_log, "carries no L3 CAT registers"));
        /* and it is not itself offered as somewhere L3 CAT applies */
        assert_non_null(strstr(test_log, "0x11"));
        assert_null(strstr(test_log, "0x10,"));
}

static void
test_mmio_l3ca_set_domain_list_says_when_truncated(void **state
                                                   __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agents[64] = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca = {0};
        unsigned i;
        int ret;

        /* more device domains than the list can hold */
        for (i = 0; i < DIM(dev_agents); i++) {
                dev_agents[i].rmdd.domain_id = (uint16_t)(0x1000 + i);
                dev_agents[i].card.reg_base_addr = 0xf0000000;
                dev_agents[i].card.reg_block_size = 1;
        }
        erdt.num_dev_agents = DIM(dev_agents);
        erdt.dev_agents = dev_agents;
        erdt.max_clos = 1;

        ca.domain_id = 0;
        ca.class_id = 0;
        ca.u.ways_mask = 1;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        ret = mmio_l3ca_set(0, 1, &ca);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        /* the list is not passed off as complete, and the message itself stays
         * inside the 319 bytes one log line carries, rather than being handed
         * to write() as a length past the end of log_printf()'s buffer
         */
        assert_non_null(strstr(test_log, " and more"));
        assert_true(strlen(test_log) < 319);
}

static void
test_alloc_reset_cat_skips_monitoring_only_domain(void **state
                                                  __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agents[2] = {0};
        struct pqos_erdt_info erdt = {0};
        const unsigned num_writes = 2; /* max_clos, for the one domain left */
        unsigned i;
        int ret;

        /* 0x10 is enumerated for monitoring alone, so its CARD block was never
         * populated, while 0x11 has one
         */
        dev_agents[0].rmdd.domain_id = 0x10;
        dev_agents[1].rmdd.domain_id = 0x11;
        dev_agents[1].rmdd.num_io_l3_ways = 4;
        dev_agents[1].card.reg_base_addr = 0xf0000000;
        dev_agents[1].card.reg_block_size = 1;
        erdt.num_dev_agents = DIM(dev_agents);
        erdt.dev_agents = dev_agents;
        erdt.max_clos = num_writes;

        will_return(__wrap__pqos_get_erdt, &erdt);

        /* the reset reaches the CARD block that exists, and only that one: a
         * write against the zeroed base of 0x10 would fail the whole reset
         */
        expect_value_count(__wrap_set_iol3_cbm_clos_v1, card,
                           &dev_agents[1].card, num_writes);
        expect_any_count(__wrap_set_iol3_cbm_clos_v1, clos_number, num_writes);
        expect_value_count(__wrap_set_iol3_cbm_clos_v1, value, 0xf, num_writes);

        for (i = 0; i < num_writes; i++)
                will_return(__wrap_set_iol3_cbm_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_alloc_reset_cat();

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_cpu_cmt_range_crosses_clump),
            cmocka_unit_test(test_io_cmt_range_crosses_page),
            cmocka_unit_test(test_cmt_range_rejects_invalid_range),
            cmocka_unit_test(test_mbm_range_rejects_invalid_range),
            cmocka_unit_test(test_mba_set_resolves_domain_id),
            cmocka_unit_test(test_mba_set_accepts_max_bw),
            cmocka_unit_test(test_mba_set_rejects_invalid_request),
            cmocka_unit_test(test_mba_set_rejects_unsupported_region),
            cmocka_unit_test(test_mba_set_validates_before_write),
            cmocka_unit_test(test_mba_get_ignores_num_clos_input),
            cmocka_unit_test(test_mba_get_uses_mrrm_count),
            cmocka_unit_test(test_mba_get_rejects_unsupported_num_regions),
            cmocka_unit_test(test_mba_reset_uses_mrrm_count),
            cmocka_unit_test(test_io_overflow_invalidates_baseline),
            cmocka_unit_test(test_mmio_l3ca_get_rejects_domain_without_l3ca),
            cmocka_unit_test(test_mmio_l3ca_set_rejects_domain_without_l3ca),
            cmocka_unit_test(test_mmio_l3ca_set_rejects_monitoring_only_domain),
            cmocka_unit_test(
                test_mmio_l3ca_set_domain_list_says_when_truncated),
            cmocka_unit_test(
                test_alloc_reset_cat_skips_monitoring_only_domain)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
