/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
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

#ifndef _COMMON_H
#define _COMMON_H

#include "cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RDT_MAX_SOCKETS 8

/**
 * Macro to return the minimum of two numbers
 */
#define RDT_MIN(a, b) ({ \
	typeof(a) _a = (a); \
	typeof(b) _b = (b); \
	_a < _b ? _a : _b; \
})

/**
 * Macro to return the maximum of two numbers
 */
#define RDT_MAX(a, b) ({ \
	typeof(a) _a = (a); \
	typeof(b) _b = (b); \
	_a > _b ? _a : _b; \
})

int g_sudo_keep; /* keep elevated privileges */
int g_verbose; /* be verbose, print out additional logging information */
pid_t g_pid; /* process id while rdtset called with -p*/

/**
 * @brief Parse CPU set
 *
 * @note Parse elem, the elem could be single number/range or '(' ')' group
 *       1) A single number elem, it's just a simple digit. e.g. 9
 *       A single range elem, two digits with a '-' between. e.g. 2-6
 *       3) A group elem, combines multiple 1) or 2) with '( )'. e.g (0,2-4,6)
 *       Within group elem, '-' used for a range separator;
 *       ',' used for a single number.
 *
 * @param input set as a string
 * @param cpusetp parsed cpuset
 *
 * @return number of parsed characters on success
 * @retval -1 on error
 */
int parse_cpu_set(const char *input, cpu_set_t *cpusetp);

/**
 * @brief Converts CPU set (cpu_set_t) to string
 *
 * @param cpustr output string
 * @param cpustr_len max output string len
 * @param cpumask input cpuset
 */
void cpuset_to_str(char *cpustr, const unsigned cpustr_len,
		const cpu_set_t *cpumask);

#ifdef __cplusplus
}
#endif

#endif /* _COMMON_H */
