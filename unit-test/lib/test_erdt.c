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

#include "erdt.h"
#include "mmio.h"
#include "mock_common.h"
#include "test.h"

#include <errno.h>
#include <unistd.h>

#define DASE (ACPI_ERDT_STRUCT_DASE_HEADER_LENGTH + 2)
#define CPU                                                                    \
        (sizeof(struct acpi_table_erdt_rmdd) +                                 \
         sizeof(struct acpi_table_erdt_cacd) + 4 +                             \
         sizeof(struct acpi_table_erdt_marc))
#define DEV                                                                    \
        (sizeof(struct acpi_table_erdt_rmdd) +                                 \
         sizeof(struct acpi_table_erdt_dacd) + DASE)
#define CARD      sizeof(struct acpi_table_erdt_card)
#define SIZE      (sizeof(struct acpi_table_erdt_header) + CPU + DEV + CARD)
#define ERDT_FILE ACPI_TABLE_FS_PATH "/" ACPI_TABLE_SIG_ERDT
struct fixture {
        _Alignas(uint64_t) uint8_t data[SIZE];
        struct acpi_table table;
};
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
/* access(), which answers \a errno queued, or 0 for a file that is there */
int
__wrap_access(const char *path, int mode)
{
        const int err = mock_type(int);

        check_expected(path);
        assert_int_equal(mode, F_OK);
        if (err == 0)
                return 0;
        errno = err;
        return -1;
}
/* the kernel's listing of the ACPI tables: there or not, and if it is, what
 * looking up the ERDT file in it answers (an errno, or 0 for a file there)
 */
static void
expect_listing(const int listed, const int file_errno)
{
        expect_string(__wrap_pqos_dir_exists, path, ACPI_TABLE_FS_PATH);
        will_return(__wrap_pqos_dir_exists, listed);
        if (!listed)
                return;
        expect_string(__wrap_access, path, ERDT_FILE);
        will_return(__wrap_access, file_errno);
}
/**
 * @brief Writes an ERDT of one CPU domain and one device domain into f
 *
 * @param [out] f the fixture
 * @param [in] with_marc whether the CPU domain carries a MARC: one without is
 *             a domain enumerated for monitoring alone, and parses as well
 */
static void
init_marc(struct fixture *f, const int with_marc)
{
        const size_t cpu_length =
            CPU - (with_marc ? 0 : sizeof(struct acpi_table_erdt_marc));
        struct acpi_table_erdt *t;
        struct acpi_table_erdt_rmdd *cpu, *dev;
        struct acpi_table_erdt_cacd *cacd;
        struct acpi_table_erdt_marc *marc;
        struct acpi_table_erdt_dacd *dacd;
        struct acpi_table_erdt_dase *dase;
        uint32_t id = 0x1234;

        memset(f, 0, sizeof(*f));
        t = (void *)f->data;
        f->table.erdt = t;
        t->header.header.revision = ACPI_ERDT_REVISION;
        t->header.header.length = SIZE - CARD - (CPU - cpu_length);
        t->header.max_clos = 16;
        cpu = (void *)t->erdt_sub_structures;
        cpu->type = 0;
        cpu->length = cpu_length;
        cpu->flags = 1;
        cpu->domain_id = 7;
        cpu->max_rmids = 3;
        cacd = (void *)(cpu + 1);
        cacd->type = 1;
        cacd->length = sizeof(*cacd) + 4;
        cacd->rmdd_domain_id = 7;
        memcpy(cacd->enumeration_ids, &id, 4);
        /* every field a value of its own, so that one read from another's
         * place shows
         */
        if (with_marc) {
                marc = (void *)((uint8_t *)cacd + cacd->length);
                marc->type = ACPI_ERDT_STRUCT_MARC_TYPE;
                marc->length = sizeof(*marc);
                marc->mba_flags = 3;
                marc->register_indexing_function_version = 1;
                marc->mba_optimal_bw_register_block_base_address = 0xf0000000;
                marc->mba_minimum_bw_register_block_base_address = 0xf0001000;
                marc->mba_maximum_bw_register_block_base_address = 0xf0002000;
                marc->mba_register_block_size = 2;
                marc->mba_bw_control_window_range = 0xff;
        }
        dev = (void *)((uint8_t *)cpu + cpu->length);
        dev->type = 0;
        dev->length = DEV;
        dev->flags = 2;
        dev->domain_id = 9;
        dacd = (void *)(dev + 1);
        dacd->type = 2;
        dacd->length = sizeof(*dacd) + DASE;
        dacd->rmdd_domain_id = 9;
        dase = dacd->dase;
        dase->type = 1;
        dase->length = DASE;
        dase->segment_number = 2;
        dase->path[0] = 4;
        dase->path[1] = 1;
}

static void
init(struct fixture *f)
{
        init_marc(f, 1);
}
/**
 * @brief Gives the device domain of f a CARD, the I/O L3 CAT registers
 *
 * @param [in,out] f the fixture init() wrote
 * @param [in] cat_reg_offset where the CAT registers start in a page of the
 *             block, the CARD's CAT Register Offset for I/O
 */
static void
init_card(struct fixture *f, const uint16_t cat_reg_offset)
{
        struct acpi_table_erdt *t = f->table.erdt;
        struct acpi_table_erdt_rmdd *cpu = (void *)t->erdt_sub_structures;
        struct acpi_table_erdt_rmdd *dev =
            (void *)((uint8_t *)cpu + cpu->length);
        struct acpi_table_erdt_card *card =
            (void *)((uint8_t *)dev + dev->length);

        card->type = ACPI_ERDT_STRUCT_CARD_TYPE;
        card->length = sizeof(*card);
        card->register_indexing_function_version = 1;
        card->register_base_address = 0xf0003000;
        card->register_block_size = 1;
        card->cache_allocation_register_offsets_for_io = cat_reg_offset;
        card->cache_allocation_register_block_size = 16;
        dev->length += sizeof(*card);
        t->header.header.length += sizeof(*card);
}
static void
expect_table(struct fixture *f)
{
        expect_function_call(__wrap_acpi_init);
        will_return(__wrap_acpi_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_acpi_get_sig);
        expect_string(__wrap_acpi_get_sig, sig, ACPI_TABLE_SIG_ERDT);
        will_return(__wrap_acpi_get_sig, &f->table);
        expect_function_call(__wrap_acpi_print);
        expect_value(__wrap_acpi_print, table, &f->table);
        expect_function_call(__wrap_acpi_free);
        expect_value(__wrap_acpi_free, table, &f->table);
}
static void
test_valid(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_cap cap = {0};
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;
        const struct pqos_erdt_marc *marc;

        init(&f);
        expect_table(&f);
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_OK);
        assert_int_equal(info->num_cpu_agents, 1);
        assert_int_equal(info->num_dev_agents, 1);
        /* Max CLOS as the table gives it, the highest class, and one more
         * class than that counted
         */
        assert_int_equal(info->max_clos, 16);
        assert_int_equal(erdt_num_clos(info), 17);
        assert_int_equal(info->cpu_agents[0].cacd.enumeration_ids[0], 0x1234);
        marc = &info->cpu_agents[0].marc;
        assert_int_equal(marc->flags, 3);
        assert_int_equal(marc->reg_index_func_ver, 1);
        assert_int_equal(marc->opt_bw_reg_block_base_addr, 0xf0000000);
        assert_int_equal(marc->min_bw_reg_block_base_addr, 0xf0001000);
        assert_int_equal(marc->max_bw_reg_block_base_addr, 0xf0002000);
        assert_int_equal(marc->reg_block_size, 2);
        /* Q, which the MMIO MBA reset writes and requests are checked by */
        assert_int_equal(marc->control_window_range, 0xff);
        assert_int_equal(info->dev_agents[0].dacd.dase[0].path[0], 4);
        erdt_fini();
        erdt_fini();
}
static void
test_valid_without_marc(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_cap cap = {0};
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;
        const struct pqos_erdt_marc *marc;

        /* a CPU domain with no MARC parses, and its MARC information is
         * zero: what the MMIO paths read as a domain without MBA registers
         */
        init_marc(&f, 0);
        expect_table(&f);
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_OK);
        assert_int_equal(info->num_cpu_agents, 1);
        assert_int_equal(info->cpu_agents[0].cacd.enumeration_ids[0], 0x1234);
        marc = &info->cpu_agents[0].marc;
        assert_int_equal(marc->opt_bw_reg_block_base_addr, 0);
        assert_int_equal(marc->min_bw_reg_block_base_addr, 0);
        assert_int_equal(marc->max_bw_reg_block_base_addr, 0);
        assert_int_equal(marc->control_window_range, 0);
        assert_int_equal(info->num_dev_agents, 1);
        assert_int_equal(info->dev_agents[0].dacd.dase[0].path[0], 4);
        erdt_fini();
}

static void
test_errors(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_cap cap = {0};
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;
        struct acpi_table_erdt_rmdd *rmdd;

        assert_int_equal(erdt_init(NULL, &cpu, &info), PQOS_RETVAL_PARAM);
        assert_int_equal(erdt_init(&cap, NULL, &info), PQOS_RETVAL_PARAM);
        expect_function_call(__wrap_acpi_init);
        will_return(__wrap_acpi_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_acpi_get_sig);
        expect_string(__wrap_acpi_get_sig, sig, ACPI_TABLE_SIG_ERDT);
        will_return(__wrap_acpi_get_sig, NULL);
        expect_listing(1, ENOENT);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_RESOURCE);
        init(&f);
        f.table.erdt->header.header.revision++;
        expect_table(&f);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_ERROR);
        init(&f);
        rmdd = (void *)f.table.erdt->erdt_sub_structures;
        rmdd->flags = 4;
        expect_table(&f);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_ERROR);
}
/**
 * @brief Parses the fixture's table and answers whether its classes fit
 *
 * The parse keeps whatever Max CLOS the table gives, as the other interfaces
 * read the table for its domains alone; erdt_classes_fit() is what the MMIO
 * interface refuses to start on.
 *
 * @param [in,out] f the fixture, written by init() and init_card()
 * @param [in] max_clos the Max CLOS to give the table
 *
 * @return what erdt_classes_fit() returns for the parsed table
 */
static int
parse_and_fit(struct fixture *f, const uint32_t max_clos)
{
        struct pqos_cap cap = {0};
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;
        int ret;

        f->table.erdt->header.max_clos = max_clos;
        expect_table(f);

        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_OK);
        assert_non_null(info);
        assert_int_equal(info->max_clos, max_clos);

        ret = erdt_classes_fit(info);
        erdt_fini();

        return ret;
}
/* the classes are 0 through Max CLOS, so a field at the type's maximum
 * describes no countable number of them: the table parses, as every interface
 * reads it, and the MMIO interface refuses to start on it. The table has no
 * register block, so that only the field's own check can refuse it
 */
static void
test_an_uncountable_max_clos_does_not_fit(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_erdt_info uncountable = {.max_clos = UINT32_MAX};

        init_marc(&f, 0);
        assert_int_equal(parse_and_fit(&f, UINT32_MAX), PQOS_RETVAL_ERROR);
        /* and it counts no classes rather than a wrapped number */
        assert_int_equal(erdt_num_clos(&uncountable), 0);

        /* one below it is counted, as 0xFFFFFFFF classes, where no register
         * block bounds them: a table of domains enumerated for monitoring
         * alone
         */
        init_marc(&f, 0);
        assert_int_equal(parse_and_fit(&f, UINT32_MAX - 1), PQOS_RETVAL_OK);
}
/* each class has to have its register in every block the table describes: a
 * bank of an MBA register block holds 64 classes
 */
static void
test_a_max_clos_past_the_mba_bank_does_not_fit(void **state
                                               __attribute__((unused)))
{
        struct fixture f;

        init(&f);
        assert_int_equal(parse_and_fit(&f, 64), PQOS_RETVAL_ERROR);

        /* the last class of the bank is one the platform has */
        init(&f);
        assert_int_equal(parse_and_fit(&f, 63), PQOS_RETVAL_OK);
}
/* and the I/O L3 CAT registers of a CARD page start at its CAT Register
 * Offset, so a class whose register would lie past the page is one the device
 * domain does not have
 */
static void
test_a_max_clos_past_the_cat_page_does_not_fit(void **state
                                               __attribute__((unused)))
{
        struct fixture f;

        /* CLOS0's register is the last of the page, CLOS1's would lie past */
        init(&f);
        init_card(&f, PAGE_SIZE - BYTES_PER_CLOS_ENTRY);
        assert_int_equal(parse_and_fit(&f, 1), PQOS_RETVAL_ERROR);

        init(&f);
        init_card(&f, PAGE_SIZE - BYTES_PER_CLOS_ENTRY);
        assert_int_equal(parse_and_fit(&f, 0), PQOS_RETVAL_OK);
}
/* a platform without ERDT, which pqos_init() takes for a feature not there:
 * nothing is said at the default verbosity
 */
static void
test_no_table_is_said_nothing_of(void **state __attribute__((unused)))
{
        struct pqos_cap cap = {0};
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;

        expect_function_call(__wrap_acpi_init);
        will_return(__wrap_acpi_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_acpi_get_sig);
        expect_string(__wrap_acpi_get_sig, sig, ACPI_TABLE_SIG_ERDT);
        will_return(__wrap_acpi_get_sig, NULL);
        expect_listing(1, ENOENT);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        test_log_count_start();
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_RESOURCE);
        assert_int_equal(test_log_count_stop(), 0);
}
/* a table that is there but cannot be read, as by a user who is not root, is
 * said: the platform has the feature
 */
static void
test_an_unreadable_table_is_said(void **state __attribute__((unused)))
{
        struct pqos_cap cap = {0};
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;

        expect_function_call(__wrap_acpi_init);
        will_return(__wrap_acpi_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_acpi_get_sig);
        expect_string(__wrap_acpi_get_sig, sig, ACPI_TABLE_SIG_ERDT);
        will_return(__wrap_acpi_get_sig, NULL);
        expect_listing(1, 0);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        test_log_count_start();
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_RESOURCE);
        assert_int_equal(test_log_count_stop(), 1);
}
/* a table the listing may hold, but which the caller may not look up, is said
 * as one that cannot be read is
 */
static void
test_a_table_that_cannot_be_looked_up_is_said(void **state
                                              __attribute__((unused)))
{
        struct pqos_cap cap = {0};
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;

        expect_function_call(__wrap_acpi_init);
        will_return(__wrap_acpi_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_acpi_get_sig);
        expect_string(__wrap_acpi_get_sig, sig, ACPI_TABLE_SIG_ERDT);
        will_return(__wrap_acpi_get_sig, NULL);
        expect_listing(1, EACCES);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        test_log_count_start();
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_RESOURCE);
        assert_int_equal(test_log_count_stop(), 1);
}
/* no listing of the tables: the scan of ACPI memory found none, and said so
 * itself if it failed; nothing more is said, and no file is looked up
 */
static void
test_no_listing_is_said_nothing_of(void **state __attribute__((unused)))
{
        struct pqos_cap cap = {0};
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;

        expect_function_call(__wrap_acpi_init);
        will_return(__wrap_acpi_init, PQOS_RETVAL_OK);
        expect_function_call(__wrap_acpi_get_sig);
        expect_string(__wrap_acpi_get_sig, sig, ACPI_TABLE_SIG_ERDT);
        will_return(__wrap_acpi_get_sig, NULL);
        expect_listing(0, 0);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        test_log_count_start();
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_RESOURCE);
        assert_int_equal(test_log_count_stop(), 0);
}
/* a table that is there but cannot be used is an error, which is said */
static void
test_an_unusable_table_is_said(void **state __attribute__((unused)))
{
        struct fixture f;
        struct pqos_cap cap = {0};
        struct pqos_cpuinfo cpu = {0};
        struct pqos_erdt_info *info = NULL;

        init(&f);
        f.table.erdt->header.header.revision++;
        expect_table(&f);
        expect_function_call(__wrap_acpi_fini);
        will_return(__wrap_acpi_fini, PQOS_RETVAL_OK);
        test_log_count_start();
        assert_int_equal(erdt_init(&cap, &cpu, &info), PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
}
int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_valid),
            cmocka_unit_test(test_valid_without_marc),
            cmocka_unit_test(test_errors),
            cmocka_unit_test(test_an_uncountable_max_clos_does_not_fit),
            cmocka_unit_test(test_a_max_clos_past_the_mba_bank_does_not_fit),
            cmocka_unit_test(test_a_max_clos_past_the_cat_page_does_not_fit),
            cmocka_unit_test(test_no_table_is_said_nothing_of),
            cmocka_unit_test(test_an_unreadable_table_is_said),
            cmocka_unit_test(test_a_table_that_cannot_be_looked_up_is_said),
            cmocka_unit_test(test_no_listing_is_said_nothing_of),
            cmocka_unit_test(test_an_unusable_table_is_said)};
        return cmocka_run_group_tests(tests, NULL, NULL);
}
