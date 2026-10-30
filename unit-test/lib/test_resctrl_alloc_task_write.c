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

#include "resctrl_alloc.h"
#include "test.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

/* ======== mock ======== */

/* what a case's tasks file is given, and the errno that the kernel answers
 * the write with, which it makes when the stream is flushed at its close: 0
 * for a write that succeeds
 */
static char written[32];
static int write_errno;

static ssize_t
tasks_write(void *cookie __attribute__((unused)), const char *buf, size_t size)
{
        if (write_errno != 0) {
                errno = write_errno;
                return 0;
        }
        (void)snprintf(written, sizeof(written), "%.*s", (int)size, buf);
        return size;
}

/* class 1's tasks file, written through tasks_write() */
FILE *
resctrl_alloc_fopen(const unsigned class_id, const char *name, const char *mode)
{
        cookie_io_functions_t io = {.write = tasks_write};

        assert_int_equal(class_id, 1);
        assert_string_equal(name, "tasks");
        assert_string_equal(mode, "w");

        return fopencookie(NULL, mode, io);
}

static int
task_write_case_start(void **state __attribute__((unused)))
{
        written[0] = '\0';
        write_errno = 0;

        return 0;
}

/* ======== resctrl_alloc_assoc_set_pid ======== */

/* the task's ID is written to its new class's tasks file */
static void
test_resctrl_alloc_task_write(void **state __attribute__((unused)))
{
        char task[16];

        (void)snprintf(task, sizeof(task), "%d\n", (int)getpid());

        test_log_count_start();
        assert_int_equal(resctrl_alloc_assoc_set_pid(getpid(), 1),
                         PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
        assert_string_equal(written, task);
}

/* a task that ends between its check and the write is answered with ESRCH at
 * the close: the write says the task gone, which its callers take in their
 * stride, with no message at the default verbosity
 */
static void
test_resctrl_alloc_task_write_task_gone(void **state __attribute__((unused)))
{
        write_errno = ESRCH;

        test_log_count_start();
        assert_int_equal(resctrl_alloc_assoc_set_pid(getpid(), 1),
                         PQOS_RETVAL_PARAM);
        assert_int_equal(test_log_count_stop(), 0);
}

/* any other answer fails the write, with its message */
static void
test_resctrl_alloc_task_write_fails(void **state __attribute__((unused)))
{
        write_errno = EIO;

        test_log_keep_start();
        assert_int_equal(resctrl_alloc_assoc_set_pid(getpid(), 1),
                         PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
        assert_non_null(strstr(test_log_said(), "Error closing file!"));
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup(test_resctrl_alloc_task_write,
                                   task_write_case_start),
            cmocka_unit_test_setup(test_resctrl_alloc_task_write_task_gone,
                                   task_write_case_start),
            cmocka_unit_test_setup(test_resctrl_alloc_task_write_fails,
                                   task_write_case_start),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
