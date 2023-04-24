/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
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

#include "uncore_monitoring.h"

#include "cap.h"
#include "cpu_registers.h"
#include "cpuinfo.h"
#include "log.h"
#include "machine.h"
#include "monitoring.h"

#include <dirent.h> /**< scandir() */
#include <fnmatch.h>
#include <string.h>

#define UNIT_CTRL_FREEZE_COUNTER   0x10100
#define UNIT_CTRL_UNFREEZE_COUNTER 0x10000
#define UNIT_CTRL_RESET_COUNTER    0x10102
#define UNIT_CTRL_RESET_CONTROL    0x10101

#define OFFSET_CTRL0   0x1
#define OFFSET_CTRL1   0x2
#define OFFSET_CTRL2   0x3
#define OFFSET_CTRL3   0x4
#define OFFSET_CTR0    0x8
#define OFFSET_CTR1    0x9
#define OFFSET_CTR2    0xA
#define OFFSET_CTR3    0xB
#define OFFSET_FILTER1 0x6

#define LOCAL_COUNTER_ENABLE 0x400000ULL

#define IAT_EVENT_TOR_INSERTS 0x35LL

#define SYS_DEVICES "/sys/devices"
#define UNCORE_CHA  "uncore_cha_"

#define UNCORE_CHA_MAX 40

/** uncore channels */
static uint64_t m_uncore_cha = 0;

/**
 * All supported uncore events mask
 */
static enum pqos_mon_event all_evt_mask = (enum pqos_mon_event)0;

/**
 * List of uncore events
 */
enum uncore_event {
        UNCORE_EVENT_LLC_MISS_PCIE_READ = 0,
        UNCORE_EVENT_LLC_MISS_PCIE_WRITE,
        UNCORE_EVENT_LLC_REF_PCIE_READ,
        UNCORE_EVENT_LLC_REF_PCIE_WRITE,

        UNCORE_EVENT_COUNT
};

/**
 * Uncore events mapping
 */
static enum pqos_mon_event uncore_event_map[] = {
    PQOS_PERF_EVENT_LLC_MISS_PCIE_READ,
    PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE,
    PQOS_PERF_EVENT_LLC_REF_PCIE_READ,
    PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE,
};

/**
 * Table of structures used to store data about
 * supported uncore monitoring events
 */
struct uncore_mon_event {
        int supported;
        uint64_t event;
        uint64_t umask;
        uint64_t xtra;
        uint64_t filter;
};

/**
 * Supported events details for Skylake platform
 */
static struct uncore_mon_event uncore_events_skx[UNCORE_EVENT_COUNT] = {
    {.supported = 1,
     .event = IAT_EVENT_TOR_INSERTS,
     .filter = 0x43C33,
     .umask = 0x24},
    {.supported = 1,
     .event = IAT_EVENT_TOR_INSERTS,
     .filter = 0x10049033,
     .umask = 0x24},
    {.supported = 1,
     .event = IAT_EVENT_TOR_INSERTS,
     .filter = 0x43C33,
     .umask = 0x14},
    {.supported = 1,
     .event = IAT_EVENT_TOR_INSERTS,
     .filter = 0x10049033,
     .umask = 0x14},
};

static struct uncore_mon_event *uncore_events = NULL;

/**
 * @brief Get the event details
 *
 * @param [in] event PQoS event ID
 * @return pointer to event details
 */
static struct uncore_mon_event *
get_event(const enum pqos_mon_event event)
{
        if (uncore_events == NULL)
                return NULL;

        switch (event) {
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
                return &uncore_events[UNCORE_EVENT_LLC_MISS_PCIE_READ];
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
                return &uncore_events[UNCORE_EVENT_LLC_MISS_PCIE_WRITE];
        case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
                return &uncore_events[UNCORE_EVENT_LLC_REF_PCIE_READ];
        case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                return &uncore_events[UNCORE_EVENT_LLC_REF_PCIE_WRITE];
        default:
                ASSERT(0);
                return NULL;
        }
}

/**
 * @brief channel filter for scandir.
 *
 * @param[in] dir scandir dirent entry
 *
 * @retval 0 for entries to be skipped
 * @retval 1 otherwise
 */
static int
filter_cha(const struct dirent *dir)
{
        return fnmatch(UNCORE_CHA "[0-9]*", dir->d_name, 0) == 0;
}

/**
 * @brief Discover uncore monitoring events
 *
 * @param [out] event Listo of supported events
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
int
uncore_mon_discover(enum pqos_mon_event *event)
{
        uint32_t model = cpuinfo_get_cpu_model();

        if (model == CPU_MODEL_SKX)
                *event =
                    (enum pqos_mon_event)(PQOS_PERF_EVENT_LLC_MISS_PCIE_READ |
                                          PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE |
                                          PQOS_PERF_EVENT_LLC_REF_PCIE_READ |
                                          PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE);
        else
                *event = (enum pqos_mon_event)0;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Obtain ctrl register for \a cha
 *
 * @param cha pci channel
 *
 * @return ctrl register address
 */
static uint32_t
uncore_unit_ctrl_skx(unsigned cha)
{
        ASSERT(cha < UNCORE_CHA_MAX);

        return IAT_MSR_C_UNIT_CTRL + 0x10 * cha;
}

static uint32_t (*uncore_unit_ctrl)(unsigned cha) = NULL;

int
uncore_mon_init(const struct pqos_cpuinfo *cpu, const struct pqos_cap *cap)
{
        int ret = PQOS_RETVAL_OK;
        struct dirent **namelist = NULL;
        int count;
        int i;

        UNUSED_PARAM(cap);
        UNUSED_PARAM(cpu);

        {
                uint32_t model = cpuinfo_get_cpu_model();

                if (model == CPU_MODEL_SKX) {
                        uncore_events = uncore_events_skx;
                        uncore_unit_ctrl = &uncore_unit_ctrl_skx;
                } else
                        return PQOS_RETVAL_RESOURCE;
        }

        count = scandir(SYS_DEVICES, &namelist, filter_cha, NULL);
        if (count <= 0 || count >= UNCORE_CHA_MAX)
                ret = PQOS_RETVAL_RESOURCE;
        else
                for (i = 0; i < count; i++) {
                        unsigned cha;

                        if (sscanf(namelist[i]->d_name, UNCORE_CHA "%u",
                                   &cha) != 1 ||
                            cha >= UNCORE_CHA_MAX) {
                                LOG_ERROR(
                                    "Could not parse uncore channel number\n");
                                ret = PQOS_RETVAL_ERROR;
                                break;
                        }

                        m_uncore_cha |= (uint64_t)(1LLU << cha);
                }

        LOG_DEBUG("Detected %lx uncore channels\n", m_uncore_cha);

        for (i = 0; i < count; i++)
                free(namelist[i]);
        free(namelist);

        for (i = 0; i < UNCORE_EVENT_COUNT; ++i) {
                struct uncore_mon_event *evt = &uncore_events[i];

                if (!evt->supported)
                        continue;

                all_evt_mask |= uncore_event_map[i];
        }

        return ret;
}

int
uncore_mon_fini(void)
{
        m_uncore_cha = 0;
        all_evt_mask = (enum pqos_mon_event)0;
        uncore_events = NULL;
        uncore_unit_ctrl = NULL;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Setup uncore monitoring counter
 *
 * @param [in] lcore logical core
 * @param [in] event PQoS event ID
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
_setup_counter(unsigned lcore, enum pqos_mon_event event)
{
        int ret = PQOS_RETVAL_OK;
        struct uncore_mon_event *evt = get_event(event);
        uint32_t reg_unit_ctrl;
        uint32_t reg_ctrl;
        uint32_t reg_filter1;

        if (evt == NULL)
                return PQOS_RETVAL_PARAM;
        if (!evt->supported)
                return PQOS_RETVAL_RESOURCE;

        switch (event) {
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
                reg_unit_ctrl =
                    uncore_unit_ctrl(UNCORE_EVENT_LLC_MISS_PCIE_READ);
                reg_ctrl = reg_unit_ctrl + OFFSET_CTRL0;
                reg_filter1 = reg_unit_ctrl + OFFSET_FILTER1;
                break;
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
                reg_unit_ctrl =
                    uncore_unit_ctrl(UNCORE_EVENT_LLC_MISS_PCIE_WRITE);
                reg_ctrl = reg_unit_ctrl + OFFSET_CTRL0;
                reg_filter1 = reg_unit_ctrl + OFFSET_FILTER1;
                break;
        case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
                reg_unit_ctrl =
                    uncore_unit_ctrl(UNCORE_EVENT_LLC_REF_PCIE_READ);
                reg_ctrl = reg_unit_ctrl + OFFSET_CTRL0;
                reg_filter1 = reg_unit_ctrl + OFFSET_FILTER1;
                break;
        case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                reg_unit_ctrl =
                    uncore_unit_ctrl(UNCORE_EVENT_LLC_REF_PCIE_WRITE);
                reg_ctrl = reg_unit_ctrl + OFFSET_CTRL0;
                reg_filter1 = reg_unit_ctrl + OFFSET_FILTER1;
                break;
        default:
                return PQOS_RETVAL_PARAM;
        }

        /* unfreeze counters */
        ret = msr_write(lcore, reg_unit_ctrl, UNIT_CTRL_UNFREEZE_COUNTER);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        /* freeze conuters */
        ret = msr_write(lcore, reg_unit_ctrl, UNIT_CTRL_FREEZE_COUNTER);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        /* select event */
        {
                uint64_t val = evt->xtra << 32 | LOCAL_COUNTER_ENABLE |
                               evt->umask << 8 | evt->event;
                uint32_t reg = reg_ctrl;

                ret = msr_write(lcore, reg, val);
                if (ret != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }

        /* set filter */
        if (evt->filter != 0) {
                uint64_t val = evt->filter;
                uint32_t reg = reg_filter1;

                ret = msr_write(lcore, reg, val);
                if (ret != MACHINE_RETVAL_OK)
                        return PQOS_RETVAL_ERROR;
        }

        /* reset counters */
        ret = msr_write(lcore, reg_unit_ctrl, UNIT_CTRL_RESET_COUNTER);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        /* unfreeze counters */
        ret = msr_write(lcore, reg_unit_ctrl, UNIT_CTRL_UNFREEZE_COUNTER);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        return ret;
}

int
uncore_mon_start(struct pqos_mon_data *group, enum pqos_mon_event event)
{
        int ret = PQOS_RETVAL_OK;
        unsigned s;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        for (s = 0; s < group->intl->uncore.num_sockets; ++s) {
                unsigned lcore;
                unsigned socket = group->intl->uncore.sockets[s];
                unsigned i;

                ret = pqos_cpu_get_one_core(cpu, socket, &lcore);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                for (i = 0; i < UNCORE_EVENT_COUNT; ++i) {
                        if ((event & uncore_event_map[i]) == 0)
                                continue;

                        ret = _setup_counter(lcore, uncore_event_map[i]);
                        if (ret != PQOS_RETVAL_OK)
                                break;
                }
        }

        group->intl->hw.event |= (enum pqos_mon_event)(event & all_evt_mask);

        return ret;
}

int
uncore_mon_stop(struct pqos_mon_data *group)
{
        int ret;
        unsigned s;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        for (s = 0; s < group->intl->uncore.num_sockets; ++s) {
                unsigned lcore;
                unsigned socket = group->intl->uncore.sockets[s];
                unsigned i;

                ret = pqos_cpu_get_one_core(cpu, socket, &lcore);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                for (i = 0; i < UNCORE_EVENT_COUNT; ++i) {
                        uint32_t reg_unit_ctrl = uncore_unit_ctrl(i);

                        if ((group->intl->hw.event & uncore_event_map[i]) == 0)
                                continue;

                        ret = msr_write(lcore, reg_unit_ctrl,
                                        UNIT_CTRL_RESET_CONTROL);
                        if (ret != MACHINE_RETVAL_OK)
                                return PQOS_RETVAL_ERROR;
                }
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Read uncore monitoring counter value
 *
 * @param [in] lcore logical core
 * @param [in] event PQoS event ID
 * @param [out] val counter value
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
static int
_read_counter(unsigned lcore, enum pqos_mon_event event, uint64_t *val)
{
        int ret;
        uint32_t reg_unit_ctrl;
        uint32_t reg_ctr;

        switch (event) {
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
                reg_unit_ctrl =
                    uncore_unit_ctrl(UNCORE_EVENT_LLC_MISS_PCIE_READ);
                reg_ctr = reg_unit_ctrl + OFFSET_CTR0;
                break;
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
                reg_unit_ctrl =
                    uncore_unit_ctrl(UNCORE_EVENT_LLC_MISS_PCIE_WRITE);
                reg_ctr = reg_unit_ctrl + OFFSET_CTR0;
                break;
        case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
                reg_unit_ctrl =
                    uncore_unit_ctrl(UNCORE_EVENT_LLC_REF_PCIE_READ);
                reg_ctr = reg_unit_ctrl + OFFSET_CTR0;
                break;
        case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                reg_unit_ctrl =
                    uncore_unit_ctrl(UNCORE_EVENT_LLC_REF_PCIE_WRITE);
                reg_ctr = reg_unit_ctrl + OFFSET_CTR0;
                break;

        default:
                return PQOS_RETVAL_PARAM;
        }

        ret = msr_read(lcore, reg_ctr, val);
        if (ret != MACHINE_RETVAL_OK)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

int
uncore_mon_poll(struct pqos_mon_data *group, const enum pqos_mon_event event)
{
        int ret;
        uint64_t old_value;
        uint64_t value = 0;
        unsigned i;
        const struct pqos_cpuinfo *cpu = _pqos_get_cpu();

        for (i = 0; i < group->intl->uncore.num_sockets; ++i) {
                unsigned lcore;
                unsigned socket = group->intl->uncore.sockets[i];
                uint64_t val;

                ret = pqos_cpu_get_one_core(cpu, socket, &lcore);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                ret = _read_counter(lcore, event, &val);
                if (ret != PQOS_RETVAL_OK)
                        return ret;

                value += val;
        }

        switch (event) {
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_READ:
                old_value = group->intl->values.pcie.llc_misses.read;
                group->intl->values.pcie.llc_misses.read = value;
                group->intl->values.pcie.llc_misses.read_delta =
                    value - old_value;
                break;
        case PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE:
                old_value = group->intl->values.pcie.llc_misses.write;
                group->intl->values.pcie.llc_misses.write = value;
                group->intl->values.pcie.llc_misses.write_delta =
                    value - old_value;
                break;
        case PQOS_PERF_EVENT_LLC_REF_PCIE_READ:
                old_value = group->intl->values.pcie.llc_references.read;
                group->intl->values.pcie.llc_references.read = value;
                group->intl->values.pcie.llc_references.read_delta =
                    value - old_value;
                break;
        case PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE:
                old_value = group->intl->values.pcie.llc_references.write;
                group->intl->values.pcie.llc_references.write = value;
                group->intl->values.pcie.llc_references.write_delta =
                    value - old_value;
                break;
        default:
                return PQOS_RETVAL_PARAM;
        }

        return PQOS_RETVAL_OK;
}

int
uncore_mon_is_event_supported(const enum pqos_mon_event event)
{
        struct uncore_mon_event *se = get_event(event);

        if (se == NULL) {
                LOG_ERROR("Unsupported event selected\n");
                return 0;
        }
        return se->supported;
}
