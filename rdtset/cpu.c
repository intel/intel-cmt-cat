/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2020 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "cpu.h"
#include "common.h"

int
parse_cpu(const char *cpustr)
{
        int ret = 0;
        const unsigned cpustr_len = strnlen(cpustr, MAX_OPTARG_LEN);

        if (cpustr_len == MAX_OPTARG_LEN)
                return -EINVAL;

        ret = str_to_cpuset(cpustr, cpustr_len, &g_cfg.cpu_aff_cpuset);
        return ret > 0 ? 0 : -EINVAL;
}

int
set_affinity(pid_t pid)
{
        int ret = -1;
        cpu_set_t cpumask;

        /* Set affinity */
#ifdef __linux__
        ret = sched_setaffinity(pid, sizeof(g_cfg.cpu_aff_cpuset),
                                &g_cfg.cpu_aff_cpuset);
#elif defined(__FreeBSD__)
        /* Current thread */
        if (0 == pid)
                ret = cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                                         sizeof(g_cfg.cpu_aff_cpuset),
                                         &g_cfg.cpu_aff_cpuset);
        /* Process via PID */
        else
                ret = cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, pid,
                                         sizeof(g_cfg.cpu_aff_cpuset),
                                         &g_cfg.cpu_aff_cpuset);
#endif
        if (ret != 0)
                return ret;

                /* Verify affinity settings */
#ifdef __linux__
        ret = sched_getaffinity(pid, sizeof(cpumask), &cpumask);
        if (ret != 0 || !CPU_EQUAL(&cpumask, &g_cfg.cpu_aff_cpuset))
                return -1;
#elif defined(__FreeBSD__)
        if (pid == 0)
                ret = cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                                         sizeof(cpumask), &cpumask);
        else
                ret = cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_PID, pid,
                                         sizeof(cpumask), &cpumask);
        if (ret != 0 || CPU_CMP(&cpumask, &g_cfg.cpu_aff_cpuset))
                return -1;
#endif

        return 0;
}

void
print_cmd_line_cpu_config(void)
{
        char cpustr[CPU_SETSIZE * 3] = {0};

        if (0 != CPU_COUNT(&g_cfg.cpu_aff_cpuset)) {
                cpuset_to_str(cpustr, sizeof(cpustr), &g_cfg.cpu_aff_cpuset);
                printf("Core Affinity: CPUs: %s\n", cpustr);
        }
}
