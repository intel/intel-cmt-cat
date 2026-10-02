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

/**
 * @brief Internal header file for the memory region description module
 *
 * MRRM says which physical ranges carry which region ID, and nothing else.
 * What a range *is* - ordinary attached memory or a window reserved for CXL -
 * and how fast it is, comes from three other ACPI tables, so this module reads
 * them and correlates them with the ranges MRRM reported:
 *
 *   SRAT  memory affinity entries, which place a range in a proximity domain
 *         and say whether firmware has it enabled;
 *   HMAT  proximity domain attributes and the latency and bandwidth matrices
 *         (SLLBIS), which give the numbers for an initiator-target pair;
 *   CEDT  CXL fixed memory window structures (CFMWS), which describe the host
 *         address windows a CXL device can be mapped into.
 *
 * None of the three is required. A table that is absent, or too short to hold
 * what it declares, leaves the facts it would have contributed unknown rather
 * than wrong, which is why every correlated field carries its own "matched"
 * flag instead of a zero that cannot be told from a measurement.
 *
 * Revisions are not checked to decide where a field is. What is read from each
 * table are the fields that have kept their place across revisions - SRAT's
 * memory affinity entry, HMAT's locality matrices and proximity attributes,
 * CEDT's fixed memory windows - and every read is bounded by the length the
 * structure itself declares. Gating on a revision number for layout would
 * refuse firmware that is newer than this code and laid out exactly as it
 * expects, which is the common case; a structure that is genuinely not what it
 * claims is caught by its length.
 *
 * Units are the exception, and HMAT is where it bites: revision 1 published
 * every locality metric in tenths of the structure's base unit and revision 2
 * publishes latency in picoseconds and bandwidth in MB/s, with no change to
 * where anything sits. A length check cannot catch that - the table is
 * perfectly well formed and means something else - so the revision is read and
 * a revision this module has no units for contributes nothing rather than
 * numbers scaled by the wrong rule.
 */

#ifndef __PQOS_MEM_REGIONS_H__
#define __PQOS_MEM_REGIONS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "acpi.h"
#include "pqos.h"

#include <stddef.h>

#ifndef ACPI_TABLE_SIG_SRAT
#define ACPI_TABLE_SIG_SRAT "SRAT"
#endif
#ifndef ACPI_TABLE_SIG_HMAT
#define ACPI_TABLE_SIG_HMAT "HMAT"
#endif
#ifndef ACPI_TABLE_SIG_CEDT
#define ACPI_TABLE_SIG_CEDT "CEDT"
#endif
/* Where the table would be if the platform has one. Needed because a CEDT that
 * is absent and a CEDT that cannot be read mean different things here: the
 * first says the platform publishes no CXL windows, the second leaves its
 * windows unknown
 */
#define ACPI_CEDT_TABLE (ACPI_TABLE_FS_PATH "/" ACPI_TABLE_SIG_CEDT)

/* SRAT sub-table types */
#define ACPI_SRAT_TYPE_MEMORY_AFFINITY 1
/* SRAT memory affinity flags */
#define ACPI_SRAT_MEM_ENABLED 0x1

/* HMAT proximity domain attribute validity: the initiator field is reserved
 * unless the table says it is valid, so a pair must not be built from it
 */
#define ACPI_HMAT_INITIATOR_VALID 0x1
/* And the same for the memory domain, in revision 1. Revision 2 deprecated the
 * bit and means the field whether or not it is set, so requiring it there would
 * refuse conformant tables - this one is asked of revision 1 alone
 */
#define ACPI_HMAT_MEMORY_VALID 0x2

/* HMAT structure types */
#define ACPI_HMAT_TYPE_PROXIMITY_DOMAIN 0
#define ACPI_HMAT_TYPE_LOCALITY         1
#define ACPI_HMAT_TYPE_CACHE            2
/* HMAT locality data types */
#define ACPI_HMAT_ACCESS_LATENCY   0
#define ACPI_HMAT_READ_LATENCY     1
#define ACPI_HMAT_WRITE_LATENCY    2
#define ACPI_HMAT_ACCESS_BANDWIDTH 3
#define ACPI_HMAT_READ_BANDWIDTH   4
#define ACPI_HMAT_WRITE_BANDWIDTH  5
/* The memory hierarchy a locality structure describes is the low nibble of its
 * flags: 0 is the memory itself, anything else is a level of memory side cache,
 * whose numbers are not what a caller asking about a region wants.
 */
#define ACPI_HMAT_HIERARCHY_MASK   0x0f
#define ACPI_HMAT_HIERARCHY_MEMORY 0
/* Set where the structure's numbers hold only for transfers of at least the
 * minimum transfer size it names. Clear means they hold for any transfer, which
 * is the broader statement and the one to report where a platform carries both
 */
#define ACPI_HMAT_MIN_TRANSFER_SIZE 0x10
/* Set where the structure's numbers describe non-sequential transfers, which is
 * the other way a structure can qualify what it publishes
 */
#define ACPI_HMAT_NON_SEQUENTIAL_TRANSFERS 0x20
/* The revisions of HMAT whose units this module knows. Revision 1 published
 * every metric in tenths of the structure's base unit; revision 2 changed
 * latency to picoseconds and bandwidth to MB/s. The structures kept their
 * layout across the two, so the fields are read the same way either way - but
 * the numbers in them mean different things, which is why this one revision
 * check exists where the rest of the module has none
 */
#define ACPI_HMAT_REVISION_TENTHS      1
#define ACPI_HMAT_REVISION_PICOSECONDS 2
#define ACPI_HMAT_TENTHS_PER_UNIT      10
/* A revision 2 latency entry counts picoseconds and the report states
 * nanoseconds
 */
#define ACPI_HMAT_PS_PER_NS 1000
/* A matrix entry of 0xffff says no value is available for that pair, so it is
 * a sentinel and not a measurement to be scaled by the base unit
 */
#define ACPI_HMAT_VALUE_UNAVAILABLE 0xffff

/* CEDT structure types */
#define ACPI_CEDT_TYPE_CHBS  0
#define ACPI_CEDT_TYPE_CFMWS 1

/**
 * SRAT table header: the fixed part before the sub-tables
 */
struct __attribute__((__packed__)) acpi_table_srat {
        struct acpi_table_header header;
        uint32_t table_revision;
        uint64_t reserved;
};

/**
 * SRAT sub-table header, common to every entry type
 */
struct __attribute__((__packed__)) acpi_srat_entry {
        uint8_t type;
        uint8_t length;
};

/**
 * SRAT Memory Affinity structure
 */
struct __attribute__((__packed__)) acpi_srat_memory {
        uint8_t type;
        uint8_t length;
        uint32_t proximity_domain;
        uint16_t reserved;
        uint32_t base_address_low;
        uint32_t base_address_high;
        uint32_t length_low;
        uint32_t length_high;
        uint32_t reserved2;
        uint32_t flags;
        uint64_t reserved3;
};

/**
 * HMAT table header: the fixed part before the structures
 */
struct __attribute__((__packed__)) acpi_table_hmat {
        struct acpi_table_header header;
        uint32_t reserved;
};

/**
 * HMAT structure header, common to every type
 */
struct __attribute__((__packed__)) acpi_hmat_entry {
        uint16_t type;
        uint16_t reserved;
        uint32_t length;
};

/**
 * HMAT Memory Proximity Domain Attributes structure
 */
struct __attribute__((__packed__)) acpi_hmat_proximity {
        struct acpi_hmat_entry entry;
        uint16_t flags;
        uint16_t reserved;
        uint32_t initiator_domain;
        uint32_t target_domain;
        uint8_t reserved2[20];
};

/* The proximity structure is 40 bytes: the three trailing reserved fields are
 * 4, 8 and 8, the last two being the address range that ACPI 6.3 deprecated
 * without shortening the structure. Only the fields up to the target domain are
 * read here, and that is all a record has to carry to be used - requiring the
 * whole 40 would refuse a record that holds everything this needs.
 */
#define ACPI_HMAT_PROXIMITY_MIN_LENGTH                                         \
        (offsetof(struct acpi_hmat_proximity, reserved2))

/**
 * HMAT System Locality Latency and Bandwidth Information structure
 *
 * The initiator list, the target list and the matrix follow in that order and
 * are sized by the two counts, so they cannot be members.
 */
struct __attribute__((__packed__)) acpi_hmat_locality {
        struct acpi_hmat_entry entry;
        uint8_t flags;
        uint8_t data_type;
        uint8_t min_transfer_size;
        uint8_t reserved;
        uint32_t num_initiators;
        uint32_t num_targets;
        uint32_t reserved2;
        uint64_t entry_base_unit;
};

/**
 * HMAT Memory Side Cache Information structure
 *
 * The SMBIOS handles that describe the cache's hardware follow the fixed
 * fields and are counted by num_smbios_handles, so they cannot be members.
 * Nothing here reads them: what the report states is the cache's size and what
 * kind of cache the attributes say it is.
 */
struct __attribute__((__packed__)) acpi_hmat_cache {
        struct acpi_hmat_entry entry;
        uint32_t memory_domain;
        uint32_t reserved;
        uint64_t cache_size;
        uint32_t attributes;
        uint16_t reserved2;
        uint16_t num_smbios_handles;
};

/* Every field this reads is inside the fixed part, so a structure holding that
 * much is usable whatever it says about SMBIOS handles
 */
#define ACPI_HMAT_CACHE_MIN_LENGTH sizeof(struct acpi_hmat_cache)

/* The cache attributes word, as ACPI lays it out: four nibbles and then the
 * line size in bytes
 */
#define ACPI_HMAT_CACHE_TOTAL_LEVELS(a)  ((a) & 0xfU)
#define ACPI_HMAT_CACHE_LEVEL(a)         (((a) >> 4) & 0xfU)
#define ACPI_HMAT_CACHE_ASSOCIATIVITY(a) (((a) >> 8) & 0xfU)
#define ACPI_HMAT_CACHE_WRITE_POLICY(a)  (((a) >> 12) & 0xfU)
#define ACPI_HMAT_CACHE_LINE_SIZE(a)     (((a) >> 16) & 0xffffU)

/**
 * CEDT structure header, common to every type
 */
struct __attribute__((__packed__)) acpi_cedt_entry {
        uint8_t type;
        uint8_t reserved;
        uint16_t length;
};

/**
 * CEDT CXL Fixed Memory Window structure
 */
struct __attribute__((__packed__)) acpi_cedt_cfmws {
        struct acpi_cedt_entry entry;
        uint32_t reserved;
        uint64_t base_hpa;
        uint64_t window_size;
        uint8_t encoded_interleave_ways;
        uint8_t interleave_arithmetic;
        uint16_t reserved2;
        uint32_t host_bridge_interleave_granularity;
        uint16_t window_restrictions;
        uint16_t qtg_id;
};

/**
 * @brief Describes the memory regions MRRM reported
 *
 * Reads SRAT, HMAT and CEDT where they are present and correlates them with the
 * ranges in @a mrrm, grouping those ranges by local region ID.
 *
 * A table carrying no ranges is described successfully, as a structure with no
 * regions and no ranges: how region IDs are assigned and how many regions the
 * platform supports are still known, and they are what the report can state.
 * A correlating table that is absent, or too damaged to walk, is not a failure
 * either: it contributes nothing, and the regions are described without what it
 * would have said - which is what the per-field match flags are for.
 *
 * @param [in] mrrm the ranges to describe, as MRRM reported them
 * @param [out] regions structure to allocate and fill
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success, including a table with no ranges in it and a
 *         correlating table that could not be read
 * @retval PQOS_RETVAL_RESOURCE out of memory
 * @retval PQOS_RETVAL_PARAM a parameter was NULL
 */
PQOS_LOCAL int mem_regions_init(const struct pqos_mrrm_info *mrrm,
                                struct pqos_mem_regions **regions);

/**
 * @brief Releases what mem_regions_init() allocated
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_LOCAL int mem_regions_fini(void);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_MEM_REGIONS_H__ */
