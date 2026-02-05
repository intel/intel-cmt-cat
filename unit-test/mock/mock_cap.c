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

#include "mock_cap.h"

#include "mock_test.h"

int
__wrap__pqos_check_init(const int expect)
{
        check_expected(expect);

        return mock_type(int);
}

const struct pqos_sysconfig *
__wrap__pqos_get_sysconfig(void)
{
        return mock_ptr_type(const struct pqos_sysconfig *);
}

const struct pqos_cap *
__wrap__pqos_get_cap(void)
{
        return mock_ptr_type(const struct pqos_cap *);
}

const struct pqos_cpuinfo *
__wrap__pqos_get_cpu(void)
{
        return mock_ptr_type(const struct pqos_cpuinfo *);
}

const struct pqos_devinfo *
__wrap__pqos_get_dev(void)
{
        return mock_ptr_type(const struct pqos_devinfo *);
}

enum pqos_interface
__wrap__pqos_get_inter(void)
{
        return mock_type(enum pqos_interface);
}

void
__wrap__pqos_cap_l3cdp_change(const enum pqos_cdp_config cdp)
{
        check_expected(cdp);
}

void
__wrap__pqos_cap_l2cdp_change(const enum pqos_cdp_config cdp)
{
        check_expected(cdp);
}

void
__wrap__pqos_cap_mba_change(const enum pqos_mba_config cfg)
{
        check_expected(cfg);
}

int
__wrap_pqos_sysconfig_get(const struct pqos_sysconfig **sysconf)
{
        int ret;

        function_called();
        assert_non_null(sysconf);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *sysconf = mock_ptr_type(const struct pqos_sysconfig *);

        return ret;
}
