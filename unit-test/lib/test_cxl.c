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
 *
 */

/**
 * The CXL devices the operating system has mapped, read from a tree built here
 * rather than from a platform's own.
 *
 * Every answer this reader has to give is a state of the bus, and a board shows
 * one state at a time: the platform with a device cannot be made to lose it,
 * the platform without one cannot be made to grow one, and neither can be made
 * to refuse a directory or to gain a target between two walks. The cases below
 * are those states, written as directories and files - the same shapes the
 * kernel publishes, in a tree this file owns.
 *
 * The reader's bus path is a compile time constant, so this binary is linked
 * against a copy of the module compiled with a *relative* one (see the recipe
 * in unit-test/lib/Makefile). That is what lets the tree live in a directory of
 * this process's own: the fixture makes one with mkdtemp() and changes into it,
 * so the reader resolves "bus/cxl/devices" inside it and a concurrent run of
 * this binary cannot unlink or replace what this one is asserting about. Same
 * reason, and the same shape, as unit-test/lib/test_common.c.
 */

#include "cxl.h"
#include "test.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/** The tree this file owns, relative to the directory the fixture makes and
 *  changes into - which is what the module under test was compiled to read.
 *  Kept in step with the recipe by hand: a mismatch would have the reader
 *  looking at the machine's own bus instead of this fixture
 */
#define FIXTURE_BUS  "bus/cxl"
#define FIXTURE_DEVS FIXTURE_BUS "/devices"
/** where the fake PCI functions live, which the endpoints point into. Absolute,
 *  because an endpoint's uport is a symbolic link and a link relative to the
 *  directory it sits in would not resolve from the working directory
 */
#define FIXTURE_PCI "pci"

#ifdef __linux__

/* ======== the fixture ======== */

/**
 * @brief Runs a shell command, and says nothing if it worked
 *
 * The fixture builds directories, files and symbolic links, which is what a
 * shell is for; a failure here is the case unable to start rather than a
 * finding about the code, so it fails the case loudly.
 *
 * @param [in] format the command, printf style
 */
static void
fixture_run(const char *format, ...)
{
        char command[512];
        va_list args;
        int ret;

        va_start(args, format);
        ret = vsnprintf(command, sizeof(command), format, args);
        va_end(args);
        assert_true(ret > 0 && (size_t)ret < sizeof(command));

        ret = system(command);
        assert_int_equal(ret, 0);
}

/** the directory this process owns, and where it was entered from */
static char fixture_root[PATH_MAX];
static char fixture_home[PATH_MAX];

/**
 * @brief An empty tree of this process's own, with no bus in it at all
 */
static int
fixture_setup(void **state)
{
        (void)state;

        if (getcwd(fixture_home, sizeof(fixture_home)) == NULL)
                return -1;

        snprintf(fixture_root, sizeof(fixture_root),
                 "/tmp/pqos_ut_lib_cxl_XXXXXX");
        if (mkdtemp(fixture_root) == NULL)
                return -1;
        if (chdir(fixture_root) != 0)
                return -1;

        return 0;
}

static int
fixture_teardown(void **state)
{
        (void)state;

        if (chdir(fixture_home) != 0)
                return -1;

        fixture_run("rm -rf %s", fixture_root);
        fixture_root[0] = '\0';

        return 0;
}

/**
 * @brief Creates the bus, with no devices on it
 */
static void
fixture_bus(void)
{
        fixture_run("mkdir -p %s", FIXTURE_DEVS);
}

/**
 * @brief Creates one region, committed or not, with an address range
 *
 * @param [in] region the region's name, e.g. "region0"
 * @param [in] commit what its commit attribute says, or NULL for no attribute
 * @param [in] resource the address it maps, or NULL for no resource attribute
 * @param [in] size how much of it, or NULL for no size attribute
 */
static void
fixture_region(const char *region,
               const char *commit,
               const char *resource,
               const char *size)
{
        fixture_bus();
        fixture_run("mkdir -p %s/%s", FIXTURE_DEVS, region);
        if (commit != NULL)
                fixture_run("echo %s > %s/%s/commit", commit, FIXTURE_DEVS,
                            region);
        if (resource != NULL)
                fixture_run("echo %s > %s/%s/resource", resource, FIXTURE_DEVS,
                            region);
        if (size != NULL)
                fixture_run("echo %s > %s/%s/size", size, FIXTURE_DEVS, region);
}

/**
 * @brief Gives a region a target, and the chain from it to a memory device
 *
 * The chain is the kernel's: the region names an endpoint decoder, the decoder
 * lives under its endpoint, and the endpoint's uport points at the memory
 * device inside its PCI function.
 *
 * @param [in] region which region
 * @param [in] target which target of it
 * @param [in] endpoint the endpoint to put the decoder under, e.g. "endpoint4"
 * @param [in] decoder the decoder's name, e.g. "decoder4.0"
 * @param [in] pci the PCI function, e.g. "0000:11:00.0"
 * @param [in] mem the memory device, e.g. "mem0"
 */
static void
fixture_target(const char *region,
               unsigned target,
               const char *endpoint,
               const char *decoder,
               const char *pci,
               const char *mem)
{
        fixture_run("echo %s > %s/%s/target%u", decoder, FIXTURE_DEVS, region,
                    target);
        fixture_run("mkdir -p %s/%s/%s", FIXTURE_DEVS, endpoint, decoder);
        fixture_run("ln -sfn %s/%s/%s/%s %s/%s", fixture_root, FIXTURE_DEVS,
                    endpoint, decoder, FIXTURE_DEVS, decoder);
        fixture_run("mkdir -p %s/%s/%s", FIXTURE_PCI, pci, mem);
        /* absolute, because this link is resolved from the working directory
         * rather than from the directory it sits in
         */
        fixture_run("ln -sfn %s/%s/%s/%s %s/%s/uport", fixture_root,
                    FIXTURE_PCI, pci, mem, FIXTURE_DEVS, endpoint);
}

/** One region, one device, everything readable: the platform the report was
 *  written against
 */
static void
fixture_one_device(void)
{
        fixture_region("region0", "1", "0x6200000000", "0x4000000000");
        fixture_target("region0", 0, "endpoint4", "decoder4.0", "0000:11:00.0",
                       "mem0");
}

/* ======== the cases ======== */

#endif /* __linux__ */

static void
test_cxl_checks_its_parameters(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 0;
        int available = 0;
        int complete = 0;

        assert_int_equal(cxl_devices_read(NULL, &complete, &num, &devices),
                         PQOS_RETVAL_PARAM);
        assert_int_equal(cxl_devices_read(&available, NULL, &num, &devices),
                         PQOS_RETVAL_PARAM);
        assert_int_equal(
            cxl_devices_read(&available, &complete, NULL, &devices),
            PQOS_RETVAL_PARAM);
        assert_int_equal(cxl_devices_read(&available, &complete, &num, NULL),
                         PQOS_RETVAL_PARAM);
        (void)state;
}

#ifdef __linux__

static void
test_cxl_reports_no_bus_as_unavailable(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 99;
        int available = -1;
        int complete = -1;

        /* an operating system that publishes no CXL bus: nothing is known about
         * the devices behind a memory range, and nothing went unread either -
         * there was nothing to read
         */
        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(available, 0);
        assert_int_equal(complete, 1);
        assert_int_equal(num, 0);
        assert_null(devices);
        (void)state;
}

static void
test_cxl_reports_an_empty_bus_as_available(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 99;
        int available = -1;
        int complete = -1;

        fixture_bus();

        /* the bus is there and carries no region: a checked absence, which is a
         * different answer from having no bus to check
         */
        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(available, 1);
        assert_int_equal(complete, 1);
        assert_int_equal(num, 0);
        assert_null(devices);
        (void)state;
}

static void
test_cxl_names_the_device_behind_a_region(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 0;
        int available = -1;
        int complete = -1;

        fixture_one_device();

        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(available, 1);
        assert_int_equal(complete, 1);
        assert_int_equal(num, 1);
        assert_non_null(devices);

        /* the three names and the range, which is everything the report prints
         * and everything a memory region is matched against
         */
        assert_string_equal(devices[0].region_name, "region0");
        assert_string_equal(devices[0].mem_name, "mem0");
        assert_string_equal(devices[0].pci_address, "0000:11:00.0");
        assert_int_equal(devices[0].address_valid, 1);
        assert_true(devices[0].base_address == 0x6200000000ULL);
        assert_true(devices[0].size == 0x4000000000ULL);

        cxl_devices_free(devices);
        (void)state;
}

static void
test_cxl_reports_a_device_it_could_not_name(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 0;
        int available = -1;
        int complete = -1;

        fixture_one_device();
        /* the link from the region's target to its endpoint is gone, which is
         * what a kernel laying the tree out differently looks like from here
         */
        fixture_run("rm -f %s/decoder4.0", FIXTURE_DEVS);

        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(num, 1);
        assert_non_null(devices);

        /* the names are unknown and the address is not: what places a device in
         * a memory region was read, so the device is reported rather than
         * dropped, and the report prints the names it does not have as unknown
         */
        assert_string_equal(devices[0].region_name, "region0");
        assert_int_equal(devices[0].mem_name[0], '\0');
        assert_int_equal(devices[0].pci_address[0], '\0');
        assert_int_equal(devices[0].address_valid, 1);
        assert_true(devices[0].base_address == 0x6200000000ULL);

        cxl_devices_free(devices);
        (void)state;
}

static void
test_cxl_reports_one_device_per_interleave_way(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 0;
        int available = -1;
        int complete = -1;

        fixture_one_device();
        fixture_target("region0", 1, "endpoint5", "decoder5.0", "0000:22:00.0",
                       "mem1");

        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(complete, 1);
        assert_int_equal(num, 2);
        assert_non_null(devices);

        /* both ways carry the region's whole range, because which part of it a
         * way holds is not something the kernel publishes per device
         */
        assert_string_equal(devices[0].mem_name, "mem0");
        assert_string_equal(devices[1].mem_name, "mem1");
        assert_true(devices[0].base_address == devices[1].base_address);
        assert_true(devices[0].size == devices[1].size);

        cxl_devices_free(devices);
        (void)state;
}

static void
test_cxl_skips_a_region_that_is_not_committed(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 99;
        int available = -1;
        int complete = -1;

        fixture_region("region0", "0", "0x6200000000", "0x4000000000");
        fixture_target("region0", 0, "endpoint4", "decoder4.0", "0000:11:00.0",
                       "mem0");

        /* a configured region that has not been committed decodes no address,
         * so the device behind it is behind no memory range. That is an answer,
         * not a gap: the read is complete
         */
        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(available, 1);
        assert_int_equal(complete, 1);
        assert_int_equal(num, 0);
        assert_null(devices);
        (void)state;
}

static void
test_cxl_reports_a_region_that_does_not_say_as_unread(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 99;
        int available = -1;
        int complete = -1;

        fixture_region("region0", NULL, "0x6200000000", "0x4000000000");
        fixture_target("region0", 0, "endpoint4", "decoder4.0", "0000:11:00.0",
                       "mem0");

        /* no commit attribute at all: whether this region decodes anything was
         * not established, and a region reported as decoding nothing would be
         * an answer nobody got
         */
        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(available, 1);
        assert_int_equal(complete, 0);
        assert_int_equal(num, 0);
        assert_null(devices);
        (void)state;
}

static void
test_cxl_reports_a_committed_region_with_no_target_as_unread(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 99;
        int available = -1;
        int complete = -1;

        /* a committed region decodes an address range, so something is behind
         * it: no target at all means the region went away between the read that
         * said it was committed and the walk over its targets, or that it lays
         * them out in a way this cannot follow. Either way the read did not
         * finish, and a region nobody could read is not a region with nothing
         * in it
         */
        fixture_region("region0", "1", "0x6200000000", "0x4000000000");

        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(available, 1);
        assert_int_equal(complete, 0);
        assert_int_equal(num, 0);
        assert_null(devices);
        (void)state;
}

static void
test_cxl_drops_half_an_address_range(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 0;
        int available = -1;
        int complete = -1;

        /* a resource and no size is not an address range. The device is there
         * and is reported; what cannot be done is placing it, so the read is
         * incomplete and both numbers are zero rather than one of them being a
         * plausible base with nothing to bound it
         */
        fixture_region("region0", "1", "0x6200000000", NULL);
        fixture_target("region0", 0, "endpoint4", "decoder4.0", "0000:11:00.0",
                       "mem0");

        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(complete, 0);
        assert_int_equal(num, 1);
        assert_non_null(devices);
        assert_int_equal(devices[0].address_valid, 0);
        assert_true(devices[0].base_address == 0);
        assert_true(devices[0].size == 0);

        cxl_devices_free(devices);
        (void)state;
}

static void
test_cxl_reports_a_device_it_cannot_place_beside_one_it_can(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 0;
        int available = -1;
        int complete = -1;

        /* Two committed regions, adjacent, the second reporting an address and
         * no size - the shape a memory range covering both would be described
         * against. Both devices are read and only the first can be placed, so
         * the memory region the first belongs to lists a device and is short of
         * the second. What says so is the read being incomplete, which is why
         * this case cares that both devices come back rather than one
         */
        fixture_region("region0", "1", "0x10000000000", "0x1000000000");
        fixture_target("region0", 0, "endpoint4", "decoder4.0", "0000:11:00.0",
                       "mem0");
        fixture_region("region1", "1", "0x11000000000", NULL);
        fixture_target("region1", 0, "endpoint5", "decoder5.0", "0000:22:00.0",
                       "mem1");

        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(available, 1);
        assert_int_equal(complete, 0);
        assert_int_equal(num, 2);
        assert_non_null(devices);

        assert_string_equal(devices[0].mem_name, "mem0");
        assert_int_equal(devices[0].address_valid, 1);
        assert_true(devices[0].base_address == 0x10000000000ULL);

        assert_string_equal(devices[1].mem_name, "mem1");
        assert_int_equal(devices[1].address_valid, 0);
        assert_true(devices[1].base_address == 0);
        assert_true(devices[1].size == 0);

        cxl_devices_free(devices);
        (void)state;
}

static void
test_cxl_ignores_what_is_not_a_region(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 99;
        int available = -1;
        int complete = -1;

        fixture_bus();
        /* the bus carries ports, decoders, a nvdimm bridge and a root - all of
         * which a board really does publish - and none of them is a region
         */
        fixture_run("mkdir -p %s/port1 %s/decoder0.0 %s/nvdimm-bridge0 "
                    "%s/root0 %s/region %s/regionfoo",
                    FIXTURE_DEVS, FIXTURE_DEVS, FIXTURE_DEVS, FIXTURE_DEVS,
                    FIXTURE_DEVS, FIXTURE_DEVS);

        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(available, 1);
        assert_int_equal(complete, 1);
        assert_int_equal(num, 0);
        assert_null(devices);
        (void)state;
}

#else /* !__linux__ */

/**
 * @brief What the reader answers where the operating system publishes no bus
 *
 * The tree the cases above build is a Linux one, and so is the reader that
 * walks it: elsewhere the module answers from its own stub, and that answer has
 * to be the one a Linux machine without the bus gives - no bus, and nothing
 * left unread, because there was nothing to read.
 */
static void
test_cxl_answers_without_a_sysfs(void **state)
{
        struct pqos_cxl_device *devices = NULL;
        unsigned num = 99;
        int available = -1;
        int complete = -1;

        assert_int_equal(
            cxl_devices_read(&available, &complete, &num, &devices),
            PQOS_RETVAL_OK);
        assert_int_equal(available, 0);
        assert_int_equal(complete, 1);
        assert_int_equal(num, 0);
        assert_null(devices);
        (void)state;
}

#endif /* __linux__ */

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_cxl_checks_its_parameters),
#ifdef __linux__
            cmocka_unit_test_setup_teardown(
                test_cxl_reports_no_bus_as_unavailable, fixture_setup,
                fixture_teardown),
            cmocka_unit_test_setup_teardown(
                test_cxl_reports_an_empty_bus_as_available, fixture_setup,
                fixture_teardown),
            cmocka_unit_test_setup_teardown(
                test_cxl_names_the_device_behind_a_region, fixture_setup,
                fixture_teardown),
            cmocka_unit_test_setup_teardown(
                test_cxl_reports_a_device_it_could_not_name, fixture_setup,
                fixture_teardown),
            cmocka_unit_test_setup_teardown(
                test_cxl_reports_one_device_per_interleave_way, fixture_setup,
                fixture_teardown),
            cmocka_unit_test_setup_teardown(
                test_cxl_skips_a_region_that_is_not_committed, fixture_setup,
                fixture_teardown),
            cmocka_unit_test_setup_teardown(
                test_cxl_reports_a_region_that_does_not_say_as_unread,
                fixture_setup, fixture_teardown),
            cmocka_unit_test_setup_teardown(
                test_cxl_reports_a_committed_region_with_no_target_as_unread,
                fixture_setup, fixture_teardown),
            cmocka_unit_test_setup_teardown(
                test_cxl_drops_half_an_address_range, fixture_setup,
                fixture_teardown),
            cmocka_unit_test_setup_teardown(
                test_cxl_reports_a_device_it_cannot_place_beside_one_it_can,
                fixture_setup, fixture_teardown),
            cmocka_unit_test_setup_teardown(
                test_cxl_ignores_what_is_not_a_region, fixture_setup,
                fixture_teardown),
#else
            cmocka_unit_test(test_cxl_answers_without_a_sysfs),
#endif
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
