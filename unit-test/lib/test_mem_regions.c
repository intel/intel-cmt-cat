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

/*
 * The memory region description, against tables built here rather than read
 * from a platform.
 *
 * Almost nothing below can be reached on hardware. A board publishes the tables
 * its firmware has: one revision of HMAT, well formed, with the fields its
 * platform happens to use. What this module spends most of its code on is the
 * rest - a revision whose units differ, a structure that does not hold what it
 * declares, a window describing no addresses, two proximity domains claiming
 * the same range - and every one of those answers was arrived at by editing a
 * copy of one board's tables by hand. These cases are those edits, written
 * down: same shapes, same expected answers, no board required.
 *
 * The tables are assembled by the helpers below, which keep each header's
 * length field in step as structures are appended, so a case says what it is
 * about and nothing else.
 */

#include "mem_regions.h"
#include "test.h"

#define SRAT_ROOM 1024
#define HMAT_ROOM 2048
#define CEDT_ROOM 1024
#define MRE_ROOM  8

/** A platform's three correlating tables, and the ranges to describe */
struct fixture {
        _Alignas(uint64_t) uint8_t srat[SRAT_ROOM];
        _Alignas(uint64_t) uint8_t hmat[HMAT_ROOM];
        _Alignas(uint64_t) uint8_t cedt[CEDT_ROOM];
        struct acpi_table srat_table;
        struct acpi_table hmat_table;
        struct acpi_table cedt_table;

        /** whether the loader finds each table at all */
        int srat_found;
        int hmat_found;
        int cedt_found;
        /** what the loader's two file system questions answer */
        int sysfs_dir;
        int cedt_file;

        struct pqos_mrrm_info mrrm;
        struct pqos_mre_info mre[MRE_ROOM];
};

/* the wrapped loader has no argument to carry the fixture in, so the case
 * being run publishes it here. One case runs at a time
 */
static struct fixture *m_fixture;

struct acpi_table *
__wrap_acpi_get_sig(const char *sig)
{
        if (strncmp(sig, ACPI_TABLE_SIG_SRAT, 4) == 0)
                return m_fixture->srat_found ? &m_fixture->srat_table : NULL;
        if (strncmp(sig, ACPI_TABLE_SIG_HMAT, 4) == 0)
                return m_fixture->hmat_found ? &m_fixture->hmat_table : NULL;
        if (strncmp(sig, ACPI_TABLE_SIG_CEDT, 4) == 0)
                return m_fixture->cedt_found ? &m_fixture->cedt_table : NULL;

        return NULL;
}

void
__wrap_acpi_free(struct acpi_table *table)
{
        assert_non_null(table);
}

int
__wrap_pqos_file_exists(const char *path)
{
        assert_non_null(path);

        return m_fixture->cedt_file;
}

int
__wrap_pqos_dir_exists(const char *path)
{
        assert_non_null(path);

        return m_fixture->sysfs_dir;
}

/**
 * @brief Starts a table: a header of the given length, and nothing in it
 */
static void
table_start(struct acpi_table *table,
            uint8_t *buffer,
            size_t header,
            const char *signature,
            uint8_t revision)
{
        struct acpi_table_header *h = (struct acpi_table_header *)buffer;

        table->generic = buffer;
        memcpy(h->signature, signature, sizeof(h->signature));
        h->revision = revision;
        h->length = (uint32_t)header;
}

/**
 * @brief The next free byte of a table, which is where a structure is appended
 */
static uint8_t *
table_end(uint8_t *buffer)
{
        const struct acpi_table_header *h =
            (const struct acpi_table_header *)buffer;

        return buffer + h->length;
}

/**
 * @brief Records that a structure of that many bytes was appended
 */
static void
table_grew(uint8_t *buffer, size_t bytes)
{
        struct acpi_table_header *h = (struct acpi_table_header *)buffer;

        h->length += (uint32_t)bytes;
}

/**
 * @brief Appends an SRAT memory affinity entry
 */
static void
srat_memory(struct fixture *f,
            uint64_t base,
            uint64_t length,
            unsigned domain,
            int enabled)
{
        struct acpi_srat_memory *m =
            (struct acpi_srat_memory *)table_end(f->srat);

        memset(m, 0, sizeof(*m));
        m->type = ACPI_SRAT_TYPE_MEMORY_AFFINITY;
        m->length = sizeof(*m);
        m->proximity_domain = domain;
        m->base_address_low = (uint32_t)base;
        m->base_address_high = (uint32_t)(base >> 32);
        m->length_low = (uint32_t)length;
        m->length_high = (uint32_t)(length >> 32);
        m->flags = enabled ? ACPI_SRAT_MEM_ENABLED : 0;
        table_grew(f->srat, sizeof(*m));
}

/**
 * @brief Appends an entry of a type this module does not read
 *
 * There to be walked over: a table is not only the entries that interest us,
 * and the walk has to carry on past the ones that do not.
 */
static void
srat_other(struct fixture *f, uint8_t length)
{
        struct acpi_srat_entry *e =
            (struct acpi_srat_entry *)table_end(f->srat);

        memset(e, 0, length);
        e->type = 0xf0;
        e->length = length;
        table_grew(f->srat, length);
}

/**
 * @brief Appends an HMAT memory proximity domain attributes structure
 */
static void
hmat_proximity(struct fixture *f,
               unsigned initiator,
               unsigned target,
               uint16_t flags)
{
        struct acpi_hmat_proximity *p =
            (struct acpi_hmat_proximity *)table_end(f->hmat);

        memset(p, 0, sizeof(*p));
        p->entry.type = ACPI_HMAT_TYPE_PROXIMITY_DOMAIN;
        p->entry.length = sizeof(*p);
        p->flags = flags;
        p->initiator_domain = initiator;
        p->target_domain = target;
        table_grew(f->hmat, sizeof(*p));
}

/**
 * @brief Appends an HMAT memory side cache information structure
 *
 * @param [in] f the fixture
 * @param [in] domain the memory proximity domain the cache is declared for
 * @param [in] size its size in bytes
 * @param [in] attributes ACPI's attributes word, as the macros lay it out
 */
static void
hmat_cache(struct fixture *f,
           unsigned domain,
           uint64_t size,
           uint32_t attributes)
{
        struct acpi_hmat_cache *c =
            (struct acpi_hmat_cache *)table_end(f->hmat);

        memset(c, 0, sizeof(*c));
        c->entry.type = ACPI_HMAT_TYPE_CACHE;
        c->entry.length = sizeof(*c);
        c->memory_domain = domain;
        c->cache_size = size;
        c->attributes = attributes;
        table_grew(f->hmat, sizeof(*c));
}

/**
 * @brief Appends an HMAT locality matrix
 *
 * @return where in the table it was put, for a case that wants to spoil it
 */
static size_t
hmat_locality(struct fixture *f,
              uint8_t data_type,
              uint8_t flags,
              uint8_t min_transfer,
              uint64_t base_unit,
              const uint32_t *initiators,
              uint32_t num_initiators,
              const uint32_t *targets,
              uint32_t num_targets,
              const uint16_t *values)
{
        uint8_t *at = table_end(f->hmat);
        struct acpi_hmat_locality *l = (struct acpi_hmat_locality *)at;
        const size_t names =
            ((size_t)num_initiators + num_targets) * sizeof(uint32_t);
        const size_t cells =
            (size_t)num_initiators * num_targets * sizeof(uint16_t);
        const size_t whole = sizeof(*l) + names + cells;
        uint8_t *cursor = at + sizeof(*l);

        memset(at, 0, whole);
        l->entry.type = ACPI_HMAT_TYPE_LOCALITY;
        l->entry.length = (uint32_t)whole;
        l->flags = flags;
        l->data_type = data_type;
        l->min_transfer_size = min_transfer;
        l->num_initiators = num_initiators;
        l->num_targets = num_targets;
        l->entry_base_unit = base_unit;

        memcpy(cursor, initiators, (size_t)num_initiators * sizeof(uint32_t));
        cursor += (size_t)num_initiators * sizeof(uint32_t);
        memcpy(cursor, targets, (size_t)num_targets * sizeof(uint32_t));
        cursor += (size_t)num_targets * sizeof(uint32_t);
        memcpy(cursor, values, cells);

        table_grew(f->hmat, whole);

        return (size_t)(at - f->hmat);
}

/**
 * @brief Appends a CEDT CXL fixed memory window
 *
 * @param [in] ways the encoded interleave ways the window declares
 * @param [in] targets how many target UIDs to append after it, which a
 *             conformant window matches to the encoding
 */
static void
cedt_window(struct fixture *f,
            uint64_t base,
            uint64_t size,
            uint8_t ways,
            unsigned targets)
{
        uint8_t *at = table_end(f->cedt);
        struct acpi_cedt_cfmws *w = (struct acpi_cedt_cfmws *)at;
        const size_t whole = sizeof(*w) + targets * sizeof(uint32_t);

        memset(at, 0, whole);
        w->entry.type = ACPI_CEDT_TYPE_CFMWS;
        w->entry.length = (uint16_t)whole;
        w->base_hpa = base;
        w->window_size = size;
        w->encoded_interleave_ways = ways;
        table_grew(f->cedt, whole);
}

/**
 * @brief Appends a CEDT structure of a type this module does not read
 */
static void
cedt_other(struct fixture *f, uint16_t length)
{
        struct acpi_cedt_entry *e =
            (struct acpi_cedt_entry *)table_end(f->cedt);

        memset(e, 0, length);
        e->type = ACPI_CEDT_TYPE_CHBS;
        e->length = length;
        table_grew(f->cedt, length);
}

/**
 * @brief Adds a range to the MRRM information the description is built from
 */
static void
mrrm_range(struct fixture *f,
           uint64_t base,
           uint64_t length,
           uint8_t local_id,
           int local_valid)
{
        struct pqos_mre_info *m = &f->mre[f->mrrm.num_mres];

        memset(m, 0, sizeof(*m));
        m->base_address_low = (uint32_t)base;
        m->base_address_high = (uint32_t)(base >> 32);
        m->length_low = (uint32_t)length;
        m->length_high = (uint32_t)(length >> 32);
        m->local_region_id = local_id;
        m->region_id_flags = local_valid ? PQOS_MRE_VALID_LOCAL_REGION_ID : 0;
        f->mrrm.num_mres++;
}

/* the ranges of the platform these cases are modelled on: two of ordinary
 * memory under one region ID, and one window's worth of CXL space under
 * another
 */
#define DDR_LOW_BASE  0x0ULL
#define DDR_LOW_SIZE  0xc0000000ULL
#define DDR_HIGH_BASE 0x100000000ULL
#define DDR_HIGH_SIZE 0x1f00000000ULL
#define CXL_BASE      0x6200000000ULL
#define CXL_SIZE      0x4000000000ULL

#define DDR_DOMAIN 0
#define CXL_DOMAIN 1

/* one initiator and both targets, which is the shape of the board's matrices */
static const uint32_t m_initiators[] = {DDR_DOMAIN};
static const uint32_t m_targets[] = {DDR_DOMAIN, CXL_DOMAIN};
/* 90 and 130 nanoseconds, 256 and 30 gigabytes a second, in the units
 * revision 2 states them in
 */
static const uint16_t m_latency[] = {90, 130};
static const uint16_t m_bandwidth[] = {256, 30};

/**
 * @brief Empties the fixture and points the loader at it
 */
static void
init(struct fixture *f)
{
        memset(f, 0, sizeof(*f));
        m_fixture = f;
        f->mrrm.max_memory_regions_supported = 4;
        f->mrrm.mre = f->mre;
        f->sysfs_dir = 1;
        f->cedt_file = 1;
}

/**
 * @brief Builds the three tables of a well described platform
 *
 * Two regions: ordinary memory that SRAT places in domain 0 and HMAT has
 * numbers for, and a CXL window that SRAT places in domain 1, CEDT covers, and
 * HMAT has its own numbers for. Cases that are about one table's failings start
 * here and spoil that one table.
 */
/**
 * @brief The proximity pairs and the locality numbers HMAT carries for them
 *
 * Separate from platform() so that a case can build a platform whose HMAT
 * describes a domain without pairing an initiator with it - which is what a
 * memory side cache on its own is.
 *
 * @param [in,out] f the fixture being built
 */
static void
platform_hmat_localities(struct fixture *f)
{
        hmat_proximity(f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_proximity(f, DDR_DOMAIN, CXL_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(f, ACPI_HMAT_READ_LATENCY, 0, 0, ACPI_HMAT_PS_PER_NS,
                      m_initiators, 1, m_targets, 2, m_latency);
        hmat_locality(f, ACPI_HMAT_WRITE_LATENCY, 0, 0, ACPI_HMAT_PS_PER_NS,
                      m_initiators, 1, m_targets, 2, m_latency);
        hmat_locality(f, ACPI_HMAT_READ_BANDWIDTH, 0, 0, 1024, m_initiators, 1,
                      m_targets, 2, m_bandwidth);
        hmat_locality(f, ACPI_HMAT_WRITE_BANDWIDTH, 0, 0, 1024, m_initiators, 1,
                      m_targets, 2, m_bandwidth);
}

static void
platform_tables(struct fixture *f, int with_hmat_pairs)
{
        init(f);

        mrrm_range(f, DDR_LOW_BASE, DDR_LOW_SIZE, 0, 1);
        mrrm_range(f, DDR_HIGH_BASE, DDR_HIGH_SIZE, 0, 1);
        mrrm_range(f, CXL_BASE, CXL_SIZE, 1, 1);

        f->srat_found = 1;
        table_start(&f->srat_table, f->srat, sizeof(struct acpi_table_srat),
                    ACPI_TABLE_SIG_SRAT, 3);
        srat_memory(f, DDR_LOW_BASE, DDR_LOW_SIZE, DDR_DOMAIN, 1);
        srat_memory(f, DDR_HIGH_BASE, DDR_HIGH_SIZE, DDR_DOMAIN, 1);
        srat_memory(f, CXL_BASE, CXL_SIZE, CXL_DOMAIN, 1);

        f->hmat_found = 1;
        table_start(&f->hmat_table, f->hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        if (with_hmat_pairs)
                platform_hmat_localities(f);

        f->cedt_found = 1;
        table_start(&f->cedt_table, f->cedt, sizeof(struct acpi_table_header),
                    ACPI_TABLE_SIG_CEDT, 1);
        cedt_other(f, 32);
        cedt_window(f, CXL_BASE, CXL_SIZE, 0, 1);
}

/** the platform every case starts from */
static void
platform(struct fixture *f)
{
        platform_tables(f, 1);
}

/**
 * @brief The same platform, with HMAT pairing no initiator with any target
 *
 * HMAT is present and says nothing about which initiator reaches which target,
 * so a cache structure added afterwards is the only thing it says about the
 * domain - which is the state a region with a memory side cache and no locality
 * figures is in.
 *
 * @param [out] f the fixture
 */
static void
platform_without_hmat_pairs(struct fixture *f)
{
        platform_tables(f, 0);
}

/**
 * @brief The region carrying a local region ID, or NULL
 */
static const struct pqos_mem_region *
region_of(const struct pqos_mem_regions *regions, uint8_t local_id)
{
        unsigned i;

        for (i = 0; i < regions->num_regions; i++)
                if (regions->region[i].local_region_id == local_id)
                        return &regions->region[i];

        return NULL;
}

/**
 * @brief Builds the description, which every case here expects to succeed
 */
static struct pqos_mem_regions *
describe(struct fixture *f)
{
        struct pqos_mem_regions *regions = NULL;

        assert_int_equal(mem_regions_init(&f->mrrm, &regions), PQOS_RETVAL_OK);
        assert_non_null(regions);

        return regions;
}

static void
test_parameters(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *regions = NULL;

        init(&f);
        assert_int_equal(mem_regions_init(NULL, &regions), PQOS_RETVAL_PARAM);
        assert_int_equal(mem_regions_init(&f.mrrm, NULL), PQOS_RETVAL_PARAM);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_describes_a_platform(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_region *ddr;
        const struct pqos_mem_region *cxl;

        platform(&f);
        r = describe(&f);

        assert_int_equal(r->num_range_entries, 3);
        assert_int_equal(r->num_regions, 2);
        assert_int_equal(r->max_regions_supported, 4);
        assert_int_equal(r->srat_available, 1);
        assert_int_equal(r->hmat_available, 1);
        assert_int_equal(r->cedt_available, 1);
        assert_int_equal(r->cedt_complete, 1);

        /* the ranges, in table order, with what MRRM said about each */
        assert_int_equal(r->range[0].base_address, DDR_LOW_BASE);
        assert_int_equal(r->range[0].length, DDR_LOW_SIZE);
        assert_int_equal(r->range[0].local_region_id_valid, 1);
        assert_int_equal(r->range[2].base_address, CXL_BASE);
        assert_int_equal(r->range[2].remote_region_id_valid, 0);

        ddr = region_of(r, 0);
        assert_non_null(ddr);
        assert_int_equal(ddr->type, PQOS_MEM_REGION_LOCAL);
        assert_int_equal(ddr->num_ranges, 2);
        assert_int_equal(ddr->total_size_valid, 1);
        assert_int_equal(ddr->total_size, DDR_LOW_SIZE + DDR_HIGH_SIZE);
        assert_int_equal(ddr->srat_match, 1);
        assert_int_equal(ddr->hmat_match, 1);
        assert_int_equal(ddr->cedt_match, 0);
        assert_int_equal(ddr->proximity_valid, 1);
        assert_int_equal(ddr->target_domain, DDR_DOMAIN);
        assert_int_equal(ddr->initiator_domain, DDR_DOMAIN);
        assert_int_equal(ddr->locality.valid, 1);
        assert_int_equal(ddr->locality.read_latency_valid, 1);
        assert_int_equal(ddr->locality.read_latency_ns, 90);
        assert_int_equal(ddr->locality.write_latency_ns, 90);
        assert_int_equal(ddr->locality.read_bandwidth_mbs, 256 * 1024);
        assert_int_equal(ddr->locality.write_bandwidth_mbs, 256 * 1024);
        assert_int_equal(ddr->locality.read_latency_min_transfer_qualified, 0);
        assert_int_equal(ddr->locality.read_latency_non_sequential, 0);

        cxl = region_of(r, 1);
        assert_non_null(cxl);
        assert_int_equal(cxl->type, PQOS_MEM_REGION_CXL);
        assert_int_equal(cxl->cedt_match, 1);
        assert_int_equal(cxl->cfmws_match, 1);
        assert_int_equal(cxl->cxl_range_match, 1);
        assert_int_equal(cxl->active_mres, 1);
        assert_int_equal(cxl->reserved_mres, 0);
        assert_int_equal(cxl->unclassified_mres, 0);
        assert_int_equal(cxl->target_domain, CXL_DOMAIN);
        assert_int_equal(cxl->locality.read_latency_ns, 130);
        assert_int_equal(cxl->locality.read_bandwidth_mbs, 30 * 1024);

        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_ranges_without_regions(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;

        /* a header and nothing under it: the two answers the header carries are
         * still answers, and the tables are still looked for
         */
        platform(&f);
        f.mrrm.num_mres = 0;
        r = describe(&f);
        assert_int_equal(r->num_regions, 0);
        assert_int_equal(r->num_range_entries, 0);
        assert_int_equal(r->max_regions_supported, 4);
        assert_int_equal(r->srat_available, 1);
        assert_int_equal(r->hmat_available, 1);
        assert_int_equal(r->cedt_available, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* ranges the platform declined to place in a region belong to none of
         * them, and are still reported
         */
        platform(&f);
        f.mre[0].region_id_flags = 0;
        f.mre[1].region_id_flags = 0;
        f.mre[2].region_id_flags = 0;
        r = describe(&f);
        assert_int_equal(r->num_regions, 0);
        assert_int_equal(r->num_range_entries, 3);
        assert_int_equal(r->range[1].local_region_id_valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_srat_collective_coverage(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_region *ddr;

        /* SRAT is fragmented by construction - the holes under 1M and under 4G
         * mean an ordinary platform describes its low memory in several entries
         * of one domain - so coverage is asked of them collectively
         */
        platform(&f);
        table_start(&f.srat_table, f.srat, sizeof(struct acpi_table_srat),
                    ACPI_TABLE_SIG_SRAT, 3);
        srat_memory(&f, DDR_LOW_BASE, DDR_LOW_SIZE, DDR_DOMAIN, 1);
        srat_memory(&f, DDR_HIGH_BASE, 0x100000000ULL, DDR_DOMAIN, 1);
        srat_memory(&f, DDR_HIGH_BASE + 0x100000000ULL,
                    DDR_HIGH_SIZE - 0x100000000ULL, DDR_DOMAIN, 1);
        srat_memory(&f, CXL_BASE, CXL_SIZE, CXL_DOMAIN, 1);
        r = describe(&f);
        ddr = region_of(r, 0);
        assert_int_equal(ddr->srat_match, 1);
        assert_int_equal(ddr->type, PQOS_MEM_REGION_LOCAL);
        assert_int_equal(ddr->locality.read_latency_ns, 90);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* the same entries with an address between them that no entry of the
         * domain describes: not covered, and nothing is claimed
         */
        platform(&f);
        table_start(&f.srat_table, f.srat, sizeof(struct acpi_table_srat),
                    ACPI_TABLE_SIG_SRAT, 3);
        srat_memory(&f, DDR_LOW_BASE, DDR_LOW_SIZE, DDR_DOMAIN, 1);
        srat_memory(&f, DDR_HIGH_BASE, 0x100000000ULL, DDR_DOMAIN, 1);
        srat_memory(&f, DDR_HIGH_BASE + 0x200000000ULL,
                    DDR_HIGH_SIZE - 0x200000000ULL, DDR_DOMAIN, 1);
        r = describe(&f);
        ddr = region_of(r, 0);
        assert_int_equal(ddr->srat_match, 0);
        assert_int_equal(ddr->hmat_match, 0);
        assert_int_equal(ddr->type, PQOS_MEM_REGION_UNKNOWN);
        assert_int_equal(ddr->locality.valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_srat_one_domain_only(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_region *cxl;

        /* two domains covering the same range: it belongs to neither, and
         * taking the first would attach one domain's numbers to the other's
         * memory
         */
        platform(&f);
        srat_memory(&f, CXL_BASE, CXL_SIZE, 7, 1);
        r = describe(&f);
        cxl = region_of(r, 1);
        assert_int_equal(cxl->srat_match, 0);
        assert_int_equal(cxl->hmat_match, 0);
        assert_int_equal(cxl->locality.valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* and a second domain over part of the range is the same
         * contradiction, about those addresses
         */
        platform(&f);
        srat_memory(&f, CXL_BASE + CXL_SIZE / 2, CXL_SIZE / 2, 7, 1);
        r = describe(&f);
        cxl = region_of(r, 1);
        assert_int_equal(cxl->srat_match, 0);
        assert_int_equal(cxl->locality.valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* a disabled entry of another domain is not a contradiction: it
         * describes space the platform has not brought up
         */
        platform(&f);
        srat_memory(&f, CXL_BASE, CXL_SIZE, 7, 0);
        r = describe(&f);
        cxl = region_of(r, 1);
        assert_int_equal(cxl->srat_match, 1);
        assert_int_equal(cxl->target_domain, CXL_DOMAIN);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_region_in_two_domains(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_region *ddr;

        /* one region, its two ranges in different domains: there is no single
         * target to hang the numbers on, so the type does not claim local
         * memory either
         */
        platform(&f);
        table_start(&f.srat_table, f.srat, sizeof(struct acpi_table_srat),
                    ACPI_TABLE_SIG_SRAT, 3);
        srat_memory(&f, DDR_LOW_BASE, DDR_LOW_SIZE, DDR_DOMAIN, 1);
        srat_memory(&f, DDR_HIGH_BASE, DDR_HIGH_SIZE, 5, 1);
        srat_memory(&f, CXL_BASE, CXL_SIZE, CXL_DOMAIN, 1);
        r = describe(&f);
        ddr = region_of(r, 0);
        assert_int_equal(ddr->srat_match, 0);
        assert_int_equal(ddr->type, PQOS_MEM_REGION_UNKNOWN);
        assert_int_equal(ddr->proximity_valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_window_counts(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_region *cxl;

        /* one domain's disabled entries accounting for the whole range is
         * firmware reserving the window
         */
        platform(&f);
        table_start(&f.srat_table, f.srat, sizeof(struct acpi_table_srat),
                    ACPI_TABLE_SIG_SRAT, 3);
        srat_memory(&f, DDR_LOW_BASE, DDR_LOW_SIZE, DDR_DOMAIN, 1);
        srat_memory(&f, DDR_HIGH_BASE, DDR_HIGH_SIZE, DDR_DOMAIN, 1);
        srat_memory(&f, CXL_BASE, CXL_SIZE, CXL_DOMAIN, 0);
        r = describe(&f);
        cxl = region_of(r, 1);
        assert_int_equal(cxl->active_mres, 0);
        assert_int_equal(cxl->reserved_mres, 1);
        assert_int_equal(cxl->unclassified_mres, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* enabled over one part and disabled over the rest is neither: a table
         * describing two different things, not a reservation
         */
        platform(&f);
        table_start(&f.srat_table, f.srat, sizeof(struct acpi_table_srat),
                    ACPI_TABLE_SIG_SRAT, 3);
        srat_memory(&f, DDR_LOW_BASE, DDR_LOW_SIZE, DDR_DOMAIN, 1);
        srat_memory(&f, DDR_HIGH_BASE, DDR_HIGH_SIZE, DDR_DOMAIN, 1);
        srat_memory(&f, CXL_BASE, CXL_SIZE / 2, CXL_DOMAIN, 1);
        srat_memory(&f, CXL_BASE + CXL_SIZE / 2, CXL_SIZE / 2, CXL_DOMAIN, 0);
        r = describe(&f);
        cxl = region_of(r, 1);
        assert_int_equal(cxl->active_mres, 0);
        assert_int_equal(cxl->reserved_mres, 0);
        assert_int_equal(cxl->unclassified_mres, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* and a window SRAT says nothing about is unclassified rather than
         * reserved: silence is not a statement
         */
        platform(&f);
        table_start(&f.srat_table, f.srat, sizeof(struct acpi_table_srat),
                    ACPI_TABLE_SIG_SRAT, 3);
        srat_memory(&f, DDR_LOW_BASE, DDR_LOW_SIZE, DDR_DOMAIN, 1);
        srat_memory(&f, DDR_HIGH_BASE, DDR_HIGH_SIZE, DDR_DOMAIN, 1);
        r = describe(&f);
        cxl = region_of(r, 1);
        assert_int_equal(cxl->active_mres, 0);
        assert_int_equal(cxl->reserved_mres, 0);
        assert_int_equal(cxl->unclassified_mres, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_windows_cover_collectively(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_region *cxl;

        /* three adjacent windows over one range: no single window contains it,
         * and together they account for every address
         */
        platform(&f);
        table_start(&f.cedt_table, f.cedt, sizeof(struct acpi_table_header),
                    ACPI_TABLE_SIG_CEDT, 1);
        cedt_window(&f, CXL_BASE, CXL_SIZE / 4, 0, 1);
        cedt_window(&f, CXL_BASE + CXL_SIZE / 4, CXL_SIZE / 4, 0, 1);
        cedt_window(&f, CXL_BASE + CXL_SIZE / 2, CXL_SIZE / 2, 0, 1);
        r = describe(&f);
        cxl = region_of(r, 1);
        assert_int_equal(cxl->cedt_match, 1);
        assert_int_equal(cxl->cfmws_match, 1);
        assert_int_equal(cxl->cxl_range_match, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* the same windows with a gap between two of them: the region is still
         * CXL space, and its coverage is not complete
         */
        platform(&f);
        table_start(&f.cedt_table, f.cedt, sizeof(struct acpi_table_header),
                    ACPI_TABLE_SIG_CEDT, 1);
        cedt_window(&f, CXL_BASE, CXL_SIZE / 4, 0, 1);
        cedt_window(&f, CXL_BASE + CXL_SIZE / 2, CXL_SIZE / 2, 0, 1);
        r = describe(&f);
        cxl = region_of(r, 1);
        assert_int_equal(cxl->cedt_match, 1);
        assert_int_equal(cxl->cxl_range_match, 0);
        assert_int_equal(cxl->type, PQOS_MEM_REGION_CXL);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_window_refused(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_region *ddr;
        const struct pqos_mem_region *cxl;
        unsigned i;

        /* three ways a window cannot be read: fewer target UIDs than its
         * encoding declares, a reserved encoding, and no addresses at all. Each
         * leaves the platform's windows unaccounted for, so a region no window
         * covers is of an unknown type rather than ordinary memory
         */
        for (i = 0; i < 3; i++) {
                platform(&f);
                table_start(&f.cedt_table, f.cedt,
                            sizeof(struct acpi_table_header),
                            ACPI_TABLE_SIG_CEDT, 1);
                if (i == 0)
                        cedt_window(&f, CXL_BASE, CXL_SIZE, 1, 1);
                else if (i == 1)
                        cedt_window(&f, CXL_BASE, CXL_SIZE, 5, 1);
                else
                        cedt_window(&f, CXL_BASE, 0, 0, 1);

                r = describe(&f);
                assert_int_equal(r->cedt_available, 1);
                assert_int_equal(r->cedt_complete, 0);
                ddr = region_of(r, 0);
                cxl = region_of(r, 1);
                assert_int_equal(ddr->type, PQOS_MEM_REGION_UNKNOWN);
                assert_int_equal(cxl->cedt_match, 0);
                assert_int_equal(cxl->type, PQOS_MEM_REGION_UNKNOWN);
                assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
        }

        /* a window of twenty-four ways, which the encoding does not reach */
        platform(&f);
        table_start(&f.cedt_table, f.cedt, sizeof(struct acpi_table_header),
                    ACPI_TABLE_SIG_CEDT, 1);
        cedt_window(&f, CXL_BASE, CXL_SIZE, 11, 24);
        r = describe(&f);
        assert_int_equal(r->cedt_complete, 0);
        assert_int_equal(region_of(r, 1)->cedt_match, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_cedt_truncated_keeps_its_windows(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        struct acpi_cedt_entry *last;

        /* a window that was read cannot be taken back by one that was not, so
         * CEDT keeps its readable prefix - and says the windows are no longer
         * all accounted for
         */
        platform(&f);
        cedt_window(&f, CXL_BASE + CXL_SIZE, CXL_SIZE, 0, 1);
        last = (struct acpi_cedt_entry *)(f.cedt + f.cedt_table.header->length -
                                          (sizeof(struct acpi_cedt_cfmws) +
                                           sizeof(uint32_t)));
        last->length += 64;
        r = describe(&f);
        assert_int_equal(r->cedt_available, 1);
        assert_int_equal(r->cedt_complete, 0);
        assert_int_equal(region_of(r, 1)->cedt_match, 1);
        assert_int_equal(region_of(r, 1)->cxl_range_match, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_cedt_absence(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;

        /* no CEDT, and the kernel's listing there to be absent from: the
         * platform publishes no windows, and a region SRAT has enabled is what
         * SRAT says it is
         */
        platform(&f);
        f.cedt_found = 0;
        f.sysfs_dir = 1;
        f.cedt_file = 0;
        r = describe(&f);
        assert_int_equal(r->cedt_available, 0);
        assert_int_equal(r->cedt_complete, 1);
        assert_int_equal(region_of(r, 0)->type, PQOS_MEM_REGION_LOCAL);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* the file is there and could not be read: the platform has CXL and
         * which ranges sit in windows is unknown
         */
        platform(&f);
        f.cedt_found = 0;
        f.sysfs_dir = 1;
        f.cedt_file = 1;
        r = describe(&f);
        assert_int_equal(r->cedt_available, 0);
        assert_int_equal(r->cedt_complete, 0);
        assert_int_equal(region_of(r, 0)->type, PQOS_MEM_REGION_UNKNOWN);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* and without the listing the tables are found by scanning memory,
         * where nothing coming back is not evidence of absence
         */
        platform(&f);
        f.cedt_found = 0;
        f.sysfs_dir = 0;
        f.cedt_file = 0;
        r = describe(&f);
        assert_int_equal(r->cedt_available, 0);
        assert_int_equal(r->cedt_complete, 0);
        assert_int_equal(region_of(r, 0)->type, PQOS_MEM_REGION_UNKNOWN);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_srat_malformed_drops_the_table(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        struct acpi_srat_entry *first;
        unsigned i;

        /* three ways SRAT cannot be walked, and each costs the table: what the
         * unread bytes would have said can contradict what was kept
         */
        for (i = 0; i < 3; i++) {
                platform(&f);
                first =
                    (struct acpi_srat_entry *)(f.srat +
                                               sizeof(struct acpi_table_srat));
                if (i == 0)
                        first->length = (uint8_t)(f.srat_table.header->length);
                else if (i == 1)
                        table_grew(f.srat, 1);
                else
                        first->length = 2;

                r = describe(&f);
                assert_int_equal(r->srat_available, 0);
                assert_int_equal(region_of(r, 0)->srat_match, 0);
                assert_int_equal(region_of(r, 0)->type,
                                 PQOS_MEM_REGION_UNKNOWN);
                assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
        }

        /* an entry of a type this module does not read is walked over, its
         * length being none of this module's business
         */
        platform(&f);
        srat_other(&f, 24);
        srat_memory(&f, 0x8000000000ULL, 0x1000000ULL, 9, 1);
        r = describe(&f);
        assert_int_equal(r->srat_available, 1);
        assert_int_equal(region_of(r, 0)->srat_match, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_hmat_malformed_drops_the_table(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        struct acpi_hmat_entry *first;
        unsigned i;

        /* the same rule for HMAT, and for the same reason: a structure beyond
         * the break can outrank one that was kept
         */
        for (i = 0; i < 4; i++) {
                platform(&f);
                first =
                    (struct acpi_hmat_entry *)(f.hmat +
                                               sizeof(struct acpi_table_hmat));
                if (i == 0)
                        first->length = f.hmat_table.header->length + 64;
                else if (i == 1)
                        table_grew(f.hmat, 4);
                else if (i == 2)
                        first->length = 8;
                else {
                        /* a structure whose length fits the table and not the
                         * matrix it declares: one more target than it holds,
                         * which the walk's own bounds check cannot see
                         */
                        const size_t at =
                            hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0,
                                          ACPI_HMAT_PS_PER_NS, m_initiators, 1,
                                          m_targets, 2, m_latency);
                        struct acpi_hmat_locality *spoilt =
                            (struct acpi_hmat_locality *)(f.hmat + at);

                        spoilt->num_targets++;
                }

                r = describe(&f);
                assert_int_equal(r->hmat_available, 0);
                assert_int_equal(region_of(r, 0)->hmat_match, 0);
                assert_int_equal(region_of(r, 0)->locality.valid, 0);
                /* SRAT still answers, so the region still has its domain */
                assert_int_equal(region_of(r, 0)->srat_match, 1);
                assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
        }
}

static void
test_hmat_revisions(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_region *ddr;

        /* revision 1 states every metric in tenths of the base unit. The same
         * physical figures as the revision 2 platform, expressed its way
         */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_TENTHS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN,
                       ACPI_HMAT_INITIATOR_VALID | ACPI_HMAT_MEMORY_VALID);
        hmat_proximity(&f, DDR_DOMAIN, CXL_DOMAIN,
                       ACPI_HMAT_INITIATOR_VALID | ACPI_HMAT_MEMORY_VALID);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0,
                      ACPI_HMAT_TENTHS_PER_UNIT, m_initiators, 1, m_targets, 2,
                      m_latency);
        hmat_locality(&f, ACPI_HMAT_READ_BANDWIDTH, 0, 0,
                      ACPI_HMAT_TENTHS_PER_UNIT * 1024, m_initiators, 1,
                      m_targets, 2, m_bandwidth);
        r = describe(&f);
        ddr = region_of(r, 0);
        assert_int_equal(r->hmat_available, 1);
        assert_int_equal(ddr->locality.read_latency_ns, 90);
        assert_int_equal(ddr->locality.read_bandwidth_mbs, 256 * 1024);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* revision 1 reserves the memory domain field unless it says otherwise,
         * and a pair built from a reserved field invents a target
         */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_TENTHS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0,
                      ACPI_HMAT_TENTHS_PER_UNIT, m_initiators, 1, m_targets, 2,
                      m_latency);
        r = describe(&f);
        ddr = region_of(r, 0);
        assert_int_equal(r->hmat_available, 1);
        assert_int_equal(ddr->hmat_match, 0);
        assert_int_equal(ddr->locality.valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* revision 2 deprecated that bit and means the field either way */
        platform(&f);
        r = describe(&f);
        assert_int_equal(region_of(r, 0)->hmat_match, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* a revision whose units are unknown contributes nothing, rather than
         * figures scaled by another revision's rule
         */
        platform(&f);
        f.hmat_table.header->revision = ACPI_HMAT_REVISION_PICOSECONDS + 1;
        r = describe(&f);
        assert_int_equal(r->hmat_available, 0);
        assert_int_equal(region_of(r, 0)->hmat_match, 0);
        assert_int_equal(region_of(r, 0)->locality.valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_hmat_scaling(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_locality *loc;
        static const uint16_t small[] = {1, 1};
        static const uint16_t large[] = {60000, 60000};

        /* a measurement smaller than the unit the report states must not
         * arrive as zero: zero is what this module uses for "no number"
         */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0, 1, m_initiators, 1,
                      m_targets, 2, small);
        r = describe(&f);
        loc = &region_of(r, 0)->locality;
        assert_int_equal(loc->read_latency_valid, 1);
        assert_int_equal(loc->read_latency_ns, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* a product that leaves the type while the quotient fits: the divide is
         * folded into the multiply, so this is reported rather than refused
         */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0, 1ULL << 63,
                      m_initiators, 1, m_targets, 2, m_latency);
        r = describe(&f);
        loc = &region_of(r, 0)->locality;
        assert_int_equal(loc->read_latency_valid, 1);
        assert_int_equal(loc->read_latency_ns,
                         (1ULL << 63) / ACPI_HMAT_PS_PER_NS * 90 +
                             ((1ULL << 63) % ACPI_HMAT_PS_PER_NS * 90 +
                              ACPI_HMAT_PS_PER_NS - 1) /
                                 ACPI_HMAT_PS_PER_NS);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* and a result that genuinely does not fit is not a figure at all */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0, UINT64_MAX,
                      m_initiators, 1, m_targets, 2, large);
        r = describe(&f);
        loc = &region_of(r, 0)->locality;
        assert_int_equal(loc->read_latency_valid, 0);
        assert_int_equal(loc->values_unrepresentable, 1);
        assert_int_equal(loc->values_unavailable, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_hmat_no_number(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_locality *loc;
        static const uint16_t unavailable[] = {ACPI_HMAT_VALUE_UNAVAILABLE,
                                               ACPI_HMAT_VALUE_UNAVAILABLE};
        static const uint16_t zero[] = {0, 0};

        /* the platform's two ways of saying it has no number for a pair, both
         * of which are the absence of a measurement rather than one
         */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0, ACPI_HMAT_PS_PER_NS,
                      m_initiators, 1, m_targets, 2, unavailable);
        r = describe(&f);
        loc = &region_of(r, 0)->locality;
        assert_int_equal(region_of(r, 0)->hmat_match, 1);
        assert_int_equal(loc->valid, 0);
        assert_int_equal(loc->read_latency_valid, 0);
        assert_int_equal(loc->read_latency_ns, 0);
        assert_int_equal(loc->values_unavailable, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0, ACPI_HMAT_PS_PER_NS,
                      m_initiators, 1, m_targets, 2, zero);
        r = describe(&f);
        loc = &region_of(r, 0)->locality;
        assert_int_equal(loc->read_latency_valid, 0);
        assert_int_equal(loc->values_unavailable, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* a structure with no base unit has no number in it either, and is
         * refused on its own while the table stands
         */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0, 0, m_initiators, 1,
                      m_targets, 2, m_latency);
        hmat_locality(&f, ACPI_HMAT_READ_BANDWIDTH, 0, 0, 1024, m_initiators, 1,
                      m_targets, 2, m_bandwidth);
        r = describe(&f);
        loc = &region_of(r, 0)->locality;
        assert_int_equal(r->hmat_available, 1);
        assert_int_equal(loc->read_latency_valid, 0);
        assert_int_equal(loc->read_bandwidth_valid, 1);
        assert_int_equal(loc->read_bandwidth_mbs, 256 * 1024);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_hmat_selection(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_locality *loc;
        static const uint16_t generic[] = {70, 70};
        static const uint16_t refused[] = {ACPI_HMAT_VALUE_UNAVAILABLE,
                                           ACPI_HMAT_VALUE_UNAVAILABLE};
        static const uint16_t other[] = {200, 200};
        unsigned i;

        /* a generic access structure stands in for both directions, and a
         * structure naming a direction overrides it
         */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(&f, ACPI_HMAT_ACCESS_LATENCY, 0, 0, ACPI_HMAT_PS_PER_NS,
                      m_initiators, 1, m_targets, 2, generic);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0, ACPI_HMAT_PS_PER_NS,
                      m_initiators, 1, m_targets, 2, m_latency);
        r = describe(&f);
        loc = &region_of(r, 0)->locality;
        assert_int_equal(loc->read_latency_ns, 90);
        assert_int_equal(loc->write_latency_ns, 70);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* and a direction-specific structure that refuses a number takes the
         * generic figure back rather than leaving it to be read as a read
         * latency
         */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(&f, ACPI_HMAT_ACCESS_LATENCY, 0, 0, ACPI_HMAT_PS_PER_NS,
                      m_initiators, 1, m_targets, 2, generic);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0, ACPI_HMAT_PS_PER_NS,
                      m_initiators, 1, m_targets, 2, refused);
        r = describe(&f);
        loc = &region_of(r, 0)->locality;
        assert_int_equal(loc->read_latency_valid, 0);
        assert_int_equal(loc->read_latency_ns, 0);
        assert_int_equal(loc->write_latency_valid, 1);
        assert_int_equal(loc->write_latency_ns, 70);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* two structures of one kind, one qualified by a minimum transfer size
         * and one not, in both table orders: the broader answer wins either way
         */
        for (i = 0; i < 2; i++) {
                platform(&f);
                table_start(&f.hmat_table, f.hmat,
                            sizeof(struct acpi_table_hmat), ACPI_TABLE_SIG_HMAT,
                            ACPI_HMAT_REVISION_PICOSECONDS);
                hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN,
                               ACPI_HMAT_INITIATOR_VALID);
                if (i == 0) {
                        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0,
                                      ACPI_HMAT_PS_PER_NS, m_initiators, 1,
                                      m_targets, 2, m_latency);
                        hmat_locality(&f, ACPI_HMAT_READ_LATENCY,
                                      ACPI_HMAT_MIN_TRANSFER_SIZE, 8,
                                      ACPI_HMAT_PS_PER_NS, m_initiators, 1,
                                      m_targets, 2, other);
                } else {
                        hmat_locality(&f, ACPI_HMAT_READ_LATENCY,
                                      ACPI_HMAT_MIN_TRANSFER_SIZE, 8,
                                      ACPI_HMAT_PS_PER_NS, m_initiators, 1,
                                      m_targets, 2, other);
                        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 0, 0,
                                      ACPI_HMAT_PS_PER_NS, m_initiators, 1,
                                      m_targets, 2, m_latency);
                }
                r = describe(&f);
                loc = &region_of(r, 0)->locality;
                assert_int_equal(loc->read_latency_ns, 90);
                assert_int_equal(loc->read_latency_min_transfer_qualified, 0);
                assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
        }

        /* where the only structure carries a condition, the figure is reported
         * with it rather than as an unconditional one
         */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(
            &f, ACPI_HMAT_READ_LATENCY,
            ACPI_HMAT_MIN_TRANSFER_SIZE | ACPI_HMAT_NON_SEQUENTIAL_TRANSFERS, 8,
            ACPI_HMAT_PS_PER_NS, m_initiators, 1, m_targets, 2, m_latency);
        r = describe(&f);
        loc = &region_of(r, 0)->locality;
        assert_int_equal(loc->read_latency_valid, 1);
        assert_int_equal(loc->read_latency_ns, 90);
        assert_int_equal(loc->read_latency_min_transfer_qualified, 1);
        assert_int_equal(loc->read_latency_min_transfer, 8);
        assert_int_equal(loc->read_latency_non_sequential, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* a cache level's numbers are not the memory's, and the structures
         * describing them are the same type
         */
        platform(&f);
        table_start(&f.hmat_table, f.hmat, sizeof(struct acpi_table_hmat),
                    ACPI_TABLE_SIG_HMAT, ACPI_HMAT_REVISION_PICOSECONDS);
        hmat_proximity(&f, DDR_DOMAIN, DDR_DOMAIN, ACPI_HMAT_INITIATOR_VALID);
        hmat_locality(&f, ACPI_HMAT_READ_LATENCY, 1, 0, ACPI_HMAT_PS_PER_NS,
                      m_initiators, 1, m_targets, 2, other);
        r = describe(&f);
        loc = &region_of(r, 0)->locality;
        assert_int_equal(loc->read_latency_valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_total_size(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;

        /* ranges whose lengths add up to an address space size have a total;
         * ranges that overlap do not, and a total computed from them would be a
         * plausible wrong figure
         */
        platform(&f);
        f.mrrm.num_mres = 0;
        mrrm_range(&f, DDR_LOW_BASE, DDR_LOW_SIZE, 0, 1);
        mrrm_range(&f, DDR_LOW_BASE, DDR_LOW_SIZE, 0, 1);
        r = describe(&f);
        assert_int_equal(region_of(r, 0)->total_size_valid, 0);
        assert_int_equal(region_of(r, 0)->total_size, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* and a range that does not end at an address has no size to add */
        platform(&f);
        f.mrrm.num_mres = 0;
        mrrm_range(&f, UINT64_MAX - 0xffff, 0x100000, 0, 1);
        r = describe(&f);
        assert_int_equal(region_of(r, 0)->total_size_valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

/** the attributes a platform really declares: one total level, level 1,
 *  direct mapped, write back, 64 byte line - which is 0x00401111 as ACPI
 *  packs it
 */
#define DECLARED_CACHE_ATTRIBUTES 0x00401111U

static void
test_memory_side_cache(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_side_cache *c;

        /* a cache declared for the CXL region's target domain, with the figures
         * a board really carries: the region is the device's memory plus this
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x2000000000ULL, DECLARED_CACHE_ATTRIBUTES);
        r = describe(&f);

        c = &region_of(r, 1)->mem_side_cache;
        assert_int_equal(c->valid, 1);
        assert_int_equal(c->memory_domain, CXL_DOMAIN);
        assert_int_equal(c->size_valid, 1);
        assert_true(c->size == 0x2000000000ULL);
        assert_int_equal(c->levels_declared, 1);
        assert_int_equal(c->total_levels, 1);
        assert_int_equal(c->level_valid, 1);
        assert_int_equal(c->level, 1);
        assert_int_equal(c->associativity, PQOS_MEM_CACHE_ASSOC_DIRECT_MAPPED);
        assert_int_equal(c->write_policy, PQOS_MEM_CACHE_WRITE_BACK);
        assert_int_equal(c->line_size_valid, 1);
        assert_int_equal(c->line_size, 64);

        /* and the DDR region, which no cache was declared for, says so rather
         * than borrowing the one beside it
         */
        assert_int_equal(region_of(r, 0)->mem_side_cache.valid, 0);
        assert_int_equal(region_of(r, 0)->mem_side_cache.size, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_memory_side_cache_without_figures(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_side_cache *c;

        /* one level declared and nothing else said about it: the level within
         * the total, the associativity, the write policy and the line size are
         * each absent rather than zero, because ACPI's zero there is a platform
         * saying nothing about a cache it has said is there
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x1000, 1U);
        r = describe(&f);

        c = &region_of(r, 1)->mem_side_cache;
        assert_int_equal(c->valid, 1);
        assert_int_equal(c->size_valid, 1);
        assert_true(c->size == 0x1000);
        assert_int_equal(c->total_levels, 1);
        assert_int_equal(c->level_valid, 0);
        assert_int_equal(c->line_size_valid, 0);
        assert_int_equal(c->associativity, PQOS_MEM_CACHE_ASSOC_UNKNOWN);
        assert_int_equal(c->write_policy, PQOS_MEM_CACHE_WRITE_UNKNOWN);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* a value ACPI has defined since this was written is reported as there
         * but unnamed, not as one of the values it is not
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x1000, 1U | (7U << 8) | (9U << 12));
        r = describe(&f);
        c = &region_of(r, 1)->mem_side_cache;
        assert_int_equal(c->associativity, PQOS_MEM_CACHE_ASSOC_OTHER);
        assert_int_equal(c->write_policy, PQOS_MEM_CACHE_WRITE_OTHER);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_memory_side_cache_without_a_locality_pair(void **state
                                               __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;

        /* A cache is declared for the target domain and nothing pairs an
         * initiator with it: HMAT has described the domain - that is what the
         * cache structure is - so hmat_match says yes, while proximity_valid
         * says there is no pair to report locality for. Reporting "HMAT Match:
         * No" above a cache block with figures in it would contradict itself.
         */
        platform_without_hmat_pairs(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x2000000000ULL, DECLARED_CACHE_ATTRIBUTES);
        r = describe(&f);

        assert_int_equal(region_of(r, 1)->mem_side_cache.valid, 1);
        assert_int_equal(region_of(r, 1)->hmat_match, 1);
        assert_int_equal(region_of(r, 1)->proximity_valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_memory_side_cache_none_declared(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;

        /* ACPI's Total Cache Levels nibble names zero None: a structure like
         * this says the domain has no memory side cache, which is a statement
         * and not a gap. Reporting it as a declared cache of unknown figures
         * would assert a cache the table denies, and a caller subtracting its
         * size from the region would misreport the device behind it
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x1000, 0);
        r = describe(&f);

        assert_int_equal(region_of(r, 1)->mem_side_cache.valid, 0);
        assert_int_equal(region_of(r, 1)->mem_side_cache.levels_declared, 0);

        /* and it is still an entry for the domain: the table was asked and it
         * answered
         */
        assert_int_equal(region_of(r, 1)->mem_side_cache.domain_declared, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* which is what the region matches HMAT on, with no locality pair to
         * match on instead. "HMAT Match: No" there would say the table had
         * nothing for the domain, and a declared absence is not nothing
         */
        platform_without_hmat_pairs(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x1000, 0);
        r = describe(&f);

        assert_int_equal(region_of(r, 1)->mem_side_cache.valid, 0);
        assert_int_equal(region_of(r, 1)->mem_side_cache.domain_declared, 1);
        assert_int_equal(region_of(r, 1)->proximity_valid, 0);
        assert_int_equal(region_of(r, 1)->hmat_match, 1);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* and a table with no structure for this domain at all leaves both
         * clear, which is the answer "No" is for
         */
        platform_without_hmat_pairs(&f);
        hmat_cache(&f, CXL_DOMAIN + 1, 0x1000, 1U | (1U << 4));
        r = describe(&f);

        assert_int_equal(region_of(r, 1)->mem_side_cache.valid, 0);
        assert_int_equal(region_of(r, 1)->mem_side_cache.domain_declared, 0);
        assert_int_equal(region_of(r, 1)->hmat_match, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* and the same structure beside one that does declare a cache: the
         * denial is not counted as a level either
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x1000, 0);
        hmat_cache(&f, CXL_DOMAIN, 0x2000, 1U | (1U << 4));
        r = describe(&f);

        assert_int_equal(region_of(r, 1)->mem_side_cache.valid, 1);
        assert_int_equal(region_of(r, 1)->mem_side_cache.levels_declared, 1);
        assert_true(region_of(r, 1)->mem_side_cache.size == 0x2000);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_memory_side_cache_without_a_size(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_side_cache *c;

        /* a declared cache whose size field is zero: the cache is reported,
         * because the table says it is there and says what level it is, and the
         * size is absent rather than a stated zero - a caller that read a zero
         * here as a size would reconcile a region with its device by adding
         * nothing, and lose the extended linear cache it was looking for
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0, DECLARED_CACHE_ATTRIBUTES);
        r = describe(&f);

        c = &region_of(r, 1)->mem_side_cache;
        assert_int_equal(c->valid, 1);
        assert_int_equal(c->size_valid, 0);
        assert_true(c->size == 0);
        assert_int_equal(c->level, 1);
        assert_int_equal(c->line_size, 64);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_memory_side_cache_levels(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_side_cache *c;

        /* two levels declared for one domain: the lowest is described and the
         * count says the other is there, because one of several published as
         * though it were the whole story is what this report refuses to do
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x2000000000ULL,
                   (2U) | (1U << 4) | (1U << 8) | (1U << 12) | (64U << 16));
        hmat_cache(&f, CXL_DOMAIN, 0x1000000000ULL,
                   (2U) | (2U << 4) | (2U << 8) | (2U << 12) | (128U << 16));
        r = describe(&f);

        c = &region_of(r, 1)->mem_side_cache;
        assert_int_equal(c->valid, 1);
        assert_int_equal(c->levels_declared, 2);
        assert_true(c->size == 0x2000000000ULL);
        assert_int_equal(c->level, 1);
        assert_int_equal(c->total_levels, 2);
        assert_int_equal(c->line_size, 64);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* the same two structures in the other order, which ACPI permits and
         * which two boards of the same design can differ by: the answer is the
         * level and not the position, so it is the same answer
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x1000000000ULL,
                   (2U) | (2U << 4) | (2U << 8) | (2U << 12) | (128U << 16));
        hmat_cache(&f, CXL_DOMAIN, 0x2000000000ULL,
                   (2U) | (1U << 4) | (1U << 8) | (1U << 12) | (64U << 16));
        r = describe(&f);

        c = &region_of(r, 1)->mem_side_cache;
        assert_int_equal(c->levels_declared, 2);
        assert_true(c->size == 0x2000000000ULL);
        assert_int_equal(c->level, 1);
        assert_int_equal(c->line_size, 64);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* two structures of which only one says which level it is: a stated
         * level is what the pick is made on, so the structure that states one
         * is chosen however the table orders them
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x1000000000ULL, 2U);
        hmat_cache(&f, CXL_DOMAIN, 0x2000000000ULL, (2U) | (2U << 4));
        r = describe(&f);

        c = &region_of(r, 1)->mem_side_cache;
        assert_int_equal(c->levels_declared, 2);
        assert_int_equal(c->level_valid, 1);
        assert_int_equal(c->level, 2);
        assert_true(c->size == 0x2000000000ULL);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_memory_side_cache_level_above_the_total(void **state
                                             __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        const struct pqos_mem_side_cache *c;

        /* one total level and a cache claiming to be the second of them: the
         * structure contradicts itself, and the level is the one field nobody
         * can use. The cache is still described - the size and the line size
         * are what the platform stated - with the level reported as not
         * stated, because "level 2 of 1" is not an answer a caller can square
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x1000, 1U | (2U << 4) | (64U << 16));
        r = describe(&f);

        c = &region_of(r, 1)->mem_side_cache;
        assert_int_equal(c->valid, 1);
        assert_int_equal(c->total_levels, 1);
        assert_int_equal(c->level_valid, 0);
        assert_int_equal(c->level, 0);
        assert_int_equal(c->line_size, 64);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);

        /* and such a level does not win the pick either. The first structure
         * here claims level 2 of 1 and the second level 3 of 3: read as
         * written, 2 is the lower of the two and the first structure would be
         * the one described, so the figures published would be the ones whose
         * level nobody can use. Read as this does, the first states no level
         * and the second is the only structure that states one
         */
        platform(&f);
        hmat_cache(&f, CXL_DOMAIN, 0x1000, 1U | (2U << 4) | (64U << 16));
        hmat_cache(&f, CXL_DOMAIN, 0x2000, 3U | (3U << 4) | (128U << 16));
        r = describe(&f);

        c = &region_of(r, 1)->mem_side_cache;
        assert_int_equal(c->levels_declared, 2);
        assert_int_equal(c->level_valid, 1);
        assert_int_equal(c->level, 3);
        assert_int_equal(c->total_levels, 3);
        assert_true(c->size == 0x2000);
        assert_int_equal(c->line_size, 128);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

static void
test_memory_side_cache_malformed(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_mem_regions *r;
        uint8_t *at;

        /* a cache structure too short to hold the fields it must have drops the
         * whole table, as a short locality or proximity structure does: the
         * structures are read in order, so the next offset after a bad one is a
         * guess
         */
        platform(&f);
        at = table_end(f.hmat);
        hmat_cache(&f, CXL_DOMAIN, 0x2000000000ULL, DECLARED_CACHE_ATTRIBUTES);
        ((struct acpi_hmat_cache *)at)->entry.length =
            sizeof(struct acpi_hmat_entry);
        r = describe(&f);

        assert_int_equal(r->hmat_available, 0);
        assert_int_equal(region_of(r, 1)->mem_side_cache.valid, 0);
        assert_int_equal(region_of(r, 1)->locality.valid, 0);
        assert_int_equal(mem_regions_fini(), PQOS_RETVAL_OK);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_parameters),
            cmocka_unit_test(test_describes_a_platform),
            cmocka_unit_test(test_ranges_without_regions),
            cmocka_unit_test(test_srat_collective_coverage),
            cmocka_unit_test(test_srat_one_domain_only),
            cmocka_unit_test(test_region_in_two_domains),
            cmocka_unit_test(test_window_counts),
            cmocka_unit_test(test_windows_cover_collectively),
            cmocka_unit_test(test_window_refused),
            cmocka_unit_test(test_cedt_truncated_keeps_its_windows),
            cmocka_unit_test(test_cedt_absence),
            cmocka_unit_test(test_srat_malformed_drops_the_table),
            cmocka_unit_test(test_hmat_malformed_drops_the_table),
            cmocka_unit_test(test_hmat_revisions),
            cmocka_unit_test(test_hmat_scaling),
            cmocka_unit_test(test_hmat_no_number),
            cmocka_unit_test(test_hmat_selection),
            cmocka_unit_test(test_total_size),
            cmocka_unit_test(test_memory_side_cache),
            cmocka_unit_test(test_memory_side_cache_without_figures),
            cmocka_unit_test(test_memory_side_cache_without_a_locality_pair),
            cmocka_unit_test(test_memory_side_cache_none_declared),
            cmocka_unit_test(test_memory_side_cache_without_a_size),
            cmocka_unit_test(test_memory_side_cache_levels),
            cmocka_unit_test(test_memory_side_cache_level_above_the_total),
            cmocka_unit_test(test_memory_side_cache_malformed)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
