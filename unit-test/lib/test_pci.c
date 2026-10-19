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
 * Which pci.ids the library opens. Distributions keep the file in different
 * places, pci_ids_files lists them in the order they are looked in, and
 * pci_ids_open() opens the first of a list that is a regular file. The cases
 * make the files, links, directories and FIFOs of a list in a directory of
 * their own, for the lstat() that tells them apart, and the mock pqos_fopen()
 * is told which names are to be opened and what each open gives: a name
 * opened that was not expected, or one expected and not opened, fails the
 * case. pci_ids_find() logs why when nothing opens, and the log is read
 * through a callback.
 *
 * pci_parse_pci_ids() searches the library's own list, pci_ids_files. For it
 * a wrapped lstat() says what is at those three places, and passes every
 * other name through to the real one.
 */

#include "test_pci.h"

#include "log.h"
#include "mock_common.h"
#include "pci.h"
#include "test.h"

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

/** the directory the files of a case are made in */
static char m_dir[] = "/tmp/test_pci.XXXXXX";

/** the full names of the files the cases make, by the index they give */
static char m_names[5][64];

/** what the library logged in the case */
static char m_logged[2048];

/**
 * What the wrapped lstat() says is at each of the library's places: 0 for
 * nothing, otherwise the file type
 */
static mode_t m_system[3];

int
__wrap_lstat(const char *path, struct stat *st)
{
        unsigned i;

        for (i = 0; i < DIM(pci_ids_files); i++) {
                if (strcmp(path, pci_ids_files[i]) != 0)
                        continue;
                if (m_system[i] == 0) {
                        errno = ENOENT;
                        return -1;
                }
                memset(st, 0, sizeof(*st));
                st->st_mode = m_system[i] | 0644;
                return 0;
        }

        return __real_lstat(path, st);
}

static void
log_callback(void *context __attribute__((unused)),
             const size_t size,
             const char *message)
{
        size_t used = strlen(m_logged);

        if (used + size + 1 < sizeof(m_logged)) {
                memcpy(m_logged + used, message, size);
                m_logged[used + size] = '\0';
        }
}

/**
 * @brief The full name of a file of the case's directory
 */
static const char *
path(unsigned idx, const char *name)
{
        assert_true(idx < DIM(m_names));
        snprintf(m_names[idx], sizeof(m_names[idx]), "%s/%s", m_dir, name);

        return m_names[idx];
}

/**
 * @brief Makes a regular file that holds its own name
 */
static void
regular(const char *full)
{
        FILE *f = fopen(full, "w");

        assert_non_null(f);
        assert_true(fputs(full, f) >= 0);
        assert_int_equal(fclose(f), 0);
}

/**
 * @brief Says that a name is opened next, and what the open gives
 *
 * @param [in] full the name
 * @param [in] data what the stream reads, NULL for an open that fails
 */
static void
opened(const char *full, const char *data)
{
        expect_string(__wrap_pqos_fopen, name, full);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, data);
}

/**
 * @brief Checks what a stream reads, and closes it
 */
static void
assert_reads(FILE *fd, const char *data)
{
        char line[64] = {0};

        assert_non_null(fd);
        assert_non_null(fgets(line, sizeof(line), fd));
        assert_string_equal(line, data);
        fclose(fd);
}

static int
dir_setup(void **state __attribute__((unused)))
{
        if (log_init(-1, log_callback, NULL, LOG_VER_VERBOSE) != LOG_RETVAL_OK)
                return -1;

        return mkdtemp(m_dir) == NULL ? -1 : 0;
}

/**
 * @brief Removes what the case made, and leaves the directory for the next
 */
static int
case_teardown(void **state __attribute__((unused)))
{
        unsigned i;

        for (i = 0; i < DIM(m_names); i++)
                if (m_names[i][0] != '\0') {
                        if (unlink(m_names[i]) != 0)
                                (void)rmdir(m_names[i]);
                        m_names[i][0] = '\0';
                }
        m_logged[0] = '\0';
        memset(m_system, 0, sizeof(m_system));

        return 0;
}

static int
dir_teardown(void **state __attribute__((unused)))
{
        log_fini();

        return rmdir(m_dir);
}

static void
test_pci_ids_files_order(void **state __attribute__((unused)))
{
        assert_int_equal(DIM(pci_ids_files), 3);
        assert_string_equal(pci_ids_files[0], "/usr/share/misc/pci.ids");
        assert_string_equal(pci_ids_files[1], "/usr/share/hwdata/pci.ids");
        assert_string_equal(pci_ids_files[2], "/usr/share/pci.ids");
}

static void
test_pci_ids_open_first_regular_file(void **state __attribute__((unused)))
{
        const char *files[2];
        const char *unusable = "";

        files[0] = path(0, "first");
        files[1] = path(1, "second");
        regular(files[0]);
        regular(files[1]);

        opened(files[0], "first");
        assert_reads(pci_ids_open(files, DIM(files), &unusable), "first");
        assert_null(unusable);
}

static void
test_pci_ids_open_passes_over_an_absent_file(void **state
                                             __attribute__((unused)))
{
        const char *files[2];
        const char *unusable = "";

        files[0] = path(0, "absent");
        files[1] = path(1, "present");
        regular(files[1]);

        opened(files[1], "present");
        assert_reads(pci_ids_open(files, DIM(files), &unusable), "present");
        assert_null(unusable);
}

static void
test_pci_ids_open_passes_over_a_symlink(void **state __attribute__((unused)))
{
        const char *files[2];
        const char *unusable = NULL;

        /* the link points at a file outside the list */
        regular(path(2, "target"));
        files[0] = path(0, "link");
        assert_int_equal(symlink(m_names[2], files[0]), 0);
        files[1] = path(1, "real");
        regular(files[1]);

        opened(files[1], "real");
        assert_reads(pci_ids_open(files, DIM(files), &unusable), "real");
        assert_string_equal(unusable, files[0]);
}

static void
test_pci_ids_open_passes_over_a_directory(void **state __attribute__((unused)))
{
        const char *files[2];
        const char *unusable = NULL;

        files[0] = path(0, "dir");
        assert_int_equal(mkdir(files[0], 0700), 0);
        files[1] = path(1, "file");
        regular(files[1]);

        opened(files[1], "file");
        assert_reads(pci_ids_open(files, DIM(files), &unusable), "file");
        assert_string_equal(unusable, files[0]);
}

static void
test_pci_ids_open_passes_over_a_fifo(void **state __attribute__((unused)))
{
        const char *files[2];
        const char *unusable = NULL;

        files[0] = path(0, "fifo");
        assert_int_equal(mkfifo(files[0], 0600), 0);
        files[1] = path(1, "file");
        regular(files[1]);

        opened(files[1], "file");
        assert_reads(pci_ids_open(files, DIM(files), &unusable), "file");
        assert_string_equal(unusable, files[0]);
}

static void
test_pci_ids_open_reports_the_first_unusable(void **state
                                             __attribute__((unused)))
{
        const char *files[3];
        const char *unusable = NULL;

        regular(path(3, "target"));
        files[0] = path(0, "link");
        assert_int_equal(symlink(m_names[3], files[0]), 0);
        files[1] = path(1, "dir");
        assert_int_equal(mkdir(files[1], 0700), 0);
        files[2] = path(2, "absent");

        assert_null(pci_ids_open(files, DIM(files), &unusable));
        assert_string_equal(unusable, files[0]);
}

static void
test_pci_ids_open_without_a_report(void **state __attribute__((unused)))
{
        const char *files[2];

        files[0] = path(0, "dir");
        assert_int_equal(mkdir(files[0], 0700), 0);
        files[1] = path(1, "file");
        regular(files[1]);

        opened(files[1], "file");
        assert_reads(pci_ids_open(files, DIM(files), NULL), "file");
}

static void
test_pci_ids_open_passes_over_a_file_that_does_not_open(void **state
                                                        __attribute__((unused)))
{
        const char *files[2];
        const char *unusable = NULL;

        files[0] = path(0, "unreadable");
        files[1] = path(1, "readable");
        regular(files[0]);
        regular(files[1]);

        opened(files[0], NULL);
        opened(files[1], "readable");
        assert_reads(pci_ids_open(files, DIM(files), &unusable), "readable");
        assert_string_equal(unusable, files[0]);
}

static void
test_pci_ids_open_third_place(void **state __attribute__((unused)))
{
        const char *files[3];
        const char *unusable = NULL;

        files[0] = path(0, "absent");
        files[1] = path(1, "dir");
        assert_int_equal(mkdir(files[1], 0700), 0);
        files[2] = path(2, "third");
        regular(files[2]);

        opened(files[2], "third");
        assert_reads(pci_ids_open(files, DIM(files), &unusable), "third");
        assert_string_equal(unusable, files[1]);
}

static void
test_pci_ids_open_only_a_symlink(void **state __attribute__((unused)))
{
        const char *files[2];
        const char *unusable = NULL;

        regular(path(2, "target"));
        files[0] = path(0, "absent");
        files[1] = path(1, "link");
        assert_int_equal(symlink(m_names[2], files[1]), 0);

        assert_null(pci_ids_open(files, DIM(files), &unusable));
        assert_string_equal(unusable, files[1]);
}

static void
test_pci_ids_open_none(void **state __attribute__((unused)))
{
        const char *files[3];
        const char *unusable = "";

        files[0] = path(0, "absent0");
        files[1] = path(1, "absent1");
        files[2] = path(2, "absent2");

        assert_null(pci_ids_open(files, DIM(files), &unusable));
        assert_null(unusable);
}

static void
test_pci_ids_find_names_the_places(void **state __attribute__((unused)))
{
        const char *files[2];
        char expected[256];

        files[0] = path(0, "absent0");
        files[1] = path(1, "absent1");

        assert_null(pci_ids_find(files, DIM(files)));
        snprintf(expected, sizeof(expected), "Unable to find any of %s, %s.",
                 files[0], files[1]);
        assert_non_null(strstr(m_logged, expected));
        snprintf(expected, sizeof(expected), "$ curl -o %s ", files[0]);
        assert_non_null(strstr(m_logged, expected));
}

static void
test_pci_ids_find_names_what_it_passed_over(void **state
                                            __attribute__((unused)))
{
        const char *files[2];
        char expected[256];

        files[0] = path(0, "absent");
        regular(path(2, "target"));
        files[1] = path(1, "link");
        assert_int_equal(symlink(m_names[2], files[1]), 0);

        assert_null(pci_ids_find(files, DIM(files)));
        snprintf(expected, sizeof(expected), "Unable to read %s: ", files[1]);
        assert_non_null(strstr(m_logged, expected));
        assert_non_null(strstr(m_logged, "A symbolic link is not followed"));
        assert_null(strstr(m_logged, "Unable to find"));
}

static void
test_pci_ids_find_opens_quietly(void **state __attribute__((unused)))
{
        const char *files[1];

        files[0] = path(0, "file");
        regular(files[0]);

        opened(files[0], "file");
        assert_reads(pci_ids_find(files, DIM(files)), "file");
        assert_string_equal(m_logged, "");
}

static void
test_pci_parse_pci_ids_where_fedora_keeps_it(void **state
                                             __attribute__((unused)))
{
        struct pqos_pci_info info;

        m_system[1] = S_IFREG;
        opened(pci_ids_files[1],
               "8086  Intel Corporation\n\t4948  6xxx Series QAT\n");

        memset(&info, 0, sizeof(info));
        pci_parse_pci_ids(&info, 0x0b4000, 0x8086, 0x4948);
        assert_string_equal(info.vendor_name, "Intel Corporation");
        assert_string_equal(info.device_name, "6xxx Series QAT");
        assert_string_equal(m_logged, "");
}

static void
test_pci_parse_pci_ids_nowhere(void **state __attribute__((unused)))
{
        struct pqos_pci_info info;

        memset(&info, 0, sizeof(info));
        pci_parse_pci_ids(&info, 0x0b4000, 0x8086, 0x4948);
        assert_string_equal(info.vendor_name, "");
        assert_non_null(strstr(m_logged, "Unable to find any of "
                                         "/usr/share/misc/pci.ids, "
                                         "/usr/share/hwdata/pci.ids, "
                                         "/usr/share/pci.ids."));
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_pci_ids_files_order),
            cmocka_unit_test_teardown(test_pci_ids_open_first_regular_file,
                                      case_teardown),
            cmocka_unit_test_teardown(
                test_pci_ids_open_passes_over_an_absent_file, case_teardown),
            cmocka_unit_test_teardown(test_pci_ids_open_passes_over_a_symlink,
                                      case_teardown),
            cmocka_unit_test_teardown(test_pci_ids_open_passes_over_a_directory,
                                      case_teardown),
            cmocka_unit_test_teardown(test_pci_ids_open_passes_over_a_fifo,
                                      case_teardown),
            cmocka_unit_test_teardown(
                test_pci_ids_open_reports_the_first_unusable, case_teardown),
            cmocka_unit_test_teardown(test_pci_ids_open_without_a_report,
                                      case_teardown),
            cmocka_unit_test_teardown(
                test_pci_ids_open_passes_over_a_file_that_does_not_open,
                case_teardown),
            cmocka_unit_test_teardown(test_pci_ids_open_third_place,
                                      case_teardown),
            cmocka_unit_test_teardown(test_pci_ids_open_only_a_symlink,
                                      case_teardown),
            cmocka_unit_test_teardown(test_pci_ids_open_none, case_teardown),
            cmocka_unit_test_teardown(test_pci_ids_find_names_the_places,
                                      case_teardown),
            cmocka_unit_test_teardown(
                test_pci_ids_find_names_what_it_passed_over, case_teardown),
            cmocka_unit_test_teardown(test_pci_ids_find_opens_quietly,
                                      case_teardown),
            cmocka_unit_test_teardown(
                test_pci_parse_pci_ids_where_fedora_keeps_it, case_teardown),
            cmocka_unit_test_teardown(test_pci_parse_pci_ids_nowhere,
                                      case_teardown)};

        return cmocka_run_group_tests(tests, dir_setup, dir_teardown);
}
