/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2026 Intel Corporation. All rights reserved.
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

#include "resctrl_schemata.h"
#include "test.h"

/* ======== mock ======== */

/* ======== resctrl_schemata_l2ca_get ======== */

static void
test_resctrl_schemata_l2ca_get_invalid_id(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l2ca ca;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_l2ca_get(schmt, 1000, &ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        resctrl_schemata_free(schmt);
}

/* ======== resctrl_schemata_l2ca_set ======== */

static void
test_resctrl_schemata_l2ca_set_invalid_id(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l2ca ca;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_l2ca_set(schmt, 1000, &ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        resctrl_schemata_free(schmt);
}

static void
test_resctrl_schemata_l2ca_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l2ca ca_set;
        struct pqos_l2ca ca_get;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ca_set.class_id = 1;
        ca_set.cdp = 0;
        ca_set.u.ways_mask = 0xf;

        ret = resctrl_schemata_l2ca_set(schmt, 0, &ca_set);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ret = resctrl_schemata_l2ca_get(schmt, 0, &ca_get);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca_set.class_id, ca_get.class_id);
        assert_int_equal(ca_set.cdp, ca_get.cdp);
        assert_int_equal(ca_set.u.ways_mask, ca_get.u.ways_mask);

        resctrl_schemata_free(schmt);
}

/* ======== resctrl_schemata_l3ca_get ======== */

static void
test_resctrl_schemata_l3ca_get_invalid_id(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l3ca ca;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_l3ca_get(schmt, 1000, &ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        resctrl_schemata_free(schmt);
}

/* ======== resctrl_schemata_l3ca_set ======== */

static void
test_resctrl_schemata_l3ca_set_invalid_id(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l3ca ca;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_l3ca_set(schmt, 1000, &ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        resctrl_schemata_free(schmt);
}

static void
test_resctrl_schemata_l3ca_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l3ca ca_set;
        struct pqos_l3ca ca_get;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ca_set.class_id = 1;
        ca_set.cdp = 0;
        ca_set.u.ways_mask = 0xf;

        ret = resctrl_schemata_l3ca_set(schmt, 0, &ca_set);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ret = resctrl_schemata_l3ca_get(schmt, 0, &ca_get);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca_set.class_id, ca_get.class_id);
        assert_int_equal(ca_set.cdp, ca_get.cdp);
        assert_int_equal(ca_set.u.ways_mask, ca_get.u.ways_mask);

        resctrl_schemata_free(schmt);
}

/* ======== resctrl_schemata_mba_get ======== */

static void
test_resctrl_schemata_mba_get_invalid_id(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_mba ca;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_mba_get(schmt, 1000, &ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        resctrl_schemata_free(schmt);
}

/* ======== resctrl_schemata_mba_set ======== */

static void
test_resctrl_schemata_mba_set_invalid_id(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_mba ca;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_mba_set(schmt, 1000, &ca);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);

        resctrl_schemata_free(schmt);
}

static void
test_resctrl_schemata_mba_set(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_mba ca_set;
        struct pqos_mba ca_get;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ca_set.class_id = 1;
        ca_set.mb_max = 50;
        ca_set.ctrl = 0;

        ret = resctrl_schemata_mba_set(schmt, 0, &ca_set);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ret = resctrl_schemata_mba_get(schmt, 0, &ca_get);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca_set.class_id, ca_get.class_id);
        assert_int_equal(ca_set.mb_max, ca_get.mb_max);
        assert_int_equal(ca_set.ctrl, ca_get.ctrl);

        resctrl_schemata_free(schmt);
}

/* ======== resctrl_schemata_read ======== */

static void
test_resctrl_schemata_read_l2(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l2ca ca;
        FILE *fd = tmpfile();

        /* init schemata file */
        assert_non_null(fd);
        fprintf(fd, "L2:0=f;1=ff;2=f0;3=1\n");
        fseek(fd, 0, SEEK_SET);

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_read(fd, schmt);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ret = resctrl_schemata_l2ca_get(schmt, 0, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca.cdp, 0);
        assert_int_equal(ca.u.ways_mask, 0xf);

        ret = resctrl_schemata_l2ca_get(schmt, 1, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca.cdp, 0);
        assert_int_equal(ca.u.ways_mask, 0xff);

        ret = resctrl_schemata_l2ca_get(schmt, 2, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca.cdp, 0);
        assert_int_equal(ca.u.ways_mask, 0xf0);

        ret = resctrl_schemata_l2ca_get(schmt, 3, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca.cdp, 0);
        assert_int_equal(ca.u.ways_mask, 0x1);

        resctrl_schemata_free(schmt);
        fclose(fd);
}

static void
test_resctrl_schemata_read_l2cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l2ca ca;
        FILE *fd = tmpfile();

        /* init schemata file */
        assert_non_null(fd);
        fprintf(fd, "L2DATA:0=f;1=ff;2=f0;3=1\n");
        fprintf(fd, "L2CODE:0=1;1=0xf;2=ff;3=2\n");
        fseek(fd, 0, SEEK_SET);

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_read(fd, schmt);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ret = resctrl_schemata_l2ca_get(schmt, 0, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca.cdp, 1);
        assert_int_equal(ca.u.s.data_mask, 0xf);
        assert_int_equal(ca.u.s.code_mask, 0x1);

        resctrl_schemata_free(schmt);
        fclose(fd);
}

static void
test_resctrl_schemata_read_l3(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l3ca ca;
        FILE *fd = tmpfile();

        /* init schemata file */
        assert_non_null(fd);
        fprintf(fd, "L3:0=f;1=ff\n");
        fseek(fd, 0, SEEK_SET);

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_read(fd, schmt);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ret = resctrl_schemata_l3ca_get(schmt, 0, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca.cdp, 0);
        assert_int_equal(ca.u.ways_mask, 0xf);

        ret = resctrl_schemata_l3ca_get(schmt, 1, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca.cdp, 0);
        assert_int_equal(ca.u.ways_mask, 0xff);

        resctrl_schemata_free(schmt);
        fclose(fd);
}

static void
test_resctrl_schemata_read_l3cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l3ca ca;
        FILE *fd = tmpfile();

        /* init schemata file */
        assert_non_null(fd);
        fprintf(fd, "L3DATA:0=f;1=ff\n");
        fprintf(fd, "L3CODE:0=1;1=0xf\n");
        fseek(fd, 0, SEEK_SET);

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_read(fd, schmt);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ret = resctrl_schemata_l3ca_get(schmt, 0, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca.cdp, 1);
        assert_int_equal(ca.u.s.data_mask, 0xf);
        assert_int_equal(ca.u.s.code_mask, 0x1);

        resctrl_schemata_free(schmt);
        fclose(fd);
}

static void
test_resctrl_schemata_read_mba(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_mba ca;
        FILE *fd = tmpfile();

        /* init schemata file */
        assert_non_null(fd);
        fprintf(fd, "MB:0=50;1=60\n");
        fseek(fd, 0, SEEK_SET);

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ret = resctrl_schemata_read(fd, schmt);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ret = resctrl_schemata_mba_get(schmt, 0, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca.mb_max, 50);

        ret = resctrl_schemata_mba_get(schmt, 1, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ca.mb_max, 60);

        resctrl_schemata_free(schmt);
        fclose(fd);
}

/* ======== resctrl_schemata_l3ca_write ======== */

static void
test_resctrl_schemata_l3ca_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l3ca ca;
        char buf[256];

        data->cap_l3ca.cdp = 0;
        data->cap_l3ca.cdp_on = 0;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ca.class_id = 1;
        ca.cdp = 0;

        ca.u.ways_mask = 0xf;
        ret = resctrl_schemata_l3ca_set(schmt, 0, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ca.u.ways_mask = 0xff;
        ret = resctrl_schemata_l3ca_set(schmt, 1, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        {
                FILE *fd = tmpfile();

                assert_non_null(fd);

                ret = resctrl_schemata_l3ca_write(fd, schmt);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                fseek(fd, 0, SEEK_SET);
                assert_string_equal(fgets(buf, DIM(buf), fd), "L3:0=f;1=ff\n");

                fclose(fd);
        }

        {
                FILE *fd = tmpfile();

                assert_non_null(fd);

                ret = resctrl_schemata_write(fd, schmt);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                fseek(fd, 0, SEEK_SET);
                assert_string_equal(fgets(buf, DIM(buf), fd), "L3:0=f;1=ff\n");

                fclose(fd);
        }

        resctrl_schemata_free(schmt);
}

static void
test_resctrl_schemata_l3ca_write_cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l3ca ca;
        char buf[256];
        FILE *fd = tmpfile();

        assert_non_null(fd);

        data->cap_l3ca.cdp = 1;
        data->cap_l3ca.cdp_on = 1;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ca.class_id = 1;
        ca.cdp = 1;

        ca.u.s.code_mask = 0xf0;
        ca.u.s.data_mask = 0xf;
        ret = resctrl_schemata_l3ca_set(schmt, 0, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ca.u.s.code_mask = 0xf0;
        ca.u.s.data_mask = 0xff;
        ret = resctrl_schemata_l3ca_set(schmt, 1, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ret = resctrl_schemata_l3ca_write(fd, schmt);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        fseek(fd, 0, SEEK_SET);
        assert_string_equal(fgets(buf, DIM(buf), fd), "L3CODE:0=f0;1=f0\n");
        assert_string_equal(fgets(buf, DIM(buf), fd), "L3DATA:0=f;1=ff\n");

        resctrl_schemata_free(schmt);
        fclose(fd);
}

/* ======== resctrl_schemata_l2ca_write ======== */

static void
test_resctrl_schemata_l2ca_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l2ca ca;
        char buf[256];

        data->cap_l2ca.cdp = 0;
        data->cap_l2ca.cdp_on = 0;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ca.class_id = 1;
        ca.cdp = 0;

        ca.u.ways_mask = 0xf;
        ret = resctrl_schemata_l2ca_set(schmt, 0, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ca.u.ways_mask = 0xff;
        ret = resctrl_schemata_l2ca_set(schmt, 1, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ca.u.ways_mask = 0xf0;
        ret = resctrl_schemata_l2ca_set(schmt, 2, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ca.u.ways_mask = 0x1;
        ret = resctrl_schemata_l2ca_set(schmt, 3, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        {
                FILE *fd = tmpfile();

                assert_non_null(fd);

                ret = resctrl_schemata_l2ca_write(fd, schmt);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                fseek(fd, 0, SEEK_SET);
                assert_string_equal(fgets(buf, DIM(buf), fd),
                                    "L2:0=f;1=ff;2=f0;3=1\n");

                fclose(fd);
        }

        {
                FILE *fd = tmpfile();

                assert_non_null(fd);

                ret = resctrl_schemata_write(fd, schmt);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                fseek(fd, 0, SEEK_SET);
                assert_string_equal(fgets(buf, DIM(buf), fd),
                                    "L2:0=f;1=ff;2=f0;3=1\n");

                fclose(fd);
        }

        resctrl_schemata_free(schmt);
}

static void
test_resctrl_schemata_l2ca_write_cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_l2ca ca;
        char buf[256];

        data->cap_l2ca.cdp = 1;
        data->cap_l2ca.cdp_on = 1;

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ca.class_id = 1;
        ca.cdp = 1;

        ca.u.s.code_mask = 0xf0;
        ca.u.s.data_mask = 0xf;
        ret = resctrl_schemata_l2ca_set(schmt, 0, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ca.u.s.code_mask = 0xf0;
        ca.u.s.data_mask = 0xff;
        ret = resctrl_schemata_l2ca_set(schmt, 1, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ca.u.s.code_mask = 0x1;
        ca.u.s.data_mask = 0x2;
        ret = resctrl_schemata_l2ca_set(schmt, 2, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ca.u.s.code_mask = 0x4;
        ca.u.s.data_mask = 0x8;
        ret = resctrl_schemata_l2ca_set(schmt, 3, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        {
                FILE *fd = tmpfile();

                assert_non_null(fd);

                ret = resctrl_schemata_l2ca_write(fd, schmt);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                fseek(fd, 0, SEEK_SET);
                assert_string_equal(fgets(buf, DIM(buf), fd),
                                    "L2CODE:0=f0;1=f0;2=1;3=4\n");
                assert_string_equal(fgets(buf, DIM(buf), fd),
                                    "L2DATA:0=f;1=ff;2=2;3=8\n");
                fclose(fd);
        }

        {
                FILE *fd = tmpfile();

                assert_non_null(fd);

                ret = resctrl_schemata_write(fd, schmt);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                fseek(fd, 0, SEEK_SET);
                assert_string_equal(fgets(buf, DIM(buf), fd),
                                    "L2CODE:0=f0;1=f0;2=1;3=4\n");
                assert_string_equal(fgets(buf, DIM(buf), fd),
                                    "L2DATA:0=f;1=ff;2=2;3=8\n");
                fclose(fd);
        }

        resctrl_schemata_free(schmt);
}

/* ======== resctrl_schemata_mba_write ======== */

static void
test_resctrl_schemata_mba_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        struct resctrl_schemata *schmt;
        struct pqos_mba ca;
        char buf[256];

        schmt = resctrl_schemata_alloc(data->cap, data->cpu);
        assert_non_null(schmt);

        ca.class_id = 1;
        ca.ctrl = 0;

        ca.mb_max = 50;
        ret = resctrl_schemata_mba_set(schmt, 0, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        ca.mb_max = 60;
        ret = resctrl_schemata_mba_set(schmt, 1, &ca);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        {
                FILE *fd = tmpfile();

                assert_non_null(fd);

                ret = resctrl_schemata_mba_write(fd, schmt);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                fseek(fd, 0, SEEK_SET);
                assert_string_equal(fgets(buf, DIM(buf), fd), "MB:0=50;1=60\n");

                fclose(fd);
        }

        {
                FILE *fd = tmpfile();

                assert_non_null(fd);

                ret = resctrl_schemata_write(fd, schmt);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                fseek(fd, 0, SEEK_SET);
                assert_string_equal(fgets(buf, DIM(buf), fd), "MB:0=50;1=60\n");

                fclose(fd);
        }

        resctrl_schemata_free(schmt);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {
            cmocka_unit_test(test_resctrl_schemata_l3ca_get_invalid_id),
            cmocka_unit_test(test_resctrl_schemata_l3ca_set_invalid_id),
            cmocka_unit_test(test_resctrl_schemata_l3ca_set),
            cmocka_unit_test(test_resctrl_schemata_read_l3),
            cmocka_unit_test(test_resctrl_schemata_read_l3cdp),
            cmocka_unit_test(test_resctrl_schemata_l3ca_write),
            cmocka_unit_test(test_resctrl_schemata_l3ca_write_cdp),
        };

        const struct CMUnitTest tests_l2ca[] = {
            cmocka_unit_test(test_resctrl_schemata_l2ca_get_invalid_id),
            cmocka_unit_test(test_resctrl_schemata_l2ca_set_invalid_id),
            cmocka_unit_test(test_resctrl_schemata_l2ca_set),
            cmocka_unit_test(test_resctrl_schemata_read_l2),
            cmocka_unit_test(test_resctrl_schemata_read_l2cdp),
            cmocka_unit_test(test_resctrl_schemata_l2ca_write),
            cmocka_unit_test(test_resctrl_schemata_l2ca_write_cdp),
        };

        const struct CMUnitTest tests_mba[] = {
            cmocka_unit_test(test_resctrl_schemata_mba_get_invalid_id),
            cmocka_unit_test(test_resctrl_schemata_mba_set_invalid_id),
            cmocka_unit_test(test_resctrl_schemata_mba_set),
            cmocka_unit_test(test_resctrl_schemata_read_mba),
            cmocka_unit_test(test_resctrl_schemata_mba_write),
        };

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);

        return result;
}
