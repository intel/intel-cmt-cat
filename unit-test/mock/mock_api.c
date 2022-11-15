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

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
/* clang-format off */
#include <setjmp.h>
#include <cmocka.h>
/* clang-format on */

#include "mock_api.h"

int
__wrap_pqos_init(const struct pqos_config *config)
{
        check_expected_ptr(config);
        return mock_type(int);
}

int
__wrap_pqos_fini(void)
{
        function_called();
        return mock_type(int);
}

int
__wrap_pqos_cap_get(const struct pqos_cap **cap,
                    const struct pqos_cpuinfo **cpu)
{
        int ret = mock_type(int);

        if (ret == PQOS_RETVAL_OK) {
                *cap = mock_ptr_type(struct pqos_cap *);
                *cpu = mock_ptr_type(struct pqos_cpuinfo *);
        }
        return ret;
}

int
__wrap_pqos_mon_reset(void)
{
        function_called();
        return mock_type(int);
}

int
__wrap_pqos_mon_assoc_get(const unsigned lcore, pqos_rmid_t *rmid)
{
        int ret = 1;

        check_expected(lcore);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *rmid = mock_type(pqos_rmid_t);
        return ret;
}

int
__wrap_pqos_mon_start(const unsigned num_cores,
                      const unsigned *cores,
                      const enum pqos_mon_event event,
                      void *context,
                      struct pqos_mon_data *group)
{
        check_expected(num_cores);
        check_expected_ptr(cores);
        check_expected(event);
        check_expected_ptr(context);
        check_expected_ptr(group);
        return mock_type(int);
}

int
__wrap_pqos_mon_start_pid(const pid_t pid,
                          const enum pqos_mon_event event,
                          void *context,
                          struct pqos_mon_data *group)
{
        check_expected(pid);
        check_expected(event);
        check_expected_ptr(context);
        check_expected_ptr(group);
        return mock_type(int);
}

int
__wrap_pqos_mon_start_pids(const unsigned num_pids,
                           const pid_t *pids,
                           const enum pqos_mon_event event,
                           void *context,
                           struct pqos_mon_data *group)
{
        check_expected(num_pids);
        check_expected_ptr(pids);
        check_expected(event);
        check_expected_ptr(context);
        check_expected_ptr(group);
        return mock_type(int);
}

int
__wrap_pqos_mon_add_pids(const unsigned num_pids,
                         const pid_t *pids,
                         struct pqos_mon_data *group)
{
        check_expected(num_pids);
        check_expected_ptr(pids);
        check_expected_ptr(group);
        return mock_type(int);
}

int
__wrap_pqos_mon_remove_pids(const unsigned num_pids,
                            const pid_t *pids,
                            struct pqos_mon_data *group)
{
        check_expected(num_pids);
        check_expected_ptr(pids);
        check_expected_ptr(group);
        return mock_type(int);
}

int
__wrap_pqos_mon_stop(struct pqos_mon_data *group)
{
        check_expected_ptr(group);
        return mock_type(int);
}

int
__wrap_pqos_mon_poll(struct pqos_mon_data **groups, const unsigned num_groups)
{
        int ret;

        check_expected(num_groups);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *groups = mock_ptr_type(struct pqos_mon_data *);
        return ret;
}

int
__wrap_pqos_alloc_assoc_set(const unsigned lcore, const unsigned class_id)
{
        check_expected(lcore);
        check_expected(class_id);
        return mock_type(int);
}

int
__wrap_pqos_alloc_assoc_get(const unsigned lcore, unsigned *class_id)
{
        int ret;

        check_expected(lcore);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *class_id = mock_type(unsigned);
        return ret;
}

int
__wrap_pqos_alloc_assoc_set_pid(const pid_t task, const unsigned class_id)
{
        check_expected(task);
        check_expected(class_id);
        return mock_type(int);
}

int
__wrap_pqos_alloc_assoc_get_pid(const pid_t task, unsigned *class_id)
{
        int ret;

        check_expected(task);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *class_id = mock_type(unsigned);
        return ret;
}

int
__wrap_pqos_alloc_assign(const unsigned technology,
                         const unsigned *core_array,
                         const unsigned core_num,
                         unsigned *class_id)
{
        int ret;

        check_expected(technology);
        check_expected_ptr(core_array);
        check_expected(core_num);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *class_id = mock_type(unsigned);
        return ret;
}

int
__wrap_pqos_alloc_release(const unsigned *core_array, const unsigned core_num)
{
        check_expected_ptr(core_array);
        check_expected(core_num);
        return mock_type(int);
}

int
__wrap_pqos_alloc_assign_pid(const unsigned technology,
                             const pid_t *task_array,
                             const unsigned task_num,
                             unsigned *class_id)
{
        int ret;

        check_expected(technology);
        check_expected_ptr(task_array);
        check_expected(task_num);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *class_id = mock_type(unsigned);
        return ret;
}

int
__wrap_pqos_alloc_release_pid(const pid_t *task_array, const unsigned task_num)
{
        check_expected_ptr(task_array);
        check_expected(task_num);
        return mock_type(int);
}

int
__wrap_pqos_alloc_reset(const enum pqos_cdp_config l3_cdp_cfg,
                        const enum pqos_cdp_config l2_cdp_cfg,
                        const enum pqos_mba_config mba_cfg)
{
        check_expected(l3_cdp_cfg);
        check_expected(l2_cdp_cfg);
        check_expected(mba_cfg);
        return mock_type(int);
}

int
__wrap_pqos_l3ca_set(const unsigned l3cat_id,
                     const unsigned num_cos,
                     const struct pqos_l3ca *ca)
{
        check_expected(l3cat_id);
        check_expected(num_cos);
        check_expected_ptr(ca);
        return mock_type(int);
}

int
__wrap_pqos_l3ca_get(const unsigned l3cat_id,
                     const unsigned max_num_ca,
                     unsigned *num_ca,
                     struct pqos_l3ca *ca)
{
        check_expected(l3cat_id);
        check_expected(max_num_ca);

        if (num_ca != NULL)
                *num_ca = mock_type(unsigned);
        if (ca != NULL)
                memcpy(ca, mock_ptr_type(struct pqos_l3ca *),
                       sizeof(struct pqos_l3ca) * *num_ca);
        return mock_type(int);
}

int
__wrap_pqos_l3ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
        int ret = mock_type(int);

        if (ret == PQOS_RETVAL_OK)
                *min_cbm_bits = mock_type(unsigned);
        return ret;
}

int
__wrap_pqos_l2ca_set(const unsigned l2id,
                     const unsigned num_cos,
                     const struct pqos_l2ca *ca)
{
        check_expected(l2id);
        check_expected(num_cos);
        check_expected_ptr(ca);
        return mock_type(int);
}

int
__wrap_pqos_l2ca_get(const unsigned l2id,
                     const unsigned max_num_ca,
                     unsigned *num_ca,
                     struct pqos_l2ca *ca)
{
        int ret;

        check_expected(l2id);
        check_expected(max_num_ca);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK) {
                *num_ca = mock_type(unsigned);
                if (ca != NULL)
                        memcpy(ca, mock_ptr_type(struct pqos_l2ca *),
                               sizeof(struct pqos_l2ca) * *num_ca);
        }
        return ret;
}

int
__wrap_pqos_l2ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
        int ret;

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *min_cbm_bits = mock_type(unsigned);

        return ret;
}

int
__wrap_pqos_mba_set(const unsigned mba_id,
                    const unsigned num_cos,
                    const struct pqos_mba *requested,
                    struct pqos_mba *actual)
{
        int ret;

        check_expected(mba_id);
        check_expected(num_cos);
        check_expected_ptr(requested);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK && actual != NULL)
                memcpy(actual, mock_ptr_type(struct pqos_mba *),
                       sizeof(struct pqos_mba));

        return ret;
}

int
__wrap_pqos_mba_get(const unsigned mba_id,
                    const unsigned max_num_cos,
                    unsigned *num_cos,
                    struct pqos_mba *mba_tab)
{
        check_expected(mba_id);
        check_expected(max_num_cos);

        if (num_cos != NULL)
                *num_cos = mock_type(unsigned);
        if (mba_tab != NULL)
                memcpy(mba_tab, mock_ptr_type(struct pqos_mba *),
                       sizeof(struct pqos_mba) * *num_cos);
        return mock_type(int);
}

unsigned *
__wrap_pqos_cpu_get_cores_l3cat_id(const struct pqos_cpuinfo *cpu,
                                   const unsigned l3cat_id,
                                   unsigned *count)
{
        unsigned *ret;

        check_expected_ptr(cpu);
        check_expected(l3cat_id);

        ret = mock_ptr_type(unsigned *);
        if (ret != NULL)
                *count = mock_type(unsigned);
        return ret;
}

unsigned *
__wrap_pqos_pid_get_pid_assoc(const unsigned class_id, unsigned *count)
{
        unsigned *ret;

        check_expected(class_id);

        ret = mock_ptr_type(unsigned *);
        if (ret != NULL)
                *count = mock_type(unsigned);
        return ret;
}

int
__wrap_pqos_inter_get(enum pqos_interface *interface)
{
        int ret = mock_type(int);

        if (ret == PQOS_RETVAL_OK)
                *interface = mock_type(enum pqos_interface);
        return ret;
}
