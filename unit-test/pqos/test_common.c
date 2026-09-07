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
#include "common.h"
#include "output.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/* ======== safe_open ======== */

/* Every case works inside a directory of its own, so the files it makes and
 * the files it expects to be absent cannot be confused with anything left
 * behind by another run.
 */
static char work_dir[PATH_MAX];

#define FILE_MODE (S_IRUSR | S_IWUSR)

static const char *
work_path(char *buffer, size_t size, const char *name)
{
        snprintf(buffer, size, "%s/%s", work_dir, name);
        buffer[size - 1] = '\0';

        return buffer;
}

static int
group_setup(void **state)
{
        UNUSED_ARG(state);

        snprintf(work_dir, sizeof(work_dir), "/tmp/pqos_ut_common_XXXXXX");

        return mkdtemp(work_dir) == NULL ? -1 : 0;
}

static int
group_teardown(void **state)
{
        UNUSED_ARG(state);

        if (work_dir[0] != '\0')
                rmdir(work_dir);

        return 0;
}

/* A file the caller offered to create does not exist yet, which is the whole
 * point of asking for it: "pqos --log-file=<new path>" names a log that pqos
 * is meant to open for the first time.
 */
static void
test_safe_open_creates_a_missing_file(void **state)
{
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "new_log.txt");
        struct stat st;
        int fd = -1;

        UNUSED_ARG(state);

        unlink(path);

        run_function(safe_open, fd, path, O_WRONLY | O_CREAT, FILE_MODE);

        assert_int_not_equal(fd, -1);
        assert_int_equal(write(fd, "logged\n", 7), 7);
        assert_int_equal(close(fd), 0);

        assert_int_equal(lstat(path, &st), 0);
        assert_true(S_ISREG(st.st_mode));
        assert_int_equal(st.st_size, 7);

        assert_int_equal(unlink(path), 0);
}

/* Without O_CREAT the caller asked for a file that is already there, so a
 * missing one is still an error - and no file appears in its place.
 */
static void
test_safe_open_refuses_a_missing_file_it_may_not_create(void **state)
{
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "absent.txt");
        int fd = 0;

        UNUSED_ARG(state);

        unlink(path);
        errno = 0;

        run_function(safe_open, fd, path, O_RDONLY, FILE_MODE);

        assert_int_equal(fd, -1);
        /* the reason reaches the caller, which is what prints it */
        assert_int_equal(errno, ENOENT);
        assert_int_equal(access(path, F_OK), -1);
}

static void
test_safe_open_opens_an_existing_file(void **state)
{
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "present.txt");
        char content[8] = {0};
        FILE *stream;
        int fd = -1;

        UNUSED_ARG(state);

        stream = fopen(path, "w");
        assert_non_null(stream);
        assert_int_equal(fwrite("logged\n", 1, 7, stream), 7);
        assert_int_equal(fclose(stream), 0);

        run_function(safe_open, fd, path, O_RDONLY, FILE_MODE);

        assert_int_not_equal(fd, -1);
        assert_int_equal(read(fd, content, sizeof(content) - 1), 7);
        assert_string_equal(content, "logged\n");
        assert_int_equal(close(fd), 0);

        assert_int_equal(unlink(path), 0);
}

/* The symlink check is what safe_open() is for, so it has to survive the
 * missing file being allowed through: a name that resolves to a file the
 * caller did not name is refused, whether the link already pointed at
 * something or the open created its target.
 */
static void
test_safe_open_refuses_a_symlink(void **state)
{
        char target_buffer[PATH_MAX];
        char link_buffer[PATH_MAX];
        const char *target =
            work_path(target_buffer, sizeof(target_buffer), "target.txt");
        const char *link =
            work_path(link_buffer, sizeof(link_buffer), "link.txt");
        FILE *stream;
        int fd = 0;

        UNUSED_ARG(state);

        unlink(link);
        stream = fopen(target, "w");
        assert_non_null(stream);
        assert_int_equal(fclose(stream), 0);
        assert_int_equal(symlink(target, link), 0);
        errno = 0;

        run_function(safe_open, fd, link, O_WRONLY | O_CREAT, FILE_MODE);

        assert_int_equal(fd, -1);
        assert_int_equal(errno, ELOOP);
        assert_int_equal(output_has_text("is a symlink"), 1);

        assert_int_equal(unlink(link), 0);
        assert_int_equal(unlink(target), 0);
}

static void
test_safe_open_refuses_a_dangling_symlink(void **state)
{
        char target_buffer[PATH_MAX];
        char link_buffer[PATH_MAX];
        const char *target =
            work_path(target_buffer, sizeof(target_buffer), "dangling.txt");
        const char *link =
            work_path(link_buffer, sizeof(link_buffer), "dangling_link.txt");
        int fd = 0;

        UNUSED_ARG(state);

        unlink(link);
        unlink(target);
        assert_int_equal(symlink(target, link), 0);
        errno = 0;

        run_function(safe_open, fd, link, O_WRONLY | O_CREAT, FILE_MODE);

        assert_int_equal(fd, -1);
        assert_int_equal(errno, ELOOP);
        assert_int_equal(output_has_text("is a symlink"), 1);
        /* refused before the open could create what the link pointed at */
        assert_int_equal(access(target, F_OK), -1);

        assert_int_equal(unlink(link), 0);
        unlink(target);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_safe_open_creates_a_missing_file),
            cmocka_unit_test(
                test_safe_open_refuses_a_missing_file_it_may_not_create),
            cmocka_unit_test(test_safe_open_opens_an_existing_file),
            cmocka_unit_test(test_safe_open_refuses_a_symlink),
            cmocka_unit_test(test_safe_open_refuses_a_dangling_symlink),
        };

        return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
