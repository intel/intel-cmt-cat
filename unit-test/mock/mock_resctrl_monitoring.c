/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2021 Intel Corporation. All rights reserved.
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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "mock_resctrl_monitoring.h"

int
__wrap_resctrl_mon_assoc_get(const unsigned lcore,
                             char *name,
                             const unsigned name_size)
{
        check_expected(lcore);
        assert_non_null(name);
        assert_int_not_equal(name_size, 0);

        return mock_type(int);
}

int
__wrap_resctrl_mon_assoc_set(const unsigned lcore, const char *name)
{
        check_expected(lcore);
        assert_non_null(name);

        return mock_type(int);
}

int
__wrap_resctrl_mon_assoc_get_pid(const pid_t task,
                                 char *name,
                                 const unsigned name_size)
{
        check_expected(task);
        assert_non_null(name);
        assert_int_not_equal(name_size, 0);

        return mock_type(int);
}

int
__wrap_resctrl_mon_assoc_set_pid(const pid_t task, const char *name)
{
        check_expected(task);
        assert_non_null(name);

        return mock_type(int);
}