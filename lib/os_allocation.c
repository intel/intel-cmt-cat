/*
 * BSD LICENSE
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.O
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <pqos.h>
#include "allocation.h"
#include "cap.h"
#include "log.h"
#include "types.h"

/*
 * Path to resctrl file system
 */
static const char *rctl_path = "/sys/fs/resctrl/";


/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cap *m_cap = NULL;
static const struct pqos_cpuinfo *m_cpu = NULL;

/**
 * @brief Function to find the maximum number of resctrl groups allowed
 *
 * @param cap pqos capabilities structure
 * @param num_rctl_grps place to store result
 * @return Operational Status
 */
static int
os_get_max_rctl_grps(const struct pqos_cap *cap,
		     unsigned *num_rctl_grps)
{
	unsigned i;
	unsigned max_rctl_grps = 0;
	int ret = PQOS_RETVAL_OK;

	ASSERT(cap != NULL);
	ASSERT(num_rctl_grps != NULL);

	/*
	 * Loop through all caps that have OS support
	 * Find max COS supported by all
	 */
	for (i = 0; i < cap->num_cap; i++) {
		unsigned num_cos = 0;
		const struct pqos_capability *p_cap = &cap->capabilities[i];

		if (!p_cap->os_support)
			continue;

		/* get L3 CAT COS num */
		if (p_cap->type == PQOS_CAP_TYPE_L3CA) {
			ret = pqos_l3ca_get_cos_num(cap, &num_cos);
			if (ret != PQOS_RETVAL_OK)
				return ret;

			if (max_rctl_grps == 0)
				max_rctl_grps = num_cos;
			else if (num_cos < max_rctl_grps)
				max_rctl_grps = num_cos;
		}
		/* get L2 CAT COS num */
		if (p_cap->type == PQOS_CAP_TYPE_L2CA) {
			ret = pqos_l2ca_get_cos_num(cap, &num_cos);
			if (ret != PQOS_RETVAL_OK)
				return ret;

			if (max_rctl_grps == 0)
				max_rctl_grps = num_cos;
			else if (num_cos < max_rctl_grps)
				max_rctl_grps = num_cos;
		}
	}
	*num_rctl_grps = max_rctl_grps;
	return PQOS_RETVAL_OK;
}

int os_alloc_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
	unsigned i, num_grps = 0;
	int ret;

	if (cpu == NULL || cap == NULL)
		return PQOS_RETVAL_PARAM;

	m_cap = cap;
	m_cpu = cpu;

	ret = os_get_max_rctl_grps(cap, &num_grps);
	if (ret != PQOS_RETVAL_OK)
		return ret;
        /*
         * Detect/Create all available COS resctrl groups
         */
	for (i = 1; i < num_grps; i++) {
		char buf[128];
		struct stat st;

		memset(buf, 0, sizeof(buf));
		if (snprintf(buf, sizeof(buf) - 1,
                             "%sCOS%d", rctl_path, (int) i) < 0)
			return PQOS_RETVAL_ERROR;

		/* if resctrl group doesn't exist - create it */
		if (stat(buf, &st) == 0) {
			LOG_DEBUG("resctrl group COS%d detected\n", i);
			continue;
		}

		if (mkdir(buf, 0755) == -1) {
			LOG_DEBUG("Failed to create resctrl group %s!\n", buf);
			return PQOS_RETVAL_BUSY;
		}
		LOG_DEBUG("resctrl group COS%d created\n", i);
	}
	return PQOS_RETVAL_OK;
}
