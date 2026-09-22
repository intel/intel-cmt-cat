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
 * @brief Describes the memory regions MRRM reports, using SRAT, HMAT and CEDT
 */

#include "mem_regions.h"

#include "acpi.h"
#include "common.h"
#include "log.h"
#include "mrrm.h"
#include "types.h"

#include <stdlib.h>
#include <string.h>

/** What this module allocated, so that fini can release it */
static struct pqos_mem_regions *m_regions = NULL;

/** How many distinct local region IDs an MRE can carry, the field being a byte.
 *  A table of any length describes no more regions than this
 */
#define MEM_REGION_IDS (UINT8_MAX + 1)

/** One SRAT memory affinity range, as parsed */
struct srat_range {
        uint64_t base;
        uint64_t length;
        unsigned proximity_domain;
        int enabled;
};

/** One CEDT window, as parsed */
struct cedt_window {
        uint64_t base;
        uint64_t size;
};

/** One HMAT locality matrix, as parsed */
struct hmat_matrix {
        uint8_t data_type;
        /** how broadly the numbers apply, as hmat_generality() computes it:
         *  zero where they hold for any transfer, and otherwise larger the more
         *  conditions the structure attaches, so that the broader statement
         *  sorts first
         */
        unsigned qualifier;
        /** the structure attaches a condition to its numbers - a minimum
         *  transfer size, or transfers being non-sequential - so they are not
         *  unconditional and whatever is reported from them has to say so
         */
        int min_transfer_qualified;
        uint8_t min_transfer_size;
        int non_sequential;
        uint64_t base_unit;
        uint32_t num_initiators;
        uint32_t num_targets;
        const uint32_t *initiators;
        const uint32_t *targets;
        const uint16_t *values;
};

/** What the three tables contributed, gathered before correlation */
struct acpi_facts {
        struct acpi_table *srat_tbl;
        struct acpi_table *hmat_tbl;
        struct acpi_table *cedt_tbl;

        /** the table was there and what it says was read. Clear where it is
         *  absent, or unreadable, or was dropped for being malformed or of a
         *  revision whose units are unknown - in all of which the report has
         *  nothing to say about the table rather than something to say about
         *  the ranges, and those are different answers
         */
        int srat_read;
        int hmat_read;
        int cedt_read;

        struct srat_range *srat;
        unsigned num_srat;

        struct cedt_window *cedt;
        unsigned num_cedt;
        /** every CXL window this platform has is in the array above: either the
         *  table was walked from end to end, or there is no table to walk and
         *  so no host bridge publishing windows. Clear where CEDT exists and
         *  could not be read in full, which leaves windows unaccounted for
         */
        int cedt_known;

        struct hmat_matrix *hmat;
        unsigned num_hmat;
        /** which revision of HMAT the matrices came from, since that decides
         *  what their entries count
         */
        unsigned hmat_revision;

        /** HMAT proximity domain attributes, as initiator-target pairs */
        unsigned *hmat_initiator;
        unsigned *hmat_target;
        unsigned num_hmat_domains;
};

/**
 * @brief Whether a locality structure can publish a number at all
 *
 * Its entries are counts of a base unit, so a base unit of zero - which ACPI
 * does not allow - leaves every entry scaling to zero. That is not a fast
 * memory: it is a structure with nothing in it, and taking it at its word would
 * print a zero-nanosecond latency as a measurement. The structure is refused
 * rather than its entries filtered, because there is no entry it could carry
 * that would mean anything.
 *
 * Asked as one question, in one place, because both passes over the table have
 * to agree on which structures they are counting.
 *
 * @param [in] e the structure, already known to fit the table
 *
 * @retval 1 a locality structure this module can read
 * @retval 0 one it cannot
 */
static int
hmat_locality_usable(const struct acpi_hmat_entry *e)
{
        const struct acpi_hmat_locality *l =
            (const struct acpi_hmat_locality *)e;

        if (e->type != ACPI_HMAT_TYPE_LOCALITY || e->length < sizeof(*l))
                return 0;

        if (l->entry_base_unit == 0) {
                LOG_DEBUG("HMAT locality structure with a zero base unit\n");
                return 0;
        }

        return 1;
}

/**
 * @brief How broadly a locality structure's numbers apply, as a sort key
 *
 * A structure can attach two conditions to what it publishes: a minimum
 * transfer size, and transfers being non-sequential. Fewer conditions is the
 * broader statement, so they are counted first; between two structures carrying
 * one condition each the smaller minimum transfer size comes first, which
 * orders a pair that the specification gives no way to compare - arbitrary, but
 * the same answer whatever order the table lists them in, and the report says
 * which conditions the figure it printed came with.
 *
 * @param [in] m the structure
 *
 * The key is the number of conditions, in the high part, and the minimum
 * transfer size in the low part - so any structure carrying fewer conditions
 * sorts ahead of one carrying more, whatever sizes they name.
 *
 * @return its key, zero for a structure that qualifies nothing
 */
static unsigned
hmat_generality(const struct hmat_matrix *m)
{
        const unsigned conditions = (m->min_transfer_qualified ? 1u : 0u) +
                                    (m->non_sequential ? 1u : 0u);

        return conditions * 256u +
               (m->min_transfer_qualified ? m->min_transfer_size : 0u);
}

/**
 * @brief The last address of a range, where the range has one
 *
 * A length of zero describes nothing, and a base and length whose sum does not
 * fit an address describe nothing either - a table declaring one is malformed,
 * and computing base + length - 1 for it wraps and yields an address inside the
 * range rather than past it. Both are refused here so that no comparison below
 * has to think about it.
 *
 * @param [in] base start of the range
 * @param [in] length its length
 * @param [out] last its last address
 *
 * @retval 1 the range has a representable last address
 * @retval 0 it does not
 */
static int
range_last(const uint64_t base, const uint64_t length, uint64_t *last)
{
        if (length == 0)
                return 0;
        if (base > UINT64_MAX - (length - 1))
                return 0;

        *last = base + (length - 1);

        return 1;
}

/**
 * @brief Whether two ranges share any address
 */
static int
ranges_overlap(const uint64_t base,
               const uint64_t length,
               const uint64_t other_base,
               const uint64_t other_length)
{
        uint64_t last;
        uint64_t other_last;

        if (!range_last(base, length, &last) ||
            !range_last(other_base, other_length, &other_last))
                return 0;

        return base <= other_last && other_base <= last;
}

/**
 * @brief Whether the CEDT windows together cover a whole range
 *
 * A region's range need not sit inside one window. Where memory is reserved for
 * hot-add, a single MRRM range spans several CFMWS windows and may have
 * addresses between them that no window describes, so coverage is asked of the
 * windows collectively: walk the range, and for each address not yet covered
 * find a window holding it and jump to that window's end.
 *
 * @param [in] facts the parsed tables
 * @param [in] base start of the range
 * @param [in] length its length
 *
 * @retval 1 every address of the range lies in some window
 * @retval 0 at least one does not
 */
static int
cedt_covers(const struct acpi_facts *facts,
            const uint64_t base,
            const uint64_t length)
{
        uint64_t cursor = base;
        uint64_t last;

        if (facts->num_cedt == 0 || !range_last(base, length, &last))
                return 0;

        while (cursor <= last) {
                unsigned i;
                int advanced = 0;

                for (i = 0; i < facts->num_cedt; i++) {
                        uint64_t wlast;

                        if (!range_last(facts->cedt[i].base,
                                        facts->cedt[i].size, &wlast))
                                continue;
                        if (cursor < facts->cedt[i].base || cursor > wlast)
                                continue;
                        if (wlast >= last)
                                return 1;

                        cursor = wlast + 1;
                        advanced = 1;
                        break;
                }

                if (!advanced)
                        return 0;
        }

        return 1;
}

/**
 * @brief Whether one proximity domain's SRAT entries together cover a range
 *
 * SRAT is fragmented by construction. A single-socket platform describes its
 * memory below 4G in two or three entries because of the holes the architecture
 * leaves - the legacy hole under 1M, the PCI hole under 4G - and all of them
 * sit in the same proximity domain. So asking whether one entry contains an
 * MRRM range answers no on an ordinary machine, and the region comes out
 * undescribed with every HMAT number missing.
 *
 * Coverage is therefore asked of one domain's entries collectively, the way
 * cedt_covers() asks it of the windows: walk the range, and for each address
 * not yet covered find an entry of that domain holding it and jump to its end.
 * Adjacency is not required, only that nothing is left over - an address no
 * entry of the domain describes ends it.
 *
 * @param [in] facts the parsed tables
 * @param [in] base start of the range
 * @param [in] length its length
 * @param [in] domain the proximity domain to ask about
 * @param [in] want_enabled 1 to base the answer on entries firmware has
 *             enabled, 0 on entries it has described and left disabled. Never
 *             both: memory that is there and memory that is not are different
 *             answers, and a range half of each is neither of them
 *
 * @retval 1 every address of the range lies in such an entry of that domain
 * @retval 0 at least one does not
 */
static int
srat_covers(const struct acpi_facts *facts,
            const uint64_t base,
            const uint64_t length,
            const unsigned domain,
            const int want_enabled)
{
        uint64_t cursor = base;
        uint64_t last;

        if (facts->num_srat == 0 || !range_last(base, length, &last))
                return 0;

        while (cursor <= last) {
                unsigned i;
                int advanced = 0;

                for (i = 0; i < facts->num_srat; i++) {
                        const struct srat_range *e = &facts->srat[i];
                        uint64_t elast;

                        if (e->proximity_domain != domain)
                                continue;
                        if (e->enabled != want_enabled)
                                continue;
                        if (!range_last(e->base, e->length, &elast))
                                continue;
                        if (cursor < e->base || cursor > elast)
                                continue;
                        if (elast >= last)
                                return 1;

                        cursor = elast + 1;
                        advanced = 1;
                        break;
                }

                if (!advanced)
                        return 0;
        }

        return 1;
}

/**
 * @brief The one proximity domain whose SRAT entries account for a range
 *
 * Two questions, and both have to answer yes. One domain's entries must cover
 * every address of the range, collectively, which is what srat_covers() asks.
 * And no other domain may reach into it at all - not merely fail to cover it:
 * an entry of a second domain over half the range makes those addresses belong
 * to two domains at once, and reporting the covering domain's latency for the
 * whole range would state a figure for memory the table says is somewhere else.
 * Partial contradiction is still contradiction, so it withdraws the answer the
 * same way a second covering domain does.
 *
 * Entries of the other status are not consulted for either question. A disabled
 * placeholder overlapping enabled memory is firmware describing space it has
 * not brought up, not two domains disagreeing, and it must not take the answer
 * away.
 *
 * @param [in] facts the parsed tables
 * @param [in] base start of the range
 * @param [in] length its length
 * @param [in] want_enabled which entries the answer may be based on, as
 *             for srat_covers()
 * @param [out] domain the domain that accounts for it, where one does
 *
 * @retval 1 one domain accounts for the range alone, and it is in domain
 * @retval 0 none does, or another domain reaches into it
 */
static int
srat_domain_of(const struct acpi_facts *facts,
               const uint64_t base,
               const uint64_t length,
               const int want_enabled,
               unsigned *domain)
{
        unsigned i;
        unsigned found = 0;
        int have = 0;

        for (i = 0; i < facts->num_srat && !have; i++) {
                const struct srat_range *e = &facts->srat[i];

                if (e->enabled != want_enabled)
                        continue;
                if (!ranges_overlap(base, length, e->base, e->length))
                        continue;
                if (!srat_covers(facts, base, length, e->proximity_domain,
                                 want_enabled))
                        continue;

                found = e->proximity_domain;
                have = 1;
        }

        if (!have)
                return 0;

        for (i = 0; i < facts->num_srat; i++) {
                const struct srat_range *e = &facts->srat[i];

                if (e->enabled != want_enabled)
                        continue;
                if (e->proximity_domain == found)
                        continue;
                if (!ranges_overlap(base, length, e->base, e->length))
                        continue;

                LOG_DEBUG("SRAT range 0x%llx is in proximity domain %u and "
                          "domain %u reaches into it\n",
                          (unsigned long long)base, found, e->proximity_domain);

                return 0;
        }

        *domain = found;

        return 1;
}

/**
 * @brief Reads the memory affinity ranges out of SRAT
 *
 * A table that is absent or carries no memory affinity entry is not an error:
 * it contributes nothing and the regions are reported without what it would
 * have said. Failing to allocate room for what it does carry is an error, and a
 * different one - reporting CXL windows as ordinary memory because a calloc()
 * failed would be a wrong answer rather than an incomplete one.
 *
 * @param [in,out] facts where to put them
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK the table was read, or is not there
 * @retval PQOS_RETVAL_RESOURCE out of memory
 */
static int
srat_parse(struct acpi_facts *facts)
{
        const struct acpi_table_srat *srat;
        const uint8_t *pos;
        const uint8_t *end;
        unsigned count = 0;

        facts->srat_tbl = acpi_get_sig(ACPI_TABLE_SIG_SRAT);
        if (facts->srat_tbl == NULL) {
                LOG_DEBUG("SRAT table not found\n");
                return PQOS_RETVAL_OK;
        }

        srat = (const struct acpi_table_srat *)facts->srat_tbl->generic;
        if (srat->header.length < sizeof(*srat)) {
                LOG_DEBUG("SRAT table too short\n");
                return PQOS_RETVAL_OK;
        }

        end = facts->srat_tbl->generic + srat->header.length;

        /* counted first, so the array is allocated once.
         *
         * An entry is only read if its own declared length says the bytes are
         * its own: a length shorter than the header, or one running past the
         * table, means the rest of this table cannot be walked, and a memory
         * entry shorter than the structure it claims to be would be filled
         * from bytes belonging to whatever follows it.
         */
        for (pos = facts->srat_tbl->generic + sizeof(*srat);
             (size_t)(end - pos) >= sizeof(struct acpi_srat_entry);) {
                const struct acpi_srat_entry *e =
                    (const struct acpi_srat_entry *)pos;

                if (e->length < sizeof(*e) || (size_t)(end - pos) < e->length) {
                        /* the rest of the table cannot be walked, and what is
                         * in it would decide the answers drawn from what came
                         * before: an entry beyond this point can enable memory
                         * a kept entry left disabled, or put a second domain
                         * over the same addresses and make the region's domain
                         * ambiguous. So the whole table is dropped rather than
                         * its readable prefix used - unknown, not a confident
                         * answer that the unread bytes might contradict
                         */
                        LOG_DEBUG("SRAT entry of length %u at offset %zd does "
                                  "not fit the table; ignoring SRAT\n",
                                  e->length, pos - facts->srat_tbl->generic);

                        return PQOS_RETVAL_OK;
                }
                /* a memory affinity entry shorter than a memory affinity
                 * entry is the same malformation as one that overruns the
                 * table: the bytes that should hold its addresses hold
                 * something else, and skipping it would leave the answers drawn
                 * from its neighbours standing. Other entry types are left
                 * alone - their payloads are not read here, so their lengths
                 * are not this module's business
                 */
                if (e->type == ACPI_SRAT_TYPE_MEMORY_AFFINITY) {
                        if (e->length < sizeof(struct acpi_srat_memory)) {
                                LOG_DEBUG("SRAT memory affinity entry of "
                                          "length %u at offset %zd is too "
                                          "short; ignoring SRAT\n",
                                          e->length,
                                          pos - facts->srat_tbl->generic);

                                return PQOS_RETVAL_OK;
                        }

                        count++;
                }
                pos += e->length;
        }

        /* and the walk has to end where the table does. Bytes left over are a
         * structure the table declared room for and did not finish, so they are
         * as unreadable as an overrunning one and the same answer applies
         */
        if (pos != end) {
                LOG_DEBUG("SRAT has %zd byte(s) after its last entry; "
                          "ignoring SRAT\n",
                          end - pos);

                return PQOS_RETVAL_OK;
        }

        if (count == 0) {
                facts->srat_read = 1;

                return PQOS_RETVAL_OK;
        }

        facts->srat = calloc(count, sizeof(*facts->srat));
        if (facts->srat == NULL)
                return PQOS_RETVAL_RESOURCE;

        for (pos = facts->srat_tbl->generic + sizeof(*srat);
             (size_t)(end - pos) >= sizeof(struct acpi_srat_entry);) {
                const struct acpi_srat_entry *e =
                    (const struct acpi_srat_entry *)pos;
                const struct acpi_srat_memory *m;

                if (e->length < sizeof(*e) || (size_t)(end - pos) < e->length)
                        break;
                if (e->type != ACPI_SRAT_TYPE_MEMORY_AFFINITY ||
                    e->length < sizeof(struct acpi_srat_memory)) {
                        pos += e->length;
                        continue;
                }

                m = (const struct acpi_srat_memory *)pos;
                facts->srat[facts->num_srat].base =
                    ((uint64_t)m->base_address_high << 32) |
                    m->base_address_low;
                facts->srat[facts->num_srat].length =
                    ((uint64_t)m->length_high << 32) | m->length_low;
                facts->srat[facts->num_srat].proximity_domain =
                    m->proximity_domain;
                facts->srat[facts->num_srat].enabled =
                    (m->flags & ACPI_SRAT_MEM_ENABLED) != 0;
                facts->num_srat++;

                if (facts->num_srat == count)
                        break;
                pos += e->length;
        }

        facts->srat_read = 1;
        LOG_DEBUG("SRAT: %u memory affinity range(s)\n", facts->num_srat);

        return PQOS_RETVAL_OK;
}

/**
 * @brief The number of interleave ways a CFMWS encoding stands for
 *
 * CXL encodes the count rather than storing it: 0 to 4 mean one way doubling to
 * sixteen, and 8 to 10 mean three ways doubling to twelve. Everything else is
 * reserved, 11 included - sixteen ways is the most the encoding reaches, so
 * there is no 24 for it to mean - and a window declaring a reserved value has
 * not said how many targets follow it, so its length cannot be checked and it
 * is refused.
 *
 * @param [in] encoded the encoded interleave ways field
 * @param [out] ways how many interleave ways it stands for
 *
 * @retval 1 the encoding is one the specification defines
 * @retval 0 it is reserved
 */
static int
cfmws_ways(const uint8_t encoded, unsigned *ways)
{
        if (encoded <= 4) {
                *ways = 1u << encoded;
                return 1;
        }

        if (encoded >= 8 && encoded <= 10) {
                *ways = 3u << (encoded - 8);
                return 1;
        }

        return 0;
}

/**
 * @brief Whether a CEDT structure is a CFMWS that carries all of itself
 *
 * A window ends in an interleave target list, one 32-bit host bridge UID per
 * interleave way, so a conformant record is the fixed part plus four bytes per
 * way - never the fixed part alone, since the smallest encoding is still one
 * way. A record shorter than that has been truncated, and while nothing here
 * reads the target list, the base and size that are read come from the same
 * structure: a window the platform did not finish writing is not evidence that
 * a region is CXL space, which is the answer it would otherwise decide.
 *
 * Asked as one question, in one place, because both passes below have to agree
 * on it - a window counted by the first and skipped by the second, or the other
 * way about, leaves the array the wrong length.
 *
 * The addresses it declares have to be a range as well. A window of no size, or
 * one whose base and size do not end at an address, describes nothing that can
 * be compared with a region's ranges - every comparison below refuses it - so
 * it would sit in the array contributing nothing while the count of windows
 * said the platform's windows were all accounted for. That is the one way a
 * window can be unusable without being short, and it is refused here for the
 * same reason: what it would have covered is unknown, not empty.
 *
 * @param [in] e the structure, already known to fit the table
 *
 * @retval 1 a CFMWS this module can use: long enough for its targets, and
 *         declaring a range of addresses
 * @retval 0 anything else
 */
static int
cfmws_complete(const struct acpi_cedt_entry *e)
{
        const struct acpi_cedt_cfmws *w = (const struct acpi_cedt_cfmws *)e;
        unsigned ways = 0;
        uint64_t last;

        if (e->type != ACPI_CEDT_TYPE_CFMWS || e->length < sizeof(*w))
                return 0;

        if (!cfmws_ways(w->encoded_interleave_ways, &ways))
                return 0;

        if ((size_t)(e->length - sizeof(*w)) < (size_t)ways * sizeof(uint32_t))
                return 0;

        return range_last(w->base_hpa, w->window_size, &last);
}

/**
 * @brief Reads the CXL fixed memory windows out of CEDT
 *
 * Absent is not an error; failing to allocate room for the windows is, and it
 * matters more here than anywhere else in this module - without the windows a
 * region of CXL space is reported as ordinary attached memory.
 *
 * @param [in,out] facts where to put them
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK the table was read, or is not there
 * @retval PQOS_RETVAL_RESOURCE out of memory
 */
static int
cedt_parse(struct acpi_facts *facts)
{
        const struct acpi_table_header *hdr;
        const uint8_t *pos;
        const uint8_t *end;
        unsigned count = 0;
        int refused = 0;

        facts->cedt_tbl = acpi_get_sig(ACPI_TABLE_SIG_CEDT);
        if (facts->cedt_tbl == NULL) {
                /* No windows to correlate, and two different reasons for it. A
                 * platform with no CEDT has no CXL host bridge to publish one,
                 * so it has no CXL windows and a region without one is what
                 * SRAT says it is. A CEDT that is there and cannot be read is
                 * the other thing entirely: the platform has CXL, and which of
                 * its ranges sit in windows is now unknown. Only the first lets
                 * a region be called local memory, so the two are told apart by
                 * whether the table exists to be read at all
                 */
                facts->cedt_known = !pqos_file_exists(ACPI_CEDT_TABLE);
                LOG_DEBUG("CEDT table not %s\n",
                          facts->cedt_known ? "present" : "readable");

                return PQOS_RETVAL_OK;
        }

        hdr = facts->cedt_tbl->header;
        if (hdr->length < sizeof(*hdr)) {
                LOG_DEBUG("CEDT table too short\n");
                return PQOS_RETVAL_OK;
        }

        facts->cedt_read = 1;

        end = facts->cedt_tbl->generic + hdr->length;

        for (pos = facts->cedt_tbl->generic + sizeof(*hdr);
             (size_t)(end - pos) >= sizeof(struct acpi_cedt_entry);) {
                const struct acpi_cedt_entry *e =
                    (const struct acpi_cedt_entry *)pos;

                if (e->length < sizeof(*e) || (size_t)(end - pos) < e->length) {
                        /* the readable windows are kept here, unlike SRAT and
                         * HMAT, because a window nobody could read cannot
                         * contradict one that was read: windows only add
                         * coverage. Keeping them can only understate - a region
                         * whose remaining windows leave a gap reports "CXL
                         * Range Match: No", and one whose only window was in
                         * the unread part reports no CEDT match at all - and
                         * understating is the direction this report errs in.
                         * Dropping them would turn CXL space into ordinary
                         * memory wherever SRAT had enabled it, which is a
                         * confident wrong answer rather than a cautious one
                         */
                        LOG_DEBUG("CEDT structure of length %u at offset %zd "
                                  "does not fit the table; keeping the %u "
                                  "window(s) before it\n",
                                  e->length, pos - facts->cedt_tbl->generic,
                                  count);
                        break;
                }
                if (e->type == ACPI_CEDT_TYPE_CFMWS) {
                        if (cfmws_complete(e)) {
                                count++;
                        } else {
                                /* a window that cannot be read is a window
                                 * whose addresses are unknown, so the windows
                                 * are no longer all accounted for even though
                                 * the walk will reach the end of the table
                                 */
                                refused = 1;
                                LOG_DEBUG("CEDT: CXL window of length %u at "
                                          "offset %zd cannot be used - it "
                                          "declares more interleave targets "
                                          "than it carries, or no range of "
                                          "addresses\n",
                                          e->length,
                                          pos - facts->cedt_tbl->generic);
                        }
                }
                pos += e->length;
        }

        /* the walk reached the end of the table and read every window in it, so
         * the windows counted are all the windows there are. A region no window
         * covers can be called local memory on that basis; where this is clear
         * - a structure that ended the walk, or a window whose own contents
         * could not be read - it cannot, because the addresses of what was
         * skipped are exactly what would have answered the question
         */
        facts->cedt_known = pos == end && !refused;

        if (count == 0)
                return PQOS_RETVAL_OK;

        facts->cedt = calloc(count, sizeof(*facts->cedt));
        if (facts->cedt == NULL)
                return PQOS_RETVAL_RESOURCE;

        for (pos = facts->cedt_tbl->generic + sizeof(*hdr);
             (size_t)(end - pos) >= sizeof(struct acpi_cedt_entry);) {
                const struct acpi_cedt_entry *e =
                    (const struct acpi_cedt_entry *)pos;
                const struct acpi_cedt_cfmws *w;

                if (e->length < sizeof(*e) || (size_t)(end - pos) < e->length)
                        break;
                if (!cfmws_complete(e)) {
                        pos += e->length;
                        continue;
                }

                w = (const struct acpi_cedt_cfmws *)pos;
                facts->cedt[facts->num_cedt].base = w->base_hpa;
                facts->cedt[facts->num_cedt].size = w->window_size;
                facts->num_cedt++;

                if (facts->num_cedt == count)
                        break;
                pos += e->length;
        }

        LOG_DEBUG("CEDT: %u CXL fixed memory window(s)%s\n", facts->num_cedt,
                  facts->cedt_known ? "" : ", and more that could not be read");

        return PQOS_RETVAL_OK;
}

/**
 * @brief Reads the proximity attributes and locality matrices out of HMAT
 *
 * The matrices are not copied: they are read where they lie in the mapped
 * table, which stays mapped until fini, so only the descriptors are allocated.
 *
 * Absent is not an error; failing to allocate the descriptors is.
 *
 * @param [in,out] facts where to put them
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK the table was read, or is not there
 * @retval PQOS_RETVAL_RESOURCE out of memory
 */
static int
hmat_parse(struct acpi_facts *facts)
{
        const struct acpi_table_hmat *hmat;
        const uint8_t *pos;
        const uint8_t *end;
        unsigned matrices = 0;
        unsigned domains = 0;

        facts->hmat_tbl = acpi_get_sig(ACPI_TABLE_SIG_HMAT);
        if (facts->hmat_tbl == NULL) {
                LOG_DEBUG("HMAT table not found\n");
                return PQOS_RETVAL_OK;
        }

        hmat = (const struct acpi_table_hmat *)facts->hmat_tbl->generic;
        if (hmat->header.length < sizeof(*hmat)) {
                LOG_DEBUG("HMAT table too short\n");
                return PQOS_RETVAL_OK;
        }

        /* the one place a revision decides anything here, and it decides what
         * the numbers mean rather than where they are. A revision whose units
         * are not known contributes nothing: scaling its entries by another
         * revision's rule would publish figures that are wrong by a factor
         * rather than missing, and nothing in the table would show it
         */
        if (hmat->header.revision != ACPI_HMAT_REVISION_TENTHS &&
            hmat->header.revision != ACPI_HMAT_REVISION_PICOSECONDS) {
                LOG_DEBUG(
                    "HMAT revision %u has no known units; ignoring HMAT\n",
                    hmat->header.revision);

                return PQOS_RETVAL_OK;
        }

        facts->hmat_revision = hmat->header.revision;

        end = facts->hmat_tbl->generic + hmat->header.length;

        /* A structure has to declare at least its own header. A length below
         * that would advance into the structure being read, and the bytes of
         * its payload would then be counted as further structures - fabricating
         * matrices and domains out of a matrix's own numbers. As with SRAT and
         * CEDT, a length that does not fit ends the walk rather than skipping
         * one entry, since the next offset would be a guess.
         */
        for (pos = facts->hmat_tbl->generic + sizeof(*hmat);
             (size_t)(end - pos) >= sizeof(struct acpi_hmat_entry);) {
                const struct acpi_hmat_entry *e =
                    (const struct acpi_hmat_entry *)pos;

                if (e->length < sizeof(*e) || (size_t)(end - pos) < e->length) {
                        /* dropped whole, as SRAT is, and for the same reason:
                         * the structures are ranked against each other, so one
                         * beyond this point can outrank a kept one and change
                         * which figure the report states. A number that would
                         * have been overridden is worse than no number
                         */
                        LOG_DEBUG("HMAT structure of length %u at offset %zd "
                                  "does not fit the table; ignoring HMAT\n",
                                  e->length, pos - facts->hmat_tbl->generic);

                        return PQOS_RETVAL_OK;
                }
                /* a structure of a type this module reads, declaring less
                 * than that type needs, is malformed rather than skippable -
                 * the same argument as the overrunning one above. A zero base
                 * unit is different and is only the structure's own loss: the
                 * structure is readable, it simply has no number in it, and the
                 * matrices beside it are unaffected by dropping it
                 */
                if (e->type == ACPI_HMAT_TYPE_LOCALITY) {
                        if (e->length < sizeof(struct acpi_hmat_locality)) {
                                LOG_DEBUG("HMAT locality structure of length "
                                          "%u at offset %zd is too short; "
                                          "ignoring HMAT\n",
                                          e->length,
                                          pos - facts->hmat_tbl->generic);

                                return PQOS_RETVAL_OK;
                        }
                        if (hmat_locality_usable(e))
                                matrices++;
                } else if (e->type == ACPI_HMAT_TYPE_PROXIMITY_DOMAIN) {
                        if (e->length < ACPI_HMAT_PROXIMITY_MIN_LENGTH) {
                                LOG_DEBUG("HMAT proximity structure of length "
                                          "%u at offset %zd is too short; "
                                          "ignoring HMAT\n",
                                          e->length,
                                          pos - facts->hmat_tbl->generic);

                                return PQOS_RETVAL_OK;
                        }

                        domains++;
                }
                pos += e->length;
        }

        if (pos != end) {
                LOG_DEBUG("HMAT has %zd byte(s) after its last structure; "
                          "ignoring HMAT\n",
                          end - pos);

                return PQOS_RETVAL_OK;
        }

        if (matrices > 0) {
                facts->hmat = calloc(matrices, sizeof(*facts->hmat));
                if (facts->hmat == NULL)
                        return PQOS_RETVAL_RESOURCE;
        }
        if (domains > 0) {
                facts->hmat_initiator =
                    calloc(domains, sizeof(*facts->hmat_initiator));
                facts->hmat_target =
                    calloc(domains, sizeof(*facts->hmat_target));
                if (facts->hmat_initiator == NULL || facts->hmat_target == NULL)
                        return PQOS_RETVAL_RESOURCE;
        }

        for (pos = facts->hmat_tbl->generic + sizeof(*hmat);
             (size_t)(end - pos) >= sizeof(struct acpi_hmat_entry);) {
                const struct acpi_hmat_entry *e =
                    (const struct acpi_hmat_entry *)pos;

                if (e->length < sizeof(*e) || (size_t)(end - pos) < e->length)
                        break;

                if (e->type == ACPI_HMAT_TYPE_PROXIMITY_DOMAIN &&
                    e->length >= ACPI_HMAT_PROXIMITY_MIN_LENGTH &&
                    facts->num_hmat_domains < domains) {
                        const struct acpi_hmat_proximity *p =
                            (const struct acpi_hmat_proximity *)pos;

                        /* the initiator field is reserved unless the structure
                         * says it is valid, and a pair built from a reserved
                         * field would invent a mapping and the numbers that
                         * hang off it
                         */
                        if ((p->flags & ACPI_HMAT_INITIATOR_VALID) == 0) {
                                LOG_DEBUG("HMAT proximity structure without a "
                                          "valid initiator domain\n");
                                pos += e->length;
                                continue;
                        }

                        facts->hmat_initiator[facts->num_hmat_domains] =
                            p->initiator_domain;
                        facts->hmat_target[facts->num_hmat_domains] =
                            p->target_domain;
                        facts->num_hmat_domains++;
                } else if (hmat_locality_usable(e) &&
                           facts->num_hmat < matrices) {
                        const struct acpi_hmat_locality *l =
                            (const struct acpi_hmat_locality *)pos;
                        size_t payload = e->length - sizeof(*l);
                        size_t lists;
                        size_t values;

                        /* the cache levels are described by structures of the
                         * same type, and their numbers are not the memory's
                         */
                        if ((l->flags & ACPI_HMAT_HIERARCHY_MASK) !=
                            ACPI_HMAT_HIERARCHY_MEMORY) {
                                pos += e->length;
                                continue;
                        }

                        /* Each dimension is checked against what the structure
                         * has room for before it is multiplied by anything.
                         * Adding the parts up first and comparing the total
                         * would be the obvious way and the wrong one: two
                         * counts near UINT32_MAX make that sum wrap in size_t
                         * and pass, after which the matrix pointers below
                         * address memory the table does not own.
                         */
                        if (l->num_initiators > payload / sizeof(uint32_t) ||
                            l->num_targets > payload / sizeof(uint32_t)) {
                                LOG_DEBUG("HMAT locality structure declares "
                                          "more domains than it holds\n");
                                pos += e->length;
                                continue;
                        }

                        lists = ((size_t)l->num_initiators +
                                 (size_t)l->num_targets) *
                                sizeof(uint32_t);
                        if (lists > payload) {
                                LOG_DEBUG("HMAT locality structure shorter "
                                          "than its domain lists\n");
                                pos += e->length;
                                continue;
                        }

                        if (l->num_targets != 0 &&
                            l->num_initiators > (payload - lists) /
                                                    sizeof(uint16_t) /
                                                    l->num_targets) {
                                LOG_DEBUG("HMAT locality structure shorter "
                                          "than the matrix it declares\n");
                                pos += e->length;
                                continue;
                        }

                        values = (size_t)l->num_initiators *
                                 (size_t)l->num_targets * sizeof(uint16_t);
                        if (values > payload - lists) {
                                LOG_DEBUG("HMAT locality structure shorter "
                                          "than the matrix it declares\n");
                                pos += e->length;
                                continue;
                        }

                        facts->hmat[facts->num_hmat].data_type = l->data_type;
                        facts->hmat[facts->num_hmat].min_transfer_qualified =
                            (l->flags & ACPI_HMAT_MIN_TRANSFER_SIZE) != 0;
                        facts->hmat[facts->num_hmat].min_transfer_size =
                            l->min_transfer_size;
                        facts->hmat[facts->num_hmat].non_sequential =
                            (l->flags & ACPI_HMAT_NON_SEQUENTIAL_TRANSFERS) !=
                            0;
                        facts->hmat[facts->num_hmat].qualifier =
                            hmat_generality(&facts->hmat[facts->num_hmat]);
                        facts->hmat[facts->num_hmat].base_unit =
                            l->entry_base_unit;
                        facts->hmat[facts->num_hmat].num_initiators =
                            l->num_initiators;
                        facts->hmat[facts->num_hmat].num_targets =
                            l->num_targets;
                        facts->hmat[facts->num_hmat].initiators =
                            (const uint32_t *)(pos + sizeof(*l));
                        facts->hmat[facts->num_hmat].targets =
                            (const uint32_t *)(pos + sizeof(*l) +
                                               (size_t)l->num_initiators *
                                                   sizeof(uint32_t));
                        facts->hmat[facts->num_hmat].values =
                            (const uint16_t *)(pos + sizeof(*l) + lists);
                        facts->num_hmat++;
                }

                pos += e->length;
        }

        facts->hmat_read = 1;
        LOG_DEBUG("HMAT: %u locality matrix/matrices, %u proximity pair(s)\n",
                  facts->num_hmat, facts->num_hmat_domains);

        return PQOS_RETVAL_OK;
}

/** How a matrix answered for one initiator-target pair */
enum hmat_lookup {
        /** the matrix does not list the pair */
        HMAT_LOOKUP_ABSENT = 0,
        /** it lists the pair, with a value this report can state */
        HMAT_LOOKUP_VALUE,
        /** it lists the pair and marks the value unavailable */
        HMAT_LOOKUP_UNAVAILABLE,
        /** it lists the pair, and the value does not survive scaling */
        HMAT_LOOKUP_UNSCALABLE,
};

/**
 * @brief Scales one matrix entry into the unit the report states
 *
 * The entry is a count of base units and the report wants base units divided by
 * something - picoseconds into nanoseconds, or nothing at all for bandwidth.
 * Multiplying first and dividing after would refuse numbers it could state: the
 * product can leave the type while the quotient fits comfortably. So the
 * division is folded into the multiplication, base unit split into whole
 * divisors and the remainder, which makes the result exact and bounds each
 * multiplication separately.
 *
 * The remainder term cannot overflow on its own: it is below the divisor, which
 * is a thousand at most, and the entry is a 16-bit number.
 *
 * Rounded up, not truncated. A latency of a few hundred picoseconds is a real
 * measurement and a nanosecond is the coarsest the report can state it in, so
 * truncating would publish it as zero - and zero is what this module uses to
 * mean "the platform has no number". Rounding up keeps every nonzero
 * measurement nonzero, which is also what Linux publishes for these entries.
 *
 * @param [in] raw the matrix entry
 * @param [in] base_unit the structure's base unit
 * @param [in] divisor what to divide by to reach the report's unit
 * @param [out] value the result
 *
 * @retval 1 the result is representable, and is in value
 * @retval 0 it is not
 */
static int
hmat_scale(const uint16_t raw,
           const uint64_t base_unit,
           const uint64_t divisor,
           uint64_t *value)
{
        const uint64_t whole = base_unit / divisor;
        const uint64_t part = base_unit % divisor;
        uint64_t scaled;
        uint64_t rest;

        if (raw == 0) {
                *value = 0;
                return 1;
        }

        if (whole > UINT64_MAX / raw)
                return 0;

        scaled = whole * raw;
        rest = (part * raw + divisor - 1) / divisor;
        if (scaled > UINT64_MAX - rest)
                return 0;

        *value = scaled + rest;

        return 1;
}

/**
 * @brief Looks a value up in one HMAT matrix
 *
 * Four answers rather than two, because the report says why a number is missing
 * and the three ways of missing it are different statements: the platform never
 * described the pair, or described it and said it has no number for it, or gave
 * a number this code cannot state without wrapping it.
 *
 * @param [in] m the matrix
 * @param [in] initiator initiator proximity domain
 * @param [in] target target proximity domain
 * @param [in] divisor what the entry has to be divided by to reach the unit the
 *             report states
 * @param [out] value the entry, in the report's unit, where the return value is
 *              HMAT_LOOKUP_VALUE
 *
 * @return which of the four the matrix answered
 */
static enum hmat_lookup
hmat_value(const struct hmat_matrix *m,
           const unsigned initiator,
           const unsigned target,
           const uint64_t divisor,
           uint64_t *value)
{
        uint32_t i;
        uint32_t j;

        for (i = 0; i < m->num_initiators; i++) {
                if (m->initiators[i] != initiator)
                        continue;
                for (j = 0; j < m->num_targets; j++) {
                        const uint16_t raw = m->values[i * m->num_targets + j];

                        if (m->targets[j] != target)
                                continue;

                        /* the pair is listed but the platform has no number
                         * for it, which it says in two ways. Scaling 0xffff
                         * would report an enormous latency or bandwidth as
                         * though it had been measured; reporting a zero would
                         * publish a zero-picosecond latency or a zero-MB/s
                         * bandwidth as a figure, which is the "zero that cannot
                         * be told from a measurement" this module avoids
                         * everywhere else. Linux reads both the same way
                         */
                        if (raw == ACPI_HMAT_VALUE_UNAVAILABLE || raw == 0)
                                return HMAT_LOOKUP_UNAVAILABLE;

                        /* the base unit comes from the table as a 64-bit
                         * number, so the result can leave the type. A wrapped
                         * one would be a small plausible figure with nothing to
                         * mark it as nonsense, which is worse than no answer
                         */
                        if (!hmat_scale(raw, m->base_unit, divisor, value)) {
                                LOG_DEBUG("HMAT entry %u scaled by base unit "
                                          "%llu does not fit\n",
                                          raw,
                                          (unsigned long long)m->base_unit);

                                return HMAT_LOOKUP_UNSCALABLE;
                        }

                        return HMAT_LOOKUP_VALUE;
                }
        }

        return HMAT_LOOKUP_ABSENT;
}

/** The four numbers the report carries, for bookkeeping across the structures
 */
enum locality_metric {
        LOCALITY_READ_LATENCY = 0,
        LOCALITY_WRITE_LATENCY,
        LOCALITY_READ_BANDWIDTH,
        LOCALITY_WRITE_BANDWIDTH,
        LOCALITY_METRICS
};

/** What has been chosen for one of the four, and on what grounds */
struct locality_pick {
        int answered;       /**< some structure described this number */
        int specific;       /**< that structure named a direction */
        int has_value;      /**< and produced a number, rather than refusing */
        unsigned qualifier; /**< how broadly it applies; smaller is broader */
        int min_transfer_qualified; /**< it holds only above a size */
        uint8_t min_transfer_size;  /**< which that structure named */
        int non_sequential; /**< and only for non-sequential transfers */
        uint64_t value;
};

/**
 * @brief Which of the four numbers a data type describes, and of what kind
 *
 * A generic access structure describes both directions of its kind, which is
 * what makes it the fallback: anything naming a direction is a more particular
 * statement about that direction.
 *
 * @param [in] data_type the structure's data type
 * @param [out] metrics the numbers it describes, at most two
 * @param [out] specific whether it names a direction
 * @param [out] latency whether they are latencies rather than bandwidths, which
 *              with the table's revision decides the unit
 *
 * @return how many of the four it describes, zero where it describes none
 */
static unsigned
locality_metrics_of(const uint8_t data_type,
                    enum locality_metric metrics[2],
                    int *specific,
                    int *latency)
{
        *specific = 1;
        *latency = 0;

        switch (data_type) {
        case ACPI_HMAT_ACCESS_LATENCY:
                *specific = 0;
                *latency = 1;
                metrics[0] = LOCALITY_READ_LATENCY;
                metrics[1] = LOCALITY_WRITE_LATENCY;
                return 2;
        case ACPI_HMAT_READ_LATENCY:
                *latency = 1;
                metrics[0] = LOCALITY_READ_LATENCY;
                return 1;
        case ACPI_HMAT_WRITE_LATENCY:
                *latency = 1;
                metrics[0] = LOCALITY_WRITE_LATENCY;
                return 1;
        case ACPI_HMAT_ACCESS_BANDWIDTH:
                *specific = 0;
                metrics[0] = LOCALITY_READ_BANDWIDTH;
                metrics[1] = LOCALITY_WRITE_BANDWIDTH;
                return 2;
        case ACPI_HMAT_READ_BANDWIDTH:
                metrics[0] = LOCALITY_READ_BANDWIDTH;
                return 1;
        case ACPI_HMAT_WRITE_BANDWIDTH:
                metrics[0] = LOCALITY_WRITE_BANDWIDTH;
                return 1;
        default:
                return 0;
        }
}

/**
 * @brief What an entry has to be divided by to reach the unit the report states
 *
 * Revision 1 published every metric in tenths of the structure's base unit, so
 * both kinds are divided by ten. Revision 2 kept bandwidth in MB/s, which is
 * the unit the report states, and moved latency to picoseconds, which is a
 * thousand to the nanosecond. Nothing else in the module needs the revision,
 * and a revision with no entry here never reaches this point - hmat_parse()
 * drops the table instead.
 *
 * @param [in] revision the revision of the table the entry came from
 * @param [in] latency whether the entry is a latency rather than a bandwidth
 *
 * @return the divisor
 */
static uint64_t
hmat_divisor(const unsigned revision, const int latency)
{
        if (revision == ACPI_HMAT_REVISION_TENTHS)
                return ACPI_HMAT_TENTHS_PER_UNIT;

        return latency ? ACPI_HMAT_PS_PER_NS : 1;
}

/**
 * @brief Whether an answer outranks the one already chosen for a number
 *
 * The rule, in order, and it is a rule rather than table order because HMAT may
 * describe one number more than once and nothing says which copy comes first:
 *
 *   naming the direction wins, so a read structure beats a generic access one,
 *   including when what it says is that it has no number - that is the whole
 *   point of the generic structure being a fallback;
 *   then a number beats a refusal, so a figure carrying a condition is still
 *   reported where the structure that carries none has nothing to report;
 *   then the broader of the two, the one applying to any transfer ahead of one
 *   that needs a minimum, and a smaller minimum ahead of a larger.
 *
 * @param [in] pick what has been chosen so far
 * @param [in] specific whether the new answer names a direction
 * @param [in] has_value whether it is a number rather than a refusal
 * @param [in] qualifier how broadly it applies
 *
 * @retval 1 the new answer is the one to keep
 * @retval 0 the old one stands
 */
static int
locality_outranks(const struct locality_pick *pick,
                  const int specific,
                  const int has_value,
                  const unsigned qualifier)
{
        if (!pick->answered)
                return 1;
        if (specific != pick->specific)
                return specific;
        if (has_value != pick->has_value)
                return has_value;

        return qualifier < pick->qualifier;
}

/**
 * @brief Offers one structure's answer for one of the four numbers
 *
 * @param [in,out] pick what has been chosen for that number
 * @param [in] specific whether the structure names a direction
 * @param [in] has_value whether it produced a number
 * @param [in] m the structure the answer came from
 * @param [in] value the number, where there is one
 */
static void
locality_offer(struct locality_pick *pick,
               const int specific,
               const int has_value,
               const struct hmat_matrix *m,
               const uint64_t value)
{
        const unsigned qualifier = m->qualifier;

        if (!locality_outranks(pick, specific, has_value, qualifier)) {
                /* two structures of the same kind, equally qualified, and not
                 * agreeing: nothing in the table breaks the tie, so the first
                 * is kept and the platform is told about the other
                 */
                if (has_value && pick->has_value &&
                    qualifier == pick->qualifier && value != pick->value)
                        LOG_DEBUG("HMAT describes one number twice, equally "
                                  "qualified, as %llu and %llu: keeping the "
                                  "first\n",
                                  (unsigned long long)pick->value,
                                  (unsigned long long)value);
                return;
        }

        pick->answered = 1;
        pick->specific = specific;
        pick->has_value = has_value;
        pick->qualifier = qualifier;
        pick->min_transfer_qualified = m->min_transfer_qualified;
        pick->min_transfer_size = m->min_transfer_size;
        pick->non_sequential = m->non_sequential;
        /* an answer that is not a number carries no number. The caller passes
         * zero for one, so this is what it already held - written explicitly
         * because the guarantee below, that a figure never outlives the flag
         * that says it is one, should not rest on that
         */
        pick->value = has_value ? value : 0;
}

/**
 * @brief Records why a matrix produced no number
 *
 * Kept for the report rather than for the reader of the values: where none of
 * the four came out, the report says why, and "the platform has no number for
 * this pair" and "the number it has cannot be stated here" are not the same
 * admission. Sticky, and never cleared, because a matrix that did answer sets
 * its own valid flag and the reason is printed only when none of them is set.
 *
 * @param [out] loc locality to mark
 * @param [in] answer what the matrix said
 */
static void
locality_note(struct pqos_mem_locality *loc, const enum hmat_lookup answer)
{
        if (answer == HMAT_LOOKUP_UNAVAILABLE)
                loc->values_unavailable = 1;
        else if (answer == HMAT_LOOKUP_UNSCALABLE)
                loc->values_unrepresentable = 1;
}

/**
 * @brief Writes one of the four numbers, with the condition it came with
 *
 * The minimum transfer size travels with the figure rather than beside it: a
 * number that holds only for large transfers is not the same statement as an
 * unconditional one, and a caller reading the value without the flag would take
 * the narrower claim for the broader.
 *
 * @param [out] loc locality to write
 * @param [in] metric which number
 * @param [in] valid whether it is known
 * @param [in] pick what was chosen for it
 */
static void
locality_store(struct pqos_mem_locality *loc,
               const enum locality_metric metric,
               const int valid,
               const struct locality_pick *pick)
{
        /* nothing of a figure survives the flag that says there is one: a
         * caller reading a value without its valid flag gets a zero, not the
         * last number some other structure happened to publish
         */
        const uint64_t value = valid ? pick->value : 0;
        const int sized = valid && pick->min_transfer_qualified;
        const uint8_t minimum = sized ? pick->min_transfer_size : 0;
        const int non_sequential = valid && pick->non_sequential;

        switch (metric) {
        case LOCALITY_READ_LATENCY:
                loc->read_latency_ns = value;
                loc->read_latency_valid = valid;
                loc->read_latency_min_transfer_qualified = sized;
                loc->read_latency_min_transfer = minimum;
                loc->read_latency_non_sequential = non_sequential;
                break;
        case LOCALITY_WRITE_LATENCY:
                loc->write_latency_ns = value;
                loc->write_latency_valid = valid;
                loc->write_latency_min_transfer_qualified = sized;
                loc->write_latency_min_transfer = minimum;
                loc->write_latency_non_sequential = non_sequential;
                break;
        case LOCALITY_READ_BANDWIDTH:
                loc->read_bandwidth_mbs = value;
                loc->read_bandwidth_valid = valid;
                loc->read_bandwidth_min_transfer_qualified = sized;
                loc->read_bandwidth_min_transfer = minimum;
                loc->read_bandwidth_non_sequential = non_sequential;
                break;
        case LOCALITY_WRITE_BANDWIDTH:
                loc->write_bandwidth_mbs = value;
                loc->write_bandwidth_valid = valid;
                loc->write_bandwidth_min_transfer_qualified = sized;
                loc->write_bandwidth_min_transfer = minimum;
                loc->write_bandwidth_non_sequential = non_sequential;
                break;
        default:
                break;
        }
}

/**
 * @brief Fills in the latency and bandwidth of an initiator-target pair
 *
 * HMAT describes the four numbers independently, and a platform may carry any
 * subset of them, so each is marked found or not rather than left as a zero
 * that cannot be told from a measurement.
 *
 * It may also describe one of them more than once - a generic access structure
 * standing in for both directions, or two structures of one kind qualified by
 * different minimum transfer sizes - so which one the report states is decided
 * by locality_outranks() above and not by where they sit in the table. Every
 * structure is offered to every number it describes, and the ranking settles
 * it; a pair a structure does not list is not an answer and is not offered.
 *
 * A latency entry is a count of picoseconds, reported in nanoseconds because
 * that is the unit the numbers are legible in; a bandwidth entry is already
 * MB/s.
 *
 * @param [in] facts the parsed tables
 * @param [in] initiator initiator proximity domain
 * @param [in] target target proximity domain
 * @param [out] loc locality to fill
 */
static void
locality_fill(const struct acpi_facts *facts,
              const unsigned initiator,
              const unsigned target,
              struct pqos_mem_locality *loc)
{
        struct locality_pick pick[LOCALITY_METRICS];
        unsigned i;

        memset(pick, 0, sizeof(pick));

        loc->initiator_domain = initiator;
        loc->target_domain = target;

        for (i = 0; i < facts->num_hmat; i++) {
                const struct hmat_matrix *m = &facts->hmat[i];
                enum locality_metric metrics[2];
                unsigned count;
                unsigned k;
                int specific = 0;
                int latency = 0;
                uint64_t value = 0;
                enum hmat_lookup answer;

                count = locality_metrics_of(m->data_type, metrics, &specific,
                                            &latency);
                if (count == 0)
                        continue;

                answer = hmat_value(m, initiator, target,
                                    hmat_divisor(facts->hmat_revision, latency),
                                    &value);
                if (answer == HMAT_LOOKUP_ABSENT)
                        continue;

                if (answer != HMAT_LOOKUP_VALUE)
                        locality_note(loc, answer);

                for (k = 0; k < count; k++)
                        locality_offer(&pick[metrics[k]], specific,
                                       answer == HMAT_LOOKUP_VALUE, m, value);
        }

        for (i = 0; i < LOCALITY_METRICS; i++)
                locality_store(loc, (enum locality_metric)i,
                               pick[i].answered && pick[i].has_value, &pick[i]);

        loc->valid = loc->read_latency_valid || loc->write_latency_valid ||
                     loc->read_bandwidth_valid || loc->write_bandwidth_valid;
}

/**
 * @brief Describes one region from the tables
 *
 * @param [in] facts the parsed tables
 * @param [in] regions the regions being built, for the range array
 * @param [in,out] region the region to describe
 */
static void
region_describe(const struct acpi_facts *facts,
                const struct pqos_mem_regions *regions,
                struct pqos_mem_region *region)
{
        unsigned i;
        unsigned j;
        unsigned covered = 0;
        unsigned srat_enabled_ranges = 0;
        int partly_covered = 0;
        int srat_domain_known = 0;
        int srat_domain_mixed = 0;
        unsigned srat_domain = 0;

        for (i = 0; i < region->num_ranges; i++) {
                const struct pqos_mem_range *r =
                    &regions->range[region->range_index[i]];
                int in_window = 0;
                int srat_enabled = 0;
                int srat_reserved = 0;

                unsigned domain = 0;

                /* firmware accounted for the whole range with memory it has
                 * described and left disabled, which is what a reserved window
                 * looks like in SRAT. Coverage is asked of it, of one domain,
                 * and of disabled entries alone: a single entry reaching into
                 * the range says nothing about the rest of it, and a range
                 * whose enabled and disabled entries cover a part each is not
                 * a reservation either - it is a table describing two different
                 * things, and calling the whole of it reserved would report a
                 * statement firmware never made. Anything short of one domain's
                 * disabled entries covering all of it stays unclassified
                 */
                srat_reserved = srat_domain_of(facts, r->base_address,
                                               r->length, 0, &domain);
                domain = 0;

                /* the domain is another matter: it is claimed only where one
                 * domain's enabled entries account for the whole range. Entries
                 * firmware has not enabled describe memory that is not there -
                 * a hot-plug placeholder, say - so they cannot carry the
                 * region's domain, and hanging HMAT numbers off one would
                 * describe the speed of memory nobody can use
                 */
                if (srat_domain_of(facts, r->base_address, r->length, 1,
                                   &domain)) {
                        srat_enabled_ranges++;
                        srat_enabled = 1;

                        if (!srat_domain_known) {
                                srat_domain = domain;
                                srat_domain_known = 1;
                        } else if (srat_domain != domain) {
                                /* the ranges of one region in two proximity
                                 * domains: there is no single target to report,
                                 * and picking the first would attach one
                                 * domain's numbers to the other's memory
                                 */
                                srat_domain_mixed = 1;
                        }
                }

                for (j = 0; j < facts->num_cedt; j++) {
                        if (!ranges_overlap(r->base_address, r->length,
                                            facts->cedt[j].base,
                                            facts->cedt[j].size))
                                continue;

                        in_window = 1;
                        break;
                }

                if (in_window &&
                    !cedt_covers(facts, r->base_address, r->length))
                        partly_covered = 1;

                if (in_window) {
                        covered++;
                        /* three answers, not two. One domain's enabled
                         * entries covering the range says memory is there; one
                         * domain's disabled entries covering it is firmware
                         * reserving the window; anything else - no entry, part
                         * of a range, two domains describing it between them,
                         * or enabled and disabled entries each covering a piece
                         * - is neither of those, and counting it as reserved
                         * would turn silence or ambiguity into a statement
                         * firmware never made. Counted per range, since these
                         * tables enumerate no devices to count by
                         */
                        if (srat_enabled)
                                region->active_mres++;
                        else if (srat_reserved)
                                region->reserved_mres++;
                        else
                                region->unclassified_mres++;
                }
        }

        /* a window touching the region is what makes it CXL space at all;
         * covering every address of every range is a stronger statement, and
         * both are reported because a platform can describe a region whose
         * windows leave gaps - memory no window accounts for
         */
        region->cedt_match = covered > 0;
        region->cfmws_match = covered > 0;
        region->cxl_range_match =
            covered == region->num_ranges && covered > 0 && !partly_covered;

        /* A window covering the ranges makes the region CXL space. The other
         * answer takes two things, because SRAT does not carry one of them:
         * SRAT says the memory is there and which domain it is in, not what
         * kind of memory it is - the CXL memory on a platform like this one is
         * an enabled SRAT entry like any other. So calling a region local
         * memory needs the knowledge that no CXL window covers it, and that is
         * only knowledge where every window is accounted for: a CEDT walked to
         * its end, or no CEDT at all, which is a platform with no host bridge
         * to publish one. A CEDT that exists and could not be read in full
         * leaves the question open, and the type is unknown rather than a guess
         * in the direction of ordinary memory.
         */
        if (region->cedt_match)
                region->type = PQOS_MEM_REGION_CXL;
        else if (facts->cedt_known && srat_domain_known && !srat_domain_mixed &&
                 srat_enabled_ranges == region->num_ranges)
                region->type = PQOS_MEM_REGION_LOCAL;
        else
                region->type = PQOS_MEM_REGION_UNKNOWN;

        /* SRAT is reported as describing the region only when it describes all
         * of it, with memory firmware has enabled, and coherently. A region
         * half of whose ranges SRAT knows, or whose ranges sit in different
         * proximity domains, has no one target to hang locality numbers on, and
         * saying otherwise would publish one range's numbers as the whole
         * region's. A disabled entry is counted where it belongs - in
         * reserved_mres above - and not here, there being no memory behind it
         * for anything to be local to.
         */
        if (!srat_domain_known || srat_domain_mixed ||
            srat_enabled_ranges != region->num_ranges) {
                if (srat_domain_known)
                        LOG_DEBUG("Region 0x%x described by SRAT only in part "
                                  "(%u of %u range(s)%s)\n",
                                  region->local_region_id, srat_enabled_ranges,
                                  region->num_ranges,
                                  srat_domain_mixed ? ", mixed domains" : "");

                return;
        }

        region->srat_match = 1;

        /* the target of the region is the domain SRAT put its memory in; the
         * initiator is whichever domain HMAT pairs with that target, and where
         * HMAT says nothing the pair cannot be reported
         */
        region->target_domain = srat_domain;

        for (i = 0; i < facts->num_hmat_domains; i++) {
                if (facts->hmat_target[i] != srat_domain)
                        continue;

                region->initiator_domain = facts->hmat_initiator[i];
                region->proximity_valid = 1;
                region->hmat_match = 1;
                locality_fill(facts, region->initiator_domain, srat_domain,
                              &region->locality);
                break;
        }
}

/**
 * @brief Releases what parsing allocated, and unmaps the tables
 *
 * @param [in] facts the parsed tables
 */
static void
facts_free(struct acpi_facts *facts)
{
        free(facts->srat);
        free(facts->cedt);
        free(facts->hmat);
        free(facts->hmat_initiator);
        free(facts->hmat_target);

        if (facts->srat_tbl != NULL)
                acpi_free(facts->srat_tbl);
        if (facts->hmat_tbl != NULL)
                acpi_free(facts->hmat_tbl);
        if (facts->cedt_tbl != NULL)
                acpi_free(facts->cedt_tbl);

        memset(facts, 0, sizeof(*facts));
}

/**
 * @brief Reads the three correlating tables, and says which of them answered
 *
 * The flags are set whether or not there is anything to correlate: which tables
 * the platform has is a fact about the platform, and a caller told nothing
 * about them cannot tell an absent table from one that described no range of
 * theirs.
 *
 * @param [out] facts what the tables contributed
 * @param [out] regions the description to record their availability in
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK the tables were read, or are not there
 * @retval PQOS_RETVAL_RESOURCE out of memory
 */
static int
facts_read(struct acpi_facts *facts, struct pqos_mem_regions *regions)
{
        memset(facts, 0, sizeof(*facts));

        if (srat_parse(facts) != PQOS_RETVAL_OK ||
            hmat_parse(facts) != PQOS_RETVAL_OK ||
            cedt_parse(facts) != PQOS_RETVAL_OK) {
                LOG_ERROR("Could not allocate memory to describe the memory "
                          "regions\n");
                facts_free(facts);

                return PQOS_RETVAL_RESOURCE;
        }

        regions->srat_available = facts->srat_read;
        regions->hmat_available = facts->hmat_read;
        regions->cedt_available = facts->cedt_read;
        regions->cedt_complete = facts->cedt_known;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Releases a region structure
 *
 * @param [in] regions the structure to release
 */
static void
regions_free(struct pqos_mem_regions *regions)
{
        unsigned i;

        if (regions == NULL)
                return;

        for (i = 0; i < regions->num_regions; i++)
                free(regions->region[i].range_index);

        free(regions->region);
        free(regions->range);
        free(regions);
}

int
mem_regions_init(const struct pqos_mrrm_info *mrrm,
                 struct pqos_mem_regions **regions)
{
        struct acpi_facts facts;
        struct pqos_mem_regions *out;
        unsigned i;
        unsigned j;

        if (mrrm == NULL || regions == NULL)
                return PQOS_RETVAL_PARAM;

        out = calloc(1, sizeof(*out));
        if (out == NULL)
                return PQOS_RETVAL_RESOURCE;

        out->dynamic_region_ids = mrrm->flags != 0;
        out->max_regions_supported = mrrm->max_memory_regions_supported;
        out->num_range_entries = mrrm->num_mres;

        /* a table with a header and no range entries is not a failure: it says
         * how region IDs are assigned and how many regions the platform
         * supports, and those are the two things the report can still state.
         * Refusing here left the caller with nothing at all, so the report said
         * no information was available about a table that had been read.
         *
         * The correlating tables are still looked for, with nothing to
         * correlate them against, because whether the platform has them is a
         * fact about the platform: leaving the flags clear would say it has
         * none, which is a different statement from having no ranges for them
         * to describe
         */
        if (mrrm->num_mres == 0) {
                if (facts_read(&facts, out) != PQOS_RETVAL_OK) {
                        regions_free(out);

                        return PQOS_RETVAL_RESOURCE;
                }

                facts_free(&facts);
                *regions = out;
                m_regions = out;

                return PQOS_RETVAL_OK;
        }

        /* the ranges are one per table entry and can be allocated now; the
         * regions are counted first, below, so that their array is the size of
         * what is there rather than of the table
         */
        out->range = calloc(mrrm->num_mres, sizeof(*out->range));
        if (out->range == NULL) {
                regions_free(out);

                return PQOS_RETVAL_RESOURCE;
        }

        /* the ranges first, in table order, because that is the order the
         * indices below and the report refer to them by
         */
        for (i = 0; i < mrrm->num_mres; i++) {
                const struct pqos_mre_info *mre = &mrrm->mre[i];

                out->range[i].base_address =
                    ((uint64_t)mre->base_address_high << 32) |
                    mre->base_address_low;
                out->range[i].length =
                    ((uint64_t)mre->length_high << 32) | mre->length_low;
                out->range[i].local_region_id = mre->local_region_id;
                out->range[i].local_region_id_valid =
                    (mre->region_id_flags & PQOS_MRE_VALID_LOCAL_REGION_ID) !=
                    0;
                out->range[i].remote_region_id = mre->remote_region_id;
                out->range[i].remote_region_id_valid =
                    (mre->region_id_flags & PQOS_MRE_VALID_REMOTE_REGION_ID) !=
                    0;
        }

        /* then the regions, one per distinct local region ID. A range whose
         * local ID MRRM did not mark valid belongs to no region: it is a range
         * the platform described without saying which region carries it.
         *
         * Counted before anything is allocated, and counted twice over: how
         * many distinct region IDs there are, and then how many ranges land in
         * each. Sizing either array by the length of the table would assume the
         * worst for every entry, and the worst is not the table's length - a
         * local region ID is a byte, so a table of thousands of ranges still
         * describes at most 256 regions, and each region holds only its own
         * ranges rather than room for all of them.
         */
        {
                unsigned char seen[MEM_REGION_IDS];
                unsigned found = 0;

                memset(seen, 0, sizeof(seen));

                for (i = 0; i < mrrm->num_mres; i++) {
                        const uint8_t id = out->range[i].local_region_id;

                        if (!out->range[i].local_region_id_valid || seen[id])
                                continue;

                        seen[id] = 1;
                        found++;
                }

                /* every range the platform declined to place in a region: there
                 * is nothing to group, and the ranges themselves still stand
                 */
                if (found == 0) {
                        if (facts_read(&facts, out) != PQOS_RETVAL_OK) {
                                regions_free(out);

                                return PQOS_RETVAL_RESOURCE;
                        }

                        facts_free(&facts);
                        m_regions = out;
                        *regions = out;

                        return PQOS_RETVAL_OK;
                }

                out->region = calloc(found, sizeof(*out->region));
                if (out->region == NULL) {
                        regions_free(out);

                        return PQOS_RETVAL_RESOURCE;
                }
        }

        for (i = 0; i < mrrm->num_mres; i++) {
                if (!out->range[i].local_region_id_valid)
                        continue;

                for (j = 0; j < out->num_regions; j++)
                        if (out->region[j].local_region_id ==
                            out->range[i].local_region_id)
                                break;

                if (j == out->num_regions) {
                        out->region[j].local_region_id =
                            out->range[i].local_region_id;
                        out->num_regions++;
                }

                out->region[j].num_ranges++;
        }

        for (i = 0; i < out->num_regions; i++) {
                struct pqos_mem_region *region = &out->region[i];

                region->range_index =
                    calloc(region->num_ranges, sizeof(*region->range_index));
                if (region->range_index == NULL) {
                        regions_free(out);
                        return PQOS_RETVAL_RESOURCE;
                }
                /* filled by the pass below, which counts them again as it goes
                 */
                region->num_ranges = 0;
        }

        for (i = 0; i < mrrm->num_mres; i++) {
                if (!out->range[i].local_region_id_valid)
                        continue;

                for (j = 0; j < out->num_regions; j++) {
                        struct pqos_mem_region *region = &out->region[j];

                        if (region->local_region_id !=
                            out->range[i].local_region_id)
                                continue;

                        region->range_index[region->num_ranges++] = i;
                        break;
                }
        }

        /* out of memory in any of the three is fatal to the description: a
         * report built from facts a calloc() failure left out would be wrong
         * rather than partial, and the caller is documented to hear about it
         */
        /* the size of each region, in a pass of its own so that a wrap can be
         * detected without leaving a half-built total behind. Firmware
         * declaring overlapping or enormous ranges under one region ID would
         * otherwise produce a small plausible size out of an addition that left
         * the type
         */
        for (i = 0; i < out->num_regions; i++) {
                struct pqos_mem_region *region = &out->region[i];
                uint64_t total = 0;
                unsigned k;

                region->total_size_valid = 1;

                for (k = 0; k < region->num_ranges && region->total_size_valid;
                     k++) {
                        const struct pqos_mem_range *a =
                            &out->range[region->range_index[k]];
                        uint64_t last;
                        unsigned l;

                        /* a range with no representable end is not a length to
                         * add: MRRM described something that is not an address
                         * range, and adding it would make a total out of it
                         */
                        if (!range_last(a->base_address, a->length, &last)) {
                                LOG_DEBUG("Region 0x%x: a range of it does not "
                                          "describe an address range\n",
                                          region->local_region_id);
                                region->total_size_valid = 0;
                                break;
                        }

                        /* and ranges that overlap cannot be summed either: the
                         * shared addresses would be counted twice, so the
                         * figure would be larger than the memory it claims to
                         * measure
                         */
                        for (l = 0; l < k; l++) {
                                const struct pqos_mem_range *b =
                                    &out->range[region->range_index[l]];

                                if (!ranges_overlap(a->base_address, a->length,
                                                    b->base_address, b->length))
                                        continue;

                                LOG_DEBUG("Region 0x%x: two of its ranges "
                                          "overlap\n",
                                          region->local_region_id);
                                region->total_size_valid = 0;
                                break;
                        }

                        if (!region->total_size_valid)
                                break;

                        if (total > UINT64_MAX - a->length) {
                                LOG_DEBUG("Region 0x%x: the lengths of its "
                                          "ranges do not add up to a size\n",
                                          region->local_region_id);
                                region->total_size_valid = 0;
                                break;
                        }

                        total += a->length;
                }

                region->total_size = region->total_size_valid ? total : 0;
        }

        if (facts_read(&facts, out) != PQOS_RETVAL_OK) {
                regions_free(out);

                return PQOS_RETVAL_RESOURCE;
        }

        for (i = 0; i < out->num_regions; i++)
                region_describe(&facts, out, &out->region[i]);

        facts_free(&facts);

        LOG_DEBUG("Memory regions: %u region(s) over %u range(s)\n",
                  out->num_regions, out->num_range_entries);

        m_regions = out;
        *regions = out;

        return PQOS_RETVAL_OK;
}

int
mem_regions_fini(void)
{
        regions_free(m_regions);
        m_regions = NULL;

        return PQOS_RETVAL_OK;
}
