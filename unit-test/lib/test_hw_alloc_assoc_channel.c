/*
 * BSD LICENSE
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
 * All rights reserved.
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

#include "allocation.h"
#include "test.h"

/* ======== hw_alloc_assoc_get_channel ======== */

static void
test_hw_alloc_assoc_get_channel(void **state)
{
        int ret;
        unsigned class_id = 0;
        struct test_data *data = (struct test_data *)*state;
        const pqos_channel_t channel = data->dev->channels[0].channel_id;

        will_return(__wrap__pqos_get_sysconfig, data->sys);

        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_iordt_enabled, 1);

        expect_function_call(__wrap_iordt_assoc_read);
        expect_value(__wrap_iordt_assoc_read, channel, channel);
        expect_value(__wrap_iordt_assoc_read, class_id, &class_id);
        will_return(__wrap_iordt_assoc_read, PQOS_RETVAL_OK);
        will_return(__wrap_iordt_assoc_read, 5);

        ret = hw_alloc_assoc_get_channel(channel, &class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(class_id, 5);
}

static void
test_hw_alloc_assoc_get_channel_fail(void **state)
{
        int ret;
        unsigned class_id = 0;
        struct test_data *data = (struct test_data *)*state;
        const pqos_channel_t channel = data->dev->channels[0].channel_id;

        /* _pqos_get_sysconfig fails */
        will_return(__wrap__pqos_get_sysconfig, NULL);

        ret = hw_alloc_assoc_get_channel(channel, &class_id);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);

        /* pqos_l3ca_iordt_enabled fails */
        will_return(__wrap__pqos_get_sysconfig, data->sys);

        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_PARAM);

        ret = hw_alloc_assoc_get_channel(channel, &class_id);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);

        /* l3ca_iordt not enabled */
        will_return(__wrap__pqos_get_sysconfig, data->sys);

        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_iordt_enabled, 0);

        ret = hw_alloc_assoc_get_channel(channel, &class_id);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);

        /* iordt_assoc_read fails */
        will_return(__wrap__pqos_get_sysconfig, data->sys);

        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_iordt_enabled, 1);

        expect_function_call(__wrap_iordt_assoc_read);
        expect_value(__wrap_iordt_assoc_read, channel, channel);
        expect_value(__wrap_iordt_assoc_read, class_id, &class_id);
        will_return(__wrap_iordt_assoc_read, PQOS_RETVAL_ERROR);

        ret = hw_alloc_assoc_get_channel(channel, &class_id);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
}

/* ======== hw_alloc_assoc_set_channel ======== */

static void
test_hw_alloc_assoc_set_channel(void **state)
{
        int ret;
        unsigned class_id = 0;
        struct test_data *data = (struct test_data *)*state;
        const pqos_channel_t channel = data->dev->channels[0].channel_id;

        will_return(__wrap__pqos_get_sysconfig, data->sys);
        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_iordt_enabled, 1);

        expect_function_call(__wrap_iordt_assoc_write);
        expect_value(__wrap_iordt_assoc_write, channel, channel);
        expect_value(__wrap_iordt_assoc_write, class_id, class_id);
        will_return(__wrap_iordt_assoc_write, PQOS_RETVAL_OK);

        ret = hw_alloc_assoc_set_channel(channel, class_id);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_hw_alloc_assoc_set_channel_fail(void **state)
{
        int ret;
        unsigned class_id = 0;
        struct test_data *data = (struct test_data *)*state;
        const pqos_channel_t channel = data->dev->channels[0].channel_id;

        /* _pqos_get_sysconfig fails */
        will_return(__wrap__pqos_get_sysconfig, NULL);
        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        ret = hw_alloc_assoc_set_channel(channel, class_id);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);

        /* pqos_l3ca_iordt_enabled fails */
        will_return(__wrap__pqos_get_sysconfig, data->sys);

        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_RESOURCE);

        ret = hw_alloc_assoc_set_channel(channel, class_id);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);

        /* l3ca_iordt not enabled */
        will_return(__wrap__pqos_get_sysconfig, data->sys);

        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_iordt_enabled, 0);

        ret = hw_alloc_assoc_set_channel(channel, class_id);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);

        /* iordt_assoc_write fails */
        will_return(__wrap__pqos_get_sysconfig, data->sys);

        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_iordt_enabled, 1);

        expect_function_call(__wrap_iordt_assoc_write);
        expect_value(__wrap_iordt_assoc_write, channel, channel);
        expect_value(__wrap_iordt_assoc_write, class_id, class_id);
        will_return(__wrap_iordt_assoc_write, PQOS_RETVAL_PARAM);

        ret = hw_alloc_assoc_set_channel(channel, class_id);
        assert_int_not_equal(ret, PQOS_RETVAL_OK);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_hw_alloc_assoc_get_channel),
            cmocka_unit_test(test_hw_alloc_assoc_get_channel_fail),

            cmocka_unit_test(test_hw_alloc_assoc_set_channel),
            cmocka_unit_test(test_hw_alloc_assoc_set_channel_fail)};

        result += cmocka_run_group_tests(tests, test_init_l3ca, test_fini);

        return result;
}
