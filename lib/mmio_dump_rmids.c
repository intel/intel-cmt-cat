/*
 * BSD LICENSE
 *
 * Copyright(c) 2025-2026 Intel Corporation. All rights reserved.
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

#include "mmio_dump_rmids.h"

#include "cap.h"
#include "common.h"
#include "log.h"
#include "mmio.h"
#include "mmio_common.h"
#include "pqos.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define UINT64_BINARY_LENGTH 64

/**
 * @brief Convert a 64-bit value to binary text
 *
 * @param [in] value value to convert
 * @param [out] binary null-terminated binary representation
 **/
void
uint64_to_binary(uint64_t value, char binary[UINT64_BINARY_LENGTH + 1])
{
        unsigned i;

        for (i = 0; i < UINT64_BINARY_LENGTH; i++)
                binary[i] = (value & (1ULL << (63 - i))) ? '1' : '0';
        binary[UINT64_BINARY_LENGTH] = '\0';
}

static int
mmio_dump_rmids_cmt(const struct pqos_mmio_dump_rmids *dump_cfg,
                    const struct pqos_erdt_info *erdt)
{
        l3_cmt_rmid_t rmid_raw;
        uint64_t value;
        int ret;
        char binary[UINT64_BINARY_LENGTH + 1];

        printf("RMID CMT DUMP:\n");

        for (unsigned int i = 0; i < dump_cfg->num_domain_ids; i++) {
                int cpu_agent_idx = get_cpu_agent_idx_by_domain_id(
                    erdt, dump_cfg->domain_ids[i]);
                if (cpu_agent_idx == -1)
                        return PQOS_RETVAL_ERROR;
                const struct pqos_erdt_cmrc *cmrc =
                    &erdt->cpu_agents[cpu_agent_idx].cmrc;
                printf("DOMAIN ID: %u\n", dump_cfg->domain_ids[i]);
                for (unsigned int j = 0; j < dump_cfg->num_rmids; j++) {
                        pqos_rmid_t rmid = dump_cfg->rmids[j];

                        ret = get_l3_cmt_rmid_range_v1(cmrc, rmid, rmid,
                                                       &rmid_raw);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;

                        value = (dump_cfg->upscale)
                                    ? scale_llc_value(
                                          cmrc, l3_cmt_rmid_to_uint64(rmid_raw))
                                    : rmid_raw;

                        if (dump_cfg->bin) {
                                uint64_to_binary(value, binary);
                                printf("RMID %04u. Value: %s\n", rmid, binary);
                        } else {
                                printf("RMID %04u. Value: 0x%016" PRIx64 "\n",
                                       rmid, value);
                        }
                }
        }

        return PQOS_RETVAL_OK;
}

static int
mmio_dump_rmids_mbm(const struct pqos_mmio_dump_rmids *dump_cfg,
                    const struct pqos_erdt_info *erdt)
{
        l3_mbm_rmid_t rmid_raw;
        uint64_t value;
        char binary[UINT64_BINARY_LENGTH + 1];
        int ret;

        printf("RMID MBM DUMP:\n");
        for (unsigned int i = 0; i < dump_cfg->num_domain_ids; i++) {
                int cpu_agent_idx = get_cpu_agent_idx_by_domain_id(
                    erdt, dump_cfg->domain_ids[i]);
                if (cpu_agent_idx == -1)
                        return PQOS_RETVAL_ERROR;
                const struct pqos_erdt_mmrc *mmrc =
                    &erdt->cpu_agents[cpu_agent_idx].mmrc;
                printf("DOMAIN ID: %u\n", dump_cfg->domain_ids[i]);
                for (int j = 0; j < dump_cfg->num_mem_regions; j++) {
                        printf("MEM REGION ID: %d\n", dump_cfg->region_num[j]);
                        for (unsigned int k = 0; k < dump_cfg->num_rmids; k++) {
                                pqos_rmid_t rmid = dump_cfg->rmids[k];

                                ret = get_l3_mbm_region_rmid_range_v1(
                                    mmrc, dump_cfg->region_num[j], rmid, rmid,
                                    &rmid_raw);

                                if (ret != PQOS_RETVAL_OK)
                                        return ret;

                                value =
                                    (dump_cfg->upscale)
                                        ? scale_mbm_value(
                                              mmrc, rmid,
                                              l3_mbm_rmid_to_uint64(rmid_raw))
                                        : rmid_raw;

                                if (dump_cfg->bin) {
                                        uint64_to_binary(value, binary);
                                        printf("RMID %04u. Value: %s\n", rmid,
                                               binary);
                                } else {
                                        printf("RMID %04u. Value: 0x%016" PRIx64
                                               "\n",
                                               rmid, value);
                                }
                        }
                }
        }

        return PQOS_RETVAL_OK;
}

static int
mmio_dump_rmids_iol3(const struct pqos_mmio_dump_rmids *dump_cfg,
                     const struct pqos_erdt_info *erdt)
{
        iol3_cmt_rmid_t rmid_raw;
        uint64_t value;
        int ret;
        char binary[UINT64_BINARY_LENGTH + 1];

        printf("RMID IO L3 CMT DUMP:\n");
        for (unsigned int i = 0; i < dump_cfg->num_domain_ids; i++) {
                int dev_agent_idx = get_dev_agent_idx_by_domain_id(
                    erdt, dump_cfg->domain_ids[i]);
                if (dev_agent_idx == -1)
                        return PQOS_RETVAL_ERROR;
                const struct pqos_erdt_cmrd *cmrd =
                    &erdt->dev_agents[dev_agent_idx].cmrd;
                printf("DOMAIN ID: %u\n", dump_cfg->domain_ids[i]);
                for (unsigned int j = 0; j < dump_cfg->num_rmids; j++) {
                        pqos_rmid_t rmid = dump_cfg->rmids[j];

                        ret = get_iol3_cmt_rmid_range_v1(cmrd, rmid, rmid,
                                                         &rmid_raw);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;

                        value =
                            (dump_cfg->upscale)
                                ? scale_io_llc_value(
                                      cmrd, iol3_cmt_rmid_to_uint64(rmid_raw))
                                : rmid_raw;

                        if (dump_cfg->bin) {
                                uint64_to_binary(value, binary);
                                printf("RMID %04u. Value: %s\n", rmid, binary);
                        } else {
                                printf("RMID %04u. Value: 0x%016" PRIx64 "\n",
                                       rmid, value);
                        }
                }
        }

        return PQOS_RETVAL_OK;
}

static int
mmio_dump_rmids_iol3_total(const struct pqos_mmio_dump_rmids *dump_cfg,
                           const struct pqos_erdt_info *erdt)
{
        iol3_mbm_rmid_t rmid_raw;
        uint64_t value;
        int ret;
        char binary[UINT64_BINARY_LENGTH + 1];

        printf("RMID IO L3 TOTAL DUMP:\n");
        for (unsigned int i = 0; i < dump_cfg->num_domain_ids; i++) {
                int dev_agent_idx = get_dev_agent_idx_by_domain_id(
                    erdt, dump_cfg->domain_ids[i]);
                if (dev_agent_idx == -1)
                        return PQOS_RETVAL_ERROR;
                const struct pqos_erdt_ibrd *ibrd =
                    &erdt->dev_agents[dev_agent_idx].ibrd;
                printf("DOMAIN ID: %u\n", dump_cfg->domain_ids[i]);
                for (unsigned int j = 0; j < dump_cfg->num_rmids; j++) {
                        pqos_rmid_t rmid = dump_cfg->rmids[j];

                        ret = get_total_iol3_mbm_rmid_range_v1(ibrd, rmid, rmid,
                                                               &rmid_raw);
                        if (ret != PQOS_RETVAL_OK)
                                return ret;

                        value = (dump_cfg->upscale)
                                    ? scale_io_mbm_value(
                                          ibrd, rmid,
                                          iol3_mbm_rmid_to_uint64(rmid_raw))
                                    : rmid_raw;

                        if (dump_cfg->bin) {
                                uint64_to_binary(value, binary);
                                printf("RMID %04u. Value: %s\n", rmid, binary);
                        } else {
                                printf("RMID %04u. Value: 0x%016" PRIx64 "\n",
                                       rmid, value);
                        }
                }
        }

        return PQOS_RETVAL_OK;
}

static int
mmio_dump_rmids_iol3_miss(const struct pqos_mmio_dump_rmids *dump_cfg,
                          const struct pqos_erdt_info *erdt)
{
        iol3_mbm_rmid_t rmid_raw;
        uint64_t value;
        int ret;
        char binary[UINT64_BINARY_LENGTH + 1];

        printf("RMID IO L3 MISS DUMP:\n");
        for (unsigned int i = 0; i < dump_cfg->num_domain_ids; i++) {
                int dev_agent_idx = get_dev_agent_idx_by_domain_id(
                    erdt, dump_cfg->domain_ids[i]);
                if (dev_agent_idx == -1)
                        return PQOS_RETVAL_ERROR;
                const struct pqos_erdt_ibrd *ibrd =
                    &erdt->dev_agents[dev_agent_idx].ibrd;
                printf("DOMAIN ID: %u\n", dump_cfg->domain_ids[i]);
                for (unsigned int j = 0; j < dump_cfg->num_rmids; j++) {
                        pqos_rmid_t rmid = dump_cfg->rmids[j];

                        ret = get_miss_iol3_mbm_rmid_range_v1(ibrd, rmid, rmid,
                                                              &rmid_raw);

                        if (ret != PQOS_RETVAL_OK)
                                return ret;

                        value = (dump_cfg->upscale)
                                    ? scale_io_mbm_value(
                                          ibrd, rmid,
                                          iol3_mbm_rmid_to_uint64(rmid_raw))
                                    : rmid_raw;

                        if (dump_cfg->bin) {
                                uint64_to_binary(value, binary);
                                printf("RMID %04u. Value: %s\n", rmid, binary);
                        } else {
                                printf("RMID %04u. Value: 0x%016" PRIx64 "\n",
                                       rmid, value);
                        }
                }
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Checks that every domain of a dump holds the RMIDs it reads
 *
 * The L3 CMT and MBM RMIDs are in the CPU domains and the I/O ones in the
 * device domains. Every domain is looked up before any is dumped, so that one
 * the platform lacks is named, rather than the dump stopping after the domains
 * before it with nothing said.
 *
 * @param [in] dump_cfg dump configuration
 * @param [in] erdt ERDT information
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK when every domain holds the RMIDs the dump reads
 * @retval PQOS_RETVAL_ERROR otherwise
 */
static int
mmio_dump_rmids_check_domains(const struct pqos_mmio_dump_rmids *dump_cfg,
                              const struct pqos_erdt_info *erdt)
{
        const int cpu = dump_cfg->rmid_type == MMIO_DUMP_RMID_TYPE_CMT ||
                        dump_cfg->rmid_type == MMIO_DUMP_RMID_TYPE_MBM;

        for (uint32_t i = 0; i < dump_cfg->num_domain_ids; i++) {
                const uint16_t domain_id = dump_cfg->domain_ids[i];
                const int idx =
                    cpu ? get_cpu_agent_idx_by_domain_id(erdt, domain_id)
                        : get_dev_agent_idx_by_domain_id(erdt, domain_id);

                if (idx < 0) {
                        LOG_ERROR("Domain ID %u is not a %s domain. Use "
                                  "--print-topology to list them\n",
                                  domain_id, cpu ? "CPU" : "device");
                        return PQOS_RETVAL_ERROR;
                }
        }

        return PQOS_RETVAL_OK;
}

int
mmio_dump_rmids(const struct pqos_mmio_dump_rmids *dump_cfg)
{
        const struct pqos_erdt_info *erdt;
        int ret;

        if (dump_cfg == NULL)
                return PQOS_RETVAL_PARAM;

        erdt = _pqos_get_erdt();
        if (erdt == NULL || dump_cfg->num_domain_ids == 0 ||
            dump_cfg->domain_ids == NULL || dump_cfg->num_rmids == 0 ||
            dump_cfg->rmids == NULL ||
            dump_cfg->rmid_type > MMIO_DUMP_RMID_IO_MISS || dump_cfg->bin > 1 ||
            dump_cfg->upscale > 1)
                return PQOS_RETVAL_PARAM;

        if (dump_cfg->rmid_type == MMIO_DUMP_RMID_TYPE_MBM) {
                unsigned num_mem_regions;
                int i;

                ret = mmio_get_num_mem_regions(&num_mem_regions);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                if (dump_cfg->num_mem_regions <= 0 ||
                    (unsigned)dump_cfg->num_mem_regions > num_mem_regions)
                        return PQOS_RETVAL_PARAM;

                for (i = 0; i < dump_cfg->num_mem_regions; i++) {
                        const int region_num = dump_cfg->region_num[i];

                        if (region_num < 0 ||
                            (unsigned)region_num >= num_mem_regions) {
                                LOG_ERROR("Memory region %d is out of range "
                                          "(0-%u)!\n",
                                          region_num, num_mem_regions - 1);
                                return PQOS_RETVAL_PARAM;
                        }
                }
        }

        ret = mmio_dump_rmids_check_domains(dump_cfg, erdt);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        switch (dump_cfg->rmid_type) {
        case MMIO_DUMP_RMID_TYPE_CMT:
                return mmio_dump_rmids_cmt(dump_cfg, erdt);
        case MMIO_DUMP_RMID_TYPE_MBM:
                return mmio_dump_rmids_mbm(dump_cfg, erdt);
        case MMIO_DUMP_RMID_IO_L3:
                return mmio_dump_rmids_iol3(dump_cfg, erdt);
        case MMIO_DUMP_RMID_IO_TOTAL:
                return mmio_dump_rmids_iol3_total(dump_cfg, erdt);
        case MMIO_DUMP_RMID_IO_MISS:
                return mmio_dump_rmids_iol3_miss(dump_cfg, erdt);
        default:
                LOG_ERROR("Unsupported RMID type %d\n", dump_cfg->rmid_type);
                return PQOS_RETVAL_PARAM;
        }
}
