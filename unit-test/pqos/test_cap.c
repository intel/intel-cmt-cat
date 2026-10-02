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

#include "test_cap.h"

#include "cap.h"
#include "common.h"
#include "output.h"
#include "pqos.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/* ======== cap_print_io_dev ======== */

/* The devices of the two cases below, in the form the report reads them: one
 * the platform gave two I/O RDT channels, and one it gave none, which is what
 * a device that is not an I/O RDT device looks like here. Everything the
 * report prints about a device comes from this structure, so the cases do not
 * depend on the PCI devices of the machine the test runs on.
 */
static struct pqos_pci_info test_pci_with_channels = {
    .subclass_name = "Signal processing controller",
    .vendor_name = "Intel Corporation",
    .is_pcie = 1,
    .pcie_type = "PCIe Device",
    .numa = 0,
    .num_channels = 2,
    .channels = {0x10004, 0x10001},
    .mmio_addr = {0x1ffff6aee000, 0x1ffff6aee000},
    .domain_id = 0x10};

static struct pqos_pci_info test_pci_without_channels = {
    .subclass_name = "Host bridge",
    .vendor_name = "Intel Corporation",
    .numa = -1,
    .num_channels = 0};

/* A platform with I/O RDT devices on it, so that a device with no channel is
 * a statement about the device rather than about the platform
 */
static struct pqos_devinfo test_devinfo = {.num_devs = 4};

/* and one that reports no I/O RDT device at all */
static struct pqos_devinfo test_devinfo_empty = {.num_devs = 0};

/* The capabilities travel in a zero length array at the end of struct
 * pqos_cap, so the space for the one capability these cases need is reserved
 * beside it. The L3 CAT capability is what makes the class of service and
 * cache way counts of the report available, so a case that expects them to be
 * left out says something about the device rather than about the capability.
 */
static struct pqos_cap_l3ca test_l3ca = {.num_classes = 6, .num_ways = 12};

static union {
        struct pqos_cap cap;
        char reserved[sizeof(struct pqos_cap) + sizeof(struct pqos_capability)];
} test_caps;

static int
test_cap_group_setup(void **state)
{
        UNUSED_ARG(state);

        test_caps.cap.num_cap = 1;
        test_caps.cap.capabilities[0].type = PQOS_CAP_TYPE_L3CA;
        test_caps.cap.capabilities[0].u.l3ca = &test_l3ca;

        return 0;
}

/**
 * @brief Select one device the way the --print-io-dev option does
 *
 * @param [in] bdf the option argument, which parse_io_dev() takes apart
 */
static void
select_io_dev(const char *bdf)
{
        char arg[] = "0000:00:00.0";

        assert_true(strlen(bdf) < sizeof(arg));
        strncpy(arg, bdf, sizeof(arg) - 1);

        parse_io_dev(arg);
}

static void
test_cap_print_io_dev_without_a_device(void **state)
{
        struct pqos_sysconfig sys = {.cap = &test_caps.cap,
                                     .dev = &test_devinfo};
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        run_function(cap_print_io_dev, ret, &sys);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(
            output_has_text("Segment and BDF information are missing"), 1);
}

static void
test_cap_print_io_dev_without_dev_info(void **state)
{
        struct pqos_sysconfig sys = {.cap = &test_caps.cap, .dev = NULL};
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        select_io_dev("0000:0d:00.0");
        run_function(cap_print_io_dev, ret, &sys);

        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("IRDT info not available"), 1);
}

/* The interface query is the one early return that carries the status it was
 * given rather than one of its own. Every path out of the function releases the
 * device selection, and the call that follows is what proves it: it has nothing
 * to report rather than the device this one selected.
 */
static void
test_cap_print_io_dev_without_an_interface(void **state)
{
        struct pqos_sysconfig sys = {.cap = &test_caps.cap,
                                     .dev = &test_devinfo};
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_ERROR);

        select_io_dev("0000:0d:00.0");
        run_function(cap_print_io_dev, ret, &sys);

        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(output_has_text("unable to get interface"), 1);

        ret = PQOS_RETVAL_OK;
        run_function(cap_print_io_dev, ret, &sys);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(
            output_has_text("Segment and BDF information are missing"), 1);
}

static void
test_cap_print_io_dev_on_another_interface(void **state)
{
        struct pqos_sysconfig sys = {.cap = &test_caps.cap,
                                     .dev = &test_devinfo};
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_OS);

        select_io_dev("0000:0d:00.0");
        run_function(cap_print_io_dev, ret, &sys);

        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(output_has_text("supported in msr and mmio "
                                         "interfaces only"),
                         1);
}

static void
test_cap_print_io_dev_without_erdt_info(void **state)
{
        struct pqos_sysconfig sys = {
            .cap = &test_caps.cap, .dev = &test_devinfo, .erdt = NULL};
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);

        select_io_dev("0000:0d:00.0");
        run_function(cap_print_io_dev, ret, &sys);

        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("ERDT info not available"), 1);
}

static void
test_cap_print_io_dev_absent_device(void **state)
{
        struct pqos_sysconfig sys = {.cap = &test_caps.cap,
                                     .dev = &test_devinfo};
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MSR);
        will_return(__wrap_pqos_l3ca_get_min_cbm_bits, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get_min_cbm_bits, 2);

        expect_value(__wrap_pqos_io_devs_get, segment, 0);
        expect_value(__wrap_pqos_io_devs_get, bdf, 0x0d00);
        will_return(__wrap_pqos_io_devs_get, PQOS_RETVAL_RESOURCE);

        select_io_dev("0000:0d:00.0");
        run_function(cap_print_io_dev, ret, &sys);

        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("Unable to get I/O device "
                                         "0000:0d:00.0 PCI information"),
                         1);
}

/* A device that is present and is not an I/O RDT device: the report says so
 * and returns a status, rather than printing an entry with nothing in it and
 * the commands to monitor and to allocate it
 */
static void
test_cap_print_io_dev_without_channels(void **state)
{
        struct pqos_sysconfig sys = {.cap = &test_caps.cap,
                                     .dev = &test_devinfo};
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MSR);
        will_return(__wrap_pqos_l3ca_get_min_cbm_bits, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get_min_cbm_bits, 2);

        expect_value(__wrap_pqos_io_devs_get, segment, 0);
        expect_value(__wrap_pqos_io_devs_get, bdf, 0);
        will_return(__wrap_pqos_io_devs_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_io_devs_get, &test_pci_without_channels);

        select_io_dev("0000:00:00.0");
        run_function(cap_print_io_dev, ret, &sys);

        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_exit_was_called(), 0);

        /* the device is named, so that the report can be told from the one of
         * a device that is absent
         */
        assert_int_equal(output_has_text("0000:00:00.0 Host bridge"), 1);
        assert_int_equal(output_has_text("not an I/O RDT device"), 1);

        assert_int_equal(output_has_text("Associated Channels"), 0);
        assert_int_equal(output_has_text("MMIO Addresses"), 0);
        assert_int_equal(output_has_text("Monitoring Commands"), 0);
        assert_int_equal(output_has_text("Allocation Commands"), 0);
        assert_int_equal(output_has_text("Available CLOS"), 0);
        assert_int_equal(output_has_text("Available Cache Ways"), 0);
}

/* The same device on a platform that reports no I/O RDT device at all, where
 * the reason is the platform and the commands printed above the entry are what
 * resolves it
 */
static void
test_cap_print_io_dev_without_iordt(void **state)
{
        struct pqos_sysconfig sys = {.cap = &test_caps.cap,
                                     .dev = &test_devinfo_empty};
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MSR);
        will_return(__wrap_pqos_l3ca_get_min_cbm_bits, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get_min_cbm_bits, 2);

        expect_value(__wrap_pqos_io_devs_get, segment, 0);
        expect_value(__wrap_pqos_io_devs_get, bdf, 0);
        will_return(__wrap_pqos_io_devs_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_io_devs_get, &test_pci_without_channels);

        select_io_dev("0000:00:00.0");
        run_function(cap_print_io_dev, ret, &sys);

        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(output_has_text("The platform reports no I/O RDT "
                                         "device"),
                         1);
        assert_int_equal(output_has_text("Enable I/O RDT Allocation"), 1);
        assert_int_equal(output_has_text("Associated Channels"), 0);
}

/* An I/O RDT device, which keeps the whole entry and the status of a report
 * that was printed
 */
static void
test_cap_print_io_dev_with_channels(void **state)
{
        struct pqos_sysconfig sys = {.cap = &test_caps.cap,
                                     .dev = &test_devinfo};
        int ret = PQOS_RETVAL_RESOURCE;

        UNUSED_ARG(state);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MSR);
        will_return(__wrap_pqos_l3ca_get_min_cbm_bits, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get_min_cbm_bits, 2);

        expect_value(__wrap_pqos_io_devs_get, segment, 0);
        expect_value(__wrap_pqos_io_devs_get, bdf, 0x0d00);
        will_return(__wrap_pqos_io_devs_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_io_devs_get, &test_pci_with_channels);

        select_io_dev("0000:0d:00.0");
        run_function(cap_print_io_dev, ret, &sys);

        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(output_has_text("0000:0d:00.0 Signal processing "
                                         "controller: Intel Corporation"),
                         1);
        assert_int_equal(output_has_text("Associated Channels  : 0x10004"), 1);
        assert_int_equal(output_has_text("pqos --mon-dev=all:0000:0d:00.0@0"),
                         1);
        assert_int_equal(output_has_text("pqos -a dev:<CLOS>=0000:0d:00.0@1"),
                         1);
        assert_int_equal(output_has_text("Available CLOS     : 0 to 5"), 1);
        assert_int_equal(output_has_text("Available Cache Ways: 12"), 1);
        assert_int_equal(output_has_text("not an I/O RDT device"), 0);
}

int
__wrap_pqos_io_devs_get(struct pqos_pci_info *pci_info,
                        uint16_t segment,
                        uint16_t bdf)
{
        const struct pqos_pci_info *info;
        int ret;

        check_expected(segment);
        check_expected(bdf);

        ret = mock_type(int);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        info = mock_ptr_type(const struct pqos_pci_info *);
        *pci_info = *info;

        return ret;
}

/* ------------------------------------------------------------------------
 * The memory side cache block of the memory region report
 *
 * cap_print_region_cache() is static, and reachable through the public
 * cap_print_mem_regions(): MMIO interface, an mrrm pointer so the report is
 * attempted, and a description built here. What is asserted is the text,
 * because the text is the product - a selection or a wording that drifts is
 * invisible to the parser tests in unit-test/lib.
 * ------------------------------------------------------------------------
 */

/** one region and one range, enough for the report to print a region */
static unsigned cache_range_index[1] = {0};
static struct pqos_mem_range cache_range = {.base_address = 0,
                                            .length = 0x1000,
                                            .local_region_id_valid = 1,
                                            .local_region_id = 0};
static struct pqos_mem_region cache_region;
static struct pqos_mem_regions cache_regions;
static struct pqos_mrrm_info cache_mrrm;

/**
 * @brief A description carrying one region, for the cache block to be read from
 *
 * @param [in] hmat_available whether HMAT was read at all
 * @param [in] srat_match whether the region has a target domain
 *
 * @return the sysconfig to hand cap_print_mem_regions()
 */
static struct pqos_sysconfig
cache_report(int hmat_available, int srat_match)
{
        struct pqos_sysconfig sys;

        memset(&cache_region, 0, sizeof(cache_region));
        memset(&cache_regions, 0, sizeof(cache_regions));
        memset(&cache_mrrm, 0, sizeof(cache_mrrm));
        memset(&sys, 0, sizeof(sys));

        cache_region.num_ranges = 1;
        cache_region.range_index = cache_range_index;
        cache_region.type = PQOS_MEM_REGION_LOCAL;
        cache_region.srat_match = srat_match;
        cache_region.proximity_valid = srat_match;
        cache_region.target_domain = 3;

        cache_regions.num_range_entries = 1;
        cache_regions.range = &cache_range;
        cache_regions.num_regions = 1;
        cache_regions.region = &cache_region;
        cache_regions.srat_available = 1;
        cache_regions.hmat_available = hmat_available;

        sys.mrrm = &cache_mrrm;
        sys.mem_regions = &cache_regions;

        return sys;
}

/**
 * @brief Whether a line appears inside the Memory Side Cache block
 *
 * The report prints several blocks per region and some of their lines read
 * alike - the Proximity block prints "Target Domain     : 3" for the same
 * fixture as the cache block does - so a search of the whole output can be
 * satisfied by a line this case is not about. This looks only between the cache
 * block's own header and the blank line that ends it.
 *
 * @param [in] line the text to look for
 *
 * @retval 1 it is in that block
 * @retval 0 it is not
 */
static int
cache_block_has(const char *line)
{
        const char *out = output_get();
        const char *start;
        const char *end;
        const char *found;

        if (out == NULL)
                return 0;

        start = strstr(out, "Memory Side Cache:");
        if (start == NULL)
                return 0;

        /* searched in place rather than copied into a buffer: a buffer is a
         * length to get wrong, and a section that grew past it would make this
         * answer no for a line that is there
         */
        end = strstr(start, "\n\n");
        if (end == NULL)
                end = start + strlen(start);

        found = strstr(start, line);

        return found != NULL && found < end ? 1 : 0;
}

/** the cache a board really declares: one level of one, direct mapped, write
 *  back, 64 byte line
 */
static void
cache_one_level(struct pqos_mem_side_cache *cache)
{
        memset(cache, 0, sizeof(*cache));
        cache->valid = 1;
        cache->memory_domain = 3;
        cache->levels_declared = 1;
        cache->total_levels = 1;
        cache->size_valid = 1;
        cache->size = 0x2000000000ULL;
        cache->level_valid = 1;
        cache->level = 1;
        cache->associativity = PQOS_MEM_CACHE_ASSOC_DIRECT_MAPPED;
        cache->write_policy = PQOS_MEM_CACHE_WRITE_BACK;
        cache->line_size_valid = 1;
        cache->line_size = 64;
}

static void
test_cap_print_region_cache_all_figures(void **state)
{
        struct pqos_sysconfig sys = cache_report(1, 1);

        UNUSED_ARG(state);

        cache_one_level(&cache_region.mem_side_cache);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);
        run_void_function(cap_print_mem_regions, &sys);

        assert_int_equal(output_has_text("Memory Side Cache:"), 1);
        assert_int_equal(cache_block_has("Size              : "
                                         "0x0000002000000000"),
                         1);
        /* through cache_block_has(), because the Proximity block prints the
         * same "Target Domain     : 3" for this fixture and a search of the
         * whole report would be satisfied by that one
         */
        assert_int_equal(cache_block_has("Target Domain     : 3"), 1);
        assert_int_equal(cache_block_has("Level             : 1 of 1"), 1);
        assert_int_equal(cache_block_has("Associativity     : Direct Mapped"),
                         1);
        assert_int_equal(cache_block_has("Write Policy      : Write Back"), 1);
        assert_int_equal(cache_block_has("Line Size         : 64"), 1);
        /* one level declared, so no note about levels this does not carry */
        assert_int_equal(output_has_text("HMAT declares"), 0);
}

static void
test_cap_print_region_cache_without_a_size(void **state)
{
        struct pqos_sysconfig sys = cache_report(1, 1);

        UNUSED_ARG(state);

        cache_one_level(&cache_region.mem_side_cache);
        cache_region.mem_side_cache.size_valid = 0;
        cache_region.mem_side_cache.size = 0;

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);
        run_void_function(cap_print_mem_regions, &sys);

        /* a stated zero would be a cache of no bytes, which is not a
         * description - so the line says the platform stated none
         */
        assert_int_equal(cache_block_has("Size              : Not Available"),
                         1);
        /* the Size line and no hexadecimal one beside it. Not the bare value: a
         * zero address prints as 0x0000000000000000 in the range list above, so
         * the assertion has to name the line rather than the number
         */
        assert_int_equal(cache_block_has("Size              : 0x"), 0);
        assert_int_equal(cache_block_has("Level             : 1 of 1"), 1);
}

static void
test_cap_print_region_cache_without_a_level(void **state)
{
        struct pqos_sysconfig sys = cache_report(1, 1);

        UNUSED_ARG(state);

        cache_one_level(&cache_region.mem_side_cache);
        cache_region.mem_side_cache.level_valid = 0;
        cache_region.mem_side_cache.level = 0;
        cache_region.mem_side_cache.total_levels = 2;

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);
        run_void_function(cap_print_mem_regions, &sys);

        /* the total is a figure the platform did state, so it reaches the
         * report even where the level within it did not
         */
        assert_int_equal(
            cache_block_has("Level             : Not Available of 2"), 1);
}

static void
test_cap_print_region_cache_names_the_level_it_describes(void **state)
{
        struct pqos_sysconfig sys = cache_report(1, 1);

        UNUSED_ARG(state);

        /* two structures declared and the lowest level described, which is the
         * selection cache_fill() makes - the note must name the level and not a
         * position in a table whose order ACPI does not fix
         */
        cache_one_level(&cache_region.mem_side_cache);
        cache_region.mem_side_cache.levels_declared = 2;
        cache_region.mem_side_cache.total_levels = 2;

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);
        run_void_function(cap_print_mem_regions, &sys);

        assert_int_equal(output_has_text("HMAT declares 2 cache structures for "
                                         "this domain; level 1, the lowest "
                                         "stated, is described above"),
                         1);
        assert_int_equal(output_has_text("the first is described"), 0);
}

static void
test_cap_print_region_cache_note_without_any_level(void **state)
{
        struct pqos_sysconfig sys = cache_report(1, 1);

        UNUSED_ARG(state);

        /* several structures and not one of them states its level: there is no
         * level to name, and the note says so rather than naming a position
         */
        cache_one_level(&cache_region.mem_side_cache);
        cache_region.mem_side_cache.levels_declared = 3;
        cache_region.mem_side_cache.total_levels = 3;
        cache_region.mem_side_cache.level_valid = 0;
        cache_region.mem_side_cache.level = 0;

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);
        run_void_function(cap_print_mem_regions, &sys);

        assert_int_equal(output_has_text("HMAT declares 3 cache structures for "
                                         "this domain, none of which states "
                                         "its level"),
                         1);
        assert_int_equal(output_has_text("the lowest stated"), 0);
}

static void
test_cap_print_region_cache_one_of_several_levels(void **state)
{
        struct pqos_sysconfig sys = cache_report(1, 1);

        UNUSED_ARG(state);

        /* one structure, and a platform that says it has two levels: the counts
         * come from different places and this is the shape where they disagree,
         * so the report says that the rest are not described rather than
         * leaving "1 of 2" to be worked out
         */
        cache_one_level(&cache_region.mem_side_cache);
        cache_region.mem_side_cache.total_levels = 2;

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);
        run_void_function(cap_print_mem_regions, &sys);

        assert_int_equal(cache_block_has("Level             : 1 of 2"), 1);
        assert_int_equal(
            cache_block_has("the platform declares 2 cache levels "
                            "and HMAT carries a structure for one "
                            "of them, so the rest are not described "
                            "above"),
            1);
}

static void
test_cap_print_region_cache_none_declared(void **state)
{
        struct pqos_sysconfig sys = cache_report(1, 1);

        UNUSED_ARG(state);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);
        run_void_function(cap_print_mem_regions, &sys);

        /* HMAT read, a target domain to look up, and nothing in the table for
         * it: one line, and none of the figures
         */
        assert_int_equal(cache_block_has("None declared for target domain 3"),
                         1);
        assert_int_equal(cache_block_has("Size              :"), 0);
        assert_int_equal(cache_block_has("Level             :"), 0);
}

static void
test_cap_print_region_cache_declared_absent(void **state)
{
        struct pqos_sysconfig sys = cache_report(1, 1);

        UNUSED_ARG(state);

        /* the platform having said this domain has no memory side cache - a
         * cache structure whose Total Cache Levels nibble is zero - which the
         * library reports as domain_declared without valid
         */
        cache_region.mem_side_cache.domain_declared = 1;

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);
        run_void_function(cap_print_mem_regions, &sys);

        /* which is a different sentence from the one above: that one is the
         * table carrying nothing for the domain, this one is the table saying
         * there is no cache. Both are "none", and a report that said the same
         * thing for each would lose the platform's statement
         */
        assert_int_equal(cache_block_has("None - HMAT declares no memory side "
                                         "cache for target domain 3"),
                         1);
        assert_int_equal(cache_block_has("None declared for target domain 3"),
                         0);
        assert_int_equal(cache_block_has("Size              :"), 0);
}

static void
test_cap_print_region_cache_hmat_not_read(void **state)
{
        struct pqos_sysconfig sys = cache_report(0, 1);

        UNUSED_ARG(state);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);
        run_void_function(cap_print_mem_regions, &sys);

        /* unknown, and which kind of unknown: the table was not read at all */
        assert_int_equal(cache_block_has("Unknown - HMAT was not read"), 1);
        assert_int_equal(cache_block_has("None declared"), 0);
}

static void
test_cap_print_region_cache_without_a_target_domain(void **state)
{
        struct pqos_sysconfig sys = cache_report(1, 0);

        UNUSED_ARG(state);

        will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_inter_get, PQOS_INTER_MMIO);
        run_void_function(cap_print_mem_regions, &sys);

        /* the other unknown: HMAT was read, but this region has no one domain
         * to ask it about
         */
        assert_int_equal(cache_block_has("Unknown - this region has no target "
                                         "domain to look up"),
                         1);
        assert_int_equal(cache_block_has("None declared"), 0);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_print_io_dev[] = {
            cmocka_unit_test(test_cap_print_io_dev_without_a_device),
            cmocka_unit_test(test_cap_print_io_dev_without_dev_info),
            cmocka_unit_test(test_cap_print_io_dev_without_an_interface),
            cmocka_unit_test(test_cap_print_io_dev_on_another_interface),
            cmocka_unit_test(test_cap_print_io_dev_without_erdt_info),
            cmocka_unit_test(test_cap_print_io_dev_absent_device),
            cmocka_unit_test(test_cap_print_io_dev_without_channels),
            cmocka_unit_test(test_cap_print_io_dev_without_iordt),
            cmocka_unit_test(test_cap_print_io_dev_with_channels)};

        /* a group of its own: these are about the memory region report rather
         * than the I/O device list, and a group name that covers both tells a
         * reader looking at a failure the wrong thing
         */
        const struct CMUnitTest tests_print_region_cache[] = {
            cmocka_unit_test(test_cap_print_region_cache_all_figures),
            cmocka_unit_test(test_cap_print_region_cache_without_a_size),
            cmocka_unit_test(test_cap_print_region_cache_without_a_level),
            cmocka_unit_test(
                test_cap_print_region_cache_names_the_level_it_describes),
            cmocka_unit_test(
                test_cap_print_region_cache_note_without_any_level),
            cmocka_unit_test(test_cap_print_region_cache_one_of_several_levels),
            cmocka_unit_test(test_cap_print_region_cache_none_declared),
            cmocka_unit_test(test_cap_print_region_cache_declared_absent),
            cmocka_unit_test(test_cap_print_region_cache_hmat_not_read),
            cmocka_unit_test(
                test_cap_print_region_cache_without_a_target_domain)};

        result += cmocka_run_group_tests(tests_print_io_dev,
                                         test_cap_group_setup, NULL);
        result += cmocka_run_group_tests(tests_print_region_cache,
                                         test_cap_group_setup, NULL);

        return result;
}
