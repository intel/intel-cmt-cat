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

/* ======== safe_fopen ======== */

/* The mode string decides the flags, so the stream a caller gets has to behave
 * the way fopen() would have behaved: read what is there, create what is not,
 * and add to what is there rather than replacing it.
 */
static void
test_safe_fopen_reads_an_existing_file(void **state)
{
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "readable.txt");
        char content[8] = {0};
        FILE *stream;

        UNUSED_ARG(state);

        stream = fopen(path, "w");
        assert_non_null(stream);
        assert_int_equal(fwrite("logged\n", 1, 7, stream), 7);
        assert_int_equal(fclose(stream), 0);

        run_function(safe_fopen, stream, path, "r");

        assert_non_null(stream);
        assert_non_null(fgets(content, sizeof(content), stream));
        assert_string_equal(content, "logged\n");
        assert_int_equal(fclose(stream), 0);

        assert_int_equal(unlink(path), 0);
}

static void
test_safe_fopen_creates_and_appends(void **state)
{
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "created.txt");
        char content[16] = {0};
        FILE *stream;

        UNUSED_ARG(state);

        unlink(path);

        run_function(safe_fopen, stream, path, "w+");
        assert_non_null(stream);
        assert_int_equal(fwrite("first\n", 1, 6, stream), 6);
        assert_int_equal(fclose(stream), 0);

        run_function(safe_fopen, stream, path, "a");
        assert_non_null(stream);
        assert_int_equal(fwrite("second\n", 1, 7, stream), 7);
        assert_int_equal(fclose(stream), 0);

        stream = fopen(path, "r");
        assert_non_null(stream);
        assert_non_null(fgets(content, sizeof(content), stream));
        assert_string_equal(content, "first\n");
        assert_non_null(fgets(content, sizeof(content), stream));
        assert_string_equal(content, "second\n");
        assert_int_equal(fclose(stream), 0);

        assert_int_equal(unlink(path), 0);
}

/* The defect this closes: a link already sitting at the name was followed by
 * fopen() and the file it pointed at was truncated by the "w+" the monitoring
 * output file uses, before anything looked at the descriptor. No race is
 * needed to reach it, so the case does not need one either - and what it
 * asserts is the target, not only the refusal.
 */
static void
test_safe_fopen_keeps_a_symlink_target_intact(void **state)
{
        char target_buffer[PATH_MAX];
        char link_buffer[PATH_MAX];
        const char *target =
            work_path(target_buffer, sizeof(target_buffer), "precious.txt");
        const char *link =
            work_path(link_buffer, sizeof(link_buffer), "output.csv");
        char content[16] = {0};
        FILE *stream;
        int error;

        UNUSED_ARG(state);

        unlink(link);
        stream = fopen(target, "w");
        assert_non_null(stream);
        assert_int_equal(fwrite("keep me\n", 1, 8, stream), 8);
        assert_int_equal(fclose(stream), 0);
        assert_int_equal(symlink(target, link), 0);
        errno = 0;

        run_function(safe_fopen, stream, link, "w+");
        /* kept before the assertions below, since every one of them is a call
         * that may set errno itself
         */
        error = errno;

        /* the state is asserted before the refusal, because the file surviving
         * is the point: what this replaced also returned NULL, after the target
         * had already been emptied
         */
        assert_null(stream);
        stream = fopen(target, "r");
        assert_non_null(stream);
        assert_non_null(fgets(content, sizeof(content), stream));
        assert_string_equal(content, "keep me\n");
        assert_int_equal(fclose(stream), 0);

        assert_int_equal(error, ELOOP);
        assert_int_equal(output_has_text("is a symlink"), 1);

        assert_int_equal(unlink(link), 0);
        assert_int_equal(unlink(target), 0);
}

/* A link with no target is refused as a link rather than created through */
static void
test_safe_fopen_refuses_a_dangling_symlink(void **state)
{
        char target_buffer[PATH_MAX];
        char link_buffer[PATH_MAX];
        const char *target =
            work_path(target_buffer, sizeof(target_buffer), "nowhere.txt");
        const char *link =
            work_path(link_buffer, sizeof(link_buffer), "dangling.csv");
        FILE *stream;
        int error;

        UNUSED_ARG(state);

        unlink(target);
        unlink(link);
        assert_int_equal(symlink(target, link), 0);
        errno = 0;

        run_function(safe_fopen, stream, link, "w+");
        error = errno;

        /* the target is checked first: what this replaced created it through
         * the link before refusing the stream. access() sets errno of its own,
         * which is why the reason was kept above
         */
        assert_null(stream);
        assert_int_equal(access(target, F_OK), -1);
        assert_int_equal(error, ELOOP);
        assert_int_equal(output_has_text("is a symlink"), 1);

        assert_int_equal(unlink(link), 0);
}

/* A mode this cannot translate must fail rather than open the file in some
 * other way, and the caller is told which kind of failure it was
 */
static void
test_safe_fopen_refuses_a_mode_it_cannot_translate(void **state)
{
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "unwritten.txt");
        /* "bw" and "rbb" are the ones that matter: a 'b' accepted wherever it
         * appeared would translate them as "w" and "r", and the first of those
         * truncates. The rest are the shapes C does not give the exclusive
         * modifier: it goes after a 'w', once, and last of all, so "wxb",
         * "wx+" and "w+xb" are no more C modes than "rx" or "xw" are
         */
        static const char *const modes[] = {
            "",     "rw", "w+e", "z",  "bw",  "b",   "rbb", "+r",  "r++",
            "ab+b", "rx", "ax",  "xw", "wxx", "wxb", "wx+", "w+xb"};
        static const char *const accepted[] = {"wb", "w+b", "wb+"};
        FILE *stream;
        unsigned i;

        UNUSED_ARG(state);

        unlink(path);

        for (i = 0; i < DIM(modes); i++) {
                errno = 0;
                run_function(safe_fopen, stream, path, modes[i]);

                assert_null(stream);
                assert_int_equal(errno, EINVAL);
                /* and nothing was created on the way */
                assert_int_equal(access(path, F_OK), -1);
        }

        /* a 'b' where C puts it is accepted, since it asks for nothing */
        for (i = 0; i < DIM(accepted); i++) {
                run_function(safe_fopen, stream, path, accepted[i]);
                assert_non_null(stream);
                assert_int_equal(fclose(stream), 0);
                assert_int_equal(unlink(path), 0);
        }
}

/* The mode is refused before the file is opened, so a malformed one cannot
 * empty a file on its way to being rejected: "bw" translated as "w" would open
 * with O_TRUNC and only then be refused by fdopen()
 */
static void
test_safe_fopen_refuses_a_malformed_mode_without_truncating(void **state)
{
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "not_emptied.txt");
        char content[16] = {0};
        FILE *stream;
        int error;

        UNUSED_ARG(state);

        stream = fopen(path, "w");
        assert_non_null(stream);
        assert_int_equal(fwrite("keep me\n", 1, 8, stream), 8);
        assert_int_equal(fclose(stream), 0);
        errno = 0;

        run_function(safe_fopen, stream, path, "bw");
        error = errno;

        assert_null(stream);
        stream = fopen(path, "r");
        assert_non_null(stream);
        assert_non_null(fgets(content, sizeof(content), stream));
        assert_string_equal(content, "keep me\n");
        assert_int_equal(fclose(stream), 0);
        assert_int_equal(error, EINVAL);

        assert_int_equal(unlink(path), 0);
}

/* The stream outlives the call, so it is not handed to whatever the process
 * executes next
 */
static void
test_safe_fopen_closes_the_stream_on_exec(void **state)
{
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "cloexec.txt");
        FILE *stream;

        UNUSED_ARG(state);

        unlink(path);

        run_function(safe_fopen, stream, path, "w");

        assert_non_null(stream);
        assert_true(fcntl(fileno(stream), F_GETFD) & FD_CLOEXEC);
        assert_int_equal(fclose(stream), 0);

        assert_int_equal(unlink(path), 0);
}

/* 'x' is C11, not a glibc extension: it asks for the file to be created and for
 * the call to fail if it is already there. What must not happen is the failure
 * arriving after the file has been touched, so this asserts the file as well as
 * the errno.
 */
static void
test_safe_fopen_creates_exclusively(void **state)
{
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "exclusive.txt");
        char content[16] = {0};
        FILE *stream;
        int error;

        UNUSED_ARG(state);

        unlink(path);

        run_function(safe_fopen, stream, path, "wx");
        assert_non_null(stream);
        assert_int_equal(fwrite("first\n", 1, 6, stream), 6);
        assert_int_equal(fclose(stream), 0);

        errno = 0;
        run_function(safe_fopen, stream, path, "wx");
        error = errno;

        assert_null(stream);
        stream = fopen(path, "r");
        assert_non_null(stream);
        assert_non_null(fgets(content, sizeof(content), stream));
        assert_string_equal(content, "first\n");
        assert_int_equal(fclose(stream), 0);
        assert_int_equal(error, EEXIST);

        assert_int_equal(unlink(path), 0);
}

/* The variants C allows, which also cover why 'x' is left out of the mode the
 * stream is made with: glibc's fdopen() refuses "w+x" outright, so a stream
 * asked for that way is only possible because the modifier is dropped once the
 * exclusion has happened
 */
static void
test_safe_fopen_accepts_the_exclusive_variants(void **state)
{
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "variants.txt");
        /* exactly the five C11 gives, with 'x' last in each */
        static const char *const modes[] = {"wx", "wbx", "w+x", "w+bx", "wb+x"};
        FILE *stream;
        unsigned i;

        UNUSED_ARG(state);

        for (i = 0; i < DIM(modes); i++) {
                unlink(path);
                errno = 0;

                run_function(safe_fopen, stream, path, modes[i]);

                assert_non_null(stream);
                assert_int_equal(fclose(stream), 0);
                assert_int_equal(access(path, F_OK), 0);
        }

        assert_int_equal(unlink(path), 0);
}

/* An exclusive mode does not weaken the symlink refusal, which is worth a case
 * because O_CREAT | O_EXCL is what makes the kernel answer EEXIST for a link
 * rather than ELOOP: the name is asked about after the failure, so the caller
 * is told it is a link either way
 */
static void
test_safe_fopen_refuses_a_symlink_under_an_exclusive_mode(void **state)
{
        char target_buffer[PATH_MAX];
        char link_buffer[PATH_MAX];
        const char *target =
            work_path(target_buffer, sizeof(target_buffer), "excl_target.txt");
        const char *link =
            work_path(link_buffer, sizeof(link_buffer), "excl_link.txt");
        char content[16] = {0};
        FILE *stream;
        int error;

        UNUSED_ARG(state);

        unlink(link);
        stream = fopen(target, "w");
        assert_non_null(stream);
        assert_int_equal(fwrite("keep me\n", 1, 8, stream), 8);
        assert_int_equal(fclose(stream), 0);
        assert_int_equal(symlink(target, link), 0);
        errno = 0;

        run_function(safe_fopen, stream, link, "wx");
        error = errno;

        assert_null(stream);
        stream = fopen(target, "r");
        assert_non_null(stream);
        assert_non_null(fgets(content, sizeof(content), stream));
        assert_string_equal(content, "keep me\n");
        assert_int_equal(fclose(stream), 0);

        assert_int_equal(error, ELOOP);
        assert_int_equal(output_has_text("is a symlink"), 1);

        assert_int_equal(unlink(link), 0);
        assert_int_equal(unlink(target), 0);
}

/* fopen() puts an append stream at the end of the file and the descriptor
 * fdopen() is handed sits at the start, so the wrapper has to put it back.
 * Where "a+" belongs is the platform's to say - glibc leaves it at the start
 * for reading, FreeBSD seeks every stream it opened with O_APPEND - so each
 * mode is asked of the fopen() on this machine and the wrapper is held to that
 * answer, rather than one platform's being written down here.
 */
static void
test_safe_fopen_positions_an_append_stream_at_the_end(void **state)
{
        static const char *const modes[] = {"a", "ab", "a+", "a+b", "ab+"};
        char buffer[PATH_MAX];
        const char *path = work_path(buffer, sizeof(buffer), "appended.txt");
        char content[32] = {0};
        FILE *stream;
        size_t i;

        UNUSED_ARG(state);

        stream = fopen(path, "w");
        assert_non_null(stream);
        assert_int_equal(fwrite("0123456789", 1, 10, stream), 10);
        assert_int_equal(fclose(stream), 0);

        for (i = 0; i < DIM(modes); i++) {
                long position;

                /* none of these writes, so the size is the same for each */
                stream = fopen(path, modes[i]);
                assert_non_null(stream);
                position = ftell(stream);
                assert_int_equal(fclose(stream), 0);

                run_function(safe_fopen, stream, path, modes[i]);
                assert_non_null(stream);
                assert_int_equal(ftell(stream), position);
                assert_int_equal(fclose(stream), 0);
        }

        /* and a write lands at the end wherever the stream started */
        run_function(safe_fopen, stream, path, "a");
        assert_non_null(stream);
        assert_int_equal(fwrite("XY", 1, 2, stream), 2);
        assert_int_equal(fclose(stream), 0);

        stream = fopen(path, "r");
        assert_non_null(stream);
        assert_non_null(fgets(content, sizeof(content), stream));
        assert_string_equal(content, "0123456789XY");
        assert_int_equal(fclose(stream), 0);

        assert_int_equal(unlink(path), 0);
}

/* ======== pqos_filter_cpu / pqos_cpu_sort ======== */

/* The two are the scandir() callbacks for /sys/devices/system/cpu, so a
 * directory entry carrying the name is all they are given.
 */
static const struct dirent *
cpu_entry(struct dirent *entry, const char *name)
{
        memset(entry, 0, sizeof(*entry));
        snprintf(entry->d_name, sizeof(entry->d_name), "%s", name);

        return entry;
}

static int
filter_name(const char *name)
{
        struct dirent entry;

        return pqos_filter_cpu(cpu_entry(&entry, name));
}

static int
sort_names(const char *name1, const char *name2)
{
        struct dirent entry1;
        struct dirent entry2;
        const struct dirent *dir1 = cpu_entry(&entry1, name1);
        const struct dirent *dir2 = cpu_entry(&entry2, name2);

        return pqos_cpu_sort(&dir1, &dir2);
}

static void
test_filter_cpu_accepts_a_cpu_number(void **state)
{
        UNUSED_ARG(state);

        assert_int_equal(filter_name("cpu0"), 1);
        assert_int_equal(filter_name("cpu7"), 1);
        assert_int_equal(filter_name("cpu4294967295"), 1);
}

static void
test_filter_cpu_rejects_everything_else(void **state)
{
        UNUSED_ARG(state);

        /* the entries a "cpu[0-9]*" glob used to let through */
        assert_int_equal(filter_name("cpu0abc"), 0);
        assert_int_equal(filter_name("cpu1_2"), 0);

        /* the two scandir() calls the filter with first */
        assert_int_equal(filter_name("."), 0);
        assert_int_equal(filter_name(".."), 0);

        assert_int_equal(filter_name("cpu"), 0);
        assert_int_equal(filter_name("cpufreq"), 0);
        assert_int_equal(filter_name("cpuidle"), 0);
        assert_int_equal(filter_name("possible"), 0);
        assert_int_equal(filter_name("cpu-1"), 0);
        assert_int_equal(filter_name("cpu4294967296"), 0);
        assert_int_equal(filter_name("cpu99999999999999999999"), 0);

        /* the forms strtoul() would take for a number the kernel never writes
         * that way, each of which would be a second entry for a CPU that
         * already has one
         */
        assert_int_equal(filter_name("cpu+7"), 0);
        assert_int_equal(filter_name("cpu 7"), 0);
        assert_int_equal(filter_name("cpu7\n"), 0);
        assert_int_equal(filter_name("cpu0x10"), 0);
        assert_int_equal(filter_name("cpu0X10"), 0);
}

/* A leading zero is still a decimal number: base 0 would read cpu010 as octal
 * 8 and order it in front of cpu9.
 */
static void
test_cpu_names_are_read_as_decimal(void **state)
{
        UNUSED_ARG(state);

        assert_int_equal(filter_name("cpu010"), 1);
        assert_true(sort_names("cpu010", "cpu9") > 0);
        assert_true(sort_names("cpu9", "cpu010") < 0);
        assert_int_equal(sort_names("cpu010", "cpu10"), 0);
}

static void
test_cpu_sort_orders_by_number(void **state)
{
        UNUSED_ARG(state);

        /* by number rather than by name, so cpu2 comes before cpu10 */
        assert_true(sort_names("cpu2", "cpu10") < 0);
        assert_true(sort_names("cpu10", "cpu2") > 0);
        assert_int_equal(sort_names("cpu5", "cpu5"), 0);
}

/* The comparator returns int, so a difference of the two unsigned numbers is
 * only the answer while it stays inside int. Beyond that the sign is whatever
 * the conversion happens to produce.
 */
static void
test_cpu_sort_orders_numbers_beyond_int_range(void **state)
{
        UNUSED_ARG(state);

        assert_true(sort_names("cpu0", "cpu4000000000") < 0);
        assert_true(sort_names("cpu4000000000", "cpu0") > 0);
}

/* pqos_filter_cpu() keeps these away from the comparator, so this is about the
 * comparator being an ordering on its own: a name it cannot read a number out
 * of has one place, not the place a number nobody parsed would have given it.
 */
static void
test_cpu_sort_puts_unreadable_names_last(void **state)
{
        UNUSED_ARG(state);

        assert_true(sort_names("cpu1", "cpu0abc") < 0);
        assert_true(sort_names("cpu0abc", "cpu1") > 0);
        assert_true(sort_names("cpu0", "cpu0abc") < 0);

        /* two of them are ordered against each other by name */
        assert_true(sort_names("cpu0abc", "cpuxyz") < 0);
        assert_true(sort_names("cpuxyz", "cpu0abc") > 0);
        assert_int_equal(sort_names("cpu0abc", "cpu0abc"), 0);
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
            cmocka_unit_test(test_safe_fopen_reads_an_existing_file),
            cmocka_unit_test(test_safe_fopen_creates_and_appends),
            cmocka_unit_test(test_safe_fopen_keeps_a_symlink_target_intact),
            cmocka_unit_test(test_safe_fopen_refuses_a_dangling_symlink),
            cmocka_unit_test(
                test_safe_fopen_refuses_a_mode_it_cannot_translate),
            cmocka_unit_test(
                test_safe_fopen_refuses_a_malformed_mode_without_truncating),
            cmocka_unit_test(test_safe_fopen_creates_exclusively),
            cmocka_unit_test(test_safe_fopen_accepts_the_exclusive_variants),
            cmocka_unit_test(
                test_safe_fopen_refuses_a_symlink_under_an_exclusive_mode),
            cmocka_unit_test(
                test_safe_fopen_positions_an_append_stream_at_the_end),
            cmocka_unit_test(test_safe_fopen_closes_the_stream_on_exec),
            cmocka_unit_test(test_filter_cpu_accepts_a_cpu_number),
            cmocka_unit_test(test_filter_cpu_rejects_everything_else),
            cmocka_unit_test(test_cpu_names_are_read_as_decimal),
            cmocka_unit_test(test_cpu_sort_orders_by_number),
            cmocka_unit_test(test_cpu_sort_orders_numbers_beyond_int_range),
            cmocka_unit_test(test_cpu_sort_puts_unreadable_names_last),
        };

        return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
