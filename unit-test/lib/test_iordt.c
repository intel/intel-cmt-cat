/*
 * BSD LICENSE
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
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
 */

#include "test_iordt.h"

#include "acpi.h"
#include "acpi_table.h"
#include "iordt.h"
#include "pci.h"
#include "test.h"

/* clang-format off */
static uint8_t acpi_irdt[] = {
    /* acpi_table_irdt instance 1 */
    0x49, 0x52, 0x44, 0x54, 0xcd, 0x00, 0x00, 0x00, 0x01, 0x5b, 0x4f, 0x45,
    0x4d, 0x20, 0x49, 0x44, 0x4f, 0x45, 0x4d, 0x20, 0x54, 0x20, 0x49, 0x44,
    0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* RMUD instance 1 (157 bytes length) */
    0x00, 0x00, 0x00, 0x00, 0x9d, 0x00, 0x00, 0x00, 0x34, 0x12, 0x00, 0x00,
    0x00,
    /* DSS instance 1 (40 bytes length) */
    0x00, 0x00, 0x28, 0x00, 0x01, 0x20, 0x10, 0x00, 0x01, 0x80, 0xc1, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    /* DSS instance 2 (24 bytes length) */
    0x00, 0x00, 0x18, 0x00, 0x01, 0x30, 0x20, 0x00, 0x01, 0xc1, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* RCS instance 1 (40 bytes length) */
    0x01, 0x00, 0x28, 0x00, 0x00, 0x00, 0x01, 0x02, 0x07, 0x00, 0x00, 0x10,
    0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x30, 0x30, 0x05, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x34, 0x12,
    0x00, 0x00, 0x00, 0x00,
    /* RCS instance 2 (40 bytes length) */
    0x01, 0x00, 0x28, 0x00, 0x00, 0x00, 0x02, 0x01, 0x0e, 0x00, 0x00, 0x10,
    0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x34, 0x12,
    0x21, 0x43, 0x65, 0x87};
/* clang-format on */

/**
 * DEBUG: IRDT Dev Info:
 * DEBUG: Num DEVs: 2
 * DEBUG:   DEV 0000:0010:04.0, type: 0x1
 * DEBUG:     CHAN ID: 0x100
 * DEBUG:     CHAN ID: 0x101
 * DEBUG:     CHAN ID: 0x200
 * DEBUG:   DEV 0000:0020:06.0, type: 0x1
 * DEBUG:     CHAN ID: 0x101
 * DEBUG: Num CHANNELs: 3
 * DEBUG:   CHAN ID: 0x100
 * DEBUG:     RMID 1-10
 * DEBUG:     CLOS 1-10
 * DEBUG:   CHAN ID: 0x101
 * DEBUG:     RMID 1-10
 * DEBUG:     CLOS 1-10
 * DEBUG:   CHAN ID: 0x200
 * DEBUG:     RMID 1-10
 * DEBUG:     CLOS 1-10
 */

int
__wrap_acpi_init(void)
{
        function_called();
        return mock_type(int);
}

int
__wrap_acpi_fini(void)
{
        function_called();
        return mock_type(int);
}

int
__wrap_pci_init(void)
{
        function_called();
        return mock_type(int);
}

int
__wrap_pci_fini(void)
{
        function_called();
        return mock_type(int);
}

struct acpi_table *
__wrap_acpi_get_sig(const char *sig)
{
        function_called();
        check_expected(sig);
        return mock_ptr_type(struct acpi_table *);
}

void
__wrap_acpi_print(struct acpi_table *table)
{
        function_called();
        check_expected_ptr(table);
}

void
__wrap_acpi_free(struct acpi_table *table)
{
        function_called();
        check_expected_ptr(table);
}

struct pci_dev *
__wrap_pci_dev_get(uint16_t domain, uint16_t bdf)
{
        struct pci_dev *dev;
        uint64_t i;

        dev = (struct pci_dev *)calloc(1, sizeof(*dev));
        dev->domain = domain;
        dev->bdf = bdf;
        dev->bus = bdf >> 8;
        dev->dev = (bdf >> 3) & 0x1F;
        dev->func = bdf & 0x7;
        dev->numa = 0;

        dev->bar_num = 6;
        for (i = 0; i < dev->bar_num; ++i)
                dev->bar[i] = (i | bdf << 16) << 32;

        return dev;
}

void
__wrap_pci_dev_release(struct pci_dev *dev)
{
        free(dev);
}

/* ======== iotdt_init ======== */

static void
test_iordt_init(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct acpi_table table = {.generic = acpi_irdt};

        struct pqos_devinfo *devinfo = NULL;

        /* Invalid params */
        ret = iordt_init(data->cap, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = iordt_init(NULL, &devinfo);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        expect_function_call(__wrap_acpi_init);
        will_return(__wrap_acpi_init, PQOS_RETVAL_OK);

        expect_function_call(__wrap_pci_init);
        will_return(__wrap_pci_init, PQOS_RETVAL_OK);

        expect_function_call(__wrap_acpi_get_sig);
        expect_string(__wrap_acpi_get_sig, sig, ACPI_TABLE_SIG_IRDT);
        will_return(__wrap_acpi_get_sig, &table);

        expect_function_call(__wrap_acpi_print);
        expect_value(__wrap_acpi_print, table, &table);

        expect_function_call(__wrap_acpi_free);
        expect_value(__wrap_acpi_free, table, &table);

        ret = iordt_init(data->cap, &devinfo);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_ptr_not_equal(devinfo, NULL);

        assert_int_equal(devinfo->num_devs, 2);
        assert_ptr_not_equal(devinfo->devs, NULL);

        assert_int_equal(devinfo->num_channels, 3);
        assert_ptr_not_equal(devinfo->channels, NULL);

        size_t i;

        /* All devices are PCI devices */
        for (i = 0; i < devinfo->num_devs; i++)
                assert_int_equal(devinfo->devs[i].type, PQOS_DEVICE_TYPE_PCI);

        /* DEV#0, 3x CHANs */
        assert_int_equal(devinfo->devs[0].channel[0], 0x10100);
        assert_int_equal(devinfo->devs[0].channel[1], 0x10101);
        assert_int_equal(devinfo->devs[0].channel[2], 0x10200);

        /* DEV#1, 1x CHAN */
        assert_int_equal(devinfo->devs[1].channel[0], 0x10101);

        assert_int_equal(devinfo->channels[0].channel_id, 0x10100);
        assert_int_equal(devinfo->channels[1].channel_id, 0x10101);
        assert_int_equal(devinfo->channels[2].channel_id, 0x10200);

        expect_function_call(__wrap_pci_fini);
        will_return(__wrap_pci_fini, PQOS_RETVAL_OK);

        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);

        ret = iordt_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_iordt_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_devinfo *devinfo = NULL;
        int ret;

        if (data->cap_mon != NULL) {
                data->cap_mon->iordt = 0;
                data->cap_mon->iordt_on = 0;
        }
        data->cap_l3ca.iordt = 0;
        data->cap_l3ca.iordt_on = 0;

        ret = iordt_init(data->cap, &devinfo);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_null(devinfo);
}

static void
test_iordt_fini(void **state __attribute__((unused)))
{
        int ret;

        expect_function_call(__wrap_pci_fini);
        will_return(__wrap_pci_fini, PQOS_RETVAL_OK);

        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_ERROR);

        ret = iordt_fini();
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
}

/* ========= iordt_check_support ======== */

static void
test_iordt_check_support_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        if (data->cap_mon != NULL) {
                data->cap_mon->iordt = 0;
                data->cap_mon->iordt_on = 0;
        }
        data->cap_l3ca.iordt = 0;
        data->cap_l3ca.iordt_on = 0;

        ret = iordt_check_support(data->cap);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_iordt_check_support_l3(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        if (data->cap_mon != NULL) {
                data->cap_mon->iordt = 0;
                data->cap_mon->iordt_on = 0;
        }

        data->cap_l3ca.iordt = 1;
        data->cap_l3ca.iordt_on = 0;

        ret = iordt_check_support(data->cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_iordt_check_support_mon(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        if (data->cap_mon != NULL) {
                data->cap_mon->iordt = 1;
                data->cap_mon->iordt_on = 0;
        }

        data->cap_l3ca.iordt = 0;
        data->cap_l3ca.iordt_on = 0;

        ret = iordt_check_support(data->cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_iordt_init),
            cmocka_unit_test(test_iordt_fini),
            cmocka_unit_test(test_iordt_check_support_unsupported),
            cmocka_unit_test(test_iordt_check_support_l3),
            cmocka_unit_test(test_iordt_unsupported)};

        result += cmocka_run_group_tests(tests, test_init_all, test_fini);

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_iordt_check_support_unsupported),
            cmocka_unit_test(test_iordt_check_support_l3),
            cmocka_unit_test(test_iordt_unsupported)};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);

        const struct CMUnitTest tests_mon[] = {
            cmocka_unit_test(test_iordt_check_support_unsupported),
            cmocka_unit_test(test_iordt_check_support_mon),
            cmocka_unit_test(test_iordt_unsupported)};

        result += cmocka_run_group_tests(tests_mon, test_init_mon, test_fini);

        return result;
}
