/*
 * BSD LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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

#include "test_acpi.h"

#include "acpi.h"
#include "acpi_table.h"
#include "test.h"

/* clang-format off */
uint8_t __irdt_hal[] = {
    /* acpi_table_irdt instance 1 */
    0x49, 0x52, 0x44, 0x54, 0xda, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x4f, 0x45,
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
    /* DSS Instance 2 (24 bytes length) */
    0x00, 0x00, 0x18, 0x00, 0x01, 0x30, 0x20, 0x00, 0x01, 0xc1, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* RCS instance 1 (40 bytes length) */
    0x01, 0x00, 0x28, 0x00, 0x00, 0x00, 0x01, 0x02, 0x0e, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12,
    0x00, 0x00, 0x00, 0x00,
    /* RCS instance 2 (40 bytes length) */
    0x01, 0x00, 0x28, 0x00, 0x00, 0x00, 0x01, 0x02, 0x0e, 0x00, 0x00, 0x10,
    0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12,
    0x21, 0x43, 0x65, 0x87,
    /* RMUD instance 2 (13 bytes length)*/
    0x00, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x78, 0x56, 0x00, 0x00,
    0x00};
/* clang-format on */

unsigned int __irdt_hal_len = 218;

size_t __irdt_tab_rmud_0_offset = 48;
size_t __irdt_tab_rmud_0_dss_0_offset = 61;
size_t __irdt_tab_rmud_0_rcs_0_offset = 125;
size_t __irdt_tab_rmud_1_offset = 205;

/**
 * DEBUG: Signature:         IRDT
 * DEBUG: Length:            218
 * DEBUG: Revision:          1
 * DEBUG: Checksum:          14
 * DEBUG: OEM ID:            OEM ID
 * DEBUG: OEM Table ID:      OEM T ID
 * DEBUG: OEM Revision:      1
 * DEBUG: Creator ID:        2
 * DEBUG: Creator Revision:  4
 * DEBUG: IO Proto Flags:    0X3: MON CTL
 * DEBUG: Cache Proto Flags: 0X3: MON CTL
 * DEBUG: 2 RMUD(s):
 * DEBUG:  Type:              0/RMUD
 * DEBUG:  Length:            157
 * DEBUG:  PCI Segment:       0X1234
 * DEBUG:  4 DEV(s):
 * DEBUG:   Type:              0/DSS
 * DEBUG:   Length:            40
 * DEBUG:   Device Type:       0X1
 * DEBUG:   Enumeration ID:    4128
 * DEBUG:   2 CHMS(s):
 * DEBUG:    RCS Enum ID:       1
 * DEBUG:     VC0 - Channel:     0
 * DEBUG:     VC1 - Channel:     1 SHARED
 * DEBUG:    RCS Enum ID:       2
 * DEBUG:     VC0 - Channel:     0
 * DEBUG:   Type:              0/DSS
 * DEBUG:   Length:            24
 * DEBUG:   Device Type:       0X1
 * DEBUG:   Enumeration ID:    8240
 * DEBUG:   1 CHMS(s):
 * DEBUG:    RCS Enum ID:       1
 * DEBUG:     VC0 - Channel:     1 SHARED
 * DEBUG:   Type:              0X1/RCS
 * DEBUG:   Length:            0X28
 * DEBUG:   Link Type:         0
 * DEBUG:   Enumeration ID:    1
 * DEBUG:   Flags:             0XE
 * DEBUG:   Channel Count:     2
 * DEBUG:   RMID Block Offset: 0X1000
 * DEBUG:   CLOS Block Offset: 0X2000
 * DEBUG:   Block MIMO:        0x00000012345678
 * DEBUG:   Type:              0X1/RCS
 * DEBUG:   Length:            0X28
 * DEBUG:   Link Type:         0
 * DEBUG:   Enumeration ID:    2
 * DEBUG:   Flags:             0XE
 * DEBUG:   Channel Count:     1
 * DEBUG:   RMID Block Offset: 0X1000
 * DEBUG:   CLOS Block Offset: 0X2000
 * DEBUG:   Block MIMO:        0x8765432112345678
 * DEBUG:  Type:              0/RMUD
 * DEBUG:  Length:            29
 * DEBUG:  PCI Segment:       0X5678
 * DEBUG:  0 DEV(s):
 */

struct acpi_table_internal {
        struct acpi_table table;
        acpi_address address;
        acpi_size size;
};

void
__wrap_free(void *ptr)
{
        __real_free(ptr);

        function_called();
        check_expected_ptr(ptr);
}

int
__wrap_munmap(void *addr, size_t length)
{
        function_called();
        check_expected_ptr(addr);
        check_expected(length);
        return mock_type(int);
}

static void
test_acpi_init(void **state __attribute__((unused)))
{
        assert_int_equal(acpi_init(), PQOS_RETVAL_OK);
}

static void
test_acpi_fini(void **state __attribute__((unused)))
{
        assert_int_equal(acpi_fini(), PQOS_RETVAL_OK);
}

static void
test_acpi_free(void **state __attribute__((unused)))
{
        struct acpi_table_internal *data =
            (struct acpi_table_internal *)malloc(sizeof(*data));

        data->table.generic = (uint8_t *)0xDEAD0000;
        data->size = 2021;

        expect_function_call(__wrap_munmap);
        expect_value(__wrap_munmap, addr, 0xDEAD0000);
        expect_value(__wrap_munmap, length, 2021);
        will_return(__wrap_munmap, 0);

        expect_function_call(__wrap_free);
        expect_value(__wrap_free, ptr, data);

        acpi_free((struct acpi_table *)data);
}

static void
test_acpi_get_irdt_rmud(void **state __attribute__((unused)))
{
        size_t num = 0;
        struct acpi_table_irdt_rmud **rmuds = NULL;
        struct acpi_table_irdt *irdt = (struct acpi_table_irdt *)__irdt_hal;
        size_t i;

        /* Invalid params */
        rmuds = acpi_get_irdt_rmud(NULL, &num);
        assert_null(rmuds);

        rmuds = acpi_get_irdt_rmud(irdt, NULL);
        assert_null(rmuds);

        /* All OK! */
        rmuds = acpi_get_irdt_rmud(irdt, &num);
        assert_int_equal(num, 2);
        assert_non_null(rmuds);

        for (i = 0; i < num; i++)
                assert_non_null(rmuds[i]);

        /* RMUD#0 */
        assert_int_equal(rmuds[0]->type, ACPI_TABLE_IRDT_TYPE_RMUD);
        assert_int_equal(rmuds[0]->segment, 0x1234);
        /* expect some devices so total length is higher than just struct */
        assert_int_not_equal(rmuds[0]->length, sizeof(*rmuds[0]));

        /* RMUD#1 */
        assert_int_equal(rmuds[1]->type, ACPI_TABLE_IRDT_TYPE_RMUD);
        assert_int_equal(rmuds[1]->segment, 0x5678);
        assert_int_equal(rmuds[1]->length, sizeof(*rmuds[1]));
}

static void
test_acpi_get_irdt_dev(void **state __attribute__((unused)))
{
        /* RMUD#1 with no DEVs */
        struct acpi_table_irdt_rmud *rmud =
            (struct acpi_table_irdt_rmud *)(__irdt_hal +
                                            __irdt_tab_rmud_1_offset);
        struct acpi_table_irdt_device **devs = NULL;
        size_t num = 0;

        /* Invalid params */
        devs = acpi_get_irdt_dev(NULL, &num);
        assert_null(devs);

        devs = acpi_get_irdt_dev(rmud, NULL);
        assert_null(devs);

        /* All OK! */
        devs = acpi_get_irdt_dev(rmud, &num);

        assert_int_equal(num, 0);
        assert_null(devs);
        /* no devices, RMUD table len is same as struct len*/
        assert_int_equal(rmud->length, sizeof(struct acpi_table_irdt_rmud));

        /* RMUD#0 with 4 devs, 2x DSS + 2xRCS */
        rmud = (struct acpi_table_irdt_rmud *)(__irdt_hal +
                                               __irdt_tab_rmud_0_offset);

        devs = acpi_get_irdt_dev(rmud, &num);
        assert_int_equal(num, 4);
        assert_non_null(devs);
        assert_int_not_equal(rmud->length, sizeof(struct acpi_table_irdt_rmud));

        size_t i;
        size_t num_dss = 0;
        size_t num_rcs = 0;

        for (i = 0; i < num; i++) {
                assert_non_null(devs[i]);

                if (devs[i]->type == ACPI_TABLE_IRDT_TYPE_DSS)
                        num_dss++;

                if (devs[i]->type == ACPI_TABLE_IRDT_TYPE_RCS)
                        num_rcs++;
        }

        assert_int_equal(num_dss, 2);
        assert_int_equal(num_rcs, 2);
}

static void
test_acpi_get_irdt_chms(void **state __attribute__((unused)))
{
        int ret;
        struct acpi_table_irdt_device *rcs =
            (struct acpi_table_irdt_device *)(__irdt_hal +
                                              __irdt_tab_rmud_0_rcs_0_offset);
        struct acpi_table_irdt_device *dss =
            (struct acpi_table_irdt_device *)(__irdt_hal +
                                              __irdt_tab_rmud_0_dss_0_offset);
        struct acpi_table_irdt_chms **chms = NULL;
        size_t num = 0;
        size_t i;

        /* Invalid params */
        ret = acpi_get_irdt_chms(NULL, &chms, &num);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = acpi_get_irdt_chms(rcs, NULL, &num);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = acpi_get_irdt_chms(rcs, &chms, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* RCS#0 table, fails as requires DSS table as an input */
        ret = acpi_get_irdt_chms(rcs, &chms, &num);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_int_equal(num, 0);
        assert_ptr_equal(chms, NULL);

        /* All OK! */
        /* DSS#0 table, 2xCHMS */
        ret = acpi_get_irdt_chms(dss, &chms, &num);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(num, 2);
        assert_ptr_not_equal(chms, NULL);

        for (i = 0; i < num; i++)
                assert_ptr_not_equal(chms[i], NULL);

        assert_int_equal(chms[0]->rcs_enum_id, 1);

        /* CHMS#0 CHAN#0, valid, not shared */
        uint8_t vc = chms[0]->vc_map[0];

        assert_int_equal(vc & ~ACPI_TABLE_IRDT_CHMS_CHAN_MASK, 0);
        assert_int_not_equal(vc & ACPI_TABLE_IRDT_CHMS_CHAN_VALID, 0);
        assert_int_equal(vc & ACPI_TABLE_IRDT_CHMS_CHAN_SHARED, 0);

        /* CHMS#0 CHAN#1, valid, shared */
        vc = chms[0]->vc_map[1];
        assert_int_equal(vc & ~ACPI_TABLE_IRDT_CHMS_CHAN_MASK, 1);
        assert_int_not_equal(vc & ACPI_TABLE_IRDT_CHMS_CHAN_VALID, 0);
        assert_int_not_equal(vc & ACPI_TABLE_IRDT_CHMS_CHAN_SHARED, 0);

        /* CHMS#0 CHAN#2, invalid */
        vc = chms[0]->vc_map[2];
        assert_int_equal(vc & ACPI_TABLE_IRDT_CHMS_CHAN_VALID, 0);

        /* CHMS#1 */
        assert_int_equal(chms[1]->rcs_enum_id, 2);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_acpi_init),
            cmocka_unit_test(test_acpi_fini),
            cmocka_unit_test(test_acpi_free),
            cmocka_unit_test(test_acpi_get_irdt_rmud),
            cmocka_unit_test(test_acpi_get_irdt_dev),
            cmocka_unit_test(test_acpi_get_irdt_chms)};

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
