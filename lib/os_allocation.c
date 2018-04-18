/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2018 Intel Corporation. All rights reserved.
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
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>             /**< scandir() */

#include "pqos.h"
#include "os_allocation.h"
#include "cap.h"
#include "log.h"
#include "types.h"
#include "resctrl.h"
#include "resctrl_alloc.h"
#include "resctrl_monitoring.h"

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cap *m_cap = NULL;
static const struct pqos_cpuinfo *m_cpu = NULL;

/**
 * @brief Function to mount the resctrl file system with CDP option
 *
 * @param l3_cdp_cfg L3 CDP option
 * @param l2_cdp_cfg L2 CDP option
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
os_interface_mount(const enum pqos_cdp_config l3_cdp_cfg,
                   const enum pqos_cdp_config l2_cdp_cfg)
{
        const struct pqos_cap_l3ca *l3_cap = NULL;
        const struct pqos_cap_l2ca *l2_cap = NULL;
        const struct pqos_capability *alloc_cap = NULL;

        if ((l3_cdp_cfg != PQOS_REQUIRE_CDP_ON &&
            l3_cdp_cfg != PQOS_REQUIRE_CDP_OFF) || (
            l2_cdp_cfg != PQOS_REQUIRE_CDP_ON &&
            l2_cdp_cfg != PQOS_REQUIRE_CDP_OFF)) {
                LOG_ERROR("Invalid CDP mounting setting %d!\n",
                          l3_cdp_cfg);
                return PQOS_RETVAL_PARAM;
        }

        if (l3_cdp_cfg == PQOS_REQUIRE_CDP_OFF &&
                l2_cdp_cfg == PQOS_REQUIRE_CDP_OFF)
                goto mount;

        /* Get L3 CAT capabilities */
        (void) pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &alloc_cap);
        if (alloc_cap != NULL)
                l3_cap = alloc_cap->u.l3ca;

        /* Get L2 CAT capabilities */
        (void) pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L2CA, &alloc_cap);
        if (alloc_cap != NULL)
                l2_cap = alloc_cap->u.l2ca;

        if (l3_cap != NULL && !l3_cap->os_cdp &&
                l3_cdp_cfg == PQOS_REQUIRE_CDP_ON) {
                /* Check against erroneous CDP request */
                LOG_ERROR("L3 CDP requested but not supported by the "
                          "platform!\n");
                return PQOS_RETVAL_PARAM;
        }
        if (l2_cap != NULL && !l2_cap->os_cdp &&
                l2_cdp_cfg == PQOS_REQUIRE_CDP_ON) {
                /* Check against erroneous CDP request */
                LOG_ERROR("L2 CDP requested but not supported by the "
                          "platform!\n");
                return PQOS_RETVAL_PARAM;
        }

 mount:
        return resctrl_mount(l3_cdp_cfg, l2_cdp_cfg);
}

/**
 * @brief Check to see if resctrl is supported.
 *        If it is attempt to mount the file system.
 *
 * @return Operational status
 */
static int
os_alloc_check(void)
{
        int ret;
        unsigned i, supported = 0;

        /**
         * Check if resctrl is supported
         */
        for (i = 0; i < m_cap->num_cap; i++) {
                if (m_cap->capabilities[i].os_support == 1) {
                        if (m_cap->capabilities[i].type == PQOS_CAP_TYPE_L3CA)
                                supported = 1;
                        if (m_cap->capabilities[i].type == PQOS_CAP_TYPE_L2CA)
                                supported = 1;
                        if (m_cap->capabilities[i].type == PQOS_CAP_TYPE_MBA)
                                supported = 1;
                }
        }

        if (!supported)
                return PQOS_RETVAL_OK;
        /**
         * Check if resctrl is mounted
         */
        if (access(RESCTRL_PATH"/cpus", F_OK) != 0) {
                const struct pqos_capability *alloc_cap = NULL;
                int l3_cdp_mount = PQOS_REQUIRE_CDP_OFF;
                int l2_cdp_mount = PQOS_REQUIRE_CDP_OFF;

                /* Get L3 CAT capabilities */
                (void) pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &alloc_cap);
                if (alloc_cap != NULL)
                        l3_cdp_mount = alloc_cap->u.l3ca->cdp_on;

                /* Get L2 CAT capabilities */
                (void) pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L2CA, &alloc_cap);
                if (alloc_cap != NULL)
                        l2_cdp_mount = alloc_cap->u.l2ca->cdp_on;

                ret = os_interface_mount(l3_cdp_mount, l2_cdp_mount);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_INFO("Unable to mount resctrl\n");
                        return PQOS_RETVAL_RESOURCE;
                }
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Prepares and authenticates resctrl file system
 *        used for OS allocation interface
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
os_alloc_prep(void)
{
        unsigned i, num_grps = 0;
        int ret;

        ASSERT(m_cap != NULL);
        ret = resctrl_alloc_get_grps_num(m_cap, &num_grps);
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
                             "%s/COS%d", RESCTRL_PATH, (int) i) < 0)
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

int
os_alloc_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
	int ret;

        if (cpu == NULL || cap == NULL)
		return PQOS_RETVAL_PARAM;

	m_cap = cap;
	m_cpu = cpu;

        ret = os_alloc_check();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = resctrl_alloc_init(cpu, cap);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = os_alloc_prep();

        return ret;
}

int
os_alloc_fini(void)
{
        int ret = PQOS_RETVAL_OK;

        m_cap = NULL;
        m_cpu = NULL;
        return ret;
}

int
os_alloc_assoc_set(const unsigned lcore, const unsigned class_id)
{
	int ret;
	unsigned grps;

        ASSERT(m_cpu != NULL);
	ASSERT(m_cap != NULL);

        ret = pqos_cpu_check_core(m_cpu, lcore);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        ret = resctrl_alloc_get_grps_num(m_cap, &grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

	if (class_id >= grps)
		/* class_id is out of bounds */
		return PQOS_RETVAL_PARAM;

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                return ret;

	ret = resctrl_alloc_assoc_set(lcore, class_id);

        resctrl_lock_release();

	return ret;
}

int
os_alloc_assoc_get(const unsigned lcore, unsigned *class_id)
{
        int ret;

        ASSERT(class_id != NULL);
        ASSERT(m_cpu != NULL);

        ret = pqos_cpu_check_core(m_cpu, lcore);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_PARAM;

        ret = resctrl_lock_shared();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = resctrl_alloc_assoc_get(lcore, class_id);

        resctrl_lock_release();

	return ret;
}

unsigned *
os_pid_get_pid_assoc(const unsigned class_id, unsigned *count)
{
        unsigned grps;
        int ret;
        unsigned *tasks;

	ASSERT(m_cap != NULL);
        ASSERT(count != NULL);
        ret = resctrl_alloc_get_grps_num(m_cap, &grps);
	if (ret != PQOS_RETVAL_OK)
		return NULL;

	if (class_id >= grps)
		/* class_id is out of bounds */
		return NULL;

        ret = resctrl_lock_shared();
        if (ret != PQOS_RETVAL_OK)
                return NULL;

	tasks = resctrl_alloc_task_read(class_id, count);

        resctrl_lock_release();

        return tasks;
}

/**
 * @brief Gets unused COS
 *
 * The lowest acceptable COS is 1, as 0 is a default one
 *
 * @param [in] hi_class_id highest acceptable COS id
 * @param [out] class_id unused COS
 *
 * @return Operation status
 */
static int
get_unused_cos(const unsigned hi_class_id,
               unsigned *class_id)
{
        unsigned used_classes[hi_class_id + 1];
        unsigned i, cos;
        int ret;

        if (class_id == NULL)
                return PQOS_RETVAL_PARAM;

        memset(used_classes, 0, sizeof(used_classes));

        for (i = hi_class_id; i != 0; i--) {
                struct resctrl_cpumask mask;
                unsigned j;

                ret = resctrl_alloc_cpumask_read(i, &mask);
                if (ret != PQOS_RETVAL_OK)
			return ret;

                for (j = 0; j < sizeof(mask.tab); j++)
                        if (mask.tab[j] > 0) {
                                used_classes[i] = 1;
                                break;
                        }

                if (used_classes[i] == 1)
                        continue;

                ret = resctrl_alloc_task_file_check(i, &used_classes[i]);
                if (ret != PQOS_RETVAL_OK)
			return ret;
        }

        /* Find unused COS */
        for (cos = hi_class_id; cos != 0; cos--) {
                if (used_classes[cos] == 0) {
                        *class_id = cos;
                        return PQOS_RETVAL_OK;
                }
        }

        return PQOS_RETVAL_RESOURCE;
}

int
os_alloc_assign(const unsigned technology,
                const unsigned *core_array,
                const unsigned core_num,
                unsigned *class_id)
{
        unsigned i, num_rctl_grps = 0;
        int ret;

        ASSERT(core_num > 0);
        ASSERT(core_array != NULL);
        ASSERT(class_id != NULL);
        ASSERT(m_cap != NULL);
        UNUSED_PARAM(technology);

        /* obtain highest class id for all requested technologies */
        ret = resctrl_alloc_get_grps_num(m_cap, &num_rctl_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (num_rctl_grps == 0)
                return PQOS_RETVAL_ERROR;

        /* find an unused class from highest down */
        ret = get_unused_cos(num_rctl_grps - 1, class_id);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* assign cores to the unused class */
        for (i = 0; i < core_num; i++) {
                ret = os_alloc_assoc_set(core_array[i], *class_id);
                if (ret != PQOS_RETVAL_OK)
                        return ret;
        }

        return ret;
}

int
os_alloc_release(const unsigned *core_array, const unsigned core_num)
{
        int ret;
        unsigned i, cos0 = 0;
        struct resctrl_cpumask mask;

	ASSERT(m_cpu != NULL);
        ASSERT(core_num > 0 && core_array != NULL);

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /**
         * Set the CPU assoc back to COS0
         */
        ret = resctrl_alloc_cpumask_read(cos0, &mask);
        if (ret != PQOS_RETVAL_OK)
                goto os_alloc_release_unlock;
        for (i = 0; i < core_num; i++) {
		if (core_array[i] >= m_cpu->num_cores) {
			ret = PQOS_RETVAL_ERROR;
                        goto os_alloc_release_unlock;
                }
                resctrl_cpumask_set(core_array[i], &mask);
	}

        ret = resctrl_alloc_cpumask_write(cos0, &mask);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("CPU assoc reset failed\n");

 os_alloc_release_unlock:
        resctrl_lock_release();

        return ret;
}

/**
 * @brief Moves all cores to COS0 (default)
 *
 * @return Operation status
 */
static int
os_alloc_reset_cores(void)
{
	const unsigned default_cos = 0;
	struct resctrl_cpumask mask;
	unsigned i;
	int ret;

	LOG_INFO("OS alloc reset - core assoc\n");

	ret = resctrl_alloc_cpumask_read(default_cos, &mask);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	for (i = 0; i < m_cpu->num_cores; i++)
		resctrl_cpumask_set(m_cpu->cores[i].lcore, &mask);

	ret = resctrl_alloc_cpumask_write(default_cos, &mask);
	if (ret != PQOS_RETVAL_OK)
		LOG_ERROR("Core assoc reset failed\n");

	return ret;
}

/**
 * @brief Resets L3, L2 and MBA schematas to default value
 *
 * @param [in] l3_cap l3 cache capability
 * @param [in] l2_cap l2 cache capability
 *
 * @return Operation status
 */
static int
os_alloc_reset_schematas(const struct pqos_cap_l3ca *l3_cap,
		     const struct pqos_cap_l2ca *l2_cap){

	const unsigned default_mba = 100;
	uint64_t default_l3ca = 0;
	uint64_t default_l2ca = 0;
	unsigned grps;
	unsigned i, j;
	int ret;

	LOG_INFO("OS alloc reset - schematas\n");

	ret = resctrl_lock_exclusive();
	if (ret != PQOS_RETVAL_OK)
		return ret;

	ret = resctrl_alloc_get_grps_num(m_cap, &grps);
	if (ret != PQOS_RETVAL_OK)
		goto os_alloc_reset_light_schematas_exit;

	/**
	 * Obtain default L2/L3 CAT mask
	 */
	if (l3_cap != NULL)
		default_l3ca = ((uint64_t)1 << l3_cap->num_ways) - 1;

	if (l2_cap != NULL)
		default_l2ca = ((uint64_t)1 << l2_cap->num_ways) - 1;

	/**
	 * Reset schematas
	 */
	for (i = 0; i < grps; i++) {
		struct resctrl_alloc_schemata schmt;

		ret = resctrl_alloc_schemata_init(i, m_cap, m_cpu, &schmt);
		if (ret != PQOS_RETVAL_OK) {
			LOG_ERROR("Error on schemata init "
				  "for resctrl group %u\n", i);
			goto os_alloc_reset_light_schematas_exit;
		}

		for (j = 0; j < schmt.l3ca_num; j++) {
			 /* l3ca_num should be greater
			  * than 0 if l3_cap is provided */
			ASSERT(l3_cap != NULL);
			if (schmt.l3ca[j].cdp) {
				schmt.l3ca[j].u.s.code_mask = default_l3ca;
				schmt.l3ca[j].u.s.data_mask = default_l3ca;
			} else
				schmt.l3ca[j].u.ways_mask = default_l3ca;
		}

		for (j = 0; j < schmt.l2ca_num; j++) {
			/* l2ca_num should be greater
			 * than 0 if l2_cap is provided */
			ASSERT(l2_cap != NULL);
			if (schmt.l2ca[j].cdp) {
				schmt.l2ca[j].u.s.code_mask = default_l2ca;
				schmt.l2ca[j].u.s.data_mask = default_l2ca;
			} else
				schmt.l2ca[j].u.ways_mask = default_l2ca;
		}

		for (j = 0; j < schmt.mba_num; j++)
			schmt.mba[j].mb_rate = default_mba;

		ret = resctrl_alloc_schemata_write(i, &schmt);

		resctrl_alloc_schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK) {
			LOG_ERROR("Error on schemata write "
				  "for resctrl group %u\n", i);
			goto os_alloc_reset_light_schematas_exit;
		}
	}

 os_alloc_reset_light_schematas_exit:
	if (ret != PQOS_RETVAL_OK)
		resctrl_lock_release();
	else
		ret = resctrl_lock_release();

	return ret;
}

/**
 * @brief filter for scandir.
 *
 * Skips entries starting with "."
 * Skips non-numeric entries
 *
 * @param[in] dir scandir dirent entry
 *
 * @retval 0 for entires to be skipped
 * @retval 1 otherwise
 */
static int
filter_pids(const struct dirent *dir)
{
	size_t char_idx;

	if (dir->d_name[0] == '.')
		return 0;

        for (char_idx = 0; char_idx < strlen(dir->d_name); ++char_idx) {
		if (!isdigit(dir->d_name[char_idx]))
			return 0;
        }

        return 1;
}

/**
 * @brief Move all tasks to COS0 (default)
 *
 * @return Operation status
 */
static int
os_alloc_reset_tasks(void)
{
	struct dirent **pids_list = NULL;
	int pid_count;
	int pid_idx;
	int alloc_result;
	pid_t pid;
	const int cos0 = 0;

	LOG_INFO("OS alloc reset - tasks\n");

	pid_count = scandir("/proc", &pids_list, filter_pids, NULL);

	for (pid_idx = 0; pid_idx < pid_count; ++pid_idx) {
		pid = atoi(pids_list[pid_idx]->d_name);
		alloc_result = os_alloc_assoc_set_pid(pid, cos0);
		if (alloc_result != PQOS_RETVAL_OK) {
                        struct stat st;
                        char path[128];

                        snprintf(path, sizeof(path), "/proc/%d", pid);
                        if (stat(path, &st) != 0) {
                                LOG_DEBUG("Task %d no longer exists\n", pid);
                                alloc_result = PQOS_RETVAL_OK;
                                continue;
                        }

			LOG_ERROR("Error allocating task %d to COS%u\n",
				   pid, cos0);
			return alloc_result;
		}
	}

	return PQOS_RETVAL_OK;
}

/**
 * @brief Performs "light reset" of CAT.
 *
 * Moves all cores to default COS
 * Resets all l3 schematas to default value
 * Resets all l2 schematas to default value
 * Resets all mba schematas to default value
 * Moves all tasks to default COS
 *
 * @param [in] l3_cap id of default COS
 * @param [in] l2_cap id of default COS
 *
 * @return Operation status
 */
static int
os_alloc_reset_light(const struct pqos_cap_l3ca *l3_cap,
		     const struct pqos_cap_l2ca *l2_cap)
{
	int ret = PQOS_RETVAL_OK;
	int step_result;

	LOG_INFO("OS alloc reset - LIGHT\n");

	step_result = os_alloc_reset_cores();
	if (step_result != PQOS_RETVAL_OK)
		ret = step_result;

	step_result = os_alloc_reset_schematas(l3_cap, l2_cap);
	if (step_result != PQOS_RETVAL_OK)
		ret = step_result;

	step_result = os_alloc_reset_tasks();
	if (step_result != PQOS_RETVAL_OK)
		ret = step_result;

	return ret;
}

/**
 * @brief updates CDP for l2 cache capability
 *
 * @param [in] l2_cdp_cfg requsted l2 cdp configuration
 * @param [in] l2_cap l2 capability
 * @param [out] l2_cdp id of default COS
 *
 * @retval > 0 on l2_cdp change
 * @retval 0 on no change
 */
static int
l2_cdp_update(const enum pqos_cdp_config l2_cdp_cfg,
	       const struct pqos_cap_l2ca *l2_cap,
	       int *l2_cdp)
{

	if (l2_cap == NULL)
		return 0;

	switch (l2_cdp_cfg) {
	case PQOS_REQUIRE_CDP_ON:
		*l2_cdp = 1;
		break;
	case PQOS_REQUIRE_CDP_ANY:
		*l2_cdp = l2_cap->cdp_on;
		break;
	case PQOS_REQUIRE_CDP_OFF:
		*l2_cdp = 0;
		break;
	}

	return l2_cap->cdp_on != *l2_cdp;
}

/**
 * @brief updates CDP for l3 cache capability
 *
 * @param [in] l3_cdp_cfg requsted l3 cdp configuration
 * @param [in] l3_cap l3 capability
 * @param [out] l3_cdp id of default COS
 *
 * @retval > 0 on l3_cdp change
 * @retval 0 on no change
 */
static int
l3_cdp_update(const enum pqos_cdp_config l3_cdp_cfg,
	       const struct pqos_cap_l3ca *l3_cap,
	       int *l3_cdp) {

	if (l3_cap == NULL)
		return 0;

	switch (l3_cdp_cfg) {
	case PQOS_REQUIRE_CDP_ON:
		*l3_cdp = 1;
		break;
	case PQOS_REQUIRE_CDP_ANY:
		*l3_cdp = l3_cap->cdp_on;
		break;
	case PQOS_REQUIRE_CDP_OFF:
		*l3_cdp = 0;
		break;
	}

	return l3_cap->cdp_on != *l3_cdp;
}

/**
 * @brief Performs "full reset" of CAT.
 *
 * Allocs all cores to default COS
 * Remounts resctrl file system with new CDP configuration
 *
 * @param [in] l3_cdp_cfg requested L3 CAT CDP config
 * @param [in] l2_cdp_cfg requested L2 CAT CDP config
 * @param [in] l3_cap l3 capability
 * @param [in] l2_cap l2 capability
 *
 * @return Operation status
 */
static int
os_alloc_reset_full(const enum pqos_cdp_config l3_cdp_cfg,
		    const enum pqos_cdp_config l2_cdp_cfg,
	            const struct pqos_cap_l3ca *l3_cap,
	            const struct pqos_cap_l2ca *l2_cap)
{
	int ret;
        int l3_cdp = 0;
	int l2_cdp = 0;
	int l3_cdp_changed = 0;
	int l2_cdp_changed = 0;

        LOG_INFO("OS alloc reset - FULL\n");

        /**
         * Set the CPU assoc back to COS0
         */
        ret = os_alloc_reset_cores();
	if (ret != PQOS_RETVAL_OK)
		goto os_alloc_reset_full_exit;

        /**
         * Umount resctrl to reset schemata
         */

	LOG_INFO("OS alloc reset - unmount resctrl\n");
        ret = resctrl_umount();
        if (ret != PQOS_RETVAL_OK)
                goto os_alloc_reset_full_exit;

        l3_cdp_changed = l3_cdp_update(l3_cdp_cfg, l3_cap, &l3_cdp);
        l2_cdp_changed = l2_cdp_update(l2_cdp_cfg, l2_cap, &l2_cdp);

        /**
         * Mount now with CDP option.
         */
	LOG_INFO("OS alloc reset - mount resctrl with "
		"l3_cdp %s and l2_cdp %s\n",
		l3_cdp ? "on" : "off",
		l2_cdp ? "on" : "off");

        ret = os_interface_mount(l3_cdp, l2_cdp);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Mount OS interface error!\n");
                goto os_alloc_reset_full_exit;
        }
        if (l3_cdp_changed) {
                ASSERT(l3_cap);
                _pqos_cap_l3cdp_change(l3_cap->cdp_on, l3_cdp);
        }
        if (l2_cdp_changed)
                _pqos_cap_l2cdp_change(l2_cdp);
        /**
         * Create the COS dir's in resctrl.
         */
        LOG_INFO("OS alloc reset - prepare resctrl fs\n");
        ret = os_alloc_prep();
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("OS alloc prep error!\n");

 os_alloc_reset_full_exit:
        return ret;
}

int
os_alloc_reset(const enum pqos_cdp_config l3_cdp_cfg,
               const enum pqos_cdp_config l2_cdp_cfg)
{
        const struct pqos_capability *alloc_cap = NULL;
        const struct pqos_cap_l3ca *l3_cap = NULL;
        const struct pqos_cap_l2ca *l2_cap = NULL;
	int l3_cdp = 0;
	int l2_cdp = 0;
	int l3_cdp_changed = 0;
	int l2_cdp_changed = 0;
        int ret;

        ASSERT(l2_cdp_cfg == PQOS_REQUIRE_CDP_ON ||
               l2_cdp_cfg == PQOS_REQUIRE_CDP_OFF ||
               l2_cdp_cfg == PQOS_REQUIRE_CDP_ANY);

        /* Get L3 CAT capabilities */
        (void) pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &alloc_cap);
        if (alloc_cap != NULL)
                l3_cap = alloc_cap->u.l3ca;

        /* Get L2 CAT capabilities */
        alloc_cap = NULL;
        (void) pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L2CA, &alloc_cap);
        if (alloc_cap != NULL)
                l2_cap = alloc_cap->u.l2ca;

        /* Check if either L2 CAT or L3 CAT is supported */
        if (l2_cap == NULL && l3_cap == NULL) {
		LOG_ERROR("L2 CAT/L3 CAT not present!\n");
		ret = PQOS_RETVAL_RESOURCE; /* no L2/L3 CAT present */
		goto os_alloc_reset_exit;
        }
        /* Check L3 CDP requested while not present */
        if (l3_cap == NULL && l3_cdp_cfg != PQOS_REQUIRE_CDP_ANY) {
		LOG_ERROR("L3 CDP setting requested but no L3 CAT present!\n");
		ret = PQOS_RETVAL_RESOURCE;
		goto os_alloc_reset_exit;
        }
        /* Check against erroneous CDP request */
        if (l3_cap != NULL && l3_cdp_cfg == PQOS_REQUIRE_CDP_ON &&
		!l3_cap->os_cdp) {
		LOG_ERROR("L3 CAT/CDP requested but not supported by the "
                          "platform!\n");
		ret = PQOS_RETVAL_PARAM;
		goto os_alloc_reset_exit;
        }
        /* Check L2 CDP requested while not present */
        if (l2_cap == NULL && l2_cdp_cfg != PQOS_REQUIRE_CDP_ANY) {
                LOG_ERROR("L2 CDP setting requested but no L2 CAT present!\n");
                ret = PQOS_RETVAL_RESOURCE;
                goto os_alloc_reset_exit;
        }
        /* Check against erroneous L2 CDP request */
        if (l2_cap != NULL && l2_cdp_cfg == PQOS_REQUIRE_CDP_ON &&
                !l2_cap->os_cdp) {
                LOG_ERROR("L2 CAT/CDP requested but not supported by the "
                          "platform!\n");
                ret = PQOS_RETVAL_PARAM;
                goto os_alloc_reset_exit;
        }

        /** Check if resctrl is in use */
        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                goto os_alloc_reset_exit;
        resctrl_lock_release();

        l3_cdp_changed = l3_cdp_update(l3_cdp_cfg, l3_cap, &l3_cdp);
        l2_cdp_changed = l2_cdp_update(l2_cdp_cfg, l2_cap, &l2_cdp);

        if (l3_cdp_changed || l2_cdp_changed) {

		unsigned monitoring_active = 0;

		ret = resctrl_mon_active(&monitoring_active);

		if (ret != PQOS_RETVAL_OK) {
			LOG_ERROR("Failed to check resctrl "
				  "monitoring activity\n");
			goto os_alloc_reset_exit;
		}

		if (monitoring_active > 0) {
			LOG_ERROR("Failed to reset allocation. Please stop "
				  "monitoring in order to change CDP settings\n");
			ret = PQOS_RETVAL_ERROR;
		} else
			ret = os_alloc_reset_full(l3_cdp_cfg, l2_cdp_cfg,
				l3_cap, l2_cap);
        } else
		ret = os_alloc_reset_light(l3_cap, l2_cap);


 os_alloc_reset_exit:
        return ret;
}

int
os_l3ca_set(const unsigned socket,
            const unsigned num_cos,
            const struct pqos_l3ca *ca)
{
	int ret;
	unsigned sockets_num = 0;
	unsigned *sockets = NULL;
	unsigned i;
	unsigned num_grps = 0, l3ca_num;
	int cdp_enabled = 0;

	ASSERT(ca != NULL);
	ASSERT(num_cos != 0);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	ret = pqos_l3ca_get_cos_num(m_cap, &l3ca_num);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

	ret = resctrl_alloc_get_grps_num(m_cap, &num_grps);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (num_cos > num_grps)
		return PQOS_RETVAL_ERROR;

	/* Get number of sockets in the system */
	sockets = pqos_cpu_get_sockets(m_cpu, &sockets_num);
	if (sockets == NULL || sockets_num == 0) {
		ret = PQOS_RETVAL_ERROR;
		goto os_l3ca_set_exit;
	}

	if (socket >= sockets_num) {
		ret = PQOS_RETVAL_PARAM;
		goto os_l3ca_set_exit;
	}

	ret = pqos_l3ca_cdp_enabled(m_cap, NULL, &cdp_enabled);
	if (ret != PQOS_RETVAL_OK)
		goto os_l3ca_set_exit;

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                goto os_l3ca_set_exit;

	for (i = 0; i < num_cos; i++) {
		struct resctrl_alloc_schemata schmt;

		if (ca[i].cdp == 1 && cdp_enabled == 0) {
			LOG_ERROR("Attempting to set CDP COS while L3 CDP "
			          "is disabled!\n");
			ret = PQOS_RETVAL_ERROR;
			goto os_l3ca_set_unlock;
		}

		ret = resctrl_alloc_schemata_init(ca[i].class_id, m_cap, m_cpu,
		                                  &schmt);

		/* read schemata file */
		if (ret == PQOS_RETVAL_OK)
			ret = resctrl_alloc_schemata_read(ca[i].class_id,
			                                  &schmt);

		/* update and write schemata */
		if (ret == PQOS_RETVAL_OK) {
			struct pqos_l3ca *l3ca = &(schmt.l3ca[socket]);

			if (cdp_enabled == 1 && ca[i].cdp == 0) {
				l3ca->cdp = 1;
				l3ca->u.s.data_mask = ca[i].u.ways_mask;
				l3ca->u.s.code_mask = ca[i].u.ways_mask;
			} else
				*l3ca = ca[i];

			ret = resctrl_alloc_schemata_write(ca[i].class_id,
				                           &schmt);
		}

		resctrl_alloc_schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_l3ca_set_unlock;
	}

 os_l3ca_set_unlock:
        resctrl_lock_release();

 os_l3ca_set_exit:
	if (sockets != NULL)
		free(sockets);

	return ret;
}

int
os_l3ca_get(const unsigned socket,
            const unsigned max_num_ca,
            unsigned *num_ca,
            struct pqos_l3ca *ca)
{
	int ret;
	unsigned class_id;
	unsigned count = 0;
	unsigned sockets_num = 0;
	unsigned *sockets = NULL;

	ASSERT(num_ca != NULL);
	ASSERT(ca != NULL);
	ASSERT(max_num_ca != 0);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	ret = pqos_l3ca_get_cos_num(m_cap, &count);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

	ret = resctrl_alloc_get_grps_num(m_cap, &count);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (count > max_num_ca)
		return PQOS_RETVAL_ERROR;

	sockets = pqos_cpu_get_sockets(m_cpu, &sockets_num);
	if (sockets == NULL || sockets_num == 0) {
		ret = PQOS_RETVAL_ERROR;
		goto os_l3ca_get_exit;
	}

	if (socket >= sockets_num) {
		ret = PQOS_RETVAL_PARAM;
		goto os_l3ca_get_exit;
	}

        ret = resctrl_lock_shared();
        if (ret != PQOS_RETVAL_OK)
                goto os_l3ca_get_exit;

	for (class_id = 0; class_id < count; class_id++) {
		struct resctrl_alloc_schemata schmt;

		ret = resctrl_alloc_schemata_init(class_id, m_cap, m_cpu,
		                                  &schmt);
		if (ret == PQOS_RETVAL_OK)
			ret = resctrl_alloc_schemata_read(class_id, &schmt);

		if (ret == PQOS_RETVAL_OK)
			ca[class_id] = schmt.l3ca[socket];

		resctrl_alloc_schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_l3ca_get_unlock;
	}
	*num_ca = count;

 os_l3ca_get_unlock:
        resctrl_lock_release();

 os_l3ca_get_exit:
	if (sockets != NULL)
		free(sockets);

	return ret;
}

int
os_l3ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
	int ret = PQOS_RETVAL_OK;
	char buf[128];
	const struct pqos_capability *l3_cap = NULL;
	FILE *fd;

	ASSERT(m_cap != NULL);
	ASSERT(min_cbm_bits != NULL);

	/**
	 * Get L3 CAT capabilities
	 */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &l3_cap);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf) - 1, "%s/info/L3/min_cbm_bits", RESCTRL_PATH);

	fd = fopen(buf, "r");
	if (fd == NULL)
		return PQOS_RETVAL_ERROR;

	if (fscanf(fd, "%u", min_cbm_bits) != 1)
		ret = PQOS_RETVAL_ERROR;

	fclose(fd);

	return ret;
}

int
os_l2ca_set(const unsigned l2id,
            const unsigned num_cos,
            const struct pqos_l2ca *ca)
{
	int ret;
	unsigned i;
	unsigned l2ids_num = 0;
	unsigned *l2ids = NULL;
	unsigned num_grps = 0, l2ca_num;
        int cdp_enabled;

	ASSERT(m_cap != NULL);
	ASSERT(ca != NULL);
	ASSERT(num_cos != 0);

	ret = pqos_l2ca_get_cos_num(m_cap, &l2ca_num);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

	ret = resctrl_alloc_get_grps_num(m_cap, &num_grps);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (num_cos > num_grps)
		return PQOS_RETVAL_PARAM;

        ret = pqos_l2ca_cdp_enabled(m_cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK)
                goto os_l2ca_set_exit;

	/*
	 * Check if class id's are within allowed range.
	 */
	for (i = 0; i < num_cos; i++) {
		if (ca[i].class_id >= l2ca_num) {
			LOG_ERROR("L2 COS%u is out of range (COS%u is max)!\n",
			          ca[i].class_id, l2ca_num - 1);
			return PQOS_RETVAL_PARAM;
		}
	}

	/* Get number of L2 ids in the system */
	l2ids = pqos_cpu_get_l2ids(m_cpu, &l2ids_num);
	if (l2ids == NULL || l2ids_num == 0) {
		ret = PQOS_RETVAL_ERROR;
		goto os_l2ca_set_exit;
	}

	if (l2id >= l2ids_num) {
		ret = PQOS_RETVAL_PARAM;
		goto os_l2ca_set_exit;
	}

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                goto os_l2ca_set_exit;

	for (i = 0; i < num_cos; i++) {
		struct resctrl_alloc_schemata schmt;

                if (ca[i].cdp == 1 && cdp_enabled == 0) {
                        LOG_ERROR("Attempting to set CDP COS while L2 CDP "
                                  "is disabled!\n");
                        ret = PQOS_RETVAL_ERROR;
                        goto os_l2ca_set_unlock;
                }

		ret = resctrl_alloc_schemata_init(ca[i].class_id, m_cap, m_cpu,
		                                  &schmt);

		/* read schemata file */
		if (ret == PQOS_RETVAL_OK)
			ret = resctrl_alloc_schemata_read(ca[i].class_id,
			                                  &schmt);

                /* update and write schemata */
                if (ret == PQOS_RETVAL_OK) {
                        struct pqos_l2ca *l2ca = &(schmt.l2ca[l2id]);

                        if (cdp_enabled == 1 && ca[i].cdp == 0) {
                                l2ca->cdp = 1;
                                l2ca->u.s.data_mask = ca[i].u.ways_mask;
                                l2ca->u.s.code_mask = ca[i].u.ways_mask;
                        } else
                                *l2ca = ca[i];

                        ret = resctrl_alloc_schemata_write(ca[i].class_id,
                                                           &schmt);
                }

		resctrl_alloc_schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_l2ca_set_unlock;
	}

 os_l2ca_set_unlock:
        resctrl_lock_release();

 os_l2ca_set_exit:
	if (l2ids != NULL)
		free(l2ids);

	return ret;
}

int
os_l2ca_get(const unsigned l2id,
            const unsigned max_num_ca,
            unsigned *num_ca,
            struct pqos_l2ca *ca)
{
	int ret;
	unsigned class_id;
	unsigned count = 0;
	unsigned l2ids_num = 0;
	unsigned *l2ids = NULL;

	ASSERT(num_ca != NULL);
	ASSERT(ca != NULL);
	ASSERT(max_num_ca != 0);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	ret = pqos_l2ca_get_cos_num(m_cap, &count);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

	ret = resctrl_alloc_get_grps_num(m_cap, &count);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (count > max_num_ca)
		/* Not enough space to store the classes */
		return PQOS_RETVAL_PARAM;

	l2ids = pqos_cpu_get_l2ids(m_cpu, &l2ids_num);
	if (l2ids == NULL || l2ids_num == 0) {
		ret = PQOS_RETVAL_ERROR;
		goto os_l2ca_get_exit;
	}

	if (l2id >= l2ids_num) {
		ret = PQOS_RETVAL_PARAM;
		goto os_l2ca_get_exit;
	}


        ret = resctrl_lock_shared();
        if (ret != PQOS_RETVAL_OK)
                goto os_l2ca_get_exit;

	for (class_id = 0; class_id < count; class_id++) {
		struct resctrl_alloc_schemata schmt;

		ret = resctrl_alloc_schemata_init(class_id, m_cap, m_cpu,
		                                  &schmt);
		if (ret == PQOS_RETVAL_OK)
			ret = resctrl_alloc_schemata_read(class_id, &schmt);

		if (ret == PQOS_RETVAL_OK)
			ca[class_id] = schmt.l2ca[l2id];

		resctrl_alloc_schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_l2ca_get_unlock;
	}
	*num_ca = count;

 os_l2ca_get_unlock:
        resctrl_lock_release();

 os_l2ca_get_exit:
	if (l2ids != NULL)
		free(l2ids);

	return ret;
}

int
os_l2ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
	int ret;
	char buf[128];
	const struct pqos_capability *l2_cap = NULL;
	FILE *fd;

	ASSERT(m_cap != NULL);
	ASSERT(min_cbm_bits != NULL);

	/**
	 * Get L2 CAT capabilities
	 */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L2CA, &l2_cap);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf) - 1, "%s/info/L2/min_cbm_bits", RESCTRL_PATH);

	fd = fopen(buf, "r");
	if (fd == NULL)
		return PQOS_RETVAL_ERROR;

	if (fscanf(fd, "%u", min_cbm_bits) != 1)
		ret = PQOS_RETVAL_ERROR;

	fclose(fd);

	return ret;
}

int
os_mba_set(const unsigned socket,
           const unsigned num_cos,
           const struct pqos_mba *requested,
           struct pqos_mba *actual)
{
	int ret;
	unsigned sockets_num = 0;
	unsigned *sockets = NULL;
	unsigned i, step = 0;
	unsigned num_grps = 0;
	const struct pqos_capability *mba_cap = NULL;

	ASSERT(requested != NULL);
	ASSERT(num_cos != 0);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	/**
	 * Check if MBA is supported
	 */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_MBA, &mba_cap);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* MBA not supported */

	ret = resctrl_alloc_get_grps_num(m_cap, &num_grps);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (num_cos > num_grps)
		return PQOS_RETVAL_PARAM;

        step = mba_cap->u.mba->throttle_step;

        /**
	 * Check if class id's are within allowed range.
	 */
	for (i = 0; i < num_cos; i++)
		if (requested[i].class_id >= num_grps) {
			LOG_ERROR("MBA COS%u is out of range (COS%u is max)!\n",
			          requested[i].class_id, num_grps - 1);
			return PQOS_RETVAL_PARAM;
		}

	/* Get number of sockets in the system */
	sockets = pqos_cpu_get_sockets(m_cpu, &sockets_num);
	if (sockets == NULL || sockets_num == 0 || socket >= sockets_num) {
		ret = PQOS_RETVAL_ERROR;
		goto os_l3ca_set_exit;
	}

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                goto os_l3ca_set_exit;

	for (i = 0; i < num_cos; i++) {
		struct resctrl_alloc_schemata schmt;

		ret = resctrl_alloc_schemata_init(requested[i].class_id,
		                                  m_cap, m_cpu, &schmt);

		/* read schemata file */
		if (ret == PQOS_RETVAL_OK)
			ret = resctrl_alloc_schemata_read(requested[i].class_id,
				                          &schmt);

		/* update and write schemata */
		if (ret == PQOS_RETVAL_OK) {
			struct pqos_mba *mba = &(schmt.mba[socket]);

			*mba = requested[i];
                        mba->mb_rate = (((requested[i].mb_rate
                                          + (step/2)) / step) * step);
			if (mba->mb_rate == 0)
				mba->mb_rate = step;

			ret = resctrl_alloc_schemata_write(
				requested[i].class_id, &schmt);
		}

		if (actual != NULL) {
			/* read actual schemata */
			if (ret == PQOS_RETVAL_OK)
				ret = resctrl_alloc_schemata_read(
					requested[i].class_id, &schmt);

			/* update actual schemata */
			if (ret == PQOS_RETVAL_OK)
				actual[i] = schmt.mba[socket];
		}
		resctrl_alloc_schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_l3ca_set_unlock;
	}

 os_l3ca_set_unlock:
        resctrl_lock_release();

 os_l3ca_set_exit:
	if (sockets != NULL)
		free(sockets);

	return ret;
}

int
os_mba_get(const unsigned socket,
           const unsigned max_num_cos,
           unsigned *num_cos,
           struct pqos_mba *mba_tab)
{
	int ret;
	unsigned class_id;
	unsigned count = 0;
	unsigned sockets_num = 0;
	unsigned *sockets = NULL;
	const struct pqos_capability *mba_cap = NULL;

	ASSERT(num_cos != NULL);
	ASSERT(max_num_cos != 0);
	ASSERT(mba_tab != NULL);
	ASSERT(m_cap != NULL);
	ASSERT(m_cpu != NULL);

	/**
	 * Check if MBA is supported
	 */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_MBA, &mba_cap);
	if (ret != PQOS_RETVAL_OK)
		return PQOS_RETVAL_RESOURCE; /* MBA not supported */

	ret = resctrl_alloc_get_grps_num(m_cap, &count);
	if (ret != PQOS_RETVAL_OK)
		return ret;

	if (count > max_num_cos)
		return PQOS_RETVAL_ERROR;

	sockets = pqos_cpu_get_sockets(m_cpu, &sockets_num);
	if (sockets == NULL || sockets_num == 0 || socket >= sockets_num) {
		ret = PQOS_RETVAL_ERROR;
		goto os_mba_get_exit;
	}

        ret = resctrl_lock_shared();
        if (ret != PQOS_RETVAL_OK)
                goto os_mba_get_exit;

	for (class_id = 0; class_id < count; class_id++) {
		struct resctrl_alloc_schemata schmt;

		ret = resctrl_alloc_schemata_init(class_id, m_cap, m_cpu,
		                                  &schmt);
		if (ret == PQOS_RETVAL_OK)
			ret = resctrl_alloc_schemata_read(class_id, &schmt);

		if (ret == PQOS_RETVAL_OK)
			mba_tab[class_id] = schmt.mba[socket];

		resctrl_alloc_schemata_fini(&schmt);

		if (ret != PQOS_RETVAL_OK)
			goto os_mba_get_unlock;
	}
	*num_cos = count;

 os_mba_get_unlock:
        resctrl_lock_release();

 os_mba_get_exit:
	if (sockets != NULL)
		free(sockets);

	return ret;
}

int
os_alloc_assoc_set_pid(const pid_t task,
                       const unsigned class_id)
{
        int ret;
	unsigned max_cos = 0;

        ASSERT(m_cap != NULL);

	/* Get number of COS */
        ret = resctrl_alloc_get_grps_num(m_cap, &max_cos);
	if (ret != PQOS_RETVAL_OK)
		return ret;

        if (class_id >= max_cos) {
                LOG_ERROR("COS out of bounds for task %d\n", (int)task);
                return PQOS_RETVAL_PARAM;
        }

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* Write to tasks file */
	ret = resctrl_alloc_assoc_set_pid(task, class_id);

        resctrl_lock_release();

        return ret;
}

int
os_alloc_assoc_get_pid(const pid_t task,
                       unsigned *class_id)
{
        int ret;

        ASSERT(class_id != NULL);

        ret = resctrl_lock_shared();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* Search tasks files */
        ret = resctrl_alloc_assoc_get_pid(task, class_id);

        resctrl_lock_release();

        return ret;
}

int
os_alloc_assign_pid(const unsigned technology,
                    const pid_t *task_array,
                    const unsigned task_num,
                    unsigned *class_id)
{
        unsigned i, num_rctl_grps = 0;
        int ret;

        ASSERT(task_num > 0);
        ASSERT(task_array != NULL);
        ASSERT(class_id != NULL);
        ASSERT(m_cap != NULL);
        UNUSED_PARAM(technology);

        /* obtain highest class id for all requested technologies */
        ret = resctrl_alloc_get_grps_num(m_cap, &num_rctl_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (num_rctl_grps == 0)
                return PQOS_RETVAL_ERROR;

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* find an unused class from highest down */
        ret = get_unused_cos(num_rctl_grps - 1, class_id);
        if (ret != PQOS_RETVAL_OK)
                goto os_alloc_assign_pid_unlock;

        /* assign tasks to the unused class */
        for (i = 0; i < task_num; i++) {
                ret = resctrl_alloc_task_write(*class_id, task_array[i]);
                if (ret != PQOS_RETVAL_OK)
                        goto os_alloc_assign_pid_unlock;
        }

 os_alloc_assign_pid_unlock:
        resctrl_lock_release();

        return ret;
}

int
os_alloc_release_pid(const pid_t *task_array,
                     const unsigned task_num)
{
        unsigned i;
	int ret;

        ASSERT(task_array != NULL);
        ASSERT(task_num != 0);

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /**
         * Write all tasks to default COS#0 tasks file
         * - return on error
         * - otherwise try next task in array
         */
        for (i = 0; i < task_num; i++) {
                ret = resctrl_alloc_task_write(0, task_array[i]);
		if (ret != PQOS_RETVAL_OK)
                        goto os_alloc_release_pid_unlock;
	}

 os_alloc_release_pid_unlock:
        resctrl_lock_release();

        return ret;
}
