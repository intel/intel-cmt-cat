/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2020 Intel Corporation. All rights reserved.
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
#include <dirent.h> /**< scandir() */

#include "pqos.h"
#include "allocation.h"
#include "os_allocation.h"
#include "cap.h"
#include "common.h"
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
static const struct pqos_cpuinfo *m_cpu = NULL;

/**
 * @brief Function to mount the resctrl file system with CDP or MBps options
 *
 * @param l3_cdp_cfg L3 CDP option
 * @param l2_cdp_cfg L2 CDP option
 * @param mba_cfg requested MBA config
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
static int
os_interface_mount(const enum pqos_cdp_config l3_cdp_cfg,
                   const enum pqos_cdp_config l2_cdp_cfg,
                   const enum pqos_mba_config mba_cfg)
{
        const struct pqos_cap_l3ca *l3_cap = NULL;
        const struct pqos_cap_l2ca *l2_cap = NULL;
        const struct pqos_cap_mba *mba_cap = NULL;
        const struct pqos_capability *alloc_cap = NULL;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;
        int ret;

        if (l3_cdp_cfg != PQOS_REQUIRE_CDP_ON &&
            l3_cdp_cfg != PQOS_REQUIRE_CDP_OFF) {
                LOG_ERROR("Invalid L3 CDP mounting setting %d!\n", l3_cdp_cfg);
                return PQOS_RETVAL_PARAM;
        }

        if (l2_cdp_cfg != PQOS_REQUIRE_CDP_ON &&
            l2_cdp_cfg != PQOS_REQUIRE_CDP_OFF) {
                LOG_ERROR("Invalid L2 CDP mounting setting %d!\n", l2_cdp_cfg);
                return PQOS_RETVAL_PARAM;
        }

        if (mba_cfg != PQOS_MBA_DEFAULT && mba_cfg != PQOS_MBA_CTRL) {
                LOG_ERROR("Invalid MBA mounting setting %d!\n", mba_cfg);
                return PQOS_RETVAL_PARAM;
        }

        _pqos_cap_get(&cap, &cpu);

        if (l3_cdp_cfg == PQOS_REQUIRE_CDP_OFF &&
            l2_cdp_cfg == PQOS_REQUIRE_CDP_OFF && mba_cfg == PQOS_MBA_DEFAULT)
                goto mount;

        /* Get L3 CAT capabilities */
        (void)pqos_cap_get_type(cap, PQOS_CAP_TYPE_L3CA, &alloc_cap);
        if (alloc_cap != NULL)
                l3_cap = alloc_cap->u.l3ca;

        /* Get L2 CAT capabilities */
        (void)pqos_cap_get_type(cap, PQOS_CAP_TYPE_L2CA, &alloc_cap);
        if (alloc_cap != NULL)
                l2_cap = alloc_cap->u.l2ca;

        /* Get MBA capabilities */
        (void)pqos_cap_get_type(cap, PQOS_CAP_TYPE_MBA, &alloc_cap);
        if (alloc_cap != NULL)
                mba_cap = alloc_cap->u.mba;

        if (l3_cap != NULL && !l3_cap->cdp &&
            l3_cdp_cfg == PQOS_REQUIRE_CDP_ON) {
                /* Check against erroneous CDP request */
                LOG_ERROR("L3 CDP requested but not supported by the "
                          "platform!\n");
                return PQOS_RETVAL_PARAM;
        }
        if (l2_cap != NULL && !l2_cap->cdp &&
            l2_cdp_cfg == PQOS_REQUIRE_CDP_ON) {
                /* Check against erroneous CDP request */
                LOG_ERROR("L2 CDP requested but not supported by the "
                          "platform!\n");
                return PQOS_RETVAL_PARAM;
        }
        if (mba_cap != NULL && mba_cap->ctrl == 0 && mba_cfg == PQOS_MBA_CTRL) {
                /* Check against erroneous MBA request */
                LOG_ERROR("MBA CTRL requested but not supported!\n");
                return PQOS_RETVAL_PARAM;
        }

mount:
        ret = resctrl_mount(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /*
         * Verify if mba ctrl is enabled
         * Some kernel versions resctrl mount parameters are not verified
         */
        if (mba_cfg == PQOS_MBA_CTRL) {
                struct resctrl_schemata *schmt;
                struct pqos_mba mba;

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL) {
                        ret = PQOS_RETVAL_ERROR;
                        goto unmount;
                }

                ret = resctrl_alloc_schemata_read(0, schmt);
                if (ret != PQOS_RETVAL_OK) {
                        resctrl_schemata_free(schmt);
                        goto unmount;
                }

                ret = resctrl_schemata_mba_get(schmt, 0, &mba);
                if (ret == PQOS_RETVAL_OK && mba.mb_max <= 100) {
                        LOG_ERROR("MBA CTRL not enabled\n");
                        ret = PQOS_RETVAL_ERROR;
                }

                resctrl_schemata_free(schmt);
        }

unmount:
        if (ret != PQOS_RETVAL_OK)
                resctrl_umount();
        return ret;
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
        const struct pqos_capability *l3_cap;
        const struct pqos_capability *l2_cap;
        const struct pqos_capability *mba_cap;

        (void)_pqos_cap_get_type(PQOS_CAP_TYPE_L3CA, &l3_cap);
        (void)_pqos_cap_get_type(PQOS_CAP_TYPE_L2CA, &l2_cap);
        (void)_pqos_cap_get_type(PQOS_CAP_TYPE_MBA, &mba_cap);

        /**
         * Check if resctrl is supported
         */
        if (l3_cap == NULL && l2_cap == NULL && mba_cap == NULL)
                return PQOS_RETVAL_OK;

        /**
         * Check if resctrl is mounted
         */
        if (access(RESCTRL_PATH "/cpus", F_OK) != 0) {
                enum pqos_cdp_config l3_cdp_mount = PQOS_REQUIRE_CDP_OFF;
                enum pqos_cdp_config l2_cdp_mount = PQOS_REQUIRE_CDP_OFF;
                enum pqos_mba_config mba_mount = PQOS_MBA_DEFAULT;

                /* Check L3 CAT capabilities */
                if (l3_cap != NULL && l3_cap->u.l3ca->cdp_on)
                        l3_cdp_mount = PQOS_REQUIRE_CDP_ON;

                /* Check L2 CAT capabilities */
                if (l2_cap != NULL && l2_cap->u.l2ca->cdp_on)
                        l2_cdp_mount = PQOS_REQUIRE_CDP_ON;

                /* Check MBA capabilities */
                if (mba_cap != NULL && mba_cap->u.mba->ctrl_on)
                        mba_mount = PQOS_MBA_CTRL;

                ret = os_interface_mount(l3_cdp_mount, l2_cdp_mount, mba_mount);
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
        const struct pqos_cap *cap;
        int ret;

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_alloc_get_grps_num(cap, &num_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;
        /*
         * Detect/Create all available COS resctrl groups
         */
        for (i = 1; i < num_grps; i++) {
                char buf[128];
                struct stat st;

                memset(buf, 0, sizeof(buf));
                if (snprintf(buf, sizeof(buf) - 1, "%s/COS%d", RESCTRL_PATH,
                             (int)i) < 0)
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

        m_cpu = NULL;
        return ret;
}

int
os_alloc_assoc_set(const unsigned lcore, const unsigned class_id)
{
        int ret;
        unsigned grps;
        int ret_mon;
        char mon_group[256];
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;

        _pqos_cap_get(&cap, &cpu);

        ret = pqos_cpu_check_core(cpu, lcore);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_PARAM;

        ret = resctrl_alloc_get_grps_num(cap, &grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (class_id >= grps)
                /* class_id is out of bounds */
                return PQOS_RETVAL_PARAM;

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /*
         * When core is moved to different COS we need to update monitoring
         * groups. Obtain monitoring group name
         */
        ret_mon = resctrl_mon_assoc_get(lcore, mon_group, sizeof(mon_group));
        if (ret_mon != PQOS_RETVAL_OK && ret_mon != PQOS_RETVAL_RESOURCE)
                LOG_WARN("Failed to obtain monitoring group assignment for "
                         "core %u\n",
                         lcore);

        ret = resctrl_alloc_assoc_set(lcore, class_id);
        if (ret != PQOS_RETVAL_OK)
                goto os_alloc_assoc_set_exit;

        /* Core monitoring was started assign it back to monitoring group */
        if (ret_mon == PQOS_RETVAL_OK) {
                ret_mon = resctrl_mon_assoc_set(lcore, mon_group);
                if (ret_mon != PQOS_RETVAL_OK)
                        LOG_WARN("Could not assign core %d back to monitoring "
                                 "group\n",
                                 lcore);
        }

os_alloc_assoc_set_exit:
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
        const struct pqos_cap *cap;

        ASSERT(count != NULL);

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_alloc_get_grps_num(cap, &grps);
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

int
os_alloc_assign(const unsigned technology,
                const unsigned *core_array,
                const unsigned core_num,
                unsigned *class_id)
{
        unsigned i, num_rctl_grps = 0;
        int ret;
        const struct pqos_cap *cap;

        ASSERT(core_num > 0);
        ASSERT(core_array != NULL);
        ASSERT(class_id != NULL);
        UNUSED_PARAM(technology);

        _pqos_cap_get(&cap, NULL);

        /* obtain highest class id for all requested technologies */
        ret = resctrl_alloc_get_grps_num(cap, &num_rctl_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (num_rctl_grps == 0)
                return PQOS_RETVAL_ERROR;

        /* find an unused class from highest down */
        ret = resctrl_alloc_get_unused_group(num_rctl_grps, class_id);
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
 * @param [in] mba_cap mba capability
 *
 * @return Operation status
 */
static int
os_alloc_reset_schematas(const struct pqos_cap_l3ca *l3_cap,
                         const struct pqos_cap_l2ca *l2_cap,
                         const struct pqos_cap_mba *mba_cap)
{
        unsigned grps;
        unsigned i;
        int ret;
        const struct pqos_cap *cap;

        LOG_INFO("OS alloc reset - schematas\n");

        _pqos_cap_get(&cap, NULL);

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        ret = resctrl_alloc_get_grps_num(cap, &grps);
        if (ret != PQOS_RETVAL_OK)
                goto os_alloc_reset_light_schematas_exit;

        /**
         * Reset schematas
         */
        for (i = 0; i < grps; i++) {
                struct resctrl_schemata *schmt;

                schmt = resctrl_schemata_alloc(cap, m_cpu);
                if (schmt == NULL) {
                        ret = PQOS_RETVAL_ERROR;
                        LOG_ERROR("Error on schemata memory allocation "
                                  "for resctrl group %u\n",
                                  i);
                        goto os_alloc_reset_light_schematas_exit;
                }

                ret = resctrl_schemata_reset(schmt, l3_cap, l2_cap, mba_cap);
                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_write(
                            i, PQOS_TECHNOLOGY_ALL, schmt);

                resctrl_schemata_free(schmt);

                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Error on schemata write "
                                  "for resctrl group %u\n",
                                  i);
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
 * @retval 0 for entries to be skipped
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
        int alloc_result = PQOS_RETVAL_ERROR;
        pid_t pid;
        const int cos0 = 0;

        LOG_INFO("OS alloc reset - tasks\n");

        pid_count = scandir("/proc", &pids_list, filter_pids, NULL);

        for (pid_idx = 0; pid_idx < pid_count; ++pid_idx) {
                pid = atoi(pids_list[pid_idx]->d_name);
                alloc_result = os_alloc_assoc_set_pid(pid, cos0);
                if (alloc_result == PQOS_RETVAL_PARAM) {
                        LOG_DEBUG("Task %d no longer exists\n", pid);
                        alloc_result = PQOS_RETVAL_OK;
                } else if (alloc_result != PQOS_RETVAL_OK) {
                        LOG_ERROR("Error allocating task %d to COS%u\n", pid,
                                  cos0);
                        break;
                }
        }

        if (pid_count > 0 && pids_list) {
                while (pid_count--)
                        free(pids_list[pid_count]);
                free(pids_list);
        }

        return alloc_result;
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
 * @param [in] l3_cap L3 CAT capability
 * @param [in] l2_cap L2 CAT capability
 * @param [in] mba_cap MBA capability
 *
 * @return Operation status
 */
static int
os_alloc_reset_light(const struct pqos_cap_l3ca *l3_cap,
                     const struct pqos_cap_l2ca *l2_cap,
                     const struct pqos_cap_mba *mba_cap)
{
        int ret = PQOS_RETVAL_OK;
        int step_result;

        LOG_INFO("OS alloc reset - LIGHT\n");

        step_result = os_alloc_reset_cores();
        if (step_result != PQOS_RETVAL_OK)
                ret = step_result;

        step_result = os_alloc_reset_schematas(l3_cap, l2_cap, mba_cap);
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
 * @param [in] l2_cdp_cfg requested l2 cdp configuration
 * @param [in] l2_cap l2 capability
 * @param [out] l2_cdp id of default COS
 *
 * @retval > 0 on l2_cdp change
 * @retval 0 on no change
 */
static int
l2_cdp_update(const enum pqos_cdp_config l2_cdp_cfg,
              const struct pqos_cap_l2ca *l2_cap,
              enum pqos_cdp_config *l2_cdp)
{
        int ret = 0;

        if (l2_cap == NULL)
                return 0;
        switch (l2_cdp_cfg) {
        case PQOS_REQUIRE_CDP_ON:
                *l2_cdp = l2_cdp_cfg;
                ret = !l2_cap->cdp_on;
                break;
        case PQOS_REQUIRE_CDP_ANY:
                if (l2_cap->cdp_on)
                        *l2_cdp = PQOS_REQUIRE_CDP_ON;
                else
                        *l2_cdp = PQOS_REQUIRE_CDP_OFF;
                break;
        case PQOS_REQUIRE_CDP_OFF:
                *l2_cdp = l2_cdp_cfg;
                ret = l2_cap->cdp_on;
                break;
        }

        return ret;
}

/**
 * @brief updates CDP for l3 cache capability
 *
 * @param [in] l3_cdp_cfg requested l3 cdp configuration
 * @param [in] l3_cap l3 capability
 * @param [out] l3_cdp id of default COS
 *
 * @retval > 0 on l3_cdp change
 * @retval 0 on no change
 */
static int
l3_cdp_update(const enum pqos_cdp_config l3_cdp_cfg,
              const struct pqos_cap_l3ca *l3_cap,
              enum pqos_cdp_config *l3_cdp)
{
        int ret = 0;

        if (l3_cap == NULL)
                return 0;
        switch (l3_cdp_cfg) {
        case PQOS_REQUIRE_CDP_ON:
                *l3_cdp = l3_cdp_cfg;
                ret = !l3_cap->cdp_on;
                break;
        case PQOS_REQUIRE_CDP_ANY:
                if (l3_cap->cdp_on)
                        *l3_cdp = PQOS_REQUIRE_CDP_ON;
                else
                        *l3_cdp = PQOS_REQUIRE_CDP_OFF;
                break;
        case PQOS_REQUIRE_CDP_OFF:
                *l3_cdp = l3_cdp_cfg;
                ret = l3_cap->cdp_on;
                break;
        }

        return ret;
}

/**
 * @brief updates MBA configuration for MBA capability
 *
 * @param [in] mba_cfg requested MBA configuration
 * @param [in] mba_cap MBA capability
 * @param [out] mba_ctrl updated MBA config
 *
 * @retval > 0 mba_ctrl change
 * @retval 0 no change
 */
static int
mba_cfg_update(const enum pqos_mba_config mba_cfg,
               const struct pqos_cap_mba *mba_cap,
               enum pqos_mba_config *mba_ctrl)
{
        int ret = 0;

        if (mba_cap == NULL)
                return 0;

        switch (mba_cfg) {
        case PQOS_MBA_CTRL:
                *mba_ctrl = mba_cfg;
                ret = (mba_cap->ctrl_on != 1);
                break;
        case PQOS_MBA_ANY:
                if (mba_cap->ctrl_on == 1)
                        *mba_ctrl = PQOS_MBA_CTRL;
                else
                        *mba_ctrl = PQOS_MBA_DEFAULT;
                break;
        case PQOS_MBA_DEFAULT:
                *mba_ctrl = mba_cfg;
                ret = (mba_cap->ctrl_on != 0);
                break;
        }

        return ret;
}

/**
 * @brief Performs "full reset" of CAT.
 *
 * Allocs all cores to default COS
 * Remounts resctrl file system with new CDP configuration
 *
 * @param [in] l3_cdp_cfg requested L3 CAT CDP config
 * @param [in] l2_cdp_cfg requested L2 CAT CDP config
 * @param [in] mba_cfg requested MBA config
 * @param [in] l3_cap l3 capability
 * @param [in] l2_cap l2 capability
 * @param [in] mba_cap mba capability
 *
 * @return Operation status
 */
static int
os_alloc_reset_full(const enum pqos_cdp_config l3_cdp_cfg,
                    const enum pqos_cdp_config l2_cdp_cfg,
                    const enum pqos_mba_config mba_cfg,
                    const struct pqos_cap_l3ca *l3_cap,
                    const struct pqos_cap_l2ca *l2_cap,
                    const struct pqos_cap_mba *mba_cap)
{
        int ret;

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

        /**
         * Mount now with CDP option.
         */
        LOG_INFO("OS alloc reset - mount resctrl with "
                 "l3_cdp %s, l2_cdp %s, mba ctrl %s\n",
                 l3_cdp_cfg == PQOS_REQUIRE_CDP_ON ? "on" : "off",
                 l2_cdp_cfg == PQOS_REQUIRE_CDP_ON ? "on" : "off",
                 mba_cfg == PQOS_MBA_CTRL ? "on" : "off");

        ret = os_interface_mount(l3_cdp_cfg, l2_cdp_cfg, mba_cfg);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Mount OS interface error!\n");
                goto os_alloc_reset_full_exit;
        }
        if (l3_cap != NULL)
                _pqos_cap_l3cdp_change(l3_cdp_cfg);
        if (l2_cap != NULL)
                _pqos_cap_l2cdp_change(l2_cdp_cfg);
        if (mba_cap != NULL)
                _pqos_cap_mba_change(mba_cfg);
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
               const enum pqos_cdp_config l2_cdp_cfg,
               const enum pqos_mba_config mba_cfg)
{
        const struct pqos_capability *alloc_cap = NULL;
        const struct pqos_cap_l3ca *l3_cap = NULL;
        const struct pqos_cap_l2ca *l2_cap = NULL;
        const struct pqos_cap_mba *mba_cap = NULL;
        enum pqos_cdp_config l3_cdp = PQOS_REQUIRE_CDP_OFF;
        enum pqos_cdp_config l2_cdp = PQOS_REQUIRE_CDP_OFF;
        enum pqos_mba_config mba_ctrl = PQOS_MBA_DEFAULT;
        int l3_cdp_changed = 0;
        int l2_cdp_changed = 0;
        int mba_changed = 0;
        int ret;

        ASSERT(l3_cdp_cfg == PQOS_REQUIRE_CDP_ON ||
               l3_cdp_cfg == PQOS_REQUIRE_CDP_OFF ||
               l3_cdp_cfg == PQOS_REQUIRE_CDP_ANY);

        ASSERT(l2_cdp_cfg == PQOS_REQUIRE_CDP_ON ||
               l2_cdp_cfg == PQOS_REQUIRE_CDP_OFF ||
               l2_cdp_cfg == PQOS_REQUIRE_CDP_ANY);

        ASSERT(mba_cfg == PQOS_MBA_DEFAULT || mba_cfg == PQOS_MBA_CTRL ||
               mba_cfg == PQOS_MBA_ANY);

        /* Get L3 CAT capabilities */
        (void)_pqos_cap_get_type(PQOS_CAP_TYPE_L3CA, &alloc_cap);
        if (alloc_cap != NULL)
                l3_cap = alloc_cap->u.l3ca;

        /* Get L2 CAT capabilities */
        alloc_cap = NULL;
        (void)_pqos_cap_get_type(PQOS_CAP_TYPE_L2CA, &alloc_cap);
        if (alloc_cap != NULL)
                l2_cap = alloc_cap->u.l2ca;

        /* Get MBA capabilities */
        alloc_cap = NULL;
        (void)_pqos_cap_get_type(PQOS_CAP_TYPE_MBA, &alloc_cap);
        if (alloc_cap != NULL)
                mba_cap = alloc_cap->u.mba;

        /* Check if either L2 CAT or L3 CAT is supported */
        if (l2_cap == NULL && l3_cap == NULL && mba_cap == NULL) {
                LOG_ERROR("L2 CAT/L3 CAT/MBA not present!\n");
                ret = PQOS_RETVAL_RESOURCE; /* no L2 CAT/L3 CAT/MBA present */
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
            !l3_cap->cdp) {
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
            !l2_cap->cdp) {
                LOG_ERROR("L2 CAT/CDP requested but not supported by the "
                          "platform!\n");
                ret = PQOS_RETVAL_PARAM;
                goto os_alloc_reset_exit;
        }
        /* Check MBA CTRL requested while not present */
        if (mba_cap == NULL && mba_cfg != PQOS_MBA_ANY) {
                LOG_ERROR("MBA CTRL setting requested but no MBA present!\n");
                ret = PQOS_RETVAL_RESOURCE;
                goto os_alloc_reset_exit;
        }
        /* Check against erroneous MBA CTRL request */
        if (mba_cap != NULL && mba_cfg == PQOS_MBA_CTRL && mba_cap->ctrl == 0) {
                LOG_ERROR("MBA CTRL requested but not supported!\n");
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
        mba_changed = mba_cfg_update(mba_cfg, mba_cap, &mba_ctrl);

        if (l3_cdp_changed || l2_cdp_changed || mba_changed) {

                unsigned monitoring_active = 0;

                ret = resctrl_mon_active(&monitoring_active);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("Failed to check resctrl "
                                  "monitoring activity\n");
                        goto os_alloc_reset_exit;
                }

                if (monitoring_active > 0) {
                        LOG_ERROR("Failed to reset allocation. Please stop "
                                  "monitoring in order to change CDP/MBA "
                                  "settings\n");
                        ret = PQOS_RETVAL_ERROR;
                } else
                        ret = os_alloc_reset_full(l3_cdp, l2_cdp, mba_ctrl,
                                                  l3_cap, l2_cap, mba_cap);
        } else
                ret = os_alloc_reset_light(l3_cap, l2_cap, mba_cap);

os_alloc_reset_exit:
        return ret;
}

/*
 * @brief Check if l3cat_id is correct
 *
 * @param [in] cpu l3cat id
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
verify_l3cat_id(const unsigned l3cat_id, const struct pqos_cpuinfo *cpu)
{
        int ret = PQOS_RETVAL_PARAM;
        unsigned i;
        unsigned l3cat_num;
        unsigned *l3cat_ids;

        /* Get l3cat ids in the system */
        l3cat_ids = pqos_cpu_get_l3cat_ids(cpu, &l3cat_num);
        if (l3cat_ids == NULL)
                return PQOS_RETVAL_ERROR;

        for (i = 0; i < l3cat_num; ++i)
                if (l3cat_id == l3cat_ids[i]) {
                        ret = PQOS_RETVAL_OK;
                        break;
                }

        free(l3cat_ids);

        return ret;
}

/*
 * @brief Check if mba_id is correct
 *
 * @param [in] mba_id MBA resource id
 * @param [in] CPU information structure from \a pqos_cap_get
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
verify_mba_id(const unsigned mba_id, const struct pqos_cpuinfo *cpu)
{
        int ret = PQOS_RETVAL_PARAM;
        unsigned i;
        unsigned mba_id_num;
        unsigned *mba_ids;

        /* Get mba ids in the system */
        mba_ids = pqos_cpu_get_mba_ids(cpu, &mba_id_num);
        if (mba_ids == NULL)
                return PQOS_RETVAL_ERROR;

        for (i = 0; i < mba_id_num; ++i)
                if (mba_id == mba_ids[i]) {
                        ret = PQOS_RETVAL_OK;
                        break;
                }

        free(mba_ids);

        return ret;
}

int
os_l3ca_set(const unsigned l3cat_id,
            const unsigned num_cos,
            const struct pqos_l3ca *ca)
{
        int ret;
        unsigned i;
        unsigned num_grps = 0, l3ca_num;
        int cdp_enabled = 0;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;

        ASSERT(ca != NULL);
        ASSERT(num_cos != 0);

        _pqos_cap_get(&cap, &cpu);

        ret = pqos_l3ca_get_cos_num(cap, &l3ca_num);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

        ret = resctrl_alloc_get_grps_num(cap, &num_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (num_cos > num_grps)
                return PQOS_RETVAL_ERROR;

        ret = verify_l3cat_id(l3cat_id, cpu);
        if (ret != PQOS_RETVAL_OK)
                goto os_l3ca_set_exit;

        ret = pqos_l3ca_cdp_enabled(cap, NULL, &cdp_enabled);
        if (ret != PQOS_RETVAL_OK)
                goto os_l3ca_set_exit;

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                goto os_l3ca_set_exit;

        for (i = 0; i < num_cos; i++) {
                struct resctrl_schemata *schmt;

                if (ca[i].cdp == 1 && cdp_enabled == 0) {
                        LOG_ERROR("Attempting to set CDP COS while L3 CDP "
                                  "is disabled!\n");
                        ret = PQOS_RETVAL_ERROR;
                        goto os_l3ca_set_unlock;
                }

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL)
                        ret = PQOS_RETVAL_ERROR;

                /* read schemata file */
                if (ret == PQOS_RETVAL_OK)
                        ret =
                            resctrl_alloc_schemata_read(ca[i].class_id, schmt);

                /* update schemata */
                if (ret == PQOS_RETVAL_OK) {
                        struct pqos_l3ca l3ca;

                        if (cdp_enabled == 1 && ca[i].cdp == 0) {
                                l3ca.cdp = 1;
                                l3ca.u.s.data_mask = ca[i].u.ways_mask;
                                l3ca.u.s.code_mask = ca[i].u.ways_mask;
                        } else
                                l3ca = ca[i];

                        ret = resctrl_schemata_l3ca_set(schmt, l3cat_id, &l3ca);
                }

                /* write schemata */
                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_write(
                            ca[i].class_id, PQOS_TECHNOLOGY_L3CA, schmt);

                resctrl_schemata_free(schmt);

                if (ret != PQOS_RETVAL_OK)
                        goto os_l3ca_set_unlock;
        }

os_l3ca_set_unlock:
        resctrl_lock_release();

os_l3ca_set_exit:
        return ret;
}

int
os_l3ca_get(const unsigned l3cat_id,
            const unsigned max_num_ca,
            unsigned *num_ca,
            struct pqos_l3ca *ca)
{
        int ret;
        unsigned class_id;
        unsigned count = 0;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;

        ASSERT(num_ca != NULL);
        ASSERT(ca != NULL);
        ASSERT(max_num_ca != 0);

        _pqos_cap_get(&cap, &cpu);

        ret = pqos_l3ca_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

        ret = resctrl_alloc_get_grps_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (count > max_num_ca)
                return PQOS_RETVAL_ERROR;

        ret = verify_l3cat_id(l3cat_id, cpu);
        if (ret != PQOS_RETVAL_OK)
                goto os_l3ca_get_exit;

        ret = resctrl_lock_shared();
        if (ret != PQOS_RETVAL_OK)
                goto os_l3ca_get_exit;

        for (class_id = 0; class_id < count; class_id++) {
                struct resctrl_schemata *schmt;

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL)
                        ret = PQOS_RETVAL_ERROR;

                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_read(class_id, schmt);

                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_schemata_l3ca_get(schmt, l3cat_id,
                                                        &ca[class_id]);

                ca[class_id].class_id = class_id;

                resctrl_schemata_free(schmt);

                if (ret != PQOS_RETVAL_OK)
                        goto os_l3ca_get_unlock;
        }
        *num_ca = count;

os_l3ca_get_unlock:
        resctrl_lock_release();

os_l3ca_get_exit:
        return ret;
}

int
os_l3ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
        int ret = PQOS_RETVAL_OK;
        char buf[128];
        const struct pqos_capability *l3_cap = NULL;
        FILE *fd;

        ASSERT(min_cbm_bits != NULL);

        /**
         * Get L3 CAT capabilities
         */
        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_L3CA, &l3_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L3 CAT not supported */

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "%s/info/L3/min_cbm_bits", RESCTRL_PATH);

        fd = pqos_fopen(buf, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        if (fscanf(fd, "%u", min_cbm_bits) != 1)
                ret = PQOS_RETVAL_ERROR;

        fclose(fd);

        return ret;
}

/*
 * @brief Check if L2 id is correct
 *
 * @param [in] l2id unique L2 cache identifier
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
static int
verify_l2_id(const unsigned l2id, const struct pqos_cpuinfo *cpu)
{
        int ret = PQOS_RETVAL_PARAM;
        unsigned i;
        unsigned l2ids_num;
        unsigned *l2ids;

        /* Get L2 ids in the system */
        l2ids = pqos_cpu_get_l2ids(cpu, &l2ids_num);
        if (l2ids == NULL)
                return PQOS_RETVAL_ERROR;

        for (i = 0; i < l2ids_num; ++i)
                if (l2id == l2ids[i]) {
                        ret = PQOS_RETVAL_OK;
                        break;
                }

        free(l2ids);

        return ret;
}

int
os_l2ca_set(const unsigned l2id,
            const unsigned num_cos,
            const struct pqos_l2ca *ca)
{
        int ret;
        unsigned i;
        unsigned num_grps = 0, l2ca_num;
        int cdp_enabled;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;

        ASSERT(ca != NULL);
        ASSERT(num_cos != 0);

        _pqos_cap_get(&cap, &cpu);

        ret = pqos_l2ca_get_cos_num(cap, &l2ca_num);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

        ret = resctrl_alloc_get_grps_num(cap, &num_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (num_cos > num_grps)
                return PQOS_RETVAL_PARAM;

        ret = pqos_l2ca_cdp_enabled(cap, NULL, &cdp_enabled);
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

        ret = verify_l2_id(l2id, cpu);
        if (ret != PQOS_RETVAL_OK)
                goto os_l2ca_set_exit;

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                goto os_l2ca_set_exit;

        for (i = 0; i < num_cos; i++) {
                struct resctrl_schemata *schmt;

                if (ca[i].cdp == 1 && cdp_enabled == 0) {
                        LOG_ERROR("Attempting to set CDP COS while L2 CDP "
                                  "is disabled!\n");
                        ret = PQOS_RETVAL_ERROR;
                        goto os_l2ca_set_unlock;
                }

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL)
                        ret = PQOS_RETVAL_ERROR;

                /* read schemata file */
                if (ret == PQOS_RETVAL_OK)
                        ret =
                            resctrl_alloc_schemata_read(ca[i].class_id, schmt);

                /* update schemata */
                if (ret == PQOS_RETVAL_OK) {
                        struct pqos_l2ca l2ca;

                        if (cdp_enabled == 1 && ca[i].cdp == 0) {
                                l2ca.cdp = 1;
                                l2ca.u.s.data_mask = ca[i].u.ways_mask;
                                l2ca.u.s.code_mask = ca[i].u.ways_mask;
                        } else
                                l2ca = ca[i];

                        ret = resctrl_schemata_l2ca_set(schmt, l2id, &l2ca);
                }

                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_write(
                            ca[i].class_id, PQOS_TECHNOLOGY_L2CA, schmt);

                resctrl_schemata_free(schmt);

                if (ret != PQOS_RETVAL_OK)
                        goto os_l2ca_set_unlock;
        }

os_l2ca_set_unlock:
        resctrl_lock_release();

os_l2ca_set_exit:
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
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;

        ASSERT(num_ca != NULL);
        ASSERT(ca != NULL);
        ASSERT(max_num_ca != 0);

        _pqos_cap_get(&cap, &cpu);

        ret = pqos_l2ca_get_cos_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

        ret = resctrl_alloc_get_grps_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (count > max_num_ca)
                /* Not enough space to store the classes */
                return PQOS_RETVAL_PARAM;

        ret = verify_l2_id(l2id, cpu);
        if (ret != PQOS_RETVAL_OK)
                goto os_l2ca_get_exit;

        ret = resctrl_lock_shared();
        if (ret != PQOS_RETVAL_OK)
                goto os_l2ca_get_exit;

        for (class_id = 0; class_id < count; class_id++) {
                struct resctrl_schemata *schmt;

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL)
                        ret = PQOS_RETVAL_ERROR;

                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_read(class_id, schmt);

                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_schemata_l2ca_get(schmt, l2id,
                                                        &ca[class_id]);

                ca[class_id].class_id = class_id;

                resctrl_schemata_free(schmt);

                if (ret != PQOS_RETVAL_OK)
                        goto os_l2ca_get_unlock;
        }
        *num_ca = count;

os_l2ca_get_unlock:
        resctrl_lock_release();

os_l2ca_get_exit:
        return ret;
}

int
os_l2ca_get_min_cbm_bits(unsigned *min_cbm_bits)
{
        int ret;
        char buf[128];
        const struct pqos_capability *l2_cap = NULL;
        FILE *fd;

        ASSERT(min_cbm_bits != NULL);

        /**
         * Get L2 CAT capabilities
         */
        ret = _pqos_cap_get_type(PQOS_CAP_TYPE_L2CA, &l2_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* L2 CAT not supported */

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "%s/info/L2/min_cbm_bits", RESCTRL_PATH);

        fd = pqos_fopen(buf, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        if (fscanf(fd, "%u", min_cbm_bits) != 1)
                ret = PQOS_RETVAL_ERROR;

        fclose(fd);

        return ret;
}

int
os_mba_set(const unsigned mba_id,
           const unsigned num_cos,
           const struct pqos_mba *requested,
           struct pqos_mba *actual)
{
        int ret;
        unsigned i, step = 0;
        unsigned num_grps = 0;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;
        const struct pqos_capability *mba_cap = NULL;

        ASSERT(requested != NULL);
        ASSERT(num_cos != 0);

        _pqos_cap_get(&cap, &cpu);

        /**
         * Check if MBA is supported
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MBA, &mba_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* MBA not supported */

        ret = resctrl_alloc_get_grps_num(cap, &num_grps);
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

        ret = verify_mba_id(mba_id, cpu);
        if (ret != PQOS_RETVAL_OK)
                goto os_mba_set_exit;

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                goto os_mba_set_exit;

        for (i = 0; i < num_cos; i++) {
                struct resctrl_schemata *schmt;

                if (mba_cap->u.mba->ctrl_on == 0 && requested[i].ctrl) {
                        LOG_ERROR("MBA controller requested but"
                                  " not enabled!\n");
                        ret = PQOS_RETVAL_PARAM;
                        goto os_mba_set_unlock;
                }

                if (mba_cap->u.mba->ctrl_on == 1 && !requested[i].ctrl) {
                        LOG_ERROR("Expected MBA controller but"
                                  " not requested!\n");
                        ret = PQOS_RETVAL_PARAM;
                        goto os_mba_set_unlock;
                }

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL)
                        ret = PQOS_RETVAL_ERROR;

                /* read schemata file */
                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_read(requested[i].class_id,
                                                          schmt);

                /* update and write schemata */
                if (ret == PQOS_RETVAL_OK) {
                        struct pqos_mba mba;

                        mba = requested[i];

                        if (mba.ctrl == 0) {
                                mba.mb_max =
                                    (((requested[i].mb_max + (step / 2)) /
                                      step) *
                                     step);
                                if (mba.mb_max == 0)
                                        mba.mb_max = step;
                        }

                        ret = resctrl_schemata_mba_set(schmt, mba_id, &mba);
                }

                /* write schemata */
                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_write(
                            requested[i].class_id, PQOS_TECHNOLOGY_MBA, schmt);

                if (actual != NULL) {
                        /* read actual schemata */
                        if (ret == PQOS_RETVAL_OK)
                                ret = resctrl_alloc_schemata_read(
                                    requested[i].class_id, schmt);

                        /* update actual schemata */
                        if (ret == PQOS_RETVAL_OK) {
                                ret = resctrl_schemata_mba_get(schmt, mba_id,
                                                               &actual[i]);
                                actual[i].class_id = requested[i].class_id;
                        }
                }
                resctrl_schemata_free(schmt);

                if (ret != PQOS_RETVAL_OK)
                        goto os_mba_set_unlock;
        }

os_mba_set_unlock:
        resctrl_lock_release();

os_mba_set_exit:
        return ret;
}

int
os_mba_set_amd(const unsigned mba_id,
               const unsigned num_cos,
               const struct pqos_mba *requested,
               struct pqos_mba *actual)
{
        int ret;
        unsigned i;
        unsigned num_grps = 0;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;
        const struct pqos_capability *mba_cap = NULL;

        ASSERT(requested != NULL);
        ASSERT(num_cos != 0);

        _pqos_cap_get(&cap, &cpu);

        /**
         * Check if MBA is supported
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MBA, &mba_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* MBA not supported */

        ret = resctrl_alloc_get_grps_num(cap, &num_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (num_cos > num_grps)
                return PQOS_RETVAL_PARAM;

        /**
         * Check if class id's are within allowed range.
         */
        for (i = 0; i < num_cos; i++)
                if (requested[i].class_id >= num_grps) {
                        LOG_ERROR("MBA COS%u is out of range (COS%u is max)!\n",
                                  requested[i].class_id, num_grps - 1);
                        return PQOS_RETVAL_PARAM;
                }

        ret = verify_mba_id(mba_id, cpu);
        if (ret != PQOS_RETVAL_OK)
                goto os_mba_set_exit;

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                goto os_mba_set_exit;

        for (i = 0; i < num_cos; i++) {
                struct resctrl_schemata *schmt;

                if (mba_cap->u.mba->ctrl_on == 0 && requested[i].ctrl) {
                        LOG_ERROR("MBA controller requested but not "
                                  "enabled!\n");
                        ret = PQOS_RETVAL_PARAM;
                        goto os_mba_set_unlock;
                }

                if (mba_cap->u.mba->ctrl_on == 1 && !requested[i].ctrl) {
                        LOG_ERROR("Expected MBA controller but not "
                                  "requested!\n");
                        ret = PQOS_RETVAL_PARAM;
                        goto os_mba_set_unlock;
                }

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL)
                        ret = PQOS_RETVAL_ERROR;

                /* read schemata file */
                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_read(requested[i].class_id,
                                                          schmt);

                /* update and write schemata */
                if (ret == PQOS_RETVAL_OK) {
                        struct pqos_mba mba = requested[i];

                        ret = resctrl_schemata_mba_set(schmt, mba_id, &mba);
                }

                /* write schemata */
                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_write(
                            requested[i].class_id, PQOS_TECHNOLOGY_MBA, schmt);

                if (actual != NULL) {
                        /* read actual schemata */
                        if (ret == PQOS_RETVAL_OK)
                                ret = resctrl_alloc_schemata_read(
                                    requested[i].class_id, schmt);

                        /* update actual schemata */
                        if (ret == PQOS_RETVAL_OK) {
                                ret = resctrl_schemata_mba_get(schmt, mba_id,
                                                               &actual[i]);
                                actual[i].class_id = requested[i].class_id;
                        }
                }
                resctrl_schemata_free(schmt);

                if (ret != PQOS_RETVAL_OK)
                        goto os_mba_set_unlock;
        }

os_mba_set_unlock:
        resctrl_lock_release();

os_mba_set_exit:
        return ret;
}

int
os_mba_get(const unsigned mba_id,
           const unsigned max_num_cos,
           unsigned *num_cos,
           struct pqos_mba *mba_tab)
{
        int ret;
        unsigned class_id;
        unsigned count = 0;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;
        const struct pqos_capability *mba_cap = NULL;

        ASSERT(num_cos != NULL);
        ASSERT(max_num_cos != 0);
        ASSERT(mba_tab != NULL);

        _pqos_cap_get(&cap, &cpu);

        /**
         * Check if MBA is supported
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MBA, &mba_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* MBA not supported */

        ret = resctrl_alloc_get_grps_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (count > max_num_cos)
                return PQOS_RETVAL_ERROR;

        ret = verify_mba_id(mba_id, cpu);
        if (ret != PQOS_RETVAL_OK)
                goto os_mba_get_exit;

        ret = resctrl_lock_shared();
        if (ret != PQOS_RETVAL_OK)
                goto os_mba_get_exit;

        for (class_id = 0; class_id < count; class_id++) {
                struct resctrl_schemata *schmt;

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL)
                        ret = PQOS_RETVAL_ERROR;

                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_read(class_id, schmt);

                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_schemata_mba_get(schmt, mba_id,
                                                       &mba_tab[class_id]);

                mba_tab[class_id].class_id = class_id;

                resctrl_schemata_free(schmt);

                if (ret != PQOS_RETVAL_OK)
                        goto os_mba_get_unlock;
        }
        *num_cos = count;

os_mba_get_unlock:
        resctrl_lock_release();

os_mba_get_exit:
        return ret;
}

int
os_mba_get_amd(const unsigned mba_id,
               const unsigned max_num_cos,
               unsigned *num_cos,
               struct pqos_mba *mba_tab)
{
        int ret;
        unsigned class_id;
        unsigned count = 0;
        const struct pqos_cap *cap;
        const struct pqos_cpuinfo *cpu;
        const struct pqos_capability *mba_cap = NULL;

        ASSERT(num_cos != NULL);
        ASSERT(max_num_cos != 0);
        ASSERT(mba_tab != NULL);

        _pqos_cap_get(&cap, &cpu);

        /**
         * Check if MBA is supported
         */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MBA, &mba_cap);
        if (ret != PQOS_RETVAL_OK)
                return PQOS_RETVAL_RESOURCE; /* MBA not supported */

        ret = resctrl_alloc_get_grps_num(cap, &count);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (count > max_num_cos)
                return PQOS_RETVAL_ERROR;

        ret = verify_mba_id(mba_id, cpu);
        if (ret != PQOS_RETVAL_OK)
                goto os_mba_get_exit;

        ret = resctrl_lock_shared();
        if (ret != PQOS_RETVAL_OK)
                goto os_mba_get_exit;

        for (class_id = 0; class_id < count; class_id++) {
                struct resctrl_schemata *schmt;

                schmt = resctrl_schemata_alloc(cap, cpu);
                if (schmt == NULL)
                        ret = PQOS_RETVAL_ERROR;

                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_alloc_schemata_read(class_id, schmt);

                if (ret == PQOS_RETVAL_OK)
                        ret = resctrl_schemata_mba_get(schmt, mba_id,
                                                       &mba_tab[class_id]);

                mba_tab[class_id].class_id = class_id;

                resctrl_schemata_free(schmt);

                if (ret != PQOS_RETVAL_OK)
                        goto os_mba_get_unlock;
        }
        *num_cos = count;

os_mba_get_unlock:
        resctrl_lock_release();

os_mba_get_exit:
        return ret;
}

int
os_alloc_assoc_set_pid(const pid_t task, const unsigned class_id)
{
        int ret;
        unsigned max_cos = 0;
        int ret_mon;
        char mon_group[256];
        const struct pqos_cap *cap;

        _pqos_cap_get(&cap, NULL);

        /* Get number of COS */
        ret = resctrl_alloc_get_grps_num(cap, &max_cos);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (class_id >= max_cos) {
                LOG_ERROR("COS out of bounds for task %d\n", (int)task);
                return PQOS_RETVAL_PARAM;
        }

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /*
         * When task is moved to different COS we need to update monitoring
         * groups. Obtain monitoring group name
         */
        ret_mon = resctrl_mon_assoc_get_pid(task, mon_group, sizeof(mon_group));
        if (ret_mon != PQOS_RETVAL_OK && ret_mon != PQOS_RETVAL_RESOURCE)
                LOG_WARN("Failed to obtain monitoring group assignment for "
                         "task %d\n",
                         task);

        /* Write to tasks file */
        ret = resctrl_alloc_assoc_set_pid(task, class_id);
        if (ret != PQOS_RETVAL_OK)
                goto os_alloc_assoc_set_pid_exit;

        /* Task monitoring was started assign it back to monitoring group */
        if (ret_mon == PQOS_RETVAL_OK) {
                ret_mon = resctrl_mon_assoc_set_pid(task, mon_group);
                if (ret_mon != PQOS_RETVAL_OK)
                        LOG_WARN("Could not assign task %d back to monitoring "
                                 "group\n",
                                 task);
        }

os_alloc_assoc_set_pid_exit:
        resctrl_lock_release();

        return ret;
}

int
os_alloc_assoc_get_pid(const pid_t task, unsigned *class_id)
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
        const struct pqos_cap *cap;

        ASSERT(task_num > 0);
        ASSERT(task_array != NULL);
        ASSERT(class_id != NULL);
        UNUSED_PARAM(technology);

        _pqos_cap_get(&cap, NULL);

        /* obtain highest class id for all requested technologies */
        ret = resctrl_alloc_get_grps_num(cap, &num_rctl_grps);
        if (ret != PQOS_RETVAL_OK)
                return ret;

        if (num_rctl_grps == 0)
                return PQOS_RETVAL_ERROR;

        ret = resctrl_lock_exclusive();
        if (ret != PQOS_RETVAL_OK)
                return ret;

        /* find an unused class from highest down */
        ret = resctrl_alloc_get_unused_group(num_rctl_grps, class_id);
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
os_alloc_release_pid(const pid_t *task_array, const unsigned task_num)
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
