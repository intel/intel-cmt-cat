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
#include "test_common.h"

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

/* The window between safe_open()'s lstat() and its open() cannot be hit by a
 * caller, so lstat() is wrapped: the first call on the armed name answers
 * truthfully and then puts a symlink there, which is exactly the race the
 * O_NOFOLLOW is for. The pass-through is fstatat() rather than __real_lstat,
 * which would not link where lstat is not an exported symbol; a C library that
 * does not route lstat() through the wrapper at all - it is exported from
 * glibc 2.33 - leaves race_wrapper_ran clear, and the cases below skip rather
 * than pretend.
 */
static const char *race_link = NULL;
static const char *race_target = NULL;
static int race_regular;
static int race_wrapper_ran;

int
__wrap_lstat(const char *pathname, struct stat *buf)
{
        int ret = fstatat(AT_FDCWD, pathname, buf, AT_SYMLINK_NOFOLLOW);
        int error = errno;

        if (race_link != NULL && strcmp(pathname, race_link) == 0) {
                race_wrapper_ran = 1;
                /* the armed name may be a directory the case created, so that
                 * it can be replaced the way a directory of a path could be;
                 * for a name that is not one this fails and changes nothing
                 */
                rmdir(race_link);
                if (race_regular) {
                        int raced = creat(race_link, S_IRUSR | S_IWUSR);

                        if (raced == -1)
                                race_wrapper_ran = -1;
                        else
                                close(raced);
                } else if (symlink(race_target, race_link) != 0) {
                        race_wrapper_ran = -1;
                }
                race_link = NULL;
        }

        errno = error;

        return ret;
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
 * missing file being allowed through: a name that is a link is refused whether
 * the link has a target or not, and the one without a target is refused
 * without its target being created.
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

/* What open() reports for a symlink depends on the flags: O_CREAT | O_EXCL
 * answers EEXIST rather than ELOOP, so a caller passing them would have been
 * told the file exists instead of that its name is a link.
 */
static void
test_safe_open_refuses_a_symlink_whatever_the_flags(void **state)
{
        char target_buffer[PATH_MAX];
        char link_buffer[PATH_MAX];
        const char *target =
            work_path(target_buffer, sizeof(target_buffer), "excl_target.txt");
        const char *link =
            work_path(link_buffer, sizeof(link_buffer), "excl_link.txt");
        int fd = 0;

        UNUSED_ARG(state);

        unlink(link);
        unlink(target);
        assert_int_equal(symlink(target, link), 0);
        errno = 0;

        run_function(safe_open, fd, link, O_WRONLY | O_CREAT | O_EXCL,
                     FILE_MODE);

        assert_int_equal(fd, -1);
        assert_int_equal(errno, ELOOP);
        assert_int_equal(output_has_text("is a symlink"), 1);
        assert_int_equal(access(target, F_OK), -1);

        assert_int_equal(unlink(link), 0);
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

/* An ELOOP is not proof that the name is a link: a loop in a directory of the
 * path answers the same way, and calling that a symlink names the wrong file.
 * Here the leaf's own lstat() is what fails, so the refusal comes before the
 * open; the promise the case pins is the one a caller sees either way - ELOOP,
 * and no claim about the leaf.
 */
static void
test_safe_open_does_not_call_a_looping_parent_a_symlink(void **state)
{
        char loop_buffer[PATH_MAX];
        char path_buffer[PATH_MAX];
        const char *loop = work_path(loop_buffer, sizeof(loop_buffer), "loop");
        const char *path =
            work_path(path_buffer, sizeof(path_buffer), "loop/leaf.txt");
        int fd = 0;

        UNUSED_ARG(state);

        unlink(loop);
        assert_int_equal(symlink(loop, loop), 0);
        errno = 0;

        run_function(safe_open, fd, path, O_WRONLY | O_CREAT, FILE_MODE);

        assert_int_equal(fd, -1);
        assert_int_equal(errno, ELOOP);
        /* the refusal is right, the explanation would not have been */
        assert_int_equal(output_has_text("is a symlink"), 0);

        assert_int_equal(unlink(loop), 0);
}

/* The link appears after the lstat() has already said the name is free, so the
 * check before the open cannot see it and the open() is the only thing left
 * between the caller and a file it never named.
 */
static void
test_safe_open_refuses_a_symlink_that_appears_in_the_window(void **state)
{
        char target_buffer[PATH_MAX];
        char link_buffer[PATH_MAX];
        const char *target =
            work_path(target_buffer, sizeof(target_buffer), "race_target.txt");
        const char *link =
            work_path(link_buffer, sizeof(link_buffer), "race_link.txt");
        int fd = 0;

        UNUSED_ARG(state);

        unlink(link);
        unlink(target);
        race_target = target;
        race_link = link;
        race_wrapper_ran = 0;
        errno = 0;

        run_function(safe_open, fd, link, O_WRONLY | O_CREAT, FILE_MODE);

        race_link = NULL;
        if (race_wrapper_ran != 1) {
                unlink(link);
                unlink(target);
                skip();
        }

        assert_int_equal(fd, -1);
        assert_int_equal(errno, ELOOP);
        assert_int_equal(output_has_text("is a symlink"), 1);
        assert_int_equal(access(target, F_OK), -1);

        assert_int_equal(unlink(link), 0);
}

/* The same race with O_EXCL, where the refusal arrives as EEXIST rather than
 * ELOOP and would otherwise be reported as "the file exists".
 */
static void
test_safe_open_names_the_symlink_that_appears_under_o_excl(void **state)
{
        char target_buffer[PATH_MAX];
        char link_buffer[PATH_MAX];
        const char *target =
            work_path(target_buffer, sizeof(target_buffer), "excl_race.txt");
        const char *link =
            work_path(link_buffer, sizeof(link_buffer), "excl_race_link.txt");
        int fd = 0;

        UNUSED_ARG(state);

        unlink(link);
        unlink(target);
        race_target = target;
        race_link = link;
        race_wrapper_ran = 0;
        errno = 0;

        run_function(safe_open, fd, link, O_WRONLY | O_CREAT | O_EXCL,
                     FILE_MODE);

        race_link = NULL;
        if (race_wrapper_ran != 1) {
                unlink(link);
                unlink(target);
                skip();
        }

        assert_int_equal(fd, -1);
        assert_int_equal(errno, ELOOP);
        assert_int_equal(output_has_text("is a symlink"), 1);
        assert_int_equal(access(target, F_OK), -1);

        assert_int_equal(unlink(link), 0);
}

/* O_DIRECTORY turns the refusal into ENOTDIR, so an errno is not what decides
 * whether a name is a link - the name is. Here a directory is replaced by a
 * link to one inside the window, which is the shape a path component takes.
 */
static void
test_safe_open_names_a_link_the_open_called_something_else(void **state)
{
        char dir_buffer[PATH_MAX];
        const char *dir = work_path(dir_buffer, sizeof(dir_buffer), "race_dir");
        int fd = 0;

        UNUSED_ARG(state);

        rmdir(dir);
        unlink(dir);
        assert_int_equal(mkdir(dir, S_IRWXU), 0);
        race_target = work_dir;
        race_link = dir;
        race_wrapper_ran = 0;
        errno = 0;

        run_function(safe_open, fd, dir, O_RDONLY | O_DIRECTORY, FILE_MODE);

        race_link = NULL;
        if (race_wrapper_ran != 1) {
                unlink(dir);
                rmdir(dir);
                skip();
        }

        assert_int_equal(fd, -1);
        assert_int_equal(errno, ELOOP);
        assert_int_equal(output_has_text("is a symlink"), 1);

        assert_int_equal(unlink(dir), 0);
}

/* Not every name that appears in the window is a link. A file somebody else
 * created there - or a FIFO, which O_NOFOLLOW has no opinion about - would be
 * adopted as the caller's new log, so the create has to be the one that made
 * the file it goes on to write to.
 */
static void
test_safe_open_does_not_adopt_a_file_that_appeared(void **state)
{
        char path_buffer[PATH_MAX];
        const char *path =
            work_path(path_buffer, sizeof(path_buffer), "raced_in.txt");
        int fd = 0;

        UNUSED_ARG(state);

        unlink(path);
        race_target = NULL;
        race_regular = 1;
        race_link = path;
        race_wrapper_ran = 0;
        errno = 0;

        run_function(safe_open, fd, path, O_WRONLY | O_CREAT, FILE_MODE);

        race_link = NULL;
        race_regular = 0;
        if (race_wrapper_ran != 1) {
                unlink(path);
                skip();
        }

        assert_int_equal(fd, -1);
        assert_int_equal(errno, EEXIST);
        /* it is not a link, and is not described as one */
        assert_int_equal(output_has_text("is a symlink"), 0);

        assert_int_equal(unlink(path), 0);
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
            cmocka_unit_test(
                test_safe_open_refuses_a_symlink_whatever_the_flags),
            cmocka_unit_test(test_safe_open_refuses_a_dangling_symlink),
            cmocka_unit_test(
                test_safe_open_does_not_call_a_looping_parent_a_symlink),
            cmocka_unit_test(
                test_safe_open_refuses_a_symlink_that_appears_in_the_window),
            cmocka_unit_test(
                test_safe_open_names_the_symlink_that_appears_under_o_excl),
            cmocka_unit_test(
                test_safe_open_names_a_link_the_open_called_something_else),
            cmocka_unit_test(
                test_safe_open_does_not_adopt_a_file_that_appeared),
        };

        return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
