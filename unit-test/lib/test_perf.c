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

#include "perf.h"
#include "test.h"

#include <errno.h>
#include <sys/syscall.h>

/* ======== mock ======== */

long __real_syscall(long number, ...);

/* perf_event_open() has no libc wrapper and is made through syscall(), so that
 * is what is mocked: the open answers the errno a case queues, or a descriptor
 * for 0, and every other call goes through
 */
long
__wrap_syscall(long number, ...)
{
        va_list ap;
        long arg[5];
        unsigned i;
        pid_t pid;
        int err;

        va_start(ap, number);
        for (i = 0; i < 5; i++)
                arg[i] = va_arg(ap, long);
        va_end(ap);

        if (number != __NR_perf_event_open)
                return __real_syscall(number, arg[0], arg[1], arg[2], arg[3],
                                      arg[4]);

        pid = (pid_t)arg[1];
        check_expected(pid);
        err = mock_type(int);
        if (err == 0)
                return 42;
        errno = err;
        return -1;
}

/* ======== perf_setup_counter ======== */

static void
test_perf_setup_counter(void **state __attribute__((unused)))
{
        struct perf_event_attr attr;
        int fd = -1;

        memset(&attr, 0, sizeof(attr));
        expect_value(__wrap_syscall, pid, 10);
        will_return(__wrap_syscall, 0);
        assert_int_equal(perf_setup_counter(&attr, 10, -1, -1, 0, &fd),
                         PQOS_RETVAL_OK);
        assert_int_equal(fd, 42);
}

static void
test_perf_setup_counter_task_ended(void **state __attribute__((unused)))
{
        struct perf_event_attr attr;
        int fd = -1;

        /* the task ended after it was listed: the caller skips it, and says
         * nothing a user would see
         */
        memset(&attr, 0, sizeof(attr));
        expect_value(__wrap_syscall, pid, 10);
        will_return(__wrap_syscall, ESRCH);
        test_log_count_start();
        assert_int_equal(perf_setup_counter(&attr, 10, -1, -1, 0, &fd),
                         PQOS_RETVAL_PARAM);
        assert_int_equal(test_log_count_stop(), 0);
        assert_int_equal(fd, -1);
}

static void
test_perf_setup_counter_core_esrch(void **state __attribute__((unused)))
{
        struct perf_event_attr attr;
        int fd = -1;

        /* a counter for a core names no task, so ESRCH ends no task */
        memset(&attr, 0, sizeof(attr));
        expect_value(__wrap_syscall, pid, -1);
        will_return(__wrap_syscall, ESRCH);
        assert_int_equal(perf_setup_counter(&attr, -1, 0, -1, 0, &fd),
                         PQOS_RETVAL_ERROR);
        assert_int_equal(fd, -1);
}

static void
test_perf_setup_counter_error(void **state __attribute__((unused)))
{
        struct perf_event_attr attr;
        int fd = -1;

        /* which an error is, and says so */
        memset(&attr, 0, sizeof(attr));
        expect_value(__wrap_syscall, pid, 10);
        will_return(__wrap_syscall, EACCES);
        test_log_count_start();
        assert_int_equal(perf_setup_counter(&attr, 10, -1, -1, 0, &fd),
                         PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
        assert_int_equal(fd, -1);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_perf_setup_counter),
            cmocka_unit_test(test_perf_setup_counter_task_ended),
            cmocka_unit_test(test_perf_setup_counter_core_esrch),
            cmocka_unit_test(test_perf_setup_counter_error),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
