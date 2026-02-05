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

#include "acpi.h"
#include "acpi_table.h"
#include "iordt.h"
#include "pci.h"
#include "test.h"

/* clang-format off */
static uint8_t m_irdt[] = {
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
    0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x34, 0x12,
    0x00, 0x00, 0x00, 0x00,
    /* RCS instance 2 (40 bytes length) */
    0x01, 0x00, 0x28, 0x00, 0x00, 0x00, 0x02, 0x01, 0x0e, 0x00, 0x00, 0x10,
    0x00, 0x20, 0x00, 0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0x34, 0x12,
    0x21, 0x43, 0x65, 0x87};
/* clang-format on */

static struct acpi_table m_irdt_table = {.generic = m_irdt};

/* Signature:         IRDT
 * Length:            205
 * Revision:          1
 * Checksum:          91
 * OEM ID:            OEM ID
 * OEM Table ID:      OEM T ID
 * OEM Revision:      1
 * Creator ID:        2
 * Creator Revision:  4
 * IO Proto Flags:    0X3: MON CTL
 * Cache Proto Flags: 0X3: MON CTL
 * RMUD #0:
 *  Type:              0/RMUD
 *  Length:            157
 *  PCI Segment:       0X1234
 *  DSS
 *   Type:              0/DSS
 *   Length:            40
 *   Device Type:       0X1
 *   Enumeration ID:    4128
 *   2 CHMS(s):
 *    RCS Enum ID:       1
 *     VC0 - Channel:     0
 *     VC1 - Channel:     1 SHARED
 *    RCS Enum ID:       2
 *     VC0 - Channel:     0
 *  DSS
 *   Type:              0/DSS
 *   Length:            24
 *   Device Type:       0X1
 *   Enumeration ID:    8240
 *   1 CHMS(s):
 *    RCS Enum ID:       1
 *     VC0 - Channel:     1 SHARED
 *  RCS
 *   Type:              0X1/RCS
 *   Length:            40
 *   Channel Type:      0
 *   Enumeration ID:    1
 *   Channel Count:     2
 *   Flags:             0X7
 *   RMID Block offset: 0X1000
 *   CLOS Block offset: 0X2000
 *   Block MMIO:        0x00000012345000
 *  RCS
 *   Type:              0X1/RCS
 *   Length:            40
 *   Channel Type:      0
 *   Enumeration ID:    2
 *   Channel Count:     1
 *   Flags:             0XE
 *   RMID Block offset: 0X1000
 *   CLOS Block offset: 0X2000
 *   Block MMIO:        0x8765432112345000
 *
 */

/* ======== mock ======== */

int
__wrap_acpi_init(void)
{
        return PQOS_RETVAL_OK;
}

int
__wrap_acpi_fini(void)
{
        return PQOS_RETVAL_OK;
}

struct acpi_table *
__wrap_acpi_get_sig(const char *sig)
{
        assert_string_equal(sig, "IRDT");
        return &m_irdt_table;
}

void
__wrap_acpi_free(struct acpi_table *table)
{
        assert_non_null(table);
}

int
__wrap_pci_init(void)
{
        return PQOS_RETVAL_OK;
}

int
__wrap_pci_fini(void)
{
        return PQOS_RETVAL_OK;
}

struct pci_dev *
__wrap_pci_dev_get(uint16_t domain, uint16_t bdf)
{
        struct pci_dev *dev;
        uint64_t i;

        assert_int_equal(domain, 0x1234);

        dev = calloc(1, sizeof(*dev));
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

uint8_t *
__wrap_pqos_mmap_read(uint64_t address, const uint64_t size)
{
        check_expected(address);
        check_expected(size);

        return mock_ptr_type(uint8_t *);
}

/**
 * @brief Map physical memory for writing
 * @param[in] address Physical memory address
 * @param[in] size Memory size
 *
 * @return Mapped memory
 */
uint8_t *
__wrap_pqos_mmap_write(uint64_t address, const uint64_t size)
{
        check_expected(address);
        check_expected(size);

        return mock_ptr_type(uint8_t *);
}

void
__wrap_pqos_munmap(void *mem, const uint64_t size)
{
        UNUSED_PARAM(size);

        assert_non_null(mem);
}

/* ======== init ======== */

static int
test_iordt_init(void **state)
{
        int ret;
        struct test_data *data;
        struct pqos_devinfo *dev;

        ret = test_init_all(state);
        if (ret != 0)
                return ret;

        data = (struct test_data *)*state;

        ret = iordt_init(data->cap, &dev);
        if (ret != PQOS_RETVAL_OK)
                return -1;

        return 0;
}

static int
test_iordt_fini(void **state)
{
        iordt_fini();

        return test_fini(state);
}

/* ======== iordt_mon_assoc_write ======== */

static void
test_iordt_mon_assoc_write_param(void **state __attribute__((unused)))
{
        pqos_rmid_t rmid = 1;
        int ret;

        /* invalid channel id */
        ret = iordt_mon_assoc_write(0, rmid);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* channel out of range */
        ret = iordt_mon_assoc_write(0x10108, rmid);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_iordt_mon_assoc_write(void **state __attribute__((unused)))
{
        pqos_channel_t channel;
        pqos_rmid_t rmid = 1;
        uint64_t address;
        uint8_t mimo[0x1000];
        int ret;

        /* MIMO */
        channel = 0x10200;
        address = 0x8765432112346000LLU;

        expect_value(__wrap_pqos_mmap_write, address, address);
        expect_value(__wrap_pqos_mmap_write, size, 0x10);
        will_return(__wrap_pqos_mmap_write, mimo);

        ret = iordt_mon_assoc_write(channel, rmid);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(rmid, *(uint16_t *)(mimo));
}

static void
test_iordt_mon_assoc_write_error(void **state __attribute__((unused)))
{
        pqos_channel_t channel;
        pqos_rmid_t rmid = 1;
        uint64_t address;
        int ret;

        channel = 0x10101;
        address = 0x12346000LLU;

        expect_value(__wrap_pqos_mmap_write, address, address);
        expect_value(__wrap_pqos_mmap_write, size, 0x20);
        will_return(__wrap_pqos_mmap_write, NULL);

        ret = iordt_mon_assoc_write(channel, rmid);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== iordt_mon_assoc_read ======== */

static void
test_iordt_mon_assoc_read_param(void **state __attribute__((unused)))
{
        pqos_channel_t channel = 0x10101;
        pqos_rmid_t rmid;
        int ret;

        ret = iordt_mon_assoc_read(channel, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* invalid channel id */
        ret = iordt_mon_assoc_read(0, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* channel out of range */
        ret = iordt_mon_assoc_write(0x10108, rmid);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_iordt_mon_assoc_read(void **state __attribute__((unused)))
{
        pqos_channel_t channel;
        pqos_rmid_t rmid;
        uint64_t address;
        uint8_t mimo[0x1000];
        int ret;

        /* MIMO */
        channel = 0x10200;
        address = 0x8765432112346000LLU;

        expect_value(__wrap_pqos_mmap_read, address, address);
        expect_value(__wrap_pqos_mmap_read, size, 0x10);
        will_return(__wrap_pqos_mmap_read, mimo);

        *(uint16_t *)(mimo) = 0x6;

        ret = iordt_mon_assoc_read(channel, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(rmid, 6);
}

static void
test_iordt_mon_assoc_read_error(void **state __attribute__((unused)))
{
        pqos_channel_t channel;
        pqos_rmid_t rmid;
        uint64_t address;
        int ret;

        channel = 0x10101;
        address = 0x12346000LLU;

        expect_value(__wrap_pqos_mmap_read, address, address);
        expect_value(__wrap_pqos_mmap_read, size, 0x20);
        will_return(__wrap_pqos_mmap_read, NULL);

        ret = iordt_mon_assoc_read(channel, &rmid);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== iordt_assoc_write ======== */

static void
test_iordt_assoc_write_param(void **state __attribute__((unused)))
{
        unsigned class_id = 1;
        int ret;

        /* invalid channel id */
        ret = iordt_assoc_write(0, class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* channel out of range */
        ret = iordt_assoc_write(0x10108, class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_iordt_assoc_write(void **state __attribute__((unused)))
{
        pqos_channel_t channel;
        unsigned class_id = 1;
        uint64_t address;
        uint8_t mimo[0x1000];
        int ret;

        /* MIMO */
        channel = 0x10200;
        address = 0x8765432112347000LLU;

        expect_value(__wrap_pqos_mmap_write, address, address);
        expect_value(__wrap_pqos_mmap_write, size, 0x10);
        will_return(__wrap_pqos_mmap_write, mimo);

        ret = iordt_assoc_write(channel, class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, *(uint16_t *)(mimo));
}

static void
test_iordt_assoc_write_error(void **state __attribute__((unused)))
{
        pqos_channel_t channel;
        unsigned class_id = 1;
        uint64_t address;
        int ret;

        channel = 0x10101;
        address = 0x12347000LLU;

        expect_value(__wrap_pqos_mmap_write, address, address);
        expect_value(__wrap_pqos_mmap_write, size, 0x20);
        will_return(__wrap_pqos_mmap_write, NULL);

        ret = iordt_assoc_write(channel, class_id);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== iordt_assoc_read ======== */

static void
test_iordt_assoc_read_param(void **state __attribute__((unused)))
{
        pqos_channel_t channel = 0x10101;
        unsigned class_id;
        int ret;

        ret = iordt_assoc_read(channel, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* invalid channel id */
        ret = iordt_assoc_read(0, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        /* channel out of range */
        ret = iordt_assoc_read(0x10108, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_iordt_assoc_read(void **state __attribute__((unused)))
{
        pqos_channel_t channel;
        unsigned class_id;
        uint64_t address;
        uint8_t mimo[0x1000];
        int ret;

        /* MIMO */
        channel = 0x10200;
        address = 0x8765432112347000LLU;

        expect_value(__wrap_pqos_mmap_read, address, address);
        expect_value(__wrap_pqos_mmap_read, size, 0x10);
        will_return(__wrap_pqos_mmap_read, mimo);

        *(uint16_t *)(mimo) = 0x6;

        ret = iordt_assoc_read(channel, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 6);
}

static void
test_iordt_assoc_read_error(void **state __attribute__((unused)))
{
        pqos_channel_t channel;
        unsigned class_id;
        uint64_t address;
        int ret;

        channel = 0x10101;
        address = 0x12347000LLU;

        expect_value(__wrap_pqos_mmap_read, address, address);
        expect_value(__wrap_pqos_mmap_read, size, 0x20);
        will_return(__wrap_pqos_mmap_read, NULL);

        ret = iordt_assoc_read(channel, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_iordt_mon_assoc_write_param),
            cmocka_unit_test(test_iordt_mon_assoc_write),
            cmocka_unit_test(test_iordt_mon_assoc_write_error),
            cmocka_unit_test(test_iordt_mon_assoc_read_param),
            cmocka_unit_test(test_iordt_mon_assoc_read),
            cmocka_unit_test(test_iordt_mon_assoc_read_error),
            cmocka_unit_test(test_iordt_assoc_write_param),
            cmocka_unit_test(test_iordt_assoc_write),
            cmocka_unit_test(test_iordt_assoc_write_error),
            cmocka_unit_test(test_iordt_assoc_read_param),
            cmocka_unit_test(test_iordt_assoc_read),
            cmocka_unit_test(test_iordt_assoc_read_error)};

        result +=
            cmocka_run_group_tests(tests, test_iordt_init, test_iordt_fini);

        return result;
}
