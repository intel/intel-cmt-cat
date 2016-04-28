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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "rdt.h"

int
parse_cpu_set(const char *input, cpu_set_t *cpusetp)
{
	unsigned idx, min, max, bracket_used = 0;
	const char *str = input;
	char *end = NULL;

	CPU_ZERO(cpusetp);

	while (isblank(*str))
		str++;

	/* only digit or left bracket is qualify for start point */
	if ((!isdigit(*str) && *str != '(') || *str == '\0')
		return -1;

	if (*str == '(') {
		bracket_used = 1;
		str++;

		while (isblank(*str))
			str++;
	}

	if (*str == '\0')
		return -1;

	min = CPU_SETSIZE;
	do {
		/* go ahead to the first digit */
		while (isblank(*str))
			str++;

		if (!isdigit(*str))
			return -1;

		/* get the digit value */
		errno = 0;
		idx = strtoul(str, &end, 10);
		if (errno != 0 || end == NULL || end == str ||
				idx >= CPU_SETSIZE)
			return -1;

		/* go ahead to separator '-',',' and ')' */
		while (isblank(*end))
			end++;

		if (*end == '-') {
			if (min == CPU_SETSIZE)
				min = idx;
			else
				/* avoid continuous '-' */
				return -1;
		} else if (*end == ',' ||
				(bracket_used && (*end == ')')) ||
				(!bracket_used && (*end == 0))) {
			max = idx;

			if (min == CPU_SETSIZE)
				min = idx;

			for (idx = RDT_MIN(min, max); idx <= RDT_MAX(min, max);
					idx++)
				CPU_SET(idx, cpusetp);

			min = CPU_SETSIZE;
		} else
			return -1;

		str = end + 1;
	} while (*end != '\0' &&
			((bracket_used && *end != ')') ||
			(!bracket_used && *end != ',')));

	if (bracket_used)
		return end - input + 1;
	else
		return end - input;
}

void
cpuset_to_str(char *cpustr, const unsigned cpustr_len,
		const cpu_set_t *cpumask)
{
	unsigned len = 0;
	unsigned j = 0;

	memset(cpustr, 0, cpustr_len);

	/* Generate CPU list */
	for (j = 0; j < CPU_SETSIZE; j++) {
		if (CPU_ISSET(j, cpumask) != 1)
			continue;

		len += snprintf(cpustr + len, cpustr_len - len - 1,
				"%u,", j);

		if (len >= cpustr_len - 1) {
			len = cpustr_len;
			memcpy(cpustr + cpustr_len - 4, "...", 3);
			break;
		}
	}

	/* Remove trailing separator */
	cpustr[len-1] = 0;
}
