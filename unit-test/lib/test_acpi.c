/*
 * BSD LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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

#include <stdarg.h>
#include <sys/mman.h>
#include <unistd.h>

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
        data->mtype = ACPI_TBL_MMAP;

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
test_acpi_get_irdt_rmud_invalid_len(void **state __attribute__((unused)))
{
        uint8_t buf[128];
        struct acpi_table_irdt *irdt = (struct acpi_table_irdt *)buf;
        struct acpi_table_irdt_rmud **rmuds = NULL;
        struct acpi_table_irdt_rmud *rmud;
        size_t num = 0;

        /* Remaining RMUD bytes shorter than the RMUD header */
        memset(buf, 0, sizeof(buf));
        irdt->header.length = sizeof(*irdt) + 5;

        expect_function_call(__wrap_free);
        expect_value(__wrap_free, ptr, NULL);

        rmuds = acpi_get_irdt_rmud(irdt, &num);
        assert_null(rmuds);
        assert_int_equal(num, 0);

        /* RMUD's own declared length shorter than the RMUD header */
        memset(buf, 0, sizeof(buf));
        irdt->header.length =
            sizeof(*irdt) + sizeof(struct acpi_table_irdt_rmud);
        rmud = irdt->rmud;
        rmud->type = ACPI_TABLE_IRDT_TYPE_RMUD;
        rmud->length = 5;

        expect_function_call(__wrap_free);
        expect_value(__wrap_free, ptr, NULL);

        rmuds = acpi_get_irdt_rmud(irdt, &num);
        assert_null(rmuds);
        assert_int_equal(num, 0);
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
test_acpi_get_irdt_dev_invalid_len(void **state __attribute__((unused)))
{
        uint8_t buf[64];
        struct acpi_table_irdt_rmud *rmud = (struct acpi_table_irdt_rmud *)buf;
        struct acpi_table_irdt_device **devs = NULL;
        struct acpi_table_irdt_device *dev;
        size_t num = 0;

        /* Remaining DEV bytes shorter than the DEV header */
        memset(buf, 0, sizeof(buf));
        rmud->type = ACPI_TABLE_IRDT_TYPE_RMUD;
        rmud->length = sizeof(*rmud) + 2;

        expect_function_call(__wrap_free);
        expect_value(__wrap_free, ptr, NULL);

        devs = acpi_get_irdt_dev(rmud, &num);
        assert_null(devs);
        assert_int_equal(num, 0);

        /* DEV's own declared length shorter than the DEV header */
        memset(buf, 0, sizeof(buf));
        rmud->type = ACPI_TABLE_IRDT_TYPE_RMUD;
        rmud->length = sizeof(*rmud) + 4;
        dev = rmud->device;
        dev->type = ACPI_TABLE_IRDT_TYPE_DSS;
        dev->length = 2;

        expect_function_call(__wrap_free);
        expect_value(__wrap_free, ptr, NULL);

        devs = acpi_get_irdt_dev(rmud, &num);
        assert_null(devs);
        assert_int_equal(num, 0);
}

/* A DSS or an RCS whose length is under its fields fails the list, as one
 * under the entry header does; a DSS of its fields and no CHMS is taken
 */
static void
test_acpi_get_irdt_dev_short_dss_rcs(void **state __attribute__((unused)))
{
        uint8_t buf[128];
        struct acpi_table_irdt_rmud *rmud = (struct acpi_table_irdt_rmud *)buf;
        struct acpi_table_irdt_device **devs = NULL;
        struct acpi_table_irdt_device *dev = rmud->device;
        const uint16_t hdr_len = sizeof(dev->type) + sizeof(dev->length);
        const uint16_t dss_len = hdr_len + sizeof(dev->dss);
        const uint16_t rcs_len = hdr_len + sizeof(dev->rcs);
        size_t num = 0;

        /* a DSS one byte short of its fields */
        memset(buf, 0, sizeof(buf));
        rmud->length = sizeof(*rmud) + dss_len - 1;
        dev->type = ACPI_TABLE_IRDT_TYPE_DSS;
        dev->length = dss_len - 1;

        expect_function_call(__wrap_free);
        expect_value(__wrap_free, ptr, NULL);

        devs = acpi_get_irdt_dev(rmud, &num);
        assert_null(devs);
        assert_int_equal(num, 0);

        /* a DSS of its fields alone */
        memset(buf, 0, sizeof(buf));
        rmud->length = sizeof(*rmud) + dss_len;
        dev->type = ACPI_TABLE_IRDT_TYPE_DSS;
        dev->length = dss_len;

        devs = acpi_get_irdt_dev(rmud, &num);
        assert_non_null(devs);
        assert_int_equal(num, 1);
        assert_ptr_equal(devs[0], dev);

        expect_function_call(__wrap_free);
        expect_value(__wrap_free, ptr, devs);
        free(devs);

        /* a whole DSS, then an RCS one byte short of its fields: the list
         * made for the DSS is freed, not left
         */
        memset(buf, 0, sizeof(buf));
        rmud->length = sizeof(*rmud) + dss_len + rcs_len - 1;
        dev->type = ACPI_TABLE_IRDT_TYPE_DSS;
        dev->length = dss_len;
        dev = (struct acpi_table_irdt_device *)((uint8_t *)dev + dss_len);
        dev->type = ACPI_TABLE_IRDT_TYPE_RCS;
        dev->length = rcs_len - 1;

        expect_function_call(__wrap_free);
        expect_not_value(__wrap_free, ptr, NULL);

        devs = acpi_get_irdt_dev(rmud, &num);
        assert_null(devs);
        assert_int_equal(num, 0);
}

/* every line the library logged since a case emptied it, so that a case can
 * tell which of a table's fields acpi_print() printed
 */
static char test_log[4096];

void
__wrap_log_printf(int type __attribute__((unused)), const char *str, ...)
{
        const size_t used = strlen(test_log);
        va_list ap;

        va_start(ap, str);
        vsnprintf(test_log + used, sizeof(test_log) - used, str, ap);
        va_end(ap);
}

/* \a len bytes that end where a page with no access allowed to it begins, so
 * that a read past them faults; the mapping's start is given in \a map
 */
static uint8_t *
guarded(const size_t len, uint8_t **map)
{
        const size_t page = (size_t)sysconf(_SC_PAGESIZE);
        uint8_t *p = mmap(NULL, 2 * page, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        assert_true(p != MAP_FAILED);
        assert_int_equal(mprotect(p + page, page, PROT_NONE), 0);
        *map = p;

        return p + page - len;
}

static void
unguard(uint8_t *map)
{
        const size_t page = (size_t)sysconf(_SC_PAGESIZE);

        assert_int_equal(__real_munmap(map, 2 * page), 0);
}

/* An IRDT too short for its protocol flags, or for the rest of its fixed
 * fields: only its header is printed, and nothing past it is read. One of
 * its fixed fields exactly has its protocol flags printed too
 */
static void
test_acpi_print_short_irdt(void **state __attribute__((unused)))
{
        const size_t lens[] = {sizeof(struct acpi_table_header) + 1,
                               sizeof(struct acpi_table_header) + 3,
                               sizeof(struct acpi_table_irdt) - 1,
                               sizeof(struct acpi_table_irdt)};
        size_t i;

        for (i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
                const int whole = lens[i] >= sizeof(struct acpi_table_irdt);
                struct acpi_table table;
                uint8_t *map;
                struct acpi_table_header *header =
                    (struct acpi_table_header *)guarded(lens[i], &map);

                memcpy(header->signature, "IRDT", 4);
                header->length = (uint32_t)lens[i];
                table.header = header;
                test_log[0] = '\0';

                acpi_print(&table);

                assert_non_null(strstr(test_log, "Signature:         IRDT"));
                assert_non_null(strstr(test_log, "Creator Revision:"));
                assert_int_equal(strstr(test_log, "IO Proto Flags:") != NULL,
                                 whole);
                assert_int_equal(strstr(test_log, "Cache Proto Flags:") != NULL,
                                 whole);

                unguard(map);
        }
}

/* An IRDT whole in its fixed fields, whose RMUD holds an RCS of its header
 * alone: the RCS is not printed, so nothing past the table is read
 */
static void
test_acpi_print_irdt_short_rcs(void **state __attribute__((unused)))
{
        const size_t dev_len = 4;
        const size_t len = sizeof(struct acpi_table_irdt) +
                           sizeof(struct acpi_table_irdt_rmud) + dev_len;
        struct acpi_table table;
        uint8_t *map;
        struct acpi_table_irdt *irdt =
            (struct acpi_table_irdt *)guarded(len, &map);
        struct acpi_table_irdt_rmud *rmud = irdt->rmud;

        memset(irdt, 0, len);
        memcpy(irdt->header.signature, "IRDT", 4);
        irdt->header.length = (uint32_t)len;
        rmud->type = ACPI_TABLE_IRDT_TYPE_RMUD;
        rmud->length = sizeof(*rmud) + dev_len;
        rmud->device[0].type = ACPI_TABLE_IRDT_TYPE_RCS;
        rmud->device[0].length = dev_len;
        table.irdt = irdt;

        /* the device list given up, then the RMUD list acpi_print() made */
        expect_function_call(__wrap_free);
        expect_value(__wrap_free, ptr, NULL);
        expect_function_call(__wrap_free);
        expect_not_value(__wrap_free, ptr, NULL);

        acpi_print(&table);

        unguard(map);
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

static void
test_acpi_addresses_are_as_wide_as_the_tables(void **state
                                              __attribute__((unused)))
{
        /* The tables carry 64-bit physical addresses - the RSDP's xsdt_address,
         * every XSDT entry, the ERDT's MMIO locations - and pqos_mmap_read()
         * takes uint64_t. A narrower acpi_address would read a table above
         * 4 GiB from the wrong place rather than fail, which is why the width
         * is asserted here instead of being left to the architecture.
         */
        assert_int_equal(sizeof(acpi_address), sizeof(uint64_t));
        assert_int_equal(sizeof(acpi_size), sizeof(uint64_t));
        assert_int_equal(
            sizeof(acpi_address),
            sizeof(((struct acpi_table_rsdp *)NULL)->xsdt_address));

        /* and the object acpi_free() reads is the one a caller here builds:
         * struct acpi_table_internal comes from lib/acpi.h, so the size this
         * asserts is the library's. This file used to keep a copy of it that
         * was one member short, and test_acpi_free() then allocated less than
         * acpi_free() reads - a copy reintroduced now would not compile.
         */
        assert_true(sizeof(struct acpi_table_internal) >=
                    sizeof(struct acpi_table) + sizeof(acpi_address) +
                        sizeof(acpi_size) + sizeof(enum acpi_tbl_mtype));
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
            cmocka_unit_test(test_acpi_get_irdt_rmud_invalid_len),
            cmocka_unit_test(test_acpi_get_irdt_dev),
            cmocka_unit_test(test_acpi_get_irdt_dev_invalid_len),
            cmocka_unit_test(test_acpi_get_irdt_dev_short_dss_rcs),
            cmocka_unit_test(test_acpi_print_short_irdt),
            cmocka_unit_test(test_acpi_print_irdt_short_rcs),
            cmocka_unit_test(test_acpi_get_irdt_chms),
            cmocka_unit_test(

                test_acpi_addresses_are_as_wide_as_the_tables)};

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
