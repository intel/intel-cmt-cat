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

#include "test_mmio.h"

#include "allocation_common.h"
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

uint8_t *
__wrap_pqos_mmap_write(uint64_t address, const uint64_t size)
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

/* the MBA caps of one class and region as the register mocks hold them, by
 * PQOS_BW_CTRL_TYPE_*_IDX, so that a reset can be followed write by write
 */
static struct {
        unsigned bw[PQOS_BW_CTRL_TYPE_COUNT];
        char log[16];    /* a letter a write: n minimum, o optimal, x maximum */
        unsigned writes; /* letters in log */
        int chain_broken; /* a write left minimum > optimal or optimal > max */
        int fail_read;    /* 1 + the PQOS_BW_CTRL_TYPE_*_IDX whose read fails */
} mba_regs;

/**
 * @brief Records a write of one cap in mba_regs
 *
 * @param [in] type PQOS_BW_CTRL_TYPE_*_IDX of the cap
 * @param [in] letter the cap's letter in mba_regs.log
 * @param [in] value value written
 */
static void
mba_regs_write(const int type, const char letter, const unsigned value)
{
        mba_regs.bw[type] = value;
        if (mba_regs.writes < sizeof(mba_regs.log) - 1)
                mba_regs.log[mba_regs.writes++] = letter;
        if (mba_regs.bw[PQOS_BW_CTRL_TYPE_MIN_IDX] >
                mba_regs.bw[PQOS_BW_CTRL_TYPE_OPT_IDX] ||
            mba_regs.bw[PQOS_BW_CTRL_TYPE_OPT_IDX] >
                mba_regs.bw[PQOS_BW_CTRL_TYPE_MAX_IDX])
                mba_regs.chain_broken = 1;
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

        if (mba_regs.fail_read == 1 + PQOS_BW_CTRL_TYPE_OPT_IDX)
                return PQOS_RETVAL_ERROR;
        *value = mba_regs.bw[PQOS_BW_CTRL_TYPE_OPT_IDX];
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

        if (mba_regs.fail_read == 1 + PQOS_BW_CTRL_TYPE_MIN_IDX)
                return PQOS_RETVAL_ERROR;
        *value = mba_regs.bw[PQOS_BW_CTRL_TYPE_MIN_IDX];
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

        if (mba_regs.fail_read == 1 + PQOS_BW_CTRL_TYPE_MAX_IDX)
                return PQOS_RETVAL_ERROR;
        *value = mba_regs.bw[PQOS_BW_CTRL_TYPE_MAX_IDX];
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

        mba_regs_write(PQOS_BW_CTRL_TYPE_OPT_IDX, 'o', value);

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

        mba_regs_write(PQOS_BW_CTRL_TYPE_MIN_IDX, 'n', value);

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

        mba_regs_write(PQOS_BW_CTRL_TYPE_MAX_IDX, 'x', value);

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

int
__wrap_alloc_reset(const struct pqos_alloc_config *cfg)
{
        function_called();
        check_expected_ptr(cfg);

        return mock_type(int);
}

int
__wrap_get_iol3_cbm_clos_v1(const struct pqos_erdt_card *card,
                            unsigned int clos_number,
                            unsigned int block_number,
                            uint64_t *value)
{
        check_expected_ptr(card);
        check_expected(clos_number);
        check_expected(block_number);

        *value = mock_type(uint64_t);

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

/* The IOL3_MASK register's write: the CBM, bits 63:32, is the one given in
 * each register block, bits 31:0, RsvdP, are kept as they were found, and
 * the next CLOS's register is not touched
 */
static void
test_iol3_cbm_set_keeps_reserved_bits(void **state __attribute__((unused)))
{
        struct pqos_erdt_card card = {0};
        uint64_t registers[2 * PAGE_SIZE / sizeof(uint64_t)] = {0};
        const unsigned clos = 3;
        const unsigned at =
            (0x100 + clos * BYTES_PER_CLOS_ENTRY) / sizeof(uint64_t);
        const unsigned at_next_page = at + PAGE_SIZE / sizeof(uint64_t);
        int ret;

        card.reg_base_addr = 0x3000;
        card.reg_block_size = 2;
        card.cat_reg_offset = 0x100;

        /* as firmware leaves them: a CBM of 0xfffff, with reserved bits set,
         * and in the next block one with its top bits set too, which are the
         * CBM's as well, so that the write replaces them
         */
        registers[at] = 0x000fffff12345678ULL;
        registers[at_next_page] = 0xf00fffff9abcdef0ULL;
        registers[at + 1] = UINT64_MAX;

        expect_value(__wrap_pqos_mmap_write, address, card.reg_base_addr);
        expect_value(__wrap_pqos_mmap_write, size, 2 * PAGE_SIZE);
        will_return(__wrap_pqos_mmap_write, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, 2 * PAGE_SIZE);

        ret = __real_set_iol3_cbm_clos_v1(&card, clos, 0xff);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(registers[at], 0x000000ff12345678ULL);
        assert_int_equal(registers[at_next_page], 0x000000ff9abcdef0ULL);
        assert_int_equal(registers[at + 1], UINT64_MAX);
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

/**
 * @brief Gives a CPU agent a domain ID and the MARC block MBA needs
 *
 * A CPU agent with a zeroed MARC block carries no MBA registers, which the
 * cases below about such an agent rely on, so every other case has to populate
 * it or it would be testing that instead of what it names.
 *
 * @param [out] cpu_agent CPU agent to fill
 * @param [in] domain_id domain the agent covers
 */
static void
cpu_agent_init(struct pqos_cpu_agent_info *cpu_agent, uint16_t domain_id)
{
        cpu_agent->rmdd.domain_id = domain_id;
        cpu_agent->marc.opt_bw_reg_block_base_addr = 0xf0000000;
        cpu_agent->marc.min_bw_reg_block_base_addr = 0xf0001000;
        cpu_agent->marc.max_bw_reg_block_base_addr = 0xf0002000;
        cpu_agent->marc.reg_block_size = 1;
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
        cpu_agent_init(&cpu_agents[0], 10);
        cpu_agent_init(&cpu_agents[1], 20);

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

/**
 * @brief Asserts that an MBA request within its domain's range is written
 *
 * @param [in] bw optimal bandwidth limit value
 * @param [in] range the MARC's MBA BW Control Window Range
 */
static void
assert_mba_set_accepts_in_range(const int bw, const uint32_t range)
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested;
        int ret;

        erdt.max_clos = 4;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        cpu_agent.marc.control_window_range = range;

        mba_request_init(&requested, cpu_agent.rmdd.domain_id, 0, bw);

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, marc,
                     &cpu_agent.marc);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, region_num, 0);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, clos_number, 1);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, value, bw);
        will_return(__wrap_set_mba_optimal_bw_region_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_mba_set_accepts_max_bw(void **state __attribute__((unused)))
{
        /* no range from the MARC: the 9-bit fields' largest value */
        assert_mba_set_accepts_in_range(0x1FF, 0);
}

/**
 * @brief Asserts that an MBA request is rejected without any register write
 *
 * No expect_*() call is registered for the setter wrappers, so cmocka fails
 * the test if the request reaches them.
 *
 * @param [in] num_mem_regions number of memory regions advertised by MRRM
 * @param [in] region_num memory region number
 * @param [in] type PQOS_BW_CTRL_TYPE_*_IDX of the bandwidth limit requested
 * @param [in] bw bandwidth limit value
 * @param [in] range the MARC's MBA BW Control Window Range
 * @param [in] log text the error logged contains, or NULL not to check it
 */
static void
assert_mba_set_rejects_type(const uint8_t num_mem_regions,
                            const int region_num,
                            const int type,
                            const int bw,
                            const uint32_t range,
                            const char *log)
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested;
        int ret;

        erdt.max_clos = 4;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        cpu_agent.marc.control_window_range = range;

        mba_request_init(&requested, cpu_agent.rmdd.domain_id, region_num, -1);
        requested.mem_regions[0].bw_ctrl_val[type] = bw;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, num_mem_regions);
        test_log[0] = '\0';

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        if (log != NULL)
                assert_non_null(strstr(test_log, log));
}

/**
 * @brief assert_mba_set_rejects_type() for the optimal bandwidth limit
 *
 * @param [in] num_mem_regions number of memory regions advertised by MRRM
 * @param [in] region_num memory region number
 * @param [in] bw optimal bandwidth limit value
 * @param [in] range the MARC's MBA BW Control Window Range
 */
static void
assert_mba_set_rejects_in_range(const uint8_t num_mem_regions,
                                const int region_num,
                                const int bw,
                                const uint32_t range)
{
        assert_mba_set_rejects_type(num_mem_regions, region_num,
                                    PQOS_BW_CTRL_TYPE_OPT_IDX, bw, range, NULL);
}

/**
 * @brief assert_mba_set_rejects_in_range() for an agent whose MARC gives no
 *        MBA BW Control Window Range, so that the fields' 0x1FF bounds it
 */
static void
assert_mba_set_rejects(const uint8_t num_mem_regions,
                       const int region_num,
                       const int bw)
{
        assert_mba_set_rejects_in_range(num_mem_regions, region_num, bw, 0);
}

static void
test_mba_set_rejects_outside_window_range(void **state __attribute__((unused)))
{
        /* Q from the MARC: 1 through 0xFF, for each of the three caps */
        for (int type = 0; type < PQOS_BW_CTRL_TYPE_COUNT; type++) {
                assert_mba_set_rejects_type(
                    PQOS_MAX_MEM_REGIONS, 0, type, 0x100, 0xFF,
                    "bandwidth value 0x100 is out of range (0x1-0xff)");
                assert_mba_set_rejects_type(
                    PQOS_MAX_MEM_REGIONS, 0, type, 0x1FF, 0xFF,
                    "bandwidth value 0x1ff is out of range (0x1-0xff)");
                /* 0 is not a value the registers take, whatever Q */
                assert_mba_set_rejects_type(
                    PQOS_MAX_MEM_REGIONS, 0, type, 0, 0xFF,
                    "bandwidth value 0 is out of range (0x1-0xff)");
        }
        /* a range wider than the fields leaves the fields' widest value */
        assert_mba_set_rejects_type(
            PQOS_MAX_MEM_REGIONS, 0, PQOS_BW_CTRL_TYPE_OPT_IDX, 0x200, 0x200,
            "bandwidth value 0x200 is out of range (0x1-0x1ff)");
        /* and so does no range */
        assert_mba_set_rejects_type(
            PQOS_MAX_MEM_REGIONS, 0, PQOS_BW_CTRL_TYPE_OPT_IDX, 0, 0,
            "bandwidth value 0 is out of range (0x1-0x1ff)");
}

static void
test_mba_set_accepts_window_range(void **state __attribute__((unused)))
{
        /* both ends of 1..Q */
        assert_mba_set_accepts_in_range(0xFF, 0xFF);
        assert_mba_set_accepts_in_range(1, 0xFF);
        assert_mba_set_accepts_in_range(1, 0);
}

static void
test_mba_set_uses_requested_domain_range(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agents[2] = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested;
        struct pqos_mba batch[2];
        int ret;

        /* 0xC0 is within domain 20's range and above domain 10's */
        erdt.max_clos = 4;
        erdt.num_cpu_agents = DIM(cpu_agents);
        erdt.cpu_agents = cpu_agents;
        cpu_agent_init(&cpu_agents[0], 10);
        cpu_agents[0].marc.control_window_range = 0x7F;
        cpu_agent_init(&cpu_agents[1], 20);
        cpu_agents[1].marc.control_window_range = 0xFF;

        mba_request_init(&requested, 20, 0, 0xC0);
        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, marc,
                     &cpu_agents[1].marc);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, region_num, 0);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, clos_number, 1);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, value, 0xC0);
        will_return(__wrap_set_mba_optimal_bw_region_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_OK);

        mba_request_init(&requested, 10, 0, 0xC0);
        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);
        test_log[0] = '\0';

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_non_null(strstr(test_log, "out of range (0x1-0x7f) in Domain "
                                         "ID 0xa!"));

        /* both in one call, the wider range first: each request is checked
         * against its own domain's range, so the call is refused before any
         * register is written - no setter is expected
         */
        mba_request_init(&batch[0], 20, 0, 0xC0);
        mba_request_init(&batch[1], 10, 0, 0xC0);
        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);
        test_log[0] = '\0';

        ret = mmio_mba_set(0, DIM(batch), batch, NULL);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_non_null(strstr(test_log, "out of range (0x1-0x7f) in Domain "
                                         "ID 0xa!"));
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
        cpu_agent_init(&cpu_agent, 10);

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
        cpu_agent_init(&cpu_agent, 10);

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

        erdt.max_clos = 1;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
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

        erdt.max_clos = 0;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
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

        erdt.max_clos = 0;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        mba_tab[0].domain_id = cpu_agent.rmdd.domain_id;
        /* more regions than the platform advertises */
        mba_tab[0].num_mem_regions = PQOS_MAX_MEM_REGIONS;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 2);

        ret = mmio_mba_get(0, 1, &num_clos, mba_tab);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/**
 * @brief Queues the reads of the three caps a reset makes before it writes
 *        them
 *
 * @param [in] marc the agent's MARC information
 * @param [in] count number of class and region pairs reset
 */
static void
expect_mba_reset_reads(const struct pqos_erdt_marc *marc, const int count)
{
        expect_value_count(__wrap_get_mba_optimal_bw_region_clos_v1, marc, marc,
                           count);
        expect_any_count(__wrap_get_mba_optimal_bw_region_clos_v1, region_num,
                         count);
        expect_any_count(__wrap_get_mba_optimal_bw_region_clos_v1, clos_number,
                         count);
        expect_value_count(__wrap_get_mba_min_bw_region_clos_v1, marc, marc,
                           count);
        expect_any_count(__wrap_get_mba_min_bw_region_clos_v1, region_num,
                         count);
        expect_any_count(__wrap_get_mba_min_bw_region_clos_v1, clos_number,
                         count);
        expect_value_count(__wrap_get_mba_max_bw_region_clos_v1, marc, marc,
                           count);
        expect_any_count(__wrap_get_mba_max_bw_region_clos_v1, region_num,
                         count);
        expect_any_count(__wrap_get_mba_max_bw_region_clos_v1, clos_number,
                         count);
}

/**
 * @brief Queues the writes of all three caps to one value, and their success
 *
 * @param [in] marc the agent's MARC information
 * @param [in] value value each cap is written
 * @param [in] count number of class and region pairs reset
 */
static void
expect_mba_reset_writes(const struct pqos_erdt_marc *marc,
                        const unsigned value,
                        const int count)
{
        expect_value_count(__wrap_set_mba_optimal_bw_region_clos_v1, marc, marc,
                           count);
        expect_any_count(__wrap_set_mba_optimal_bw_region_clos_v1, region_num,
                         count);
        expect_any_count(__wrap_set_mba_optimal_bw_region_clos_v1, clos_number,
                         count);
        expect_value_count(__wrap_set_mba_optimal_bw_region_clos_v1, value,
                           value, count);
        will_return_count(__wrap_set_mba_optimal_bw_region_clos_v1,
                          PQOS_RETVAL_OK, count);
        expect_value_count(__wrap_set_mba_min_bw_region_clos_v1, marc, marc,
                           count);
        expect_any_count(__wrap_set_mba_min_bw_region_clos_v1, region_num,
                         count);
        expect_any_count(__wrap_set_mba_min_bw_region_clos_v1, clos_number,
                         count);
        expect_value_count(__wrap_set_mba_min_bw_region_clos_v1, value, value,
                           count);
        will_return_count(__wrap_set_mba_min_bw_region_clos_v1, PQOS_RETVAL_OK,
                          count);
        expect_value_count(__wrap_set_mba_max_bw_region_clos_v1, marc, marc,
                           count);
        expect_any_count(__wrap_set_mba_max_bw_region_clos_v1, region_num,
                         count);
        expect_any_count(__wrap_set_mba_max_bw_region_clos_v1, clos_number,
                         count);
        expect_value_count(__wrap_set_mba_max_bw_region_clos_v1, value, value,
                           count);
        will_return_count(__wrap_set_mba_max_bw_region_clos_v1, PQOS_RETVAL_OK,
                          count);
}

static void
test_mba_reset_uses_mrrm_count(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        const uint8_t supported = 2;
        const int num_writes = 2 * supported; /* classes * regions */
        int ret;

        erdt.max_clos = 1;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        /* Q, the MBA BW Control Window Range, below the fields' 0x1FF */
        cpu_agent.marc.control_window_range = 0xFF;
        memset(&mba_regs, 0, sizeof(mba_regs));

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, supported);
        expect_mba_reset_reads(&cpu_agent.marc, num_writes);
        expect_mba_reset_writes(&cpu_agent.marc, 0xFF, num_writes);

        ret = mmio_alloc_reset_mba();

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/**
 * @brief Asserts that a reset writes the fields' largest value into all three
 *        caps of an agent whose MARC gives the range given
 *
 * @param [in] range the MARC's MBA BW Control Window Range
 */
static void
assert_mba_reset_writes_field_max(const uint32_t range)
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        int ret;

        erdt.max_clos = 0;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        cpu_agent.marc.control_window_range = range;
        memset(&mba_regs, 0, sizeof(mba_regs));

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 1);
        expect_mba_reset_reads(&cpu_agent.marc, 1);
        /* the field's width, not PQOS_MMIO_MBA_MAX_BW, which it is tested by */
        expect_mba_reset_writes(&cpu_agent.marc, 0x1FF, 1);

        ret = mmio_alloc_reset_mba();

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_mba_reset_without_window_range_writes_field_max(void **state
                                                     __attribute__((unused)))
{
        /* a MARC that gives no MBA BW Control Window Range */
        assert_mba_reset_writes_field_max(0);
}

static void
test_mba_reset_window_range_above_field_writes_field_max(
    void **state __attribute__((unused)))
{
        /* a range wider than the 9-bit fields: a write of it would spill
         * into the bits beside the field
         */
        assert_mba_reset_writes_field_max(0x200);
}

static void
test_mba_reset_writes_each_domain_range(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agents[2] = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        const int num_writes = 2; /* classes * regions, an agent */
        int ret;

        erdt.max_clos = 1;
        erdt.num_cpu_agents = DIM(cpu_agents);
        erdt.cpu_agents = cpu_agents;
        cpu_agent_init(&cpu_agents[0], 10);
        cpu_agents[0].marc.control_window_range = 0x7F;
        cpu_agent_init(&cpu_agents[1], 20);
        cpu_agents[1].marc.control_window_range = 0xFF;
        memset(&mba_regs, 0, sizeof(mba_regs));

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 1);
        /* each agent's caps get that agent's Q */
        expect_mba_reset_reads(&cpu_agents[0].marc, num_writes);
        expect_mba_reset_reads(&cpu_agents[1].marc, num_writes);
        expect_mba_reset_writes(&cpu_agents[0].marc, 0x7F, num_writes);
        expect_mba_reset_writes(&cpu_agents[1].marc, 0xFF, num_writes);

        ret = mmio_alloc_reset_mba();

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/**
 * @brief Asserts the order a reset to Q = 0xFF writes the caps in from the
 *        caps given, and that no write leaves the minimum above the optimal
 *        or the optimal above the maximum
 *
 * @param [in] min minimum bandwidth cap found
 * @param [in] opt optimal bandwidth cap found
 * @param [in] max maximum bandwidth cap found
 * @param [in] order the caps' letters in the order written, as mba_regs.log
 */
static void
assert_mba_reset_order(const unsigned min,
                       const unsigned opt,
                       const unsigned max,
                       const char *order)
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        int ret;

        erdt.max_clos = 0;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        cpu_agent.marc.control_window_range = 0xFF;
        memset(&mba_regs, 0, sizeof(mba_regs));
        mba_regs.bw[PQOS_BW_CTRL_TYPE_MIN_IDX] = min;
        mba_regs.bw[PQOS_BW_CTRL_TYPE_OPT_IDX] = opt;
        mba_regs.bw[PQOS_BW_CTRL_TYPE_MAX_IDX] = max;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 1);
        expect_mba_reset_reads(&cpu_agent.marc, 1);
        expect_mba_reset_writes(&cpu_agent.marc, 0xFF, 1);

        ret = mmio_alloc_reset_mba();

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_string_equal(mba_regs.log, order);
        assert_false(mba_regs.chain_broken);
}

/**
 * @brief Asserts that a reset whose read of one cap fails returns the error
 *        and writes no cap: the order of the writes is decided from all three
 *        reads, so none is made on two of them
 *
 * @param [in] type PQOS_BW_CTRL_TYPE_*_IDX of the cap whose read fails
 * @param [in] reads the reads made up to the failing one, in the order the
 *             reset makes them: minimum, optimal, maximum
 */
static void
assert_mba_reset_read_fails(const int type, const int reads)
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        int ret;

        erdt.max_clos = 0;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        cpu_agent.marc.control_window_range = 0xFF;
        memset(&mba_regs, 0, sizeof(mba_regs));
        mba_regs.fail_read = 1 + type;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 1);
        expect_value(__wrap_get_mba_min_bw_region_clos_v1, marc,
                     &cpu_agent.marc);
        expect_any(__wrap_get_mba_min_bw_region_clos_v1, region_num);
        expect_any(__wrap_get_mba_min_bw_region_clos_v1, clos_number);
        if (reads > 1) {
                expect_value(__wrap_get_mba_optimal_bw_region_clos_v1, marc,
                             &cpu_agent.marc);
                expect_any(__wrap_get_mba_optimal_bw_region_clos_v1,
                           region_num);
                expect_any(__wrap_get_mba_optimal_bw_region_clos_v1,
                           clos_number);
        }
        if (reads > 2) {
                expect_value(__wrap_get_mba_max_bw_region_clos_v1, marc,
                             &cpu_agent.marc);
                expect_any(__wrap_get_mba_max_bw_region_clos_v1, region_num);
                expect_any(__wrap_get_mba_max_bw_region_clos_v1, clos_number);
        }

        /* no setter expectation: a write would fail the case */
        ret = mmio_alloc_reset_mba();

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(mba_regs.writes, 0);
}

static void
test_mba_reset_stops_at_a_cap_it_cannot_read(void **state
                                             __attribute__((unused)))
{
        assert_mba_reset_read_fails(PQOS_BW_CTRL_TYPE_MIN_IDX, 1);
        assert_mba_reset_read_fails(PQOS_BW_CTRL_TYPE_OPT_IDX, 2);
        assert_mba_reset_read_fails(PQOS_BW_CTRL_TYPE_MAX_IDX, 3);
}

static void
test_mba_reset_keeps_cap_order(void **state __attribute__((unused)))
{
        /* all above Q, as resets that wrote 0x1FF left them: lowered,
         * minimum first
         */
        assert_mba_reset_order(0x1FF, 0x1FF, 0x1FF, "nox");
        /* all below Q: raised, maximum first */
        assert_mba_reset_order(1, 1, 1, "xon");
        /* on both sides of Q: those above lowered, then the rest raised */
        assert_mba_reset_order(1, 0x1FF, 0x1FF, "oxn");
        /* the minimum above the optimal, as restoring the optimal caps alone
         * left them: the minimum is lowered first, which mends that
         */
        assert_mba_reset_order(0x1FF, 0xFF, 0x1FF, "nxo");
        /* at Q already */
        assert_mba_reset_order(0xFF, 0xFF, 0xFF, "xon");
}

static void
test_mba_reset_skips_domain_without_marc(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agents[2] = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        const uint8_t supported = 1;
        const int num_writes = 1 * supported; /* classes * regions, one agent */
        int ret;

        /* domain 10 was enumerated without a MARC block, so it holds no MBA
         * register; domain 20 has one
         */
        erdt.max_clos = 0;
        erdt.num_cpu_agents = DIM(cpu_agents);
        erdt.cpu_agents = cpu_agents;
        cpu_agents[0].rmdd.domain_id = 10;
        cpu_agent_init(&cpu_agents[1], 20);
        cpu_agents[1].marc.control_window_range = 0xFF;
        memset(&mba_regs, 0, sizeof(mba_regs));

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, supported);

        /* the reads and writes are expected against the second agent's MARC
         * only, so one through the zeroed MARC would fail the case
         */
        expect_mba_reset_reads(&cpu_agents[1].marc, num_writes);
        expect_mba_reset_writes(&cpu_agents[1].marc, 0xFF, num_writes);

        ret = mmio_alloc_reset_mba();

        /* the domain that cannot be reset does not take the reset of the whole
         * platform down with it
         */
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_mba_set_rejects_domain_without_marc(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agents[2] = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested = {0};
        int ret;

        erdt.max_clos = 2;
        erdt.num_cpu_agents = DIM(cpu_agents);
        erdt.cpu_agents = cpu_agents;
        cpu_agents[0].rmdd.domain_id = 10;
        cpu_agent_init(&cpu_agents[1], 20);

        mba_request_init(&requested, cpu_agents[0].rmdd.domain_id, 0,
                         MBA_MAX_BW);

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);
        test_log[0] = '\0';

        ret = mmio_mba_set(0, 1, &requested, NULL);

        /* no write is queued, so the request is refused before the registers
         * are reached rather than by a mapping of the zeroed block
         */
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_non_null(strstr(test_log, "carries no MBA registers"));
        assert_non_null(strstr(test_log, "--print-topology"));
        /* the domain that does carry MBA is named, and the one that does not
         * is not offered as somewhere MBA applies: it appears as the domain
         * that was asked for, "Domain ID 0xa", and never as a list entry
         */
        assert_non_null(strstr(test_log, "0x14"));
        assert_null(strstr(test_log, "0xa, "));
}

static void
test_mba_get_rejects_domain_without_marc(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba mba_tab[1] = {0};
        unsigned num_clos = 0;
        int ret;

        erdt.max_clos = 0;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent.rmdd.domain_id = 10;
        mba_tab[0].domain_id = cpu_agent.rmdd.domain_id;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 1);
        test_log[0] = '\0';

        ret = mmio_mba_get(0, 1, &num_clos, mba_tab);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(num_clos, 0);
        assert_non_null(strstr(test_log, "carries no MBA registers"));
        /* the platform has no domain that carries MBA, which the message says
         * rather than offering an empty list
         */
        assert_non_null(strstr(test_log, "reports none"));
}

static void
test_mba_set_rejects_partly_populated_marc(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested = {0};
        int ret;

        erdt.max_clos = 2;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        /* the three bandwidth control types have a base address each, and a
         * request programs all three, so one missing address is as unusable as
         * a block that is missing altogether
         */
        cpu_agent.marc.max_bw_reg_block_base_addr = 0;

        mba_request_init(&requested, cpu_agent.rmdd.domain_id, 0, MBA_MAX_BW);

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);
        test_log[0] = '\0';

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_non_null(strstr(test_log, "carries no MBA registers"));
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

/* A mask is held to the RMDD's Number of IO L3 ways: one way past them is
 * refused with nothing written, and the full mask of those ways is set
 */
static void
test_mmio_l3ca_set_bounds_mask_by_io_l3_ways(void **state
                                             __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca = {0};
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.rmdd.num_io_l3_ways = 4;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 2;

        ca.domain_id = 0x10;
        ca.class_id = 1;
        ca.u.ways_mask = 0x1f;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        ret = mmio_l3ca_set(0, 1, &ca);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_non_null(strstr(test_log, "available Cache Ways 0xf."));

        ca.u.ways_mask = 0xf;

        will_return(__wrap__pqos_get_erdt, &erdt);
        expect_value(__wrap_set_iol3_cbm_clos_v1, card, &dev_agent.card);
        expect_value(__wrap_set_iol3_cbm_clos_v1, clos_number, 1);
        expect_value(__wrap_set_iol3_cbm_clos_v1, value, 0xf);
        will_return(__wrap_set_iol3_cbm_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_l3ca_set(0, 1, &ca);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* the classes read are 0 through the ERDT's Max CLOS, each from its own
 * register, and the count reported is theirs
 */
static void
test_mmio_l3ca_get_reads_every_class(void **state __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca[3] = {{0}};
        unsigned num_ca = 0;
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;
        ca[0].domain_id = 0x10;
        ca[1].domain_id = 0x10;
        /* past the count, a sentinel the read has no business touching */
        ca[2].domain_id = 0x42;
        ca[2].class_id = 7;
        ca[2].cdp = 1;
        ca[2].u.ways_mask = 0xabc;

        will_return(__wrap__pqos_get_erdt, &erdt);
        expect_value_count(__wrap_get_iol3_cbm_clos_v1, card, &dev_agent.card,
                           2);
        expect_value(__wrap_get_iol3_cbm_clos_v1, clos_number, 0);
        expect_value(__wrap_get_iol3_cbm_clos_v1, clos_number, 1);
        expect_value_count(__wrap_get_iol3_cbm_clos_v1, block_number,
                           REG_BLOCK_SIZE_ZERO, 2);
        will_return(__wrap_get_iol3_cbm_clos_v1, 0xf);
        will_return(__wrap_get_iol3_cbm_clos_v1, PQOS_RETVAL_OK);
        will_return(__wrap_get_iol3_cbm_clos_v1, 0x3);
        will_return(__wrap_get_iol3_cbm_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_l3ca_get(0, DIM(ca), &num_ca, ca);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(num_ca, 2);
        assert_int_equal(ca[0].class_id, 0);
        assert_int_equal(ca[0].u.ways_mask, 0xf);
        assert_int_equal(ca[1].class_id, 1);
        assert_int_equal(ca[1].u.ways_mask, 0x3);
        /* the table past the count is left as it was */
        assert_int_equal(ca[2].domain_id, 0x42);
        assert_int_equal(ca[2].class_id, 7);
        assert_int_equal(ca[2].cdp, 1);
        assert_int_equal(ca[2].u.ways_mask, 0xabc);
}

/* a table with fewer entries than the platform has classes is refused before
 * any register is read, rather than filled up to its end
 */
static void
test_mmio_l3ca_get_rejects_short_table(void **state __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca[1] = {{0}};
        unsigned num_ca = 0;
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;
        ca[0].domain_id = 0x10;

        will_return(__wrap__pqos_get_erdt, &erdt);

        ret = mmio_l3ca_get(0, DIM(ca), &num_ca, ca);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(num_ca, 0);
}

/* ======== the last class of the ERDT, and the one above it ======== */

/* Max CLOS names the highest class, so a request for it is written, and one
 * for the class above it is refused before any register is touched
 */
static void
test_mba_set_accepts_the_last_class(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested;
        int ret;

        erdt.max_clos = 1;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);

        mba_request_init(&requested, cpu_agent.rmdd.domain_id, 0, 0x50);
        requested.class_id = erdt.max_clos;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, marc,
                     &cpu_agent.marc);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, region_num, 0);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, clos_number,
                     erdt.max_clos);
        expect_value(__wrap_set_mba_optimal_bw_region_clos_v1, value, 0x50);
        will_return(__wrap_set_mba_optimal_bw_region_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_mba_set_rejects_class_above_max_clos(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested;
        int ret;

        erdt.max_clos = 1;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);

        mba_request_init(&requested, cpu_agent.rmdd.domain_id, 0, 0x50);
        requested.class_id = erdt.max_clos + 1;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_mmio_l3ca_set_accepts_the_last_class(void **state __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca = {0};
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.rmdd.num_io_l3_ways = 4;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;

        ca.domain_id = 0x10;
        ca.class_id = erdt.max_clos;
        ca.u.ways_mask = 0xf;

        will_return(__wrap__pqos_get_erdt, &erdt);
        expect_value(__wrap_set_iol3_cbm_clos_v1, card, &dev_agent.card);
        expect_value(__wrap_set_iol3_cbm_clos_v1, clos_number, erdt.max_clos);
        expect_value(__wrap_set_iol3_cbm_clos_v1, value, 0xf);
        will_return(__wrap_set_iol3_cbm_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_l3ca_set(0, 1, &ca);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_mmio_l3ca_set_rejects_class_above_max_clos(void **state
                                                __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca = {0};
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.rmdd.num_io_l3_ways = 4;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;

        ca.domain_id = 0x10;
        ca.class_id = erdt.max_clos + 1;
        ca.u.ways_mask = 0xf;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        ret = mmio_l3ca_set(0, 1, &ca);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_non_null(
            strstr(test_log, "CLOS2 is out of range (CLOS1 is max)"));
}

/* two classes in one request are both written, each to its own register */
static void
test_mmio_l3ca_set_writes_every_class(void **state __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca[2] = {{0}};
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.rmdd.num_io_l3_ways = 4;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;

        ca[0].domain_id = 0x10;
        ca[0].class_id = 0;
        ca[0].u.ways_mask = 0x3;
        ca[1].domain_id = 0x10;
        ca[1].class_id = 1;
        ca[1].u.ways_mask = 0xc;

        will_return(__wrap__pqos_get_erdt, &erdt);
        expect_value_count(__wrap_set_iol3_cbm_clos_v1, card, &dev_agent.card,
                           2);
        expect_value(__wrap_set_iol3_cbm_clos_v1, clos_number, 0);
        expect_value(__wrap_set_iol3_cbm_clos_v1, clos_number, 1);
        expect_value(__wrap_set_iol3_cbm_clos_v1, value, 0x3);
        expect_value(__wrap_set_iol3_cbm_clos_v1, value, 0xc);
        will_return_count(__wrap_set_iol3_cbm_clos_v1, PQOS_RETVAL_OK, 2);

        ret = mmio_l3ca_set(0, DIM(ca), ca);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* a request is written whole or not at all: an entry refused for its mask
 * leaves the entries before it unwritten too
 */
static void
test_mmio_l3ca_set_writes_nothing_when_an_entry_is_refused(
    void **state __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca[2] = {{0}};
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.rmdd.num_io_l3_ways = 4;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;

        /* the first mask is contiguous, the second is not, which the domain
         * does not allow: no write is queued, so one would fail the case
         */
        ca[0].domain_id = 0x10;
        ca[0].class_id = 0;
        ca[0].u.ways_mask = 0x3;
        ca[1].domain_id = 0x10;
        ca[1].class_id = 1;
        ca[1].u.ways_mask = 0x5;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        ret = mmio_l3ca_set(0, DIM(ca), ca);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_non_null(strstr(test_log, "CLOS1 bit mask is not contiguous"));
}

/* ======== the ERDT's classes against the register blocks ======== */

/* the bounds themselves: a bank of the MBA block holds 64 classes of 8 bytes,
 * banks of four regions follow each other, the block is Register Block Size
 * pages, and no class number wraps into it
 */
static void
test_mba_clos_in_block_bounds(void **state __attribute__((unused)))
{
        struct pqos_erdt_marc marc = {0};

        marc.reg_block_size = 1;
        assert_true(mba_clos_in_block(&marc, 0, 0));
        assert_true(mba_clos_in_block(&marc, 0, 63));
        assert_false(mba_clos_in_block(&marc, 0, 64));
        assert_true(mba_clos_in_block(&marc, PQOS_MAX_MEM_REGIONS - 1, 63));
        assert_false(mba_clos_in_block(&marc, -1, 0));
        assert_false(mba_clos_in_block(&marc, 0, UINT32_MAX));

        marc.reg_block_size = 0;
        assert_false(mba_clos_in_block(&marc, 0, 0));
}

/* the CAT registers of a page start at the CARD's CAT Register Offset and
 * hold 8 bytes per class, every page of the block alike
 */
static void
test_iol3_clos_in_block_bounds(void **state __attribute__((unused)))
{
        struct pqos_erdt_card card = {0};

        card.reg_block_size = 1;
        assert_true(iol3_clos_in_block(&card, 0));
        assert_true(
            iol3_clos_in_block(&card, PAGE_SIZE / BYTES_PER_CLOS_ENTRY - 1));
        assert_false(
            iol3_clos_in_block(&card, PAGE_SIZE / BYTES_PER_CLOS_ENTRY));

        card.cat_reg_offset = PAGE_SIZE - BYTES_PER_CLOS_ENTRY;
        assert_true(iol3_clos_in_block(&card, 0));
        assert_false(iol3_clos_in_block(&card, 1));
        assert_false(iol3_clos_in_block(&card, UINT32_MAX));

        card.reg_block_size = 0;
        assert_false(iol3_clos_in_block(&card, 0));
}

/* the accessors refuse a class past the block before they map it: no mapping
 * is queued, so one would fail the case
 */
static void
test_mba_accessors_refuse_class_past_bank(void **state __attribute__((unused)))
{
        struct pqos_erdt_marc marc = {0};
        unsigned value = 0;

        marc.opt_bw_reg_block_base_addr = 0xf0000000;
        marc.min_bw_reg_block_base_addr = 0xf0001000;
        marc.max_bw_reg_block_base_addr = 0xf0002000;
        marc.reg_block_size = 1;

        assert_int_equal(
            __real_get_mba_optimal_bw_region_clos_v1(&marc, 0, 64, &value),
            PQOS_RETVAL_ERROR);
        assert_int_equal(
            __real_set_mba_optimal_bw_region_clos_v1(&marc, 0, 64, 0x50),
            PQOS_RETVAL_ERROR);
        assert_int_equal(
            __real_get_mba_min_bw_region_clos_v1(&marc, 0, 64, &value),
            PQOS_RETVAL_ERROR);
        assert_int_equal(
            __real_set_mba_min_bw_region_clos_v1(&marc, 0, 64, 0x50),
            PQOS_RETVAL_ERROR);
        assert_int_equal(
            __real_get_mba_max_bw_region_clos_v1(&marc, 0, 64, &value),
            PQOS_RETVAL_ERROR);
        assert_int_equal(
            __real_set_mba_max_bw_region_clos_v1(&marc, 0, 64, 0x50),
            PQOS_RETVAL_ERROR);

        /* nor is a block of no page mapped */
        marc.reg_block_size = 0;
        assert_int_equal(
            __real_get_mba_optimal_bw_region_clos_v1(&marc, 0, 0, &value),
            PQOS_RETVAL_ERROR);
        assert_int_equal(
            __real_set_mba_optimal_bw_region_clos_v1(&marc, 0, 0, 0x50),
            PQOS_RETVAL_ERROR);
        assert_int_equal(
            __real_get_mba_min_bw_region_clos_v1(&marc, 0, 0, &value),
            PQOS_RETVAL_ERROR);
        assert_int_equal(
            __real_set_mba_min_bw_region_clos_v1(&marc, 0, 0, 0x50),
            PQOS_RETVAL_ERROR);
        assert_int_equal(
            __real_get_mba_max_bw_region_clos_v1(&marc, 0, 0, &value),
            PQOS_RETVAL_ERROR);
        assert_int_equal(
            __real_set_mba_max_bw_region_clos_v1(&marc, 0, 0, 0x50),
            PQOS_RETVAL_ERROR);
}

/* and the last class of a bank is written where the indexing puts it */
static void
test_mba_accessor_writes_the_last_class_of_a_bank(void **state
                                                  __attribute__((unused)))
{
        struct pqos_erdt_marc marc = {0};
        uint64_t registers[PAGE_SIZE / sizeof(uint64_t)] = {0};
        int ret;

        marc.opt_bw_reg_block_base_addr = 0xf0000000;
        marc.reg_block_size = 1;

        expect_value(__wrap_pqos_mmap_write, address,
                     marc.opt_bw_reg_block_base_addr);
        expect_value(__wrap_pqos_mmap_write, size, PAGE_SIZE);
        will_return(__wrap_pqos_mmap_write, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);

        ret = __real_set_mba_optimal_bw_region_clos_v1(&marc, 0, 63, 0x50);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(registers[63] & MBA_BW_ALL_BR0_MASK, 0x50);
}

static void
test_iol3_accessors_refuse_class_past_page(void **state __attribute__((unused)))
{
        struct pqos_erdt_card card = {0};
        uint64_t value = 0;

        card.reg_base_addr = 0x3000;
        card.reg_block_size = 1;
        /* CLOS0's register is the last of the page, CLOS1's would lie past */
        card.cat_reg_offset = PAGE_SIZE - BYTES_PER_CLOS_ENTRY;

        assert_int_equal(__real_get_iol3_cbm_clos_v1(&card, 1, 0, &value),
                         PQOS_RETVAL_ERROR);
        assert_int_equal(__real_set_iol3_cbm_clos_v1(&card, 1, 0xf),
                         PQOS_RETVAL_ERROR);

        card.reg_block_size = 0;
        assert_int_equal(__real_get_iol3_cbm_clos_v1(&card, 0, 0, &value),
                         PQOS_RETVAL_ERROR);
}

static void
test_iol3_accessor_reads_the_last_class_of_a_page(void **state
                                                  __attribute__((unused)))
{
        struct pqos_erdt_card card = {0};
        uint64_t registers[PAGE_SIZE / sizeof(uint64_t)] = {0};
        uint64_t value = 0;
        int ret;

        card.reg_base_addr = 0x3000;
        card.reg_block_size = 1;
        card.cat_reg_offset = PAGE_SIZE - BYTES_PER_CLOS_ENTRY;
        registers[PAGE_SIZE / sizeof(uint64_t) - 1] = 0xfULL << IOL3_CBM_SHIFT;

        expect_value(__wrap_pqos_mmap_read, address, card.reg_base_addr);
        expect_value(__wrap_pqos_mmap_read, size, PAGE_SIZE);
        will_return(__wrap_pqos_mmap_read, registers);
        expect_value(__wrap_pqos_munmap, mem, registers);
        expect_value(__wrap_pqos_munmap, size, PAGE_SIZE);

        ret = __real_get_iol3_cbm_clos_v1(&card, 0, 0, &value);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(value, 0xf);
}

/* a get refuses a count that reaches past the block before any read */
static void
test_mba_get_rejects_count_past_bank(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba mba_tab[65] = {{0}};
        unsigned num_clos = 0;
        int ret;

        erdt.max_clos = 64;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        mba_tab[0].domain_id = cpu_agent.rmdd.domain_id;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 1);
        test_log[0] = '\0';

        ret = mmio_mba_get(0, DIM(mba_tab), &num_clos, mba_tab);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(num_clos, 0);
        assert_non_null(strstr(test_log, "CLOS64"));
        assert_non_null(strstr(test_log, "lies past"));
}

static void
test_mmio_l3ca_get_rejects_class_past_page(void **state __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca[2] = {{0}};
        unsigned num_ca = 0;
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        dev_agent.card.cat_reg_offset = PAGE_SIZE - BYTES_PER_CLOS_ENTRY;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;
        ca[0].domain_id = 0x10;
        ca[1].domain_id = 0x10;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        ret = mmio_l3ca_get(0, DIM(ca), &num_ca, ca);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(num_ca, 0);
        assert_non_null(strstr(test_log, "CLOS1"));
        assert_non_null(strstr(test_log, "lies past"));
}

/* a bank of the MBA register block holds 64 classes, so a class past them
 * is refused before any register is touched, however many classes the
 * ERDT's Max CLOS gives the platform
 */
static void
test_mba_set_rejects_class_past_bank(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_mba requested;
        int ret;

        erdt.max_clos = 64;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);

        mba_request_init(&requested, cpu_agent.rmdd.domain_id, 0, MBA_MAX_BW);
        requested.class_id = 64;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, PQOS_MAX_MEM_REGIONS);
        test_log[0] = '\0';

        ret = mmio_mba_set(0, 1, &requested, NULL);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_non_null(strstr(test_log, "CLOS64"));
        assert_non_null(strstr(test_log, "lies past"));
}

/* a table whose field counts no classes is refused before any register */
static void
test_mba_reset_rejects_uncountable_classes(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        int ret;

        erdt.max_clos = UINT32_MAX;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        memset(&mba_regs, 0, sizeof(mba_regs));

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 1);
        test_log[0] = '\0';

        ret = mmio_alloc_reset_mba();

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_non_null(strstr(test_log, "counts no classes"));
}

static void
test_mba_reset_rejects_class_past_bank(void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        int ret;

        erdt.max_clos = 64;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        cpu_agent.marc.control_window_range = 0xFF;
        memset(&mba_regs, 0, sizeof(mba_regs));

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 1);
        test_log[0] = '\0';

        /* no read or write is queued: the reset ends before the registers */
        ret = mmio_alloc_reset_mba();

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_non_null(strstr(test_log, "CLOS64"));
        assert_non_null(strstr(test_log, "lies past"));
}

/* the CAT registers of a page start at the CARD's CAT Register Offset, so a
 * class whose register would lie past the page is refused
 */
static void
test_mmio_l3ca_set_rejects_class_past_page(void **state __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_l3ca ca = {0};
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.rmdd.num_io_l3_ways = 4;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        /* room for CLOS0 alone */
        dev_agent.card.cat_reg_offset = PAGE_SIZE - BYTES_PER_CLOS_ENTRY;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;

        ca.class_id = 1;
        ca.domain_id = 0x10;
        ca.u.ways_mask = 0xf;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        ret = mmio_l3ca_set(0, 1, &ca);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_non_null(strstr(test_log, "CLOS1"));
        assert_non_null(strstr(test_log, "lies past"));
}

static void
test_alloc_reset_cat_rejects_class_past_page(void **state
                                             __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        int ret;

        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.rmdd.num_io_l3_ways = 4;
        dev_agent.card.reg_base_addr = 0xf0000000;
        dev_agent.card.reg_block_size = 1;
        dev_agent.card.cat_reg_offset = PAGE_SIZE - BYTES_PER_CLOS_ENTRY;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        erdt.max_clos = 1;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        /* no write is queued: the reset ends before the registers */
        ret = mmio_alloc_reset_cat();

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_non_null(strstr(test_log, "CLOS1"));
        assert_non_null(strstr(test_log, "lies past"));
}

/* every domain is checked before any is written: a later domain whose classes
 * do not fit stops the reset before the first domain is touched
 */
static void
test_alloc_reset_cat_checks_every_domain_before_writing(void **state
                                                        __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agents[2] = {0};
        struct pqos_erdt_info erdt = {0};
        int ret;

        /* the first domain's page holds every class; the second's registers
         * start so late in the page that CLOS1 would lie past it
         */
        dev_agents[0].rmdd.domain_id = 0x10;
        dev_agents[0].rmdd.num_io_l3_ways = 4;
        dev_agents[0].card.reg_base_addr = 0xf0000000;
        dev_agents[0].card.reg_block_size = 1;
        dev_agents[1].rmdd.domain_id = 0x11;
        dev_agents[1].rmdd.num_io_l3_ways = 4;
        dev_agents[1].card.reg_base_addr = 0xf0001000;
        dev_agents[1].card.reg_block_size = 1;
        dev_agents[1].card.cat_reg_offset = PAGE_SIZE - BYTES_PER_CLOS_ENTRY;
        erdt.num_dev_agents = DIM(dev_agents);
        erdt.dev_agents = dev_agents;
        erdt.max_clos = 1;

        will_return(__wrap__pqos_get_erdt, &erdt);
        test_log[0] = '\0';

        /* no write is queued for either domain */
        ret = mmio_alloc_reset_cat();

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_non_null(strstr(test_log, "0x11"));
}

/* ==== mmio_alloc_reset: every block checked before anything runs ==== */

/* a CAT page that cannot hold the classes stops the whole reset before its MSR
 * part and before any register is written, with the MBA blocks fitting
 */
static void
test_alloc_reset_refuses_before_its_msr_part(void **state
                                             __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_alloc_config cfg = {0};
        int ret;

        erdt.max_clos = 1;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        cpu_agent.marc.control_window_range = 0xFF;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.rmdd.num_io_l3_ways = 4;
        dev_agent.card.reg_base_addr = 0xf0003000;
        dev_agent.card.reg_block_size = 1;
        dev_agent.card.cat_reg_offset = PAGE_SIZE - BYTES_PER_CLOS_ENTRY;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 1);
        memset(&mba_regs, 0, sizeof(mba_regs));
        test_log[0] = '\0';

        /* neither alloc_reset() nor a register read or write is queued */
        ret = mmio_alloc_reset(&cfg);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_non_null(strstr(test_log, "do not fit the register blocks"));
}

/* and an MBA bank that cannot hold them stops it as early, with the CAT page
 * fitting
 */
static void
test_alloc_reset_refuses_class_past_bank_before_its_msr_part(
    void **state __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_alloc_config cfg = {0};
        int ret;

        erdt.max_clos = 64;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        cpu_agent.marc.control_window_range = 0xFF;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.rmdd.num_io_l3_ways = 4;
        dev_agent.card.reg_base_addr = 0xf0003000;
        dev_agent.card.reg_block_size = 1;

        will_return(__wrap__pqos_get_erdt, &erdt);
        mrrm_will_return(&mrrm, 1);
        test_log[0] = '\0';

        ret = mmio_alloc_reset(&cfg);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_non_null(strstr(test_log, "do not fit the register blocks"));
}

/* where every block holds the classes, the reset runs whole: the MSR part
 * first, then every class of every region of the MBA block, then every class
 * of the CAT page
 */
static void
test_alloc_reset_runs_every_part_when_the_classes_fit(void **state
                                                      __attribute__((unused)))
{
        struct pqos_cpu_agent_info cpu_agent = {0};
        struct pqos_device_agent_info dev_agent = {0};
        struct pqos_erdt_info erdt = {0};
        struct pqos_mrrm_info mrrm;
        struct pqos_alloc_config cfg = {0};
        int ret;

        erdt.max_clos = 0;
        erdt.num_cpu_agents = 1;
        erdt.cpu_agents = &cpu_agent;
        cpu_agent_init(&cpu_agent, 10);
        cpu_agent.marc.control_window_range = 0xFF;
        erdt.num_dev_agents = 1;
        erdt.dev_agents = &dev_agent;
        dev_agent.rmdd.domain_id = 0x10;
        dev_agent.rmdd.num_io_l3_ways = 4;
        dev_agent.card.reg_base_addr = 0xf0003000;
        dev_agent.card.reg_block_size = 1;
        memset(&mba_regs, 0, sizeof(mba_regs));

        /* the ERDT is asked for by the reset and by each of its parts, the
         * region count by the reset and by the MBA part
         */
        will_return_count(__wrap__pqos_get_erdt, &erdt, 3);
        mrrm_will_return(&mrrm, 1);
        mrrm_will_return(&mrrm, 1);
        expect_function_call(__wrap_alloc_reset);
        expect_value(__wrap_alloc_reset, cfg, &cfg);
        will_return(__wrap_alloc_reset, PQOS_RETVAL_OK);
        expect_mba_reset_reads(&cpu_agent.marc, 1);
        expect_mba_reset_writes(&cpu_agent.marc, 0xFF, 1);
        expect_value(__wrap_set_iol3_cbm_clos_v1, card, &dev_agent.card);
        expect_value(__wrap_set_iol3_cbm_clos_v1, clos_number, 0);
        expect_value(__wrap_set_iol3_cbm_clos_v1, value, 0xf);
        will_return(__wrap_set_iol3_cbm_clos_v1, PQOS_RETVAL_OK);

        ret = mmio_alloc_reset(&cfg);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_alloc_reset_cat_skips_monitoring_only_domain(void **state
                                                  __attribute__((unused)))
{
        struct pqos_device_agent_info dev_agents[2] = {0};
        struct pqos_erdt_info erdt = {0};
        const unsigned num_writes = 2; /* classes, for the one domain left */
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
        erdt.max_clos = num_writes - 1;

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
            cmocka_unit_test(test_iol3_cbm_set_keeps_reserved_bits),
            cmocka_unit_test(test_cmt_range_rejects_invalid_range),
            cmocka_unit_test(test_mbm_range_rejects_invalid_range),
            cmocka_unit_test(test_mba_set_resolves_domain_id),
            cmocka_unit_test(test_mba_set_accepts_max_bw),
            cmocka_unit_test(test_mba_set_accepts_window_range),
            cmocka_unit_test(test_mba_set_rejects_outside_window_range),
            cmocka_unit_test(test_mba_set_uses_requested_domain_range),
            cmocka_unit_test(test_mba_set_rejects_invalid_request),
            cmocka_unit_test(test_mba_set_rejects_unsupported_region),
            cmocka_unit_test(test_mba_set_validates_before_write),
            cmocka_unit_test(test_mba_get_ignores_num_clos_input),
            cmocka_unit_test(test_mba_get_uses_mrrm_count),
            cmocka_unit_test(test_mba_get_rejects_unsupported_num_regions),
            cmocka_unit_test(test_mba_reset_uses_mrrm_count),
            cmocka_unit_test(test_mba_reset_writes_each_domain_range),
            cmocka_unit_test(test_mba_reset_keeps_cap_order),
            cmocka_unit_test(test_mba_reset_stops_at_a_cap_it_cannot_read),
            cmocka_unit_test(test_mba_reset_skips_domain_without_marc),
            cmocka_unit_test(
                test_mba_reset_without_window_range_writes_field_max),
            cmocka_unit_test(
                test_mba_reset_window_range_above_field_writes_field_max),
            cmocka_unit_test(test_mba_set_rejects_domain_without_marc),
            cmocka_unit_test(test_mba_get_rejects_domain_without_marc),
            cmocka_unit_test(test_mba_set_rejects_partly_populated_marc),
            cmocka_unit_test(test_io_overflow_invalidates_baseline),
            cmocka_unit_test(test_mmio_l3ca_get_rejects_domain_without_l3ca),
            cmocka_unit_test(test_mmio_l3ca_set_rejects_domain_without_l3ca),
            cmocka_unit_test(test_mmio_l3ca_set_rejects_monitoring_only_domain),
            cmocka_unit_test(
                test_mmio_l3ca_set_domain_list_says_when_truncated),
            cmocka_unit_test(test_mmio_l3ca_set_bounds_mask_by_io_l3_ways),
            cmocka_unit_test(test_alloc_reset_cat_skips_monitoring_only_domain),
            cmocka_unit_test(test_mmio_l3ca_get_reads_every_class),
            cmocka_unit_test(test_mmio_l3ca_get_rejects_short_table),
            cmocka_unit_test(test_mba_set_accepts_the_last_class),
            cmocka_unit_test(test_mba_set_rejects_class_above_max_clos),
            cmocka_unit_test(test_mmio_l3ca_set_accepts_the_last_class),
            cmocka_unit_test(test_mmio_l3ca_set_rejects_class_above_max_clos),
            cmocka_unit_test(test_mmio_l3ca_set_writes_every_class),
            cmocka_unit_test(
                test_mmio_l3ca_set_writes_nothing_when_an_entry_is_refused),
            cmocka_unit_test(
                test_alloc_reset_cat_checks_every_domain_before_writing),
            cmocka_unit_test(test_alloc_reset_refuses_before_its_msr_part),
            cmocka_unit_test(
                test_alloc_reset_refuses_class_past_bank_before_its_msr_part),
            cmocka_unit_test(
                test_alloc_reset_runs_every_part_when_the_classes_fit),
            cmocka_unit_test(test_mba_clos_in_block_bounds),
            cmocka_unit_test(test_iol3_clos_in_block_bounds),
            cmocka_unit_test(test_mba_accessors_refuse_class_past_bank),
            cmocka_unit_test(test_mba_accessor_writes_the_last_class_of_a_bank),
            cmocka_unit_test(test_iol3_accessors_refuse_class_past_page),
            cmocka_unit_test(test_iol3_accessor_reads_the_last_class_of_a_page),
            cmocka_unit_test(test_mba_get_rejects_count_past_bank),
            cmocka_unit_test(test_mmio_l3ca_get_rejects_class_past_page),
            cmocka_unit_test(test_mba_set_rejects_class_past_bank),
            cmocka_unit_test(test_mba_reset_rejects_class_past_bank),
            cmocka_unit_test(test_mba_reset_rejects_uncountable_classes),
            cmocka_unit_test(test_mmio_l3ca_set_rejects_class_past_page),
            cmocka_unit_test(test_alloc_reset_cat_rejects_class_past_page)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
