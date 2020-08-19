/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2020 Intel Corporation. All rights reserved.
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

/**
 * @brief Host implementation of PQoS API / capabilities.
 *
 * This module is responsible for PQoS management and capability
 * functionalities.
 *
 * Management functions include:
 * - management includes initializing and shutting down all other sub-modules
 *   including: monitoring, allocation, log, cpuinfo and machine
 * - provide functions for safe access to PQoS API - this is required for
 *   allocation and monitoring modules which also implement PQoS API
 *
 * Capability functions:
 * - monitoring detection, this is to discover all monitoring event types.
 * - LLC allocation detection, this is to discover last level cache
 *   allocation feature.
 * - A new targeted function has to be implemented to discover new allocation
 *   technology.
 */

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>    /* O_CREAT */
#include <unistd.h>   /* usleep(), lockf() */
#include <sys/stat.h> /* S_Ixxx */
#include <pthread.h>

#include "cap.h"
#include "os_cap.h"
#include "hw_cap.h"
#include "allocation.h"
#include "monitoring.h"
#include "cpuinfo.h"
#include "machine.h"
#include "types.h"
#include "log.h"
#include "api.h"
#include "utils.h"
#include "resctrl.h"

/**
 * ---------------------------------------
 * Local macros
 * ---------------------------------------
 */

#ifndef LOCKFILE
#ifdef __linux__
#define LOCKFILE "/var/lock/libpqos"
#endif
#ifdef __FreeBSD__
#define LOCKFILE "/var/tmp/libpqos.lockfile"
#endif
#endif /*!LOCKFILE*/

#define PROC_CPUINFO "/proc/cpuinfo"

/**
 * ---------------------------------------
 * Local data types
 * ---------------------------------------
 */

/**
 * ---------------------------------------
 * Local data structures
 * ---------------------------------------
 */

/**
 * This pointer is allocated and initialized in this module.
 * Then other sub-modules get this pointer in order to retrieve
 * capability information.
 */
static struct pqos_cap *m_cap = NULL;

/**
 * This gets allocated and initialized in this module.
 * This hold information about CPU topology in PQoS format.
 */
static const struct pqos_cpuinfo *m_cpu = NULL;

/**
 * Library initialization status.
 */
static int m_init_done = 0;

/**
 * API thread/process safe access is secured through these locks.
 */
static int m_apilock = -1;
static pthread_mutex_t m_apilock_mutex;

/**
 * Interface status
 *   0  PQOS_INTER_MSR
 *   1  PQOS_INTER_OS
 *   2  PQOS_INTER_OS_RESCTRL_MON
 */
static enum pqos_interface m_interface = PQOS_INTER_MSR;
/**
 * ---------------------------------------
 * Functions for safe multi-threading
 * ---------------------------------------
 */

/**
 * @brief Initializes API locks
 *
 * @return Operation status
 * @retval 0 success
 * @retval -1 error
 */
static int
_pqos_api_init(void)
{

        const char *lock_filename = LOCKFILE;

        if (m_apilock != -1)
                return -1;

        m_apilock = open(lock_filename, O_WRONLY | O_CREAT,
                         S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (m_apilock == -1)
                return -1;

        if (pthread_mutex_init(&m_apilock_mutex, NULL) != 0) {
                close(m_apilock);
                m_apilock = -1;
                return -1;
        }

        return 0;
}

/**
 * @brief Uninitializes API locks
 *
 * @return Operation status
 * @retval 0 success
 * @retval -1 error
 */
static int
_pqos_api_exit(void)
{
        int ret = 0;

        if (close(m_apilock) != 0)
                ret = -1;

        if (pthread_mutex_destroy(&m_apilock_mutex) != 0)
                ret = -1;

        m_apilock = -1;

        return ret;
}

void
_pqos_api_lock(void)
{
        int err = 0;

        if (lockf(m_apilock, F_LOCK, 0) != 0)
                err = 1;

        if (pthread_mutex_lock(&m_apilock_mutex) != 0)
                err = 1;

        if (err)
                LOG_ERROR("API lock error!\n");
}

void
_pqos_api_unlock(void)
{
        int err = 0;

        if (lockf(m_apilock, F_ULOCK, 0) != 0)
                err = 1;

        if (pthread_mutex_unlock(&m_apilock_mutex) != 0)
                err = 1;

        if (err)
                LOG_ERROR("API unlock error!\n");
}

/**
 * ---------------------------------------
 * Function for library initialization
 * ---------------------------------------
 */

int
_pqos_check_init(const int expect)
{
        if (m_init_done && (!expect)) {
                LOG_ERROR("PQoS library already initialized\n");
                return PQOS_RETVAL_INIT;
        }

        if ((!m_init_done) && expect) {
                LOG_ERROR("PQoS library not initialized\n");
                return PQOS_RETVAL_INIT;
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Discovers support of L3 CAT
 *
 * @param[out] r_cap place to store L3 CAT capabilities structure
 * @param[in] cpu detected cpu topology
 * @param[in] iface Selected interface
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
cap_l3ca_discover(struct pqos_cap_l3ca **r_cap,
                  const struct pqos_cpuinfo *cpu,
                  const enum pqos_interface iface)
{
        struct pqos_cap_l3ca *cap = NULL;
        int ret;

        cap = (struct pqos_cap_l3ca *)malloc(sizeof(*cap));
        if (cap == NULL)
                return PQOS_RETVAL_RESOURCE;

        switch (iface) {
        case PQOS_INTER_MSR:
                ret = hw_cap_l3ca_discover(cap, cpu);
                break;
#ifdef __linux__
        case PQOS_INTER_OS:
        case PQOS_INTER_OS_RESCTRL_MON:
                ret = os_cap_l3ca_discover(cap, cpu);
                break;
#endif
        default:
                ret = PQOS_RETVAL_RESOURCE;
                break;
        }

        if (ret == PQOS_RETVAL_OK)
                *r_cap = cap;
        else
                free(cap);

        return ret;
}

/**
 * @brief Discovers support of L2 CAT
 *
 * @param[out] r_cap place to store L2 CAT capabilities structure
 * @param[in] cpu detected cpu topology
 * @param[in] iface Selected interface
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
cap_l2ca_discover(struct pqos_cap_l2ca **r_cap,
                  const struct pqos_cpuinfo *cpu,
                  const enum pqos_interface iface)
{
        struct pqos_cap_l2ca *cap = NULL;
        int ret;

        cap = (struct pqos_cap_l2ca *)malloc(sizeof(*cap));
        if (cap == NULL)
                return PQOS_RETVAL_RESOURCE;

        switch (iface) {
        case PQOS_INTER_MSR:
                ret = hw_cap_l2ca_discover(cap, cpu);
                break;
#ifdef __linux__
        case PQOS_INTER_OS:
        case PQOS_INTER_OS_RESCTRL_MON:
                ret = os_cap_l2ca_discover(cap, cpu);
                break;
#endif
        default:
                ret = PQOS_RETVAL_RESOURCE;
                break;
        }

        if (ret == PQOS_RETVAL_OK)
                *r_cap = cap;
        else
                free(cap);

        return ret;
}

/**
 * @brief Discovers support of MBA
 *
 * @param[out] r_cap place to store MBA capabilities structure
 * @param[in] cpu detected cpu topology
 * @param[in] iface Selected interface
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
cap_mba_discover(struct pqos_cap_mba **r_cap,
                 const struct pqos_cpuinfo *cpu,
                 const enum pqos_interface iface)
{
        struct pqos_cap_mba *cap = NULL;
        int ret;

        cap = (struct pqos_cap_mba *)malloc(sizeof(*cap));
        if (cap == NULL)
                return PQOS_RETVAL_RESOURCE;

        switch (iface) {
        case PQOS_INTER_MSR:
                if (cpu->vendor == PQOS_VENDOR_AMD)
                        ret = amd_cap_mba_discover(cap, cpu);
                else
                        ret = hw_cap_mba_discover(cap, cpu);
                break;
#ifdef __linux__
        case PQOS_INTER_OS:
        case PQOS_INTER_OS_RESCTRL_MON:
                ret = os_cap_mba_discover(cap, cpu);
                break;
#endif
        default:
                ret = PQOS_RETVAL_RESOURCE;
                break;
        }

        if (ret == PQOS_RETVAL_OK)
                *r_cap = cap;
        else
                free(cap);

        return ret;
}

/**
 * @brief Runs detection of platform monitoring and allocation capabilities
 *
 * @param p_cap place to store allocated capabilities structure
 * @param cpu detected cpu topology
 * @param inter selected pqos interface
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
discover_capabilities(struct pqos_cap **p_cap,
                      const struct pqos_cpuinfo *cpu,
                      enum pqos_interface inter)
{
        struct pqos_cap_mon *det_mon = NULL;
        struct pqos_cap_l3ca *det_l3ca = NULL;
        struct pqos_cap_l2ca *det_l2ca = NULL;
        struct pqos_cap_mba *det_mba = NULL;
        struct pqos_cap *_cap = NULL;
        unsigned sz = 0;
        int ret = PQOS_RETVAL_RESOURCE;

        /**
         * Monitoring init
         */
        if (inter == PQOS_INTER_MSR)
                ret = hw_cap_mon_discover(&det_mon, cpu);
#ifdef __linux__
        else if (inter == PQOS_INTER_OS || inter == PQOS_INTER_OS_RESCTRL_MON)
                ret = os_cap_mon_discover(&det_mon, cpu);
#endif
        switch (ret) {
        case PQOS_RETVAL_OK:
                LOG_INFO("Monitoring capability detected\n");
                sz += sizeof(struct pqos_capability);
                break;
        case PQOS_RETVAL_RESOURCE:
                LOG_INFO("Monitoring capability not detected\n");
                break;
        default:
                LOG_ERROR("Error encounter in monitoring discovery!\n");
                ret = PQOS_RETVAL_ERROR;
                goto error_exit;
        }

        /**
         * L3 Cache allocation init
         */
        ret = cap_l3ca_discover(&det_l3ca, cpu, inter);
        switch (ret) {
        case PQOS_RETVAL_OK:
                LOG_INFO("L3CA capability detected\n");
                LOG_INFO("L3 CAT details: CDP support=%d, CDP on=%d, "
                         "#COS=%u, #ways=%u, ways contention bit-mask 0x%lx\n",
                         det_l3ca->cdp, det_l3ca->cdp_on, det_l3ca->num_classes,
                         det_l3ca->num_ways,
                         (unsigned long)det_l3ca->way_contention);
                LOG_INFO("L3 CAT details: cache size %u bytes, "
                         "way size %u bytes\n",
                         det_l3ca->way_size * det_l3ca->num_ways,
                         det_l3ca->way_size);
                sz += sizeof(struct pqos_capability);
                break;
        case PQOS_RETVAL_RESOURCE:
                LOG_INFO("L3CA capability not detected\n");
                break;
        default:
                LOG_ERROR("Fatal error encounter in L3 CAT discovery!\n");
                ret = PQOS_RETVAL_ERROR;
                goto error_exit;
        }

        /**
         * L2 Cache allocation init
         */
        ret = cap_l2ca_discover(&det_l2ca, cpu, inter);
        switch (ret) {
        case PQOS_RETVAL_OK:
                LOG_INFO("L2CA capability detected\n");
                LOG_INFO("L2 CAT details: CDP support=%d, CDP on=%d, "
                         "#COS=%u, #ways=%u, ways contention bit-mask 0x%lx\n",
                         det_l2ca->cdp, det_l2ca->cdp_on, det_l2ca->num_classes,
                         det_l2ca->num_ways,
                         (unsigned long)det_l2ca->way_contention);
                LOG_INFO("L2 CAT details: cache size %u bytes, way size %u "
                         "bytes\n",
                         det_l2ca->way_size * det_l2ca->num_ways,
                         det_l2ca->way_size);
                sz += sizeof(struct pqos_capability);
                break;
        case PQOS_RETVAL_RESOURCE:
                LOG_INFO("L2CA capability not detected\n");
                break;
        default:
                LOG_ERROR("Fatal error encounter in L2 CAT discovery!\n");
                ret = PQOS_RETVAL_ERROR;
                goto error_exit;
        }

        /**
         * Memory bandwidth allocation init
         */
        ret = cap_mba_discover(&det_mba, cpu, inter);
        switch (ret) {
        case PQOS_RETVAL_OK:
                LOG_INFO("MBA capability detected\n");
                LOG_INFO("MBA details: "
                         "#COS=%u, %slinear, max=%u, step=%u\n",
                         det_mba->num_classes, det_mba->is_linear ? "" : "non-",
                         det_mba->throttle_max, det_mba->throttle_step);
                sz += sizeof(struct pqos_capability);
                break;
        case PQOS_RETVAL_RESOURCE:
                LOG_INFO("MBA capability not detected\n");
                break;
        default:
                LOG_ERROR("Fatal error encounter in MBA discovery!\n");
                ret = PQOS_RETVAL_ERROR;
                goto error_exit;
        }

        if (sz == 0) {
                LOG_ERROR("No Platform QoS capability discovered\n");
                ret = PQOS_RETVAL_ERROR;
                goto error_exit;
        }

        sz += sizeof(struct pqos_cap);
        _cap = (struct pqos_cap *)malloc(sz);
        if (_cap == NULL) {
                LOG_ERROR("Allocation error in %s()\n", __func__);
                ret = PQOS_RETVAL_ERROR;
                goto error_exit;
        }

        memset(_cap, 0, sz);
        _cap->mem_size = sz;
        _cap->version = PQOS_VERSION;

        if (det_mon != NULL) {
                _cap->capabilities[_cap->num_cap].type = PQOS_CAP_TYPE_MON;
                _cap->capabilities[_cap->num_cap].u.mon = det_mon;
                _cap->num_cap++;
                ret = PQOS_RETVAL_OK;
        }

        if (det_l3ca != NULL) {
                _cap->capabilities[_cap->num_cap].type = PQOS_CAP_TYPE_L3CA;
                _cap->capabilities[_cap->num_cap].u.l3ca = det_l3ca;
                _cap->num_cap++;
                ret = PQOS_RETVAL_OK;
        }

        if (det_l2ca != NULL) {
                _cap->capabilities[_cap->num_cap].type = PQOS_CAP_TYPE_L2CA;
                _cap->capabilities[_cap->num_cap].u.l2ca = det_l2ca;
                _cap->num_cap++;
                ret = PQOS_RETVAL_OK;
        }

        if (det_mba != NULL) {
                _cap->capabilities[_cap->num_cap].type = PQOS_CAP_TYPE_MBA;
                _cap->capabilities[_cap->num_cap].u.mba = det_mba;
                _cap->num_cap++;
                ret = PQOS_RETVAL_OK;
#ifdef __linux__
                /**
                 * Check status of MBA CTRL
                 */
                if (inter == PQOS_INTER_OS ||
                    inter == PQOS_INTER_OS_RESCTRL_MON) {
                        ret = os_cap_get_mba_ctrl(_cap, cpu, &det_mba->ctrl,
                                                  &det_mba->ctrl_on);
                        if (ret != PQOS_RETVAL_OK)
                                goto error_exit;
                }
#endif
        }

        (*p_cap) = _cap;

error_exit:
        if (ret != PQOS_RETVAL_OK) {
                if (det_mon != NULL)
                        free(det_mon);
                if (det_l3ca != NULL)
                        free(det_l3ca);
                if (det_l2ca != NULL)
                        free(det_l2ca);
                if (det_mba != NULL)
                        free(det_mba);
                if (_cap != NULL)
                        free(_cap);
        }

        return ret;
}

/*
 * =======================================
 * =======================================
 *
 * initialize and shutdown
 *
 * =======================================
 * =======================================
 */
int
pqos_init(const struct pqos_config *config)
{
        int ret = PQOS_RETVAL_OK;
        unsigned i = 0, max_core = 0;
        int cat_init = 0, mon_init = 0;
        char *environment = NULL;

        if (config == NULL)
                return PQOS_RETVAL_PARAM;

#ifdef __linux__
        if (config->interface != PQOS_INTER_MSR &&
            config->interface != PQOS_INTER_OS &&
            config->interface != PQOS_INTER_OS_RESCTRL_MON)
#else
        if (config->interface != PQOS_INTER_MSR)
#endif
                return PQOS_RETVAL_PARAM;

        environment = getenv("RDT_IFACE");
        if (environment != NULL) {
                if (strncasecmp(environment, "OS", 2) == 0) {
                        if (config->interface != PQOS_INTER_OS) {
                                fprintf(stderr,
                                        "Interface initialization error!\n"
                                        "Your system has been restricted "
                                        "to use the OS interface only!\n");
                                return PQOS_RETVAL_ERROR;
                        }
                } else if (strncasecmp(environment, "MSR", 3) == 0) {
                        if (config->interface != PQOS_INTER_MSR) {
                                fprintf(stderr,
                                        "Interface initialization error!\n"
                                        "Your system has been restricted "
                                        "to use the MSR interface only!\n");
                                return PQOS_RETVAL_ERROR;
                        }
                } else {
                        fprintf(stderr,
                                "Interface initialization error!\n"
                                "Invalid interface enforcement selection.\n");
                        return PQOS_RETVAL_ERROR;
                }
        }

        if (_pqos_api_init() != 0) {
                fprintf(stderr, "API lock initialization error!\n");
                return PQOS_RETVAL_ERROR;
        }

        _pqos_api_lock();

        ret = _pqos_check_init(0);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        ret = log_init(config->fd_log, config->callback_log,
                       config->context_log, config->verbose);
        if (ret != LOG_RETVAL_OK) {
                fprintf(stderr, "log_init() error\n");
                goto init_error;
        }

        /**
         * Topology not provided through config.
         * CPU discovery done through internal mechanism.
         */
        ret = cpuinfo_init(&m_cpu);
        if (ret != 0 || m_cpu == NULL) {
                LOG_ERROR("cpuinfo_init() error %d\n", ret);
                ret = PQOS_RETVAL_ERROR;
                goto log_init_error;
        }

        /**
         * Find max core id in the topology
         */
        for (i = 0; i < m_cpu->num_cores; i++)
                if (m_cpu->cores[i].lcore > max_core)
                        max_core = m_cpu->cores[i].lcore;

        ret = machine_init(max_core);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("machine_init() error %d\n", ret);
                goto cpuinfo_init_error;
        }

#ifdef __linux__
        if (config->interface == PQOS_INTER_OS ||
            config->interface == PQOS_INTER_OS_RESCTRL_MON) {
                ret = os_cap_init(config->interface);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("os_cap_init() error %d\n", ret);
                        goto machine_init_error;
                }
        } else if (access(RESCTRL_PATH "/cpus", F_OK) == 0)
                LOG_WARN("resctl filesystem mounted! Using MSR "
                         "interface may corrupt resctrl filesystem "
                         "and cause unexpected behaviour\n");
#endif

        ret = discover_capabilities(&m_cap, m_cpu, config->interface);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("discover_capabilities() error %d\n", ret);
                goto machine_init_error;
        }

        ret = _pqos_utils_init(config->interface);
        if (ret != PQOS_RETVAL_OK) {
                fprintf(stderr, "Utils initialization error!\n");
                goto machine_init_error;
        }

        ret = api_init(config->interface, m_cpu->vendor);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("_pqos_api_init() error %d\n", ret);
                goto machine_init_error;
        }

        m_interface = config->interface;

        ret = pqos_alloc_init(m_cpu, m_cap, config);
        switch (ret) {
        case PQOS_RETVAL_BUSY:
                LOG_ERROR("OS allocation init error!\n");
                goto machine_init_error;
        case PQOS_RETVAL_OK:
                LOG_DEBUG("allocation init OK\n");
                cat_init = 1;
                break;
        default:
                LOG_ERROR("allocation init error %d\n", ret);
                break;
        }

        /**
         * If monitoring capability has been discovered
         * then get max RMID supported by a CPU socket
         * and allocate memory for RMID table
         */
        ret = pqos_mon_init(m_cpu, m_cap, config);
        switch (ret) {
        case PQOS_RETVAL_RESOURCE:
                LOG_DEBUG("monitoring init aborted: feature not present\n");
                ret = PQOS_RETVAL_OK;
                break;
        case PQOS_RETVAL_OK:
                LOG_DEBUG("monitoring init OK\n");
                mon_init = 1;
                break;
        case PQOS_RETVAL_ERROR:
        default:
                LOG_ERROR("monitoring init error %d\n", ret);
                break;
        }

        if (cat_init == 0 && mon_init == 0) {
                LOG_ERROR("None of detected capabilities could be "
                          "initialized!\n");
                ret = PQOS_RETVAL_ERROR;
        }

machine_init_error:
        if (ret != PQOS_RETVAL_OK)
                (void)machine_fini();
cpuinfo_init_error:
        if (ret != PQOS_RETVAL_OK)
                (void)cpuinfo_fini();
log_init_error:
        if (ret != PQOS_RETVAL_OK)
                (void)log_fini();
init_error:
        if (ret != PQOS_RETVAL_OK) {
                if (m_cap != NULL) {
                        for (i = 0; i < m_cap->num_cap; i++)
                                free(m_cap->capabilities[i].u.generic_ptr);
                        free(m_cap);
                }
                m_cpu = NULL;
                m_cap = NULL;
        }

        if (ret == PQOS_RETVAL_OK)
                m_init_done = 1;

        _pqos_api_unlock();

        if (ret != PQOS_RETVAL_OK)
                _pqos_api_exit();

        return ret;
}

int
pqos_fini(void)
{
        int ret = PQOS_RETVAL_OK;
        int retval = PQOS_RETVAL_OK;
        unsigned i = 0;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                _pqos_api_exit();
                return ret;
        }

        pqos_mon_fini();
        pqos_alloc_fini();

        ret = cpuinfo_fini();
        if (ret != 0) {
                retval = PQOS_RETVAL_ERROR;
                LOG_ERROR("cpuinfo_fini() error %d\n", ret);
        }

        ret = machine_fini();
        if (ret != PQOS_RETVAL_OK) {
                retval = ret;
                LOG_ERROR("machine_fini() error %d\n", ret);
        }

        ret = log_fini();
        if (ret != PQOS_RETVAL_OK)
                retval = ret;

        m_cpu = NULL;

        for (i = 0; i < m_cap->num_cap; i++)
                free(m_cap->capabilities[i].u.generic_ptr);
        free((void *)m_cap);
        m_cap = NULL;

        m_init_done = 0;

        _pqos_api_unlock();

        if (_pqos_api_exit() != 0)
                retval = PQOS_RETVAL_ERROR;

        return retval;
}

/**
 * =======================================
 * =======================================
 *
 * capabilities
 *
 * =======================================
 * =======================================
 */

int
pqos_cap_get(const struct pqos_cap **cap, const struct pqos_cpuinfo **cpu)
{
        int ret = PQOS_RETVAL_OK;

        if (cap == NULL && cpu == NULL)
                return PQOS_RETVAL_PARAM;

        _pqos_api_lock();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                _pqos_api_unlock();
                return ret;
        }

        _pqos_cap_get(cap, cpu);

        _pqos_api_unlock();
        return PQOS_RETVAL_OK;
}

void
_pqos_cap_l3cdp_change(const enum pqos_cdp_config cdp)
{
        struct pqos_cap_l3ca *l3_cap = NULL;
        struct pqos_cap_l3ca cap;
        int ret;
        unsigned i;

        ASSERT(cdp == PQOS_REQUIRE_CDP_ON || cdp == PQOS_REQUIRE_CDP_OFF ||
               cdp == PQOS_REQUIRE_CDP_ANY);
        ASSERT(m_cap != NULL);

        if (m_cap == NULL)
                return;

        for (i = 0; i < m_cap->num_cap && l3_cap == NULL; i++)
                if (m_cap->capabilities[i].type == PQOS_CAP_TYPE_L3CA)
                        l3_cap = m_cap->capabilities[i].u.l3ca;

        if (l3_cap == NULL)
                return;

        switch (m_interface) {
        case PQOS_INTER_MSR:
                ret = hw_cap_l3ca_discover(&cap, m_cpu);
                break;
#ifdef __linux__
        case PQOS_INTER_OS: /* fall through */
        case PQOS_INTER_OS_RESCTRL_MON:
                ret = os_cap_l3ca_discover(&cap, m_cpu);
                break;
#endif
        default:
                ret = PQOS_RETVAL_RESOURCE;
                break;
        }

        if (ret == PQOS_RETVAL_OK) {
                *l3_cap = cap;
                return;
        }

        if (cdp == PQOS_REQUIRE_CDP_ON && !l3_cap->cdp_on) {
                /* turn on */
                l3_cap->cdp_on = 1;
                l3_cap->num_classes = l3_cap->num_classes / 2;
        }

        if (cdp == PQOS_REQUIRE_CDP_OFF && l3_cap->cdp_on) {
                /* turn off */
                l3_cap->cdp_on = 0;
                l3_cap->num_classes = l3_cap->num_classes * 2;
        }
}

void
_pqos_cap_l2cdp_change(const enum pqos_cdp_config cdp)
{
        struct pqos_cap_l2ca *l2_cap = NULL;
        struct pqos_cap_l2ca cap;
        unsigned i;
        int ret;

        ASSERT(cdp == PQOS_REQUIRE_CDP_ON || cdp == PQOS_REQUIRE_CDP_OFF ||
               cdp == PQOS_REQUIRE_CDP_ANY);
        ASSERT(m_cap != NULL);

        if (m_cap == NULL)
                return;

        for (i = 0; i < m_cap->num_cap && l2_cap == NULL; i++)
                if (m_cap->capabilities[i].type == PQOS_CAP_TYPE_L2CA)
                        l2_cap = m_cap->capabilities[i].u.l2ca;

        if (l2_cap == NULL)
                return;

        switch (m_interface) {
        case PQOS_INTER_MSR:
                ret = hw_cap_l2ca_discover(&cap, m_cpu);
                break;
#ifdef __linux__
        case PQOS_INTER_OS:
        case PQOS_INTER_OS_RESCTRL_MON:
                ret = os_cap_l2ca_discover(&cap, m_cpu);
                break;
#endif
        default:
                ret = PQOS_RETVAL_RESOURCE;
                break;
        }

        if (ret == PQOS_RETVAL_OK) {
                *l2_cap = cap;
                return;
        }

        if (cdp == PQOS_REQUIRE_CDP_ON && !l2_cap->cdp_on) {
                /* turn on */
                l2_cap->cdp_on = 1;
                l2_cap->num_classes = l2_cap->num_classes / 2;
        }

        if (cdp == PQOS_REQUIRE_CDP_OFF && l2_cap->cdp_on) {
                /* turn off */
                l2_cap->cdp_on = 0;
                l2_cap->num_classes = l2_cap->num_classes * 2;
        }
}

void
_pqos_cap_mba_change(const enum pqos_mba_config cfg)
{
        struct pqos_cap_mba *mba_cap = NULL;
        unsigned i;

        ASSERT(cfg == PQOS_MBA_DEFAULT || cfg == PQOS_MBA_CTRL ||
               cfg == PQOS_MBA_ANY);
        ASSERT(m_cap != NULL);

        if (m_cap == NULL)
                return;

        for (i = 0; i < m_cap->num_cap && mba_cap == NULL; i++)
                if (m_cap->capabilities[i].type == PQOS_CAP_TYPE_MBA)
                        mba_cap = m_cap->capabilities[i].u.mba;

        if (mba_cap == NULL)
                return;

        if (cfg == PQOS_MBA_DEFAULT)
                mba_cap->ctrl_on = 0;
        else if (cfg == PQOS_MBA_CTRL) {
#ifdef __linux__
                if (m_interface != PQOS_INTER_MSR)
                        mba_cap->ctrl = 1;
#endif
                mba_cap->ctrl_on = 1;
        }
}

enum pqos_interface
_pqos_iface(void)
{
        return m_interface;
}

void
_pqos_cap_get(const struct pqos_cap **cap, const struct pqos_cpuinfo **cpu)
{
        if (cap != NULL) {
                ASSERT(m_cap != NULL);
                *cap = m_cap;
        }

        if (cpu != NULL) {
                ASSERT(m_cpu != NULL);
                *cpu = m_cpu;
        }
}

int
_pqos_cap_get_type(const enum pqos_cap_type type,
                   const struct pqos_capability **cap_item)
{
        return pqos_cap_get_type(m_cap, type, cap_item);
}
