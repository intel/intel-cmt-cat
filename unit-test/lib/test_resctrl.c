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
            cmocka_unit_test(test_resctrl_cpumask_write),
            cmocka_unit_test(test_resctrl_cpumask_write_zero),
        };

        result += cmocka_run_group_tests(tests, NULL, NULL);

        return result;
}
