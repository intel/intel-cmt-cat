/*
 * BSD  LICENSE
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

#include "machine.h"
#include "test.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

/* the descriptor the wrapped open() gives an MSR device */
#define MSR_FD 1000

/*
 * An MSR device opens as the case queues it: MSR_FD, or -1 with the errno
 * queued after it. Nothing else in these cases opens a file.
 */
int
__wrap_open(const char *path, int flags __attribute__((unused)), ...)
{
        int fd;

        check_expected(path);
        fd = mock_type(int);
        if (fd < 0)
                errno = mock_type(int);
        return fd;
}

/* machine_fini() closing the device it opened */
int
__wrap_close(int fd)
{
        assert_int_equal(fd, MSR_FD);
        return 0;
}

/*
 * A read of the MSR device: the errno queued, or 0 and the value queued
 * after it, which the read gives as the register's
 */
ssize_t
__wrap_pread(int fd, void *buf, size_t count, off_t offset)
{
        int err;

        assert_int_equal(fd, MSR_FD);
        assert_int_equal(count, sizeof(uint64_t));
        check_expected(offset);

        err = mock_type(int);
        if (err != 0) {
                errno = err;
                return -1;
        }
        *(uint64_t *)buf = mock_type(uint64_t);
        return (ssize_t)count;
}

#ifdef __linux__
/*
 * The cases below fake Linux's MSR device, /dev/cpu/<n>/msr read with pread().
 * FreeBSD's is cpuctl's, /dev/cpuctl<n> read with ioctl(), which they do not
 * fake: there the binary runs none of them.
 */

static int
_init(void **state __attribute__((unused)))
{
        return machine_init(3) == MACHINE_RETVAL_OK ? 0 : -1;
}

static int
_fini(void **state __attribute__((unused)))
{
        return machine_fini() == MACHINE_RETVAL_OK ? 0 : -1;
}

/* core 2's device opens, and its read gives err (0 for the value) */
static void
_msr_queue(const uint32_t reg, const int err, const uint64_t value)
{
        expect_string(__wrap_open, path, "/dev/cpu/2/msr");
        will_return(__wrap_open, MSR_FD);
        expect_value(__wrap_pread, offset, (off_t)reg);
        will_return(__wrap_pread, err);
        if (err == 0)
                will_return(__wrap_pread, value);
}

static void
test_msr_probe_reads(void **state __attribute__((unused)))
{
        uint64_t value = 0;

        _msr_queue(0xC84, 0, 0x4);
        assert_int_equal(msr_probe(2, 0xC84, &value), MACHINE_RETVAL_OK);
        assert_int_equal(value, 0x4);
}

/* RDMSR faulted on the core, which the msr driver reports as EIO */
static void
test_msr_probe_refused(void **state __attribute__((unused)))
{
        uint64_t value = 0;

        _msr_queue(0xC84, EIO, 0);
        test_log_count_start();
        assert_int_equal(msr_probe(2, 0xC84, &value), MACHINE_RETVAL_FAULT);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the core went offline after its device was opened: the read says ENXIO,
 * which tells nothing about the register
 */
static void
test_msr_probe_core_offline(void **state __attribute__((unused)))
{
        uint64_t value = 0;

        _msr_queue(0xC84, ENXIO, 0);
        test_log_count_start();
        assert_int_equal(msr_probe(2, 0xC84, &value), MACHINE_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the device of the core cannot be opened, which is warned about */
static void
test_msr_probe_open_fails(void **state __attribute__((unused)))
{
        uint64_t value = 0;

        expect_string(__wrap_open, path, "/dev/cpu/2/msr");
        will_return(__wrap_open, -1);
        will_return(__wrap_open, ENXIO);
        test_log_count_start();
        assert_int_equal(msr_probe(2, 0xC84, &value), MACHINE_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
}

/*
 * msr_read() tells no failure from another, its callers checking for success,
 * and logs the failure as an error
 */
static void
test_msr_read_refused_is_error(void **state __attribute__((unused)))
{
        uint64_t value = 0;

        _msr_queue(0xC84, EIO, 0);
        test_log_count_start();
        assert_int_equal(msr_read(2, 0xC84, &value), MACHINE_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
}
#endif /* __linux__ */

int
main(void)
{
        int result = 0;

#ifdef __linux__
        const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_msr_probe_reads, _init, _fini),
            cmocka_unit_test_setup_teardown(test_msr_probe_refused, _init,
                                            _fini),
            cmocka_unit_test_setup_teardown(test_msr_probe_core_offline, _init,
                                            _fini),
            cmocka_unit_test_setup_teardown(test_msr_probe_open_fails, _init,
                                            _fini),
            cmocka_unit_test_setup_teardown(test_msr_read_refused_is_error,
                                            _init, _fini),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);
#endif

        return result;
}
