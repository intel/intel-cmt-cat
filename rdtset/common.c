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
str_to_cpuset(const char *cpustr, const unsigned cpustr_len, cpu_set_t *cpuset)
{
	unsigned idx, min, max;
	char *buff = malloc(cpustr_len + 1);
	char *end = NULL;
	const char *str = buff;

	memcpy(buff, cpustr, cpustr_len);
	buff[cpustr_len] = 0;
	CPU_ZERO(cpuset);

	while (isblank(*str))
		str++;

	/* only digit is qualify for start point */
	if (!isdigit(*str) || *str == '\0')
		return -EINVAL;

	min = CPU_SETSIZE;
	do {
		/* go ahead to the first digit */
		while (isblank(*str))
			str++;

		if (!isdigit(*str))
			return -EINVAL;

		/* get the digit value */
		errno = 0;
		idx = strtoul(str, &end, 10);
		if (errno != 0 || end == NULL || end == str ||
				idx >= CPU_SETSIZE)
			return -EINVAL;

		/* go ahead to separator '-',',' */
		while (isblank(*end))
			end++;

		if (*end == '-') {
			if (min == CPU_SETSIZE)
				min = idx;
			else
				/* avoid continuous '-' */
				return -EINVAL;
		} else if (*end == ',' || *end == 0) {
			max = idx;

			if (min == CPU_SETSIZE)
				min = idx;

			for (idx = MIN(min, max); idx <= MAX(min, max);
					idx++)
				CPU_SET(idx, cpuset);

			min = CPU_SETSIZE;
		} else
			return -EINVAL;

		str = end + 1;
	} while (*end != '\0');

	return end - buff;
}

void
cpuset_to_str(char *cpustr, const unsigned cpustr_len,
		const cpu_set_t *cpuset)
{
	unsigned len = 0, j = 0;

	memset(cpustr, 0, cpustr_len);

	/* Generate CPU list */
	for (j = 0; j < CPU_SETSIZE; j++) {
		if (CPU_ISSET(j, cpuset) != 1)
			continue;

		len += snprintf(cpustr + len, cpustr_len - len - 1, "%u,", j);

		if (len >= cpustr_len - 1) {
			len = cpustr_len;
			memcpy(cpustr + cpustr_len - 4, "...", 3);
			break;
		}
	}

	/* Remove trailing separator */
	cpustr[len-1] = 0;
}
