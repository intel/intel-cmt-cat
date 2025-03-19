/*
 * BSD LICENSE
 *
 * Copyright(c) 2025 Intel Corporation. All rights reserved.
 * All rights reserved.
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
 * @brief Internal header file for ERDT module
 */

#ifndef __PQOS_ERDT_H__
#define __PQOS_ERDT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "acpi.h"
#include "pqos.h"
#include "types.h"

#define ACPI_ERDT_REVISION 1
#ifndef ACPI_TABLE_SIG_ERDT
#define ACPI_TABLE_SIG_ERDT "ERDT"
#endif

#define ACPI_ERDT_STRUCT_RMDD_TYPE 0
#define ACPI_ERDT_STRUCT_CACD_TYPE 1
#define ACPI_ERDT_STRUCT_DACD_TYPE 2
#define ACPI_ERDT_STRUCT_CMRC_TYPE 3
#define ACPI_ERDT_STRUCT_MMRC_TYPE 4
#define ACPI_ERDT_STRUCT_MARC_TYPE 5
#define ACPI_ERDT_STRUCT_CARC_TYPE 6
#define ACPI_ERDT_STRUCT_CMRD_TYPE 7
#define ACPI_ERDT_STRUCT_IBRD_TYPE 8
#define ACPI_ERDT_STRUCT_IBAD_TYPE 9
#define ACPI_ERDT_STRUCT_CARD_TYPE 10

#define IMH_MAX_PATH 256

/**
 * Cache Allocation Registers for Device Agents Description Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_card {
        uint16_t type;
        uint16_t length;
        uint8_t reserved[4];
        uint32_t flags;
        uint32_t contention_bitmask;
        uint8_t register_indexing_function_version;
        uint8_t reserved2[11];
        uint64_t register_base_address;
        uint32_t register_block_size;
        uint16_t cache_allocation_rgister_ofsets_for_io;
        uint16_t cache_allocation_register_block_size;
};

/**
 * IO Bandwidth Monitoring Registers for Device Agents Description Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_ibrd {
        uint16_t type;
        uint16_t length;
        uint8_t reserved[4];
        uint32_t flags;
        uint8_t register_indexing_function_version;
        uint8_t reserved2[11];
        uint64_t register_base_address;
        uint32_t register_blockSize;
        uint16_t total_io_bw_registerOffset;
        uint16_t io_miss_bw_registerOffset;
        uint16_t total_io_bwr_register_clumpSize;
        uint16_t io_miss_register_clumpSize;
        uint8_t reserved3[7];
        uint8_t io_bw_counter_width;
        uint64_t io_bw_counter_upscaling_factor;
        uint32_t io_bw_counter_correction_factor_list_length;
        uint32_t io_bw_counter_correction_factor[0];
};

/**
 * Cache Monitoring Registers for Device Agents Description Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_cmrd {
        uint16_t type;
        uint16_t length;
        uint8_t reserved[4];
        uint32_t flags;
        uint8_t register_indexing_function_version;
        uint8_t reserved2[11];
        uint64_t register_base_address;
        uint32_t register_block_size;
        uint16_t cmt_register_offset_for_io;
        uint16_t cmt_register_clump_size_ror_io;
        uint64_t cmt_counter_upscaling_factor;
};

/**
 * Memory-bandwidth Allocation Registers for CPU Agents Description Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_marc {
        uint16_t type;
        uint16_t length;
        uint8_t reserved[2];
        uint16_t mba_flags;
        uint8_t register_indexing_function_version;
        uint8_t reserved2[7];
        uint64_t mba_optimal_bw_register_block_base_address;
        uint64_t mba_minimum_bw_register_block_base_address;
        uint64_t mba_maximum_bw_register_block_base_address;
        uint32_t mba_register_block_size;
        uint32_t mba_bw_control_window_range;
};

/**
 * Memory-bandwidth Monitoring Registers for CPU Agents Description Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_mmrc {
        uint16_t type;
        uint16_t length;
        uint8_t reserved[4];
        uint32_t flags;
        uint8_t register_indexing_function_version;
        uint8_t reserved2[11];
        uint64_t mbm_register_block_base_address;
        uint32_t mbm_register_blockSize;
        uint8_t mbm_counter_width;
        uint64_t mbm_counter_upscaling_factor;
        uint8_t reserved3[7];
        uint32_t mbm_correction_factor_list_length;
        uint32_t mbm_correction_factor[0];
};

/**
 * Cache Monitoring Registers for CPU Agents Description Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_cmrc {
        uint16_t type;
        uint16_t length;
        uint8_t reserved[4];
        uint32_t flags;
        uint8_t register_indexing_function_version;
        uint8_t reserved2[11];
        uint64_t cmt_register_block_base_address_for_cpu;
        uint32_t cmt_register_block_size_for_cpu;
        uint16_t cmt_register_clump_size_for_cpu;
        uint16_t cmt_register_clump_stride_for_cpu;
        uint64_t cmt_counter_upscaling_factor;
};

/**
 * The hierarchical path from the Host Bridge to the device structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_path_entry {
        uint16_t segment;
        uint8_t bus;
        uint8_t device;
        uint8_t function;
};

/**
 * Device Agent Scope Entry (DASE) Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_device_entry {
        uint8_t type;
        uint8_t length;
        uint16_t segmentNumber;
        uint8_t startBusNumber;
        struct acpi_table_erdt_path_entry path[IMH_MAX_PATH];
        uint8_t pathNumber;
};

/**
 * Device Agent Collection Description Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_dacd {
        uint16_t type;
        uint16_t length;
        uint8_t reserved[2];
        uint16_t rmdd_domain_id;
};

/**
 * CPU Agent Collection Description Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_cacd {
        uint16_t type;
        uint16_t length;
        uint8_t reserved[2];
        uint16_t rmdd_domain_id;
        uint32_t enumeration_ids[0];
};

/**
 * Resource Management Domain Description Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_rmdd {
        uint16_t type;
        uint16_t length;
        uint16_t flags;
        uint16_t number_of_io_l3Slices;
        uint8_t number_of_io_l3_sets;
        uint8_t number_of_io_l3_ways;
        uint8_t reserved[8];
        uint16_t domainId;
        uint32_t max_rmids;
        uint64_t control_register_base_address;
        uint16_t control_register_size;
};

/**
 * ERDT CPU Agents Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_rmdd_cpu_agent {
        struct acpi_table_erdt_rmdd rmdd;
        struct acpi_table_erdt_cacd cacd;
        struct acpi_table_erdt_cmrc cmrc;
        struct acpi_table_erdt_mmrc mmrc;
        struct acpi_table_erdt_marc marc;
};

/**
 * ERDT Device Agents Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_rmdd_device_agent {
        struct acpi_table_erdt_rmdd rmdd;
        struct acpi_table_erdt_dacd dacd;
        struct acpi_table_erdt_cmrd cmrd;
        struct acpi_table_erdt_ibrd ibrd;
        struct acpi_table_erdt_card card;
};

/**
 * ERDT Sub-Structures for CPU & Device Agents
 */
struct __attribute__((__packed__)) acpi_table_erdt_sub_structures {
        union {
                struct acpi_table_erdt_rmdd_cpu_agent rmdd_cpu_agent;
                struct acpi_table_erdt_rmdd_device_agent rmdd_device_agent;
        };
};

/**
 * ERDT Top-Level ACPI Structure
 */
struct __attribute__((__packed__)) acpi_table_erdt_header {
        struct acpi_table_header header;
        uint32_t max_clos;
        char reserved[24];
};

/**
 * I/O RDT Top Level Description Table
 * Top-level descriptor table indicating the presence of I/O RDT on the platform
 */
struct __attribute__((__packed__)) acpi_table_erdt {
        struct acpi_table_erdt_header header;
        /**
         * ERDT Sub-structures
         *
         * A list of ERDT Sub-structures. The list will contain following
         * Resource Management Domain Description (RMDD),
         * CPU Agent Collection Description (CACD)
         *
         */
        struct acpi_table_erdt_sub_structures erdt_sub_structures[0];
};

/**
 * @brief Initializes ERDT module
 *
 * Initialize ERDT module
 * Detect ACPI ERDT tables
 * Initialize mmio data
 * Print logs about detected ACPI configuration
 *
 * @param [in] cap capabilities structure
 * @param [in] cpu topology structure
 * @param [out] pqos_erdt pqos_erdt_info structure to be initialized
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int erdt_init(const struct pqos_cap *cap,
                         struct pqos_cpuinfo *cpu,
                         struct pqos_erdt_info **pqos_erdt);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_ERDT_H_ */
