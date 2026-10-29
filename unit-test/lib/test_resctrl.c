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

#include "resctrl.h"
#include "test.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

/* ======== mock ======== */

int
__wrap_mount(const char *source,
             const char *target,
             const char *filesystemtype,
             unsigned long mountflags,
             const void *data)
{
        check_expected(source);
        check_expected(target);
        check_expected(filesystemtype);
        check_expected(mountflags);
        if (data != NULL)
                check_expected(data);
        else
                check_expected_ptr(data);

        return mock_type(int);
}

int
__wrap_umount2(const char *target, int flags)
{
        check_expected(target);
        assert_int_equal(flags, 0);

        return mock_type(int);
}

/* the directory a case has the resctrl lock taken on, by opening it for
 * RESCTRL_PATH, which a test host may not have; "" for none
 */
static char lock_dir[64];

typedef int open_fn(const char *path, int oflags, ...);
open_fn __real_open;

int
__wrap_open(const char *path, int oflags, ...)
{
        int mode = 0;

#ifdef O_TMPFILE
        if ((oflags & O_TMPFILE) == O_TMPFILE || (oflags & O_CREAT) != 0) {
#else
        if ((oflags & O_CREAT) != 0) {
#endif
                va_list args;

                va_start(args, oflags);
                mode = va_arg(args, int);
                va_end(args);
        }
        if (lock_dir[0] != '\0' && strcmp(path, RESCTRL_PATH) == 0)
                path = lock_dir;

        return __real_open(path, oflags, mode);
}

int __real_nanosleep(const struct timespec *req, struct timespec *rem);

/* what the waits of a case found and asked for: the lock free to be taken
 * by another during each, and the time each was asked to wait; the first
 * wait_interrupted waits end early, as a signal ends them, half way
 */
static unsigned waits;
static unsigned waits_lock_free;
static long wait_ns[4];
static unsigned waits_interrupted;
/* a case that has another take the lock during the wait, and keep it */
static int taker_fd = -1;

/* whether another open file description could take the lock now, of type */
static int
lock_free_for(const int type)
{
        const int fd = __real_open(lock_dir, O_DIRECTORY | O_CLOEXEC, 0);
        int free_now;

        assert_true(fd >= 0);
        free_now = flock(fd, type | LOCK_NB) == 0;
        if (free_now)
                (void)flock(fd, LOCK_UN);
        close(fd);

        return free_now;
}

int
__wrap_nanosleep(const struct timespec *req, struct timespec *rem)
{
        assert_non_null(req);
        assert_int_equal(req->tv_sec, 0);
        if (waits < DIM(wait_ns))
                wait_ns[waits] = req->tv_nsec;
        waits++;
        if (lock_free_for(LOCK_EX))
                waits_lock_free++;
        if (taker_fd == -2) {
                taker_fd = __real_open(lock_dir, O_DIRECTORY | O_CLOEXEC, 0);
                assert_true(taker_fd >= 0);
                assert_int_equal(flock(taker_fd, LOCK_EX | LOCK_NB), 0);
        }
        if (waits <= waits_interrupted) {
                assert_non_null(rem);
                rem->tv_sec = 0;
                rem->tv_nsec = req->tv_nsec / 2;
                errno = EINTR;
                return -1;
        }

        return 0;
}

static int
lock_dir_make(void **state)
{
        (void)state;
        snprintf(lock_dir, sizeof(lock_dir), "/tmp/pqos_ut_lock_XXXXXX");
        assert_non_null(mkdtemp(lock_dir));
        waits = 0;
        waits_lock_free = 0;
        waits_interrupted = 0;
        memset(wait_ns, 0, sizeof(wait_ns));
        taker_fd = -1;

        return 0;
}

static int
lock_dir_remove(void **state)
{
        (void)state;
        if (taker_fd >= 0)
                close(taker_fd);
        taker_fd = -1;
        rmdir(lock_dir);
        lock_dir[0] = '\0';

        return 0;
}

/* ======== resctrl_mount ======== */

static void
test_resctrl_mount_default(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_mount, source, "resctrl");
        expect_string(__wrap_mount, target, "/sys/fs/resctrl");
        expect_string(__wrap_mount, filesystemtype, "resctrl");
        expect_value(__wrap_mount, mountflags, 0);
        expect_value(__wrap_mount, data, NULL);
        will_return(__wrap_mount, 0);

        ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                            PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_string(__wrap_mount, source, "resctrl");
        expect_string(__wrap_mount, target, "/sys/fs/resctrl");
        expect_string(__wrap_mount, filesystemtype, "resctrl");
        expect_value(__wrap_mount, mountflags, 0);
        expect_value(__wrap_mount, data, NULL);
        will_return(__wrap_mount, -1);

        ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                            PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_resctrl_mount_l3cdp(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_mount, source, "resctrl");
        expect_string(__wrap_mount, target, "/sys/fs/resctrl");
        expect_string(__wrap_mount, filesystemtype, "resctrl");
        expect_value(__wrap_mount, mountflags, 0);
        expect_string(__wrap_mount, data, "cdp");
        will_return(__wrap_mount, 0);

        ret = resctrl_mount(PQOS_REQUIRE_CDP_ON, PQOS_REQUIRE_CDP_OFF,
                            PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mount_l2cdp(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_mount, source, "resctrl");
        expect_string(__wrap_mount, target, "/sys/fs/resctrl");
        expect_string(__wrap_mount, filesystemtype, "resctrl");
        expect_value(__wrap_mount, mountflags, 0);
        expect_string(__wrap_mount, data, "cdpl2");
        will_return(__wrap_mount, 0);

        ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_ON,
                            PQOS_MBA_DEFAULT);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_mount_mba_ctrl(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_mount, source, "resctrl");
        expect_string(__wrap_mount, target, "/sys/fs/resctrl");
        expect_string(__wrap_mount, filesystemtype, "resctrl");
        expect_value(__wrap_mount, mountflags, 0);
        expect_string(__wrap_mount, data, "mba_MBps");
        will_return(__wrap_mount, 0);

        ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF,
                            PQOS_MBA_CTRL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== resctrl_umount ======== */

static void
test_resctrl_umount(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_umount2, target, "/sys/fs/resctrl");
        will_return(__wrap_umount2, 0);

        ret = resctrl_umount();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_resctrl_umount_error(void **state __attribute__((unused)))
{
        int ret;

        expect_string(__wrap_umount2, target, "/sys/fs/resctrl");
        will_return(__wrap_umount2, -1);

        ret = resctrl_umount();
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== resctrl_cpumask_set ======== */

static void
test_resctrl_cpumask_set(void **state __attribute__((unused)))
{
        unsigned lcore = 5;
        struct resctrl_cpumask mask;
        unsigned i;

        memset(&mask, 0, sizeof(mask));

        resctrl_cpumask_set(lcore, &mask);
        for (i = 0; i < RESCTRL_MAX_CPUS; ++i)
                if (i == lcore)
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 1);
                else
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 0);
}

/* ======== resctrl_cpumask_unset ======== */

static void
test_resctrl_cpumask_unset(void **state __attribute__((unused)))
{
        unsigned lcore = 5;
        struct resctrl_cpumask mask;
        unsigned i;

        memset(&mask, 0xff, sizeof(mask));

        resctrl_cpumask_unset(lcore, &mask);
        for (i = 0; i < RESCTRL_MAX_CPUS; ++i)
                if (i == lcore)
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 0);
                else
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 1);
}

/* ======== resctrl_cpumask_read ======== */

static void
test_resctrl_cpumask_read(void **state __attribute__((unused)))
{
        int ret;
        struct resctrl_cpumask mask;
        FILE *fd = tmpfile();
        unsigned i;

        assert_non_null(fd);
        fprintf(fd, "0000,00000000,00001000,00000aA1\n");
        fseek(fd, 0, SEEK_SET);

        ret = resctrl_cpumask_read(fd, &mask);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        for (i = 0; i < RESCTRL_MAX_CPUS; ++i)
                if (i == 0 || i == 5 || i == 7 || i == 9 || i == 11 || i == 44)
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 1);
                else
                        assert_int_equal(resctrl_cpumask_get(i, &mask), 0);
        fclose(fd);
}

/* a mask of RESCTRL_MAX_CPUS, with its commas, is read whole: the first CPU
 * and the last
 */
static void
test_resctrl_cpumask_read_every_cpu(void **state __attribute__((unused)))
{
        struct resctrl_cpumask mask;
        FILE *fd = tmpfile();
        unsigned i;

        assert_non_null(fd);
        fprintf(fd, "80000000");
        for (i = 1; i < RESCTRL_MAX_CPUS / 32 - 1; i++)
                fprintf(fd, ",00000000");
        fprintf(fd, ",00000001\n");
        fseek(fd, 0, SEEK_SET);

        assert_int_equal(resctrl_cpumask_read(fd, &mask), PQOS_RETVAL_OK);
        for (i = 0; i < RESCTRL_MAX_CPUS; ++i)
                assert_int_equal(resctrl_cpumask_get(i, &mask),
                                 i == 0 || i == RESCTRL_MAX_CPUS - 1);
        fclose(fd);
}

/* a mask longer than one of RESCTRL_MAX_CPUS is refused, with its commas or
 * without them, rather than read in part
 */
static void
test_resctrl_cpumask_read_longer_than_the_mask(void **state
                                               __attribute__((unused)))
{
        struct resctrl_cpumask mask;
        FILE *fd = tmpfile();
        unsigned i;

        assert_non_null(fd);
        fprintf(fd, "1");
        for (i = 0; i < RESCTRL_MAX_CPUS / 32; i++)
                fprintf(fd, ",00000000");
        fprintf(fd, "\n");
        fseek(fd, 0, SEEK_SET);
        assert_int_equal(resctrl_cpumask_read(fd, &mask), PQOS_RETVAL_ERROR);
        fclose(fd);

        fd = tmpfile();
        assert_non_null(fd);
        fprintf(fd, "1");
        for (i = 0; i < RESCTRL_MAX_CPUS / 4; i++)
                fprintf(fd, "0");
        fprintf(fd, "\n");
        fseek(fd, 0, SEEK_SET);
        assert_int_equal(resctrl_cpumask_read(fd, &mask), PQOS_RETVAL_ERROR);
        fclose(fd);

        /* a file one character longer than a whole mask's, whose extra
         * character, a comma at its end, is not a digit
         */
        fd = tmpfile();
        assert_non_null(fd);
        fprintf(fd, "80000000");
        for (i = 1; i < RESCTRL_MAX_CPUS / 32; i++)
                fprintf(fd, ",00000000");
        fprintf(fd, ",\n");
        fseek(fd, 0, SEEK_SET);
        assert_int_equal(resctrl_cpumask_read(fd, &mask), PQOS_RETVAL_ERROR);
        fclose(fd);

        /* and a file longer than one of such a mask, which a read of that
         * length would cut, whatever digits its start has
         */
        fd = tmpfile();
        assert_non_null(fd);
        for (i = 0; i < RESCTRL_MAX_CPUS / 32; i++)
                fprintf(fd, "%s00000001", i == 0 ? "" : ",  ");
        fprintf(fd, "\n");
        fseek(fd, 0, SEEK_SET);
        assert_int_equal(resctrl_cpumask_read(fd, &mask), PQOS_RETVAL_ERROR);
        fclose(fd);
}

/* a core past the mask has no bit in it: it is not set, and setting it or
 * unsetting it leaves the mask as it was
 */
static void
test_resctrl_cpumask_core_past_the_mask(void **state __attribute__((unused)))
{
        struct resctrl_cpumask mask;
        struct resctrl_cpumask copy;

        memset(&mask, 0xff, sizeof(mask));
        memcpy(&copy, &mask, sizeof(copy));

        assert_int_equal(resctrl_cpumask_get(RESCTRL_MAX_CPUS, &mask), 0);
        resctrl_cpumask_unset(RESCTRL_MAX_CPUS, &mask);
        assert_memory_equal(&mask, &copy, sizeof(mask));

        memset(&mask, 0, sizeof(mask));
        memcpy(&copy, &mask, sizeof(copy));
        resctrl_cpumask_set(RESCTRL_MAX_CPUS, &mask);
        assert_memory_equal(&mask, &copy, sizeof(mask));
}

/* ======== resctrl_cpumask_write ======== */

static void
test_resctrl_cpumask_write(void **state __attribute__((unused)))
{
        int ret;
        struct resctrl_cpumask mask_write;
        struct resctrl_cpumask mask_read;
        FILE *fd = tmpfile();

        assert_non_null(fd);

        memset(&mask_write, 0, sizeof(mask_write));

        resctrl_cpumask_set(2, &mask_write);
        resctrl_cpumask_set(5, &mask_write);
        resctrl_cpumask_set(30, &mask_write);

        ret = resctrl_cpumask_write(fd, &mask_write);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        fseek(fd, 0, SEEK_SET);
        ret = resctrl_cpumask_read(fd, &mask_read);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_true(memcmp(&mask_read, &mask_write,
                           sizeof(struct resctrl_cpumask)) == 0);

        fclose(fd);
}

static void
test_resctrl_cpumask_write_zero(void **state __attribute__((unused)))
{
        int ret;
        struct resctrl_cpumask mask_write;
        struct resctrl_cpumask mask_read;
        FILE *fd = tmpfile();

        assert_non_null(fd);

        memset(&mask_write, 0, sizeof(mask_write));

        ret = resctrl_cpumask_write(fd, &mask_write);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        fseek(fd, 0, SEEK_SET);
        ret = resctrl_cpumask_read(fd, &mask_read);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_true(memcmp(&mask_read, &mask_write,
                           sizeof(struct resctrl_cpumask)) == 0);

        fclose(fd);
}

/* ======== resctrl_lock_yield ======== */

/* the lock is given back for the wait, so that another can take it then, and
 * held again, of its type, after it
 */
static void
test_resctrl_lock_yield_exclusive(void **state __attribute__((unused)))
{
        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);
        assert_false(lock_free_for(LOCK_SH));

        assert_int_equal(resctrl_lock_yield(100000000L), PQOS_RETVAL_OK);
        assert_int_equal(waits, 1);
        assert_int_equal(waits_lock_free, 1);
        assert_int_equal(wait_ns[0], 100000000L);
        assert_false(lock_free_for(LOCK_SH));

        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
        assert_true(lock_free_for(LOCK_EX));
}

static void
test_resctrl_lock_yield_shared(void **state __attribute__((unused)))
{
        assert_int_equal(resctrl_lock_shared(), PQOS_RETVAL_OK);

        assert_int_equal(resctrl_lock_yield(100000000L), PQOS_RETVAL_OK);
        assert_int_equal(waits_lock_free, 1);
        /* shared again: another can share it, and not have it alone */
        assert_true(lock_free_for(LOCK_SH));
        assert_false(lock_free_for(LOCK_EX));

        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

/* a signal that ends the wait early has the rest of it waited */
static void
test_resctrl_lock_yield_waits_the_rest_after_a_signal(void **state
                                                      __attribute__((unused)))
{
        waits_interrupted = 2;
        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);

        assert_int_equal(resctrl_lock_yield(100000000L), PQOS_RETVAL_OK);
        assert_int_equal(waits, 3);
        assert_int_equal(wait_ns[0], 100000000L);
        assert_int_equal(wait_ns[1], 50000000L);
        assert_int_equal(wait_ns[2], 25000000L);

        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_OK);
}

/* no lock held, nothing to give back: no wait either */
static void
test_resctrl_lock_yield_not_locked(void **state __attribute__((unused)))
{
        test_log_count_start();
        assert_int_equal(resctrl_lock_yield(100000000L), PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
        assert_int_equal(waits, 0);
}

/* another that takes the lock during the wait and keeps it: the lock is not
 * held again, each of the tries to take it timing out, and is not held after
 */
static void
test_resctrl_lock_yield_lock_kept_by_another(void **state
                                             __attribute__((unused)))
{
        taker_fd = -2;
        assert_int_equal(resctrl_lock_exclusive(), PQOS_RETVAL_OK);

        test_log_count_start();
        assert_int_equal(resctrl_lock_yield(100000000L), PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 3);

        test_log_count_start();
        assert_int_equal(resctrl_lock_release(), PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_resctrl_mount_default),
            cmocka_unit_test(test_resctrl_mount_l3cdp),
            cmocka_unit_test(test_resctrl_mount_l2cdp),
            cmocka_unit_test(test_resctrl_mount_mba_ctrl),
            cmocka_unit_test(test_resctrl_umount),
            cmocka_unit_test(test_resctrl_umount_error),
            cmocka_unit_test(test_resctrl_cpumask_set),
            cmocka_unit_test(test_resctrl_cpumask_unset),
            cmocka_unit_test(test_resctrl_cpumask_read),
            cmocka_unit_test(test_resctrl_cpumask_read_every_cpu),
            cmocka_unit_test(test_resctrl_cpumask_read_longer_than_the_mask),
            cmocka_unit_test(test_resctrl_cpumask_core_past_the_mask),
            cmocka_unit_test(test_resctrl_cpumask_write),
            cmocka_unit_test(test_resctrl_cpumask_write_zero),
        };

        const struct CMUnitTest tests_lock[] = {
            cmocka_unit_test_setup_teardown(test_resctrl_lock_yield_exclusive,
                                            lock_dir_make, lock_dir_remove),
            cmocka_unit_test_setup_teardown(test_resctrl_lock_yield_shared,
                                            lock_dir_make, lock_dir_remove),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_yield_waits_the_rest_after_a_signal,
                lock_dir_make, lock_dir_remove),
            cmocka_unit_test_setup_teardown(test_resctrl_lock_yield_not_locked,
                                            lock_dir_make, lock_dir_remove),
            cmocka_unit_test_setup_teardown(
                test_resctrl_lock_yield_lock_kept_by_another, lock_dir_make,
                lock_dir_remove),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);
        result += cmocka_run_group_tests(tests_lock, NULL, NULL);

        return result;
}
