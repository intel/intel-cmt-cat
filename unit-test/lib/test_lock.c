/*
 * BSD LICENSE
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
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

#include "test_lock.h"

#include "lock.h"
#include "test.h"

#include <fcntl.h> /* O_CREAT */
#include <pthread.h>
#include <sys/stat.h> /* S_Ixxx */
#include <unistd.h>   /* usleep(), lockf() */

/* ======== mock ========*/

int
__wrap_pthread_mutex_init(pthread_mutex_t *restrict mutex,
                          const pthread_mutexattr_t *restrict attr
                          __attribute__((unused)))
{
        assert_non_null(mutex);
        function_called();
        return mock();
}

int
__wrap_pthread_mutex_destroy(pthread_mutex_t *mutex)
{
        assert_non_null(mutex);
        function_called();
        return mock();
};

int
__wrap_pthread_mutex_lock(pthread_mutex_t *mutex)
{
        assert_non_null(mutex);
        function_called();
        return mock();
};

int
__wrap_pthread_mutex_unlock(pthread_mutex_t *mutex)
{
        assert_non_null(mutex);
        function_called();
        return mock();
};

int
__wrap_open(const char *path, int oflags, int mode)
{
        if (strcmp(path, LOCKFILE))
                return __real_open(path, oflags, mode);

        function_called();
        check_expected(path);
        check_expected(oflags);
        check_expected(mode);

        return mock_type(int);
}

int
__wrap_close(int fildes)
{
        if (fildes != LOCKFILENO)
                return __real_close(fildes);

        function_called();
        check_expected(fildes);

        return mock_type(int);
}

int
__wrap_lockf(int fd, int cmd, off_t len)
{
        if (fd != LOCKFILENO)
                return __real_lockf(fd, cmd, len);

        function_called();
        check_expected(fd);
        check_expected(cmd);
        check_expected(len);
        return 0;
}

static void
test_lock_init_error(void **state __attribute__((unused)))
{
        int ret;

        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, -1);
        ret = lock_init();
        assert_int_equal(ret, -1);

        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, LOCKFILENO);
        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, -1);
        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, LOCKFILENO);
        will_return(__wrap_close, 0);
        ret = lock_init();
        assert_int_equal(ret, -1);
}

static void
test_lock_init_exit(void **state __attribute__((unused)))
{
        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, LOCKFILENO);
        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        assert_int_equal(lock_init(), 0);

        assert_int_equal(lock_init(), -1);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, LOCKFILENO);
        will_return(__wrap_close, 0);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);
        assert_int_equal(lock_fini(), 0);

        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, LOCKFILENO);
        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        assert_int_equal(lock_init(), 0);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, LOCKFILENO);
        will_return(__wrap_close, 0);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, -1);
        assert_int_equal(lock_fini(), -1);

        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, LOCKFILENO);
        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        assert_int_equal(lock_init(), 0);

        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, LOCKFILENO);
        will_return(__wrap_close, -1);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);
        assert_int_equal(lock_fini(), -1);
}

static void
test_lock_get(void **state __attribute__((unused)))
{
        /* init */
        expect_function_call(__wrap_open);
        expect_string(__wrap_open, path, LOCKFILE);
        expect_value(__wrap_open, oflags, O_WRONLY | O_CREAT);
        expect_value(__wrap_open, mode, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        will_return(__wrap_open, LOCKFILENO);
        expect_function_call(__wrap_pthread_mutex_init);
        will_return(__wrap_pthread_mutex_init, 0);
        assert_int_equal(lock_init(), 0);

        /* lock */
        expect_function_call(__wrap_lockf);
        expect_value(__wrap_lockf, fd, LOCKFILENO);
        expect_value(__wrap_lockf, cmd, F_LOCK);
        expect_value(__wrap_lockf, len, 0);
        expect_function_call(__wrap_pthread_mutex_lock);
        will_return(__wrap_pthread_mutex_lock, 0);
        lock_get();

        /* unlock */
        expect_function_call(__wrap_lockf);
        expect_value(__wrap_lockf, fd, LOCKFILENO);
        expect_value(__wrap_lockf, cmd, F_ULOCK);
        expect_value(__wrap_lockf, len, 0);
        expect_function_call(__wrap_pthread_mutex_unlock);
        will_return(__wrap_pthread_mutex_unlock, 0);
        lock_release();

        /* fini */
        expect_function_call(__wrap_close);
        expect_value(__wrap_close, fildes, LOCKFILENO);
        will_return(__wrap_close, 0);
        expect_function_call(__wrap_pthread_mutex_destroy);
        will_return(__wrap_pthread_mutex_destroy, 0);
        assert_int_equal(lock_fini(), 0);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_lock_init_error),
            cmocka_unit_test(test_lock_init_exit),
            cmocka_unit_test(test_lock_get),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
