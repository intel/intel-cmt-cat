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

#include "allocation.h"
#include "mock_cap.h"
#include "test.h"

/* the ERDT of the MMIO cases, where one is set */
static const struct pqos_erdt_info *mmio_erdt;

const struct pqos_erdt_info *
__wrap__pqos_get_erdt(void)
{
        return mmio_erdt;
}

/**
 * @brief The L3 CAT capability of the fixture, whose count is CPUID's
 *
 * @param [in] data the fixture
 *
 * @return the capability
 */
static const struct pqos_cap_l3ca *
fixture_l3ca(const struct test_data *data)
{
        unsigned i;

        for (i = 0; i < data->cap->num_cap; i++)
                if (data->cap->capabilities[i].type == PQOS_CAP_TYPE_L3CA)
                        return data->cap->capabilities[i].u.l3ca;

        return NULL;
}

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
        will_return_maybe(__wrap__pqos_get_inter, PQOS_INTER_MSR);

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

/* on the MMIO interface the class tags an I/O L3 CAT register the ERDT
 * describes, so the classes are the ERDT's, 0 through its Max CLOS: one more
 * than CPUID's here is accepted, and one past the ERDT's is refused
 */
static void
test_hw_alloc_assoc_set_channel_mmio_erdt_classes(void **state)
{
        int ret;
        struct test_data *data = (struct test_data *)*state;
        const pqos_channel_t channel = data->dev->channels[0].channel_id;
        const struct pqos_cap_l3ca *l3ca = fixture_l3ca(data);
        struct pqos_erdt_info erdt = {0};

        assert_non_null(l3ca);
        erdt.max_clos = l3ca->num_classes;
        mmio_erdt = &erdt;

        will_return(__wrap__pqos_get_sysconfig, data->sys);
        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_inter, PQOS_INTER_MMIO);

        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_iordt_enabled, 1);
        expect_function_call(__wrap_iordt_assoc_write);
        expect_value(__wrap_iordt_assoc_write, channel, channel);
        expect_value(__wrap_iordt_assoc_write, class_id, l3ca->num_classes);
        will_return(__wrap_iordt_assoc_write, PQOS_RETVAL_OK);

        ret = hw_alloc_assoc_set_channel(channel, l3ca->num_classes);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        /* the class past the ERDT's: refused before any write */
        will_return(__wrap__pqos_get_sysconfig, data->sys);
        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_iordt_enabled, 1);

        ret = hw_alloc_assoc_set_channel(channel, l3ca->num_classes + 1);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        mmio_erdt = NULL;
}

/* on the MSR interface the same table changes nothing: CPUID's classes */
static void
test_hw_alloc_assoc_set_channel_msr_cpuid_classes(void **state)
{
        int ret;
        struct test_data *data = (struct test_data *)*state;
        const pqos_channel_t channel = data->dev->channels[0].channel_id;
        const struct pqos_cap_l3ca *l3ca = fixture_l3ca(data);
        struct pqos_erdt_info erdt = {0};

        assert_non_null(l3ca);
        erdt.max_clos = l3ca->num_classes;
        mmio_erdt = &erdt;

        will_return(__wrap__pqos_get_sysconfig, data->sys);
        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_inter, PQOS_INTER_MSR);
        expect_function_call(__wrap_pqos_l3ca_iordt_enabled);
        expect_value(__wrap_pqos_l3ca_iordt_enabled, cap, data->sys->cap);
        will_return(__wrap_pqos_l3ca_iordt_enabled, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_iordt_enabled, 1);

        ret = hw_alloc_assoc_set_channel(channel, l3ca->num_classes);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        mmio_erdt = NULL;
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
        will_return_maybe(__wrap__pqos_get_inter, PQOS_INTER_MSR);

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
            cmocka_unit_test(test_hw_alloc_assoc_set_channel_mmio_erdt_classes),
            cmocka_unit_test(test_hw_alloc_assoc_set_channel_msr_cpuid_classes),
            cmocka_unit_test(test_hw_alloc_assoc_set_channel_fail)};

        result += cmocka_run_group_tests(tests, test_init_l3ca, test_fini);

        return result;
}
