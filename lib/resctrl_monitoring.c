/*
 * BSD LICENSE
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
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
 *
 */

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "pqos.h"
#include "log.h"
#include "types.h"
#include "resctrl_monitoring.h"
#include "resctrl.h"


#define RESCTRL_PATH_INFO_L3_MON RESCTRL_PATH_INFO"/L3_MON"

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */
static const struct pqos_cap *m_cap = NULL;
static const struct pqos_cpuinfo *m_cpu = NULL;

static int supported_events = 0;

/**
 * @brief Update monitoring capability structure with supported events
 *
 * @param cap pqos capability structure
 *
 * @return Operational Status
 * @retval PQOS_RETVAL_OK on success
 */
static void
set_mon_caps(const struct pqos_cap *cap)
{
        int ret;
        unsigned i;
        const struct pqos_capability *p_cap = NULL;

        ASSERT(cap != NULL);

        /* find monitoring capability */
        ret = pqos_cap_get_type(cap, PQOS_CAP_TYPE_MON, &p_cap);
        if (ret != PQOS_RETVAL_OK)
                return;

        /* update capabilities structure */
        for (i = 0; i < p_cap->u.mon->num_events; i++) {
                struct pqos_monitor *mon = &p_cap->u.mon->events[i];

                if (supported_events & mon->type)
                        mon->os_support = PQOS_OS_MON_RESCTRL;
        }
}


int
resctrl_mon_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
        int ret;
        char buf[64];
        FILE *fd;
        struct stat st;

        ASSERT(cpu != NULL);
        ASSERT(cap != NULL);

        /**
         * Check if resctrl is mounted
         */
        if (stat(RESCTRL_PATH_INFO, &st) != 0) {
                ret = resctrl_mount(PQOS_REQUIRE_CDP_OFF, PQOS_REQUIRE_CDP_OFF);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_INFO("Unable to mount resctrl\n");
                        return PQOS_RETVAL_RESOURCE;
                }
        }

        /**
         * Resctrl monitiring not supported
         */
        if (stat(RESCTRL_PATH_INFO_L3_MON, &st) != 0)
                return PQOS_RETVAL_OK;

        /**
         * Discover supported events
         */
        fd = fopen(RESCTRL_PATH_INFO_L3_MON"/mon_features", "r");
        if (fd == NULL) {
                LOG_ERROR("Failed to obtain resctrl monitoring features\n");
                return PQOS_RETVAL_ERROR;
        }

        while (fgets(buf, sizeof(buf), fd) != NULL) {
                if (strncmp(buf, "llc_occupancy\n", sizeof(buf)) == 0) {
                        LOG_INFO("Detected resctrl support for "
                                 "LLC Occupancy\n");
                        supported_events |= PQOS_MON_EVENT_L3_OCCUP;
                        continue;
                }

                if (strncmp(buf, "mbm_local_bytes\n", sizeof(buf)) == 0) {
                        LOG_INFO("Detected resctrl support for "
                                 "Local Memory B/W\n");
                        supported_events |= PQOS_MON_EVENT_LMEM_BW;
                        continue;
                }

                if (strncmp(buf, "mbm_total_bytes\n", sizeof(buf)) == 0) {
                        LOG_INFO("Detected resctrl support "
                                "Total Memory B/W\n");
                        supported_events |= PQOS_MON_EVENT_TMEM_BW;
                }
        }

        if ((supported_events & PQOS_MON_EVENT_LMEM_BW) &&
                (supported_events & PQOS_MON_EVENT_TMEM_BW))
                supported_events |= PQOS_MON_EVENT_RMEM_BW;

        fclose(fd);

        /**
         * Update mon capabilities
         */
        set_mon_caps(cap);

        m_cap = cap;
        m_cpu = cpu;

        return ret;
}

int
resctrl_mon_fini(void)
{
        m_cap = NULL;
        m_cpu = NULL;

        return PQOS_RETVAL_OK;
}

int
resctrl_mon_is_event_supported(const enum pqos_mon_event event)
{
        return (supported_events & event) == event;
}
