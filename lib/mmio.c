/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2025 Intel Corporation. All rights reserved.
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
 *
 */

/**
 * @brief implementation of MMIO functions.
 */

#include "mmio.h"

#include "common.h"
#include "log.h"
#include "string.h"

#include <inttypes.h>

/* Helper functions for MMIO data retriveal */

static uint64_t *
_get_clos_addr_by_region(uint64_t *mem,
                         unsigned int region_number,
                         unsigned int clos_number)
{
        LOG_INFO("%s(): mem: %p, region_number: %u, clos_number: %u\n",
                 __func__, (const void *)mem, region_number, clos_number);
        return (uint64_t *)((uint8_t *)mem +
                            (region_number / 4) * BYTES_PER_REGION_SET +
                            clos_number * BYTES_PER_CLOS_ENTRY);
}

static int
_get_clos_region_value(uint64_t clos_value,
                       unsigned int region_number,
                       unsigned int *value)
{
        LOG_INFO("%s(): clos_value: %#" PRIx64 ", region_number: %u\n",
                 __func__, clos_value, region_number);

        switch (region_number) {
        case 0:
                *value = (unsigned int)(clos_value & MBA_BW_ALL_BR0_MASK);
                break;
        case 1:
                *value = (unsigned int)((clos_value & MBA_BW_ALL_BR1_MASK) >>
                                        MBA_BW_ALL_BR1_SHIFT);
                break;
        case 2:
                *value = (unsigned int)((clos_value & MBA_BW_ALL_BR2_MASK) >>
                                        MBA_BW_ALL_BR2_SHIFT);
                break;
        case 3:
                *value = (unsigned int)((clos_value & MBA_BW_ALL_BR2_MASK) >>
                                        MBA_BW_ALL_BR2_SHIFT);
                break;
        default:
                LOG_ERROR("%s: wrong region number provided: %u\n", __func__,
                          region_number);
                return PQOS_RETVAL_ERROR;
        }

        LOG_INFO("%s(): output_value: %u\n", __func__, *value);

        return PQOS_RETVAL_OK;
}

static int
_set_clos_region_value(uint64_t *clos_addr,
                       unsigned int region_number,
                       uint64_t value)
{
        uint64_t clos_value = *clos_addr;

        LOG_INFO("%s(): clos addr %p , region_number: %u, value %#" PRIx64 "\n",
                 __func__, (void *)clos_addr, region_number, value);

        switch (region_number) {
        case 0:
                *clos_addr = (uint64_t)(
                    (clos_value & MBA_BW_ALL_BR0_RESET_MASK) | value);
                break;
        case 1:
                *clos_addr =
                    (uint64_t)((clos_value & MBA_BW_ALL_BR1_RESET_MASK) |
                               value << MBA_BW_ALL_BR1_SHIFT);
                break;
        case 2:
                *clos_addr =
                    (uint64_t)((clos_value & MBA_BW_ALL_BR2_RESET_MASK) |
                               value << MBA_BW_ALL_BR2_SHIFT);
                break;
        case 3:
                *clos_addr =
                    (uint64_t)((clos_value & MBA_BW_ALL_BR3_RESET_MASK) |
                               value << MBA_BW_ALL_BR3_SHIFT);
                break;
        default:
                LOG_ERROR("%s: wrong region number provided: %u\n", __func__,
                          region_number);
                return PQOS_RETVAL_ERROR;
        }

        LOG_INFO("%s(): output_value %#" PRIx64 "\n", __func__,
                 *(uint64_t *)clos_addr);

        return PQOS_RETVAL_OK;
}

static int
_copy_generic_rmid_range(unsigned int rmid_first,
                         unsigned int rmid_last,
                         uint64_t *register_base_address,
                         uint16_t register_clump_size,
                         uint16_t register_clump_stride,
                         uint16_t register_offset,
                         void *rmids_val)
{
        unsigned int rmid_count = rmid_last - rmid_first + 1;
        unsigned int rmid_idx, rmid_to_copy;
        uint64_t *cur_clump_addr;

        LOG_INFO("%s(): rmid_first: %u, rmid_last: %u,"
                 " register_base_address: %p,"
                 " register_clump_size: %u,"
                 " register_clump_stride: %u,"
                 " register_offset: %u,"
                 " rmids_val: %p\n",
                 __func__, rmid_first, rmid_last, (void *)register_base_address,
                 register_clump_size, register_clump_stride, register_offset,
                 rmids_val);

        /* Initial clump address */
        cur_clump_addr = (uint64_t *)((uint8_t *)register_base_address +
                                      (rmid_first / register_clump_size) *
                                          register_clump_stride +
                                      register_offset);

        /* First RMID index in the initial clump */
        rmid_idx = rmid_first % register_clump_size;

        /* Initial number of RMIDs in clump to copy */
        rmid_to_copy = register_clump_size - rmid_idx + 1;

        while (rmid_count > 0) {
                if (rmid_count <= rmid_to_copy) {
                        memcpy(rmids_val,
                               (const void *)((uint8_t *)cur_clump_addr +
                                              rmid_idx * BYTES_PER_RMID_ENTRY),
                               rmid_count * BYTES_PER_RMID_ENTRY);
                        break;
                } else {
                        memcpy(rmids_val,
                               (const void *)((uint8_t *)cur_clump_addr +
                                              rmid_idx * BYTES_PER_RMID_ENTRY),
                               rmid_to_copy * BYTES_PER_RMID_ENTRY);

                        rmids_val = (uint64_t *)rmids_val + rmid_to_copy;
                        rmid_count -= rmid_to_copy;
                        rmid_to_copy = register_clump_size;
                        rmid_idx = 0;
                        cur_clump_addr += register_offset;
                }
        }

        return PQOS_RETVAL_OK;
}

/* MMIO data retrieval functions */

int
get_mba_mode_v1(const struct pqos_erdt_rmdd *rmdd, uint64_t *value)
{
        uint64_t *mem;

        mem = (uint64_t *)pqos_mmap_read(rmdd->control_reg_base_addr,
                                         RDT_REG_SIZE);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        *value = *mem;

        pqos_munmap(mem, RDT_REG_SIZE);

        return PQOS_RETVAL_OK;
}

int
set_mba_mode_v1(const struct pqos_erdt_rmdd *rmdd, unsigned int value)
{

        uint64_t *mem;

        ASSERT(value <= 1);

        mem = (uint64_t *)pqos_mmap_write(rmdd->control_reg_base_addr,
                                          RDT_REG_SIZE);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        *mem |= (uint64_t)(value << RDT_CTRL_TME_SHIFT);

        pqos_munmap(mem, RDT_REG_SIZE);

        return PQOS_RETVAL_OK;
}

int
get_l3_cmt_rmid_range_v1(const struct pqos_erdt_cmrc *cmrc,
                         unsigned int rmid_first,
                         unsigned int rmid_last,
                         l3_cmt_rmid_t *rmids_val)
{
        unsigned ret;
        uint64_t *mem;
        uint64_t size = (uint64_t)cmrc->block_size * PAGE_SIZE;

        mem = (uint64_t *)pqos_mmap_read(cmrc->block_base_addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): cmrc: %p, rmid_first: %u, rmid_last: %u, rmids_val:"
                 " %p\n",
                 __func__, (const void *)cmrc, rmid_first, rmid_last,
                 rmids_val);

        LOG_INFO("Base Addr: %#" PRIx64
                 ", Block size in 4k pages: %u, Clump size:"
                 " %u, Clump Stride: %u\n",
                 cmrc->block_base_addr, cmrc->block_size, cmrc->clump_size,
                 cmrc->clump_stride);

        ret = _copy_generic_rmid_range(rmid_first, rmid_last, mem,
                                       cmrc->clump_size, cmrc->clump_stride, 0,
                                       (void *)rmids_val);

        pqos_munmap(mem, size);

        return ret;
}

int
get_l3_mbm_region_rmid_range_v1(const struct pqos_erdt_mmrc *mmrc,
                                unsigned int region_number,
                                unsigned int rmid_first,
                                unsigned int rmid_last,
                                l3_mbm_rmid_t *rmids_val)
{
        unsigned int rmid_count = rmid_last - rmid_first + 1;
        uint64_t rmid_block_addr, rmid_offset;
        uint64_t *mem;
        uint64_t size = (uint64_t)mmrc->reg_block_size * PAGE_SIZE;

        mem = (uint64_t *)pqos_mmap_read(mmrc->reg_block_base_addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): mmrc: %p, rmid_first: %u, rmid_last: %u,"
                 " region_number: %u, rmids_val: %p\n",
                 __func__, (const void *)mmrc, rmid_first, rmid_last,
                 region_number, (void *)rmids_val);

        LOG_INFO("Base Addr: %#" PRIx64 ", Block size in 4k pages: %u\n",
                 mmrc->reg_block_base_addr, mmrc->reg_block_size);

        rmid_block_addr = ((rmid_first % 32) / 8) * 4 * PAGE_SIZE;
        rmid_offset =
            ((((rmid_first / 32) * BYTES_PER_RMID_ENTRY) + rmid_first % 8) *
             BYTES_PER_RMID_ENTRY) +
            (uint64_t)region_number * MBM_REGION_SIZE;

        memcpy(rmids_val,
               (const void *)((uint8_t *)mem + rmid_block_addr + rmid_offset),
               rmid_count * BYTES_PER_RMID_ENTRY);

        pqos_munmap(mem, size);

        return PQOS_RETVAL_OK;
}

int
get_mba_optimal_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                  unsigned int region_number,
                                  unsigned int clos_number,
                                  unsigned int *value)
{
        unsigned int ret;
        uint64_t *mem;
        uint64_t size = (uint64_t)marc->reg_block_size * PAGE_SIZE;

        mem =
            (uint64_t *)pqos_mmap_read(marc->opt_bw_reg_block_base_addr, size);

        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): marc: %p, region_number: %u, clos_number: %u "
                 "value addr: %p\n",
                 __func__, (const void *)marc, region_number, clos_number,
                 (void *)value);

        LOG_INFO("Optimal Base Addr: %#" PRIx64
                 ", Block size in 4k pages: %u\n",
                 marc->opt_bw_reg_block_base_addr, marc->reg_block_size);

        ret = _get_clos_region_value(
            *_get_clos_addr_by_region(mem, region_number, clos_number),
            region_number, value);
        pqos_munmap(mem, size);

        return ret;
}

int
set_mba_optimal_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                  unsigned int region_number,
                                  unsigned int clos_number,
                                  unsigned int value)
{
        unsigned ret;
        uint64_t *mem;

        mem = (uint64_t *)pqos_mmap_write(marc->opt_bw_reg_block_base_addr,
                                          RDT_REG_SIZE);

        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): marc: %p, region_number: %u, clos_number: %u "
                 "value: %u\n",
                 __func__, (const void *)marc, region_number, clos_number,
                 value);

        LOG_INFO("Optimal Base Addr: %#" PRIx64
                 ", Block size in 4k pages: %u\n",
                 marc->opt_bw_reg_block_base_addr, marc->reg_block_size);

        ret = _set_clos_region_value(
            _get_clos_addr_by_region(mem, region_number, clos_number),
            region_number, value);

        pqos_munmap(mem, RDT_REG_SIZE);

        return ret;
}

int
get_mba_min_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                              unsigned int region_number,
                              unsigned int clos_number,
                              unsigned int *value)
{
        unsigned ret;
        uint64_t *mem;
        uint64_t size = (uint64_t)marc->reg_block_size * PAGE_SIZE;

        mem =
            (uint64_t *)pqos_mmap_read(marc->min_bw_reg_block_base_addr, size);

        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): marc: %p, region_number: %u, clos_number: %u "
                 "value addr: %p\n",
                 __func__, (const void *)marc, region_number, clos_number,
                 (void *)value);

        LOG_INFO("Min Base Addr: %#" PRIx64 ", Block size in 4k pages: %u\n",
                 marc->min_bw_reg_block_base_addr, marc->reg_block_size);

        ret = _get_clos_region_value(
            *_get_clos_addr_by_region(mem, region_number, clos_number),
            region_number, value);

        pqos_munmap(mem, size);

        return ret;
}

int
set_mba_min_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                              unsigned int region_number,
                              unsigned int clos_number,
                              unsigned int value)
{
        unsigned ret;
        uint64_t *mem;

        mem = (uint64_t *)pqos_mmap_write(marc->min_bw_reg_block_base_addr,
                                          RDT_REG_SIZE);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): marc: %p, region_number: %u, clos_number: %u "
                 "value: %u\n",
                 __func__, (const void *)marc, region_number, clos_number,
                 value);

        LOG_INFO("Minimal Base Addr: %#" PRIx64
                 ", Block size in 4k pages: %u\n",
                 marc->min_bw_reg_block_base_addr, marc->reg_block_size);

        ret = _set_clos_region_value(
            _get_clos_addr_by_region(mem, region_number, clos_number),
            region_number, value);

        pqos_munmap(mem, RDT_REG_SIZE);

        return ret;
}

int
get_mba_max_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                              unsigned int region_number,
                              unsigned int clos_number,
                              unsigned int *value)
{
        unsigned ret;
        uint64_t *mem;

        mem = (uint64_t *)pqos_mmap_read(marc->max_bw_reg_block_base_addr,
                                         RDT_REG_SIZE);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): marc: %p, region_number: %u, clos_number: %u "
                 "value: %p\n",
                 __func__, (const void *)marc, region_number, clos_number,
                 (void *)value);

        LOG_INFO("Max Base Addr: %#" PRIx64 ", Block size in 4k pages: %u\n",
                 marc->max_bw_reg_block_base_addr, marc->reg_block_size);

        ret = _get_clos_region_value(
            *_get_clos_addr_by_region(mem, region_number, clos_number),
            region_number, value);
        pqos_munmap(mem, RDT_REG_SIZE);

        return ret;
}

int
set_mba_max_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                              unsigned int region_number,
                              unsigned int clos_number,
                              unsigned int value)
{
        unsigned ret;
        uint64_t *mem;

        mem = (uint64_t *)pqos_mmap_write(marc->max_bw_reg_block_base_addr,
                                          RDT_REG_SIZE);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): marc: %p, region_number: %u, clos_number: %u "
                 "value: %u\n",
                 __func__, (const void *)marc, region_number, clos_number,
                 value);

        LOG_INFO("Maximal Base Addr: %#" PRIx64
                 ", Block size in 4k pages: %u\n",
                 marc->max_bw_reg_block_base_addr, marc->reg_block_size);

        ret = _set_clos_region_value(
            _get_clos_addr_by_region(mem, region_number, clos_number),
            region_number, value);

        pqos_munmap(mem, RDT_REG_SIZE);

        return ret;
}

int
get_iol3_cmt_rmid_range_v1(const struct pqos_erdt_cmrd *cmrd,
                           unsigned int rmid_first,
                           unsigned int rmid_last,
                           iol3_cmt_rmid_t *rmids_val)
{
        unsigned ret;
        uint64_t *mem;
        uint64_t size = (uint64_t)cmrd->reg_block_size * PAGE_SIZE;

        mem = (uint64_t *)pqos_mmap_read(cmrd->reg_base_addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): cmrd: %p, rmid_first: %u, rmid_last: %u, rmids_val: "
                 " %p\n",
                 __func__, (const void *)cmrd, rmid_first, rmid_last,
                 (void *)rmids_val);

        LOG_INFO("Base Addr: %#" PRIx64
                 ", Block size in 4k pages: %u, Offset for IO: "
                 " %u, Clump Size: %u\n",
                 cmrd->reg_base_addr, cmrd->reg_block_size, cmrd->offset,
                 cmrd->clump_size);

        ret = _copy_generic_rmid_range(rmid_first, rmid_last, mem,
                                       cmrd->clump_size, PAGE_SIZE,
                                       cmrd->offset, (void *)rmids_val);
        pqos_munmap(mem, size);

        return ret;
}

int
get_total_iol3_mbm_rmid_range_v1(const struct pqos_erdt_ibrd *ibrd,
                                 unsigned int rmid_first,
                                 unsigned int rmid_last,
                                 iol3_mbm_rmid_t *rmids_val)
{
        unsigned ret;
        uint64_t *mem;
        uint64_t size = (uint64_t)ibrd->reg_block_size * PAGE_SIZE;

        mem = (uint64_t *)pqos_mmap_read(ibrd->reg_base_addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): ibrd: %p, rmid_first: %u, rmid_last: %u, rmids_val: "
                 "%p\n",
                 __func__, (const void *)ibrd, rmid_first, rmid_last,
                 (void *)rmids_val);

        LOG_INFO("Base Addr: %#lx, Block size in 4k pages: %u, Total IO "
                 "register offset: %u, Total IO register Clump Size: %u\n",
                 ibrd->reg_base_addr, ibrd->reg_block_size, ibrd->bw_reg_offset,
                 ibrd->bw_reg_clump_size);

        ret = _copy_generic_rmid_range(rmid_first, rmid_last, mem,
                                       ibrd->bw_reg_clump_size, PAGE_SIZE,
                                       ibrd->bw_reg_offset, (void *)rmids_val);
        pqos_munmap(mem, size);

        return ret;
}

int
get_miss_iol3_mbm_rmid_range_v1(const struct pqos_erdt_ibrd *ibrd,
                                unsigned int rmid_first,
                                unsigned int rmid_last,
                                iol3_mbm_rmid_t *rmids_val)
{
        unsigned ret;
        uint64_t *mem;
        uint64_t size = (uint64_t)ibrd->reg_block_size * PAGE_SIZE;

        mem = (uint64_t *)pqos_mmap_read(ibrd->reg_base_addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): ibrd: %p, rmid_first: %u, rmid_last: %u, rmids_val: "
                 "%p\n",
                 __func__, (const void *)ibrd, rmid_first, rmid_last,
                 (void *)rmids_val);

        LOG_INFO("Base Addr: %#" PRIx64
                 ", Block size in 4k pages: %u, IO Miss BW "
                 "register offset: %u, IO miss register Clump Size: %u\n",
                 ibrd->reg_base_addr, ibrd->reg_block_size,
                 ibrd->miss_bw_reg_offset, ibrd->bw_reg_clump_size);

        ret = _copy_generic_rmid_range(
            rmid_first, rmid_last, mem, ibrd->miss_reg_clump_size, PAGE_SIZE,
            ibrd->miss_bw_reg_offset, (void *)rmids_val);

        pqos_munmap(mem, size);

        return ret;
}

int
get_iol3_cbm_clos_v1(const struct pqos_erdt_card *card,
                     unsigned int clos_number,
                     unsigned int block_number,
                     uint64_t *value)
{
        uint64_t *mem;
        uint64_t size = (uint64_t)card->reg_block_size * (uint64_t)PAGE_SIZE;

        mem = (uint64_t *)pqos_mmap_read(card->reg_base_addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): card: %p, clos_number: %u, value: %p\n", __func__,
                 (const void *)card, clos_number, (void *)value);

        LOG_INFO("Base Addr: %#" PRIx64
                 ", Block size in 4k pages: %u\n, CAT register "
                 "4k page offset for IO %u, CAT register block size: %u\n",
                 card->reg_base_addr, card->reg_block_size,
                 card->cat_reg_offset, card->cat_reg_block_size);

        *value = *(uint64_t *)((uint8_t *)mem + card->cat_reg_offset +
                               (clos_number * BYTES_PER_CLOS_ENTRY) +
                               (PAGE_SIZE * block_number));
        *value = *value >> IOL3_CBM_SHIFT;

        pqos_munmap(mem, size);

        return PQOS_RETVAL_OK;
}

int
set_iol3_cbm_clos_v1(const struct pqos_erdt_card *card,
                     unsigned int clos_number,
                     uint64_t value)
{
        uint64_t *clos_addr;
        uint64_t *mem;
        uint64_t size = (uint64_t)card->reg_block_size * (uint64_t)PAGE_SIZE;

        mem = (uint64_t *)pqos_mmap_write(card->reg_base_addr, size);
        if (mem == NULL)
                return PQOS_RETVAL_ERROR;

        LOG_INFO("%s(): card: %p, clos_number: %u, value: %lu\n", __func__,
                 (const void *)card, clos_number, value);

        LOG_INFO("Base Addr: %#" PRIx64
                 ", Block size in 4k pages: %u\n, CAT register "
                 "4k page offset for IO %u, CAT register block size: %u\n",
                 card->reg_base_addr, card->reg_block_size,
                 card->cat_reg_offset, card->cat_reg_block_size);

        if (card->reg_block_size == 0) {
                LOG_ERROR("%s: Register Block Size is 0. "
                          "Unable to write IO L3 CBM.\n",
                          __func__);
                pqos_munmap(mem, RDT_REG_SIZE);
                return PQOS_RETVAL_ERROR;
        }

        for (unsigned i = 0; i < card->reg_block_size; i++) {
                clos_addr = (uint64_t *)((uint8_t *)mem + card->cat_reg_offset +
                                         (clos_number * BYTES_PER_CLOS_ENTRY) +
                                         (PAGE_SIZE * i));
                *clos_addr = value << IOL3_CBM_SHIFT;
        }

        pqos_munmap(mem, size);

        return PQOS_RETVAL_OK;
}

/* Helper functions for handling MMIO registers formats */

uint64_t
l3_cmt_rmid_to_uint64(l3_cmt_rmid_t value)
{
        return value & L3_CMT_RMID_COUNT_MASK;
}

int
is_available_l3_cmt_rmid(l3_cmt_rmid_t value)
{
        return !(value & L3_CMT_RMID_U_MASK);
}

uint64_t
l3_mbm_rmid_to_uint64(l3_mbm_rmid_t value)
{
        return value & MBM_REGION_RMID_COUNT_MASK;
}

int
is_available_l3_mbm_rmid(l3_mbm_rmid_t value)
{
        return !(value & MBM_REGION_RMID_U_MASK);
}

int
is_overflow_l3_mbm_rmid(l3_mbm_rmid_t value)
{
        return value & MBM_REGION_RMID_O_MASK;
}

uint64_t
iol3_cmt_rmid_to_uint64(iol3_cmt_rmid_t value)
{
        return value & IOL3_CMT_RMID_COUNT_MASK;
}

int
is_available_iol3_cmt_rmid(iol3_cmt_rmid_t value)
{
        return !(value & IOL3_CMT_RMID_U_MASK);
}

uint64_t
iol3_mbm_rmid_to_uint64(iol3_mbm_rmid_t value)
{
        return value & TOTAL_IO_BW_RMID_COUNT_MASK;
}

int
is_available_iol3_mbm_rmid(iol3_mbm_rmid_t value)
{
        return !(value & TOTAL_IO_BW_RMID_U_MASK);
}

int
is_overflow_iol3_mbm_rmid(iol3_mbm_rmid_t value)
{
        return value & TOTAL_IO_BW_RMID_O_MASK;
}
