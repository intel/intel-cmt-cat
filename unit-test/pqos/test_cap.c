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

int
main(void)
{
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

        return cmocka_run_group_tests(tests_print_io_dev, test_cap_group_setup,
                                      NULL);
}
