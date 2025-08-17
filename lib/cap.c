/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2023 Intel Corporation. All rights reserved.
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
 *
 * Capability functions:
 * - monitoring detection, this is to discover all monitoring event types.
 * - LLC allocation detection, this is to discover last level cache
 *   allocation feature.
 * - A new targeted function has to be implemented to discover new allocation
 *   technology.
 */

#include "cap.h"

#include "allocation.h"
#include "api.h"
#include "cores_domains.h"
#include "cpu_registers.h"
#include "cpuinfo.h"
#include "erdt.h"
#include "hw_cap.h"
#include "iordt.h"
#include "lock.h"
#include "log.h"
#include "machine.h"
#include "monitoring.h"
#include "mrrm.h"
#include "os_cap.h"
#include "resctrl.h"
#include "resctrl_alloc.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

/**
 * ---------------------------------------
 * Local macros
 * ---------------------------------------
 */

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
 * This structure is initialized in this module. Then other sub-modules get this
 * structure in order to retrieve capability and CPU information.
 */
static struct pqos_sysconfig m_sysconf;

/**
 * Library initialization status.
 */
static int m_init_done = 0;

/**
 * Interface status
 *   0  PQOS_INTER_MSR
 *   1  PQOS_INTER_OS
 *   2  PQOS_INTER_OS_RESCTRL_MON
 *   3  PQOS_INTER_AUTO
 *   4  PQOS_INTER_MMIO
 */
static enum pqos_interface m_interface = PQOS_INTER_MSR;

/**
 * ---------------------------------------
 * Local functions
 * ---------------------------------------
 */

enum pqos_interface
_pqos_get_inter(void)
{
        return m_interface;
}

PQOS_STATIC void
_pqos_set_inter(const enum pqos_interface iface)
{
        m_interface = iface;
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
PQOS_STATIC int
cap_l3ca_discover(struct pqos_cap_l3ca **r_cap,
                  const struct pqos_cpuinfo *cpu,
                  const enum pqos_interface iface)
{
        struct pqos_cap_l3ca *cap = NULL;
        int ret;

        if (r_cap == NULL || cpu == NULL)
                return PQOS_RETVAL_PARAM;

        cap = (struct pqos_cap_l3ca *)malloc(sizeof(*cap));
        if (cap == NULL)
                return PQOS_RETVAL_RESOURCE;

        switch (iface) {
        case PQOS_INTER_MSR:
        /* MMIO interface shares similar L3 CAT functionality with MSR
         * interface, so it uses the same l3ca discovery function. */
        case PQOS_INTER_MMIO:
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
PQOS_STATIC int
cap_l2ca_discover(struct pqos_cap_l2ca **r_cap,
                  const struct pqos_cpuinfo *cpu,
                  const enum pqos_interface iface)
{
        struct pqos_cap_l2ca *cap = NULL;
        int ret;

        if (r_cap == NULL || cpu == NULL)
                return PQOS_RETVAL_PARAM;

        cap = (struct pqos_cap_l2ca *)malloc(sizeof(*cap));
        if (cap == NULL)
                return PQOS_RETVAL_RESOURCE;

        switch (iface) {
        case PQOS_INTER_MSR:
        /* MMIO interface shares same L2 CAT functionality with MSR interface,
         * so it uses the same l2ca discovery function. */
        case PQOS_INTER_MMIO:
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
PQOS_STATIC int
cap_mba_discover(struct pqos_cap_mba **r_cap,
                 const struct pqos_cpuinfo *cpu,
                 const enum pqos_interface iface)
{
        struct pqos_cap_mba *cap = NULL;
        int ret;

        if (r_cap == NULL || cpu == NULL)
                return PQOS_RETVAL_PARAM;

        cap = (struct pqos_cap_mba *)malloc(sizeof(*cap));
        if (cap == NULL)
                return PQOS_RETVAL_RESOURCE;

        switch (iface) {
        case PQOS_INTER_MSR:
        /* MMIO interface shares similar MBA functionality with MSR interface,
         * so it uses the same mba discovery function. */
        case PQOS_INTER_MMIO:
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
 * @brief Discovers support of SMBA
 *
 * @param[out] r_cap place to store SMBA capabilities structure
 * @param[in] cpu detected cpu topology
 * @param[in] iface Selected interface
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
static int
cap_smba_discover(struct pqos_cap_mba **r_cap,
                  const struct pqos_cpuinfo *cpu,
                  const enum pqos_interface iface)
{
        struct pqos_cap_mba *cap = NULL;
        int ret = PQOS_RETVAL_OK;

        cap = (struct pqos_cap_mba *)malloc(sizeof(*cap));
        if (cap == NULL)
                return PQOS_RETVAL_RESOURCE;

        switch (iface) {
        case PQOS_INTER_MSR:
                if (cpu->vendor == PQOS_VENDOR_AMD)
                        ret = amd_cap_smba_discover(cap, cpu);
                else
                        ret = PQOS_RETVAL_RESOURCE;
                break;
#ifdef __linux__
        case PQOS_INTER_OS:
        case PQOS_INTER_OS_RESCTRL_MON:
                ret = os_cap_smba_discover(cap, cpu);
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
PQOS_STATIC int
discover_capabilities(struct pqos_cap **p_cap,
                      const struct pqos_cpuinfo *cpu,
                      enum pqos_interface inter)
{
        struct pqos_cap_mon *det_mon = NULL;
        struct pqos_cap_l3ca *det_l3ca = NULL;
        struct pqos_cap_l2ca *det_l2ca = NULL;
        struct pqos_cap_mba *det_mba = NULL;
        struct pqos_cap_mba *det_smba = NULL;
        struct pqos_cap *_cap = NULL;
        unsigned sz = 0;
        int ret = PQOS_RETVAL_RESOURCE;

        if (p_cap == NULL || cpu == NULL)
                return PQOS_RETVAL_PARAM;
        /**
         * Monitoring init
         */
        if (inter == PQOS_INTER_MSR || inter == PQOS_INTER_MMIO)
                ret = hw_cap_mon_discover(&det_mon, cpu, inter);
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
                LOG_INFO("L3 CAT details: I/O RDT support=%d, I/O RDT on=%d\n",
                         det_l3ca->iordt, det_l3ca->iordt_on);
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

        /**
         * Slow Memory bandwidth allocation init
         */
        ret = cap_smba_discover(&det_smba, cpu, inter);
        switch (ret) {
        case PQOS_RETVAL_OK:
                LOG_INFO("SMBA capability detected\n");
                LOG_INFO("SMBA details: "
                         "#COS=%u, %slinear, max=%u, step=%u\n",
                         det_smba->num_classes,
                         det_smba->is_linear ? "" : "non-",
                         det_smba->throttle_max, det_smba->throttle_step);
                sz += sizeof(struct pqos_capability);
                break;
        case PQOS_RETVAL_RESOURCE:
                LOG_INFO("SMBA capability not detected\n");
                break;
        default:
                LOG_ERROR("Fatal error encounter in SMBA discovery!\n");
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

        if (det_smba != NULL) {
                _cap->capabilities[_cap->num_cap].type = PQOS_CAP_TYPE_SMBA;
                _cap->capabilities[_cap->num_cap].u.smba = det_smba;
                _cap->num_cap++;
                ret = PQOS_RETVAL_OK;
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

/**
 * @brief Converts enumeration value into string
 *
 * @param interface interface
 *
 * @return converted enumeration value into string
 */
PQOS_STATIC const char *
_cap_interface_to_string(enum pqos_interface interface)
{
        switch (interface) {
        case PQOS_INTER_MSR:
                return "MSR";
        case PQOS_INTER_OS:
                return "OS";
        case PQOS_INTER_OS_RESCTRL_MON:
                return "OS_RESCTRL_MON";
        case PQOS_INTER_AUTO:
                return "AUTO";
        case PQOS_INTER_MMIO:
                return "MMIO";
        default:
                return "Unknown";
        }
}

/**
 * @brief Detects interface
 *
 * @param requested_interface requested interface
 * @param interface detected interface
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
PQOS_STATIC int
discover_interface(enum pqos_interface requested_interface,
                   enum pqos_interface *interface)
{
        char *environment = NULL;

        LOG_INFO("Requested interface: %s\n",
                 _cap_interface_to_string(requested_interface));

        if (interface == NULL)
                return PQOS_RETVAL_PARAM;
#ifdef __linux__
        if (requested_interface != PQOS_INTER_MSR &&
            requested_interface != PQOS_INTER_OS &&
            requested_interface != PQOS_INTER_OS_RESCTRL_MON &&
            requested_interface != PQOS_INTER_MMIO &&
            requested_interface != PQOS_INTER_AUTO)
#else
        if (requested_interface != PQOS_INTER_MSR &&
            requested_interface != PQOS_INTER_MMIO &&
            requested_interface != PQOS_INTER_AUTO)
#endif
                return PQOS_RETVAL_PARAM;

        environment = getenv("RDT_IFACE");
        if (environment != NULL) {
                if (strncasecmp(environment, "OS", 2) == 0) {
                        if (requested_interface != PQOS_INTER_OS &&
                            requested_interface != PQOS_INTER_AUTO) {
                                LOG_ERROR("Interface initialization error!\n"
                                          "Your system has been restricted "
                                          "to use the OS interface only!\n");
                                return PQOS_RETVAL_ERROR;
                        }

                        *interface = PQOS_INTER_OS;
                } else if (strncasecmp(environment, "MSR", 3) == 0) {
                        if (requested_interface != PQOS_INTER_MSR &&
                            requested_interface != PQOS_INTER_AUTO) {
                                LOG_ERROR("Interface initialization error!\n"
                                          "Your system has been restricted "
                                          "to use the MSR interface only!\n");
                                return PQOS_RETVAL_ERROR;
                        }

                        *interface = PQOS_INTER_MSR;
                } else if (strncasecmp(environment, "MMIO", 4) == 0) {
                        if (requested_interface != PQOS_INTER_MSR &&
                            requested_interface != PQOS_INTER_AUTO) {
                                LOG_ERROR("Interface initialization error!\n"
                                          "Your system has been restricted "
                                          "to use the MMIO interface only!\n");
                                return PQOS_RETVAL_ERROR;
                        }
                        *interface = PQOS_INTER_MMIO;
                } else {
                        LOG_ERROR("Interface initialization error!\n"
                                  "Invalid interface enforcement selection.\n");
                        return PQOS_RETVAL_ERROR;
                }
        } else if (requested_interface == PQOS_INTER_AUTO) {
#ifdef __linux__
                if (resctrl_is_supported() == PQOS_RETVAL_OK)
                        *interface = PQOS_INTER_OS;
                else
                        *interface = PQOS_INTER_MSR;
#else
                *interface = PQOS_INTER_MSR;
#endif
        } else {
                *interface = requested_interface;
        }

        LOG_INFO("Selected interface: %s\n",
                 _cap_interface_to_string(*interface));
        return PQOS_RETVAL_OK;
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
        struct pqos_cap *cap = NULL;
        struct pqos_cpuinfo *cpu = NULL;
        struct pqos_devinfo *dev = NULL;
        struct pqos_erdt_info *erdt = NULL;
        struct pqos_mrrm_info *mrrm = NULL;
        struct pqos_cores_domains *cores_domains = NULL;
        struct pqos_channels_domains *channels_domains = NULL;
        enum pqos_interface interface;

        if (config == NULL)
                return PQOS_RETVAL_PARAM;

        if (lock_init() != 0) {
                fprintf(stderr, "API lock initialization error!\n");
                return PQOS_RETVAL_ERROR;
        }

        lock_get();

        ret = _pqos_check_init(0);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                lock_fini();
                return ret;
        }

        memset(&m_sysconf, 0, sizeof(m_sysconf));

        ret = log_init(config->fd_log, config->callback_log,
                       config->context_log, config->verbose);
        if (ret != LOG_RETVAL_OK) {
                fprintf(stderr, "log_init() error\n");
                goto init_error;
        }

        ret = discover_interface(config->interface, &interface);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("Cannot select the interface!\n");
                goto log_init_error;
        }

        /**
         * Topology not provided through config.
         * CPU discovery done through internal mechanism.
         */
        ret = cpuinfo_init(interface, &cpu);
        if (ret != 0 || cpu == NULL) {
                LOG_ERROR("cpuinfo_init() error %d\n", ret);
                ret = PQOS_RETVAL_ERROR;
                goto log_init_error;
        }

        /**
         * Find max core id in the topology
         */
        for (i = 0; i < cpu->num_cores; i++)
                if (cpu->cores[i].lcore > max_core)
                        max_core = cpu->cores[i].lcore;

        ret = machine_init(max_core);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("machine_init() error %d\n", ret);
                goto cpuinfo_init_error;
        }

        if (hw_detect_hybrid())
                LOG_WARN(
                    "Hybrid part with L2 CAT support detected.\n"
                    "      L2 CAT on hybrid parts is not yet supported in "
                    "pqos\n"
                    "      tools and may not behave as expected and cause\n"
                    "      performance degradation. For more information, "
                    "see:\n"
                    "      "
                    "https://github.com/intel/intel-cmt-cat/issues/272\n");

#ifdef __linux__
        if (interface == PQOS_INTER_OS ||
            interface == PQOS_INTER_OS_RESCTRL_MON) {
                ret = os_cap_init(interface);
                if (ret != PQOS_RETVAL_OK) {
                        LOG_ERROR("os_cap_init() error %d\n", ret);
                        goto machine_init_error;
                }
        } else if (access(RESCTRL_PATH "/cpus", F_OK) == 0)
                LOG_WARN("resctl filesystem mounted! Using MSR "
                         "interface may corrupt resctrl filesystem "
                         "and cause unexpected behaviour\n");
#endif

        ret = discover_capabilities(&cap, cpu, interface);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("discover_capabilities() error %d\n", ret);
                goto machine_init_error;
        }

        ret = _pqos_utils_init(interface);
        if (ret != PQOS_RETVAL_OK) {
                fprintf(stderr, "Utils initialization error!\n");
                goto machine_init_error;
        }

        ret = api_init(interface, cpu->vendor);
        if (ret != PQOS_RETVAL_OK) {
                LOG_ERROR("lock_init() error %d\n", ret);
                goto machine_init_error;
        }

        _pqos_set_inter(interface);

        ret = pqos_alloc_init(cpu, cap, config);
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
        ret = pqos_mon_init(cpu, cap, config);
        switch (ret) {
        case PQOS_RETVAL_RESOURCE:
                LOG_DEBUG("monitoring init aborted: feature not present\n");
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
                goto machine_init_error;
        }

        ret = mrrm_init(cap, &mrrm);
        switch (ret) {
        case PQOS_RETVAL_RESOURCE:
                LOG_DEBUG("MRRM init aborted: feature not present\n");
                break;
        case PQOS_RETVAL_OK:
                LOG_DEBUG("MRRM init OK\n");
                break;
        case PQOS_RETVAL_ERROR:
        default:
                LOG_ERROR("MRRM init error %d\n", ret);
                break;
        }

        ret = erdt_init(cap, cpu, &erdt);
        switch (ret) {
        case PQOS_RETVAL_RESOURCE:
                LOG_DEBUG("ERDT init aborted: feature not present\n");
                break;
        case PQOS_RETVAL_OK:
                LOG_DEBUG("ERDT init OK\n");
                break;
        case PQOS_RETVAL_ERROR:
        default:
                LOG_ERROR("ERDT init error %d\n", ret);
                break;
        }

        if (ret == PQOS_RETVAL_OK && interface == PQOS_INTER_MMIO) {
                /* Initialized after ERDT due to its data usage */
                ret = cores_domains_init(cpu->num_cores, erdt, &cores_domains);
                if (ret != PQOS_RETVAL_OK)
                        goto machine_init_error;
        }

        ret = iordt_init(cap, &dev);
        switch (ret) {
        case PQOS_RETVAL_RESOURCE:
                LOG_DEBUG("I/O RDT init aborted: feature not present\n");
                break;
        case PQOS_RETVAL_OK:
                LOG_DEBUG("I/O RDT init OK\n");
                break;
        case PQOS_RETVAL_ERROR:
        default:
                LOG_ERROR("I/O RDT init error %d\n", ret);
                break;
        }

        if (ret == PQOS_RETVAL_OK && interface == PQOS_INTER_MMIO) {
                ret = channels_domains_init(dev->num_channels, erdt, dev,
                                            &channels_domains);
                if (ret != PQOS_RETVAL_OK)
                        goto machine_init_error;
        }

        if (ret == PQOS_RETVAL_RESOURCE)
                ret = PQOS_RETVAL_OK;

        if (ret != PQOS_RETVAL_OK)
                iordt_fini();
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
                if (cap != NULL) {
                        for (i = 0; i < cap->num_cap; i++)
                                free(cap->capabilities[i].u.generic_ptr);
                        free(cap);
                }
        }

        if (ret == PQOS_RETVAL_OK) {
                m_init_done = 1;
                m_sysconf.cap = cap;
                m_sysconf.cpu = cpu;
                m_sysconf.dev = dev;
                m_sysconf.erdt = erdt;
                m_sysconf.mrrm = mrrm;
                m_sysconf.cores_domains = cores_domains;
                m_sysconf.channels_domains = channels_domains;
        }

        lock_release();

        if (ret != PQOS_RETVAL_OK)
                lock_fini();

        return ret;
}

int
pqos_fini(void)
{
        int ret = PQOS_RETVAL_OK;
        int retval = PQOS_RETVAL_OK;
        unsigned i = 0;
        enum pqos_interface interface = _pqos_get_inter();

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                lock_fini();
                return ret;
        }

        pqos_mon_fini();
        pqos_alloc_fini();

        if (interface == PQOS_INTER_MMIO) {
                cores_domains_fini();
                channels_domains_fini();
        }

        ret = iordt_fini();
        if (ret != 0) {
                retval = PQOS_RETVAL_ERROR;
                LOG_ERROR("iordt_fini() error %d\n", ret);
        }

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

        if (m_sysconf.cap)
                for (i = 0; i < m_sysconf.cap->num_cap; i++)
                        free(m_sysconf.cap->capabilities[i].u.generic_ptr);
        free(m_sysconf.cap);
        m_sysconf.cap = NULL;
        m_sysconf.cpu = NULL;
        m_sysconf.dev = NULL;
        m_sysconf.cores_domains = NULL;

        m_init_done = 0;

        lock_release();

        if (lock_fini() != 0)
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

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        if (cap != NULL) {
                *cap = _pqos_get_cap();
                ASSERT(*cap != NULL);
        }
        if (cpu != NULL) {
                *cpu = _pqos_get_cpu();
                ASSERT(*cpu != NULL);
        }

        lock_release();
        return PQOS_RETVAL_OK;
}

int
pqos_sysconfig_get(const struct pqos_sysconfig **sysconf)
{
        int ret = PQOS_RETVAL_OK;

        if (sysconf == NULL)
                return PQOS_RETVAL_PARAM;

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        *sysconf = _pqos_get_sysconfig();

        lock_release();
        return PQOS_RETVAL_OK;
}

void
_pqos_cap_l3cdp_change(const enum pqos_cdp_config cdp)
{
        struct pqos_cap_l3ca *l3_cap = NULL;
        struct pqos_cap_l3ca cap;
        int ret;
        unsigned i;
        enum pqos_interface interface = _pqos_get_inter();

        ASSERT(cdp == PQOS_REQUIRE_CDP_ON || cdp == PQOS_REQUIRE_CDP_OFF ||
               cdp == PQOS_REQUIRE_CDP_ANY);
        ASSERT(m_sysconf.cap != NULL);

        if (m_sysconf.cap == NULL)
                return;

        for (i = 0; i < m_sysconf.cap->num_cap && l3_cap == NULL; i++)
                if (m_sysconf.cap->capabilities[i].type == PQOS_CAP_TYPE_L3CA)
                        l3_cap = m_sysconf.cap->capabilities[i].u.l3ca;

        if (l3_cap == NULL)
                return;

        switch (interface) {
        case PQOS_INTER_MSR:
        case PQOS_INTER_MMIO:
                ret = hw_cap_l3ca_discover(&cap, m_sysconf.cpu);
                break;
#ifdef __linux__
        case PQOS_INTER_OS: /* fall through */
        case PQOS_INTER_OS_RESCTRL_MON:
                ret = os_cap_l3ca_discover(&cap, m_sysconf.cpu);
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
_pqos_cap_l3iordt_change(const enum pqos_iordt_config iordt)
{
        struct pqos_cap_l3ca *l3_cap = NULL;
        struct pqos_cap *cap = m_sysconf.cap;
        unsigned i;

        ASSERT(iordt == PQOS_REQUIRE_IORDT_ON ||
               iordt == PQOS_REQUIRE_IORDT_OFF ||
               iordt == PQOS_REQUIRE_IORDT_ANY);
        ASSERT(m_sysconf.cap != NULL);

        if (cap == NULL)
                return;

        for (i = 0; i < cap->num_cap && l3_cap == NULL; i++)
                if (cap->capabilities[i].type == PQOS_CAP_TYPE_L3CA)
                        l3_cap = cap->capabilities[i].u.l3ca;

        if (l3_cap == NULL)
                return;

        /* turn on */
        if (iordt == PQOS_REQUIRE_IORDT_ON && !l3_cap->iordt_on)
                l3_cap->iordt_on = 1;

        /* turn off */
        if (iordt == PQOS_REQUIRE_IORDT_OFF && l3_cap->iordt_on)
                l3_cap->iordt_on = 0;
}

void
_pqos_cap_l2cdp_change(const enum pqos_cdp_config cdp)
{
        struct pqos_cap_l2ca *l2_cap = NULL;
        struct pqos_cap_l2ca cap;
        unsigned i;
        int ret;
        enum pqos_interface interface = _pqos_get_inter();

        ASSERT(cdp == PQOS_REQUIRE_CDP_ON || cdp == PQOS_REQUIRE_CDP_OFF ||
               cdp == PQOS_REQUIRE_CDP_ANY);
        ASSERT(m_sysconf.cap != NULL);

        if (m_sysconf.cap == NULL)
                return;

        for (i = 0; i < m_sysconf.cap->num_cap && l2_cap == NULL; i++)
                if (m_sysconf.cap->capabilities[i].type == PQOS_CAP_TYPE_L2CA)
                        l2_cap = m_sysconf.cap->capabilities[i].u.l2ca;

        if (l2_cap == NULL)
                return;

        switch (interface) {
        case PQOS_INTER_MSR:
                ret = hw_cap_l2ca_discover(&cap, m_sysconf.cpu);
                break;
#ifdef __linux__
        case PQOS_INTER_OS:
        case PQOS_INTER_OS_RESCTRL_MON:
                ret = os_cap_l2ca_discover(&cap, m_sysconf.cpu);
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
#ifdef __linux__
        enum pqos_interface interface = _pqos_get_inter();
#endif

        ASSERT(cfg == PQOS_MBA_DEFAULT || cfg == PQOS_MBA_CTRL ||
               cfg == PQOS_MBA_ANY);
        ASSERT(m_sysconf.cap != NULL);

        if (m_sysconf.cap == NULL)
                return;

        for (i = 0; i < m_sysconf.cap->num_cap && mba_cap == NULL; i++)
                if (m_sysconf.cap->capabilities[i].type == PQOS_CAP_TYPE_MBA)
                        mba_cap = m_sysconf.cap->capabilities[i].u.mba;

        if (mba_cap == NULL)
                return;

#ifdef __linux__
        /* refresh number of classes */
        if (interface == PQOS_INTER_OS ||
            interface == PQOS_INTER_OS_RESCTRL_MON) {
                int ret;
                unsigned num_classes;

                ret = resctrl_alloc_get_num_closids(&num_classes);
                if (ret == PQOS_RETVAL_OK)
                        mba_cap->num_classes = num_classes;
        }
#endif

        if (cfg == PQOS_MBA_DEFAULT)
                mba_cap->ctrl_on = 0;
        else if (cfg == PQOS_MBA_CTRL) {
#ifdef __linux__
                if (interface != PQOS_INTER_MSR)
                        mba_cap->ctrl = 1;
#endif
                mba_cap->ctrl_on = 1;
        }
}

void
_pqos_cap_mon_iordt_change(const enum pqos_iordt_config iordt)
{
        struct pqos_cap_mon *mon_cap = NULL;
        unsigned i;

        ASSERT(iordt == PQOS_REQUIRE_IORDT_ON ||
               iordt == PQOS_REQUIRE_IORDT_OFF ||
               iordt == PQOS_REQUIRE_IORDT_ANY);
        ASSERT(m_sysconf.cap != NULL);

        if (m_sysconf.cap == NULL)
                return;

        for (i = 0; i < m_sysconf.cap->num_cap && mon_cap == NULL; i++)
                if (m_sysconf.cap->capabilities[i].type == PQOS_CAP_TYPE_MON)
                        mon_cap = m_sysconf.cap->capabilities[i].u.mon;

        if (mon_cap == NULL)
                return;

        /* turn on */
        if (iordt == PQOS_REQUIRE_IORDT_ON && !mon_cap->iordt_on)
                mon_cap->iordt_on = 1;

        /* turn off */
        if (iordt == PQOS_REQUIRE_IORDT_OFF && mon_cap->iordt_on)
                mon_cap->iordt_on = 0;
}

void
_pqos_cap_mon_snc_change(const enum pqos_snc_config cfg)
{
        struct pqos_cap_mon *mon_cap = NULL;
        unsigned i;

        ASSERT(cfg == PQOS_REQUIRE_SNC_LOCAL || cfg == PQOS_REQUIRE_SNC_TOTAL ||
               cfg == PQOS_REQUIRE_SNC_ANY);
        ASSERT(m_sysconf.cap != NULL);

        if (m_sysconf.cap == NULL)
                return;

        for (i = 0; i < m_sysconf.cap->num_cap && mon_cap == NULL; i++)
                if (m_sysconf.cap->capabilities[i].type == PQOS_CAP_TYPE_MON)
                        mon_cap = m_sysconf.cap->capabilities[i].u.mon;

        if (mon_cap == NULL)
                return;

        if (mon_cap->snc_num == 1)
                return;

        if (cfg == PQOS_REQUIRE_SNC_TOTAL)
                mon_cap->snc_mode = PQOS_SNC_TOTAL;

        if (cfg == PQOS_REQUIRE_SNC_LOCAL)
                mon_cap->snc_mode = PQOS_SNC_LOCAL;
}

const struct pqos_sysconfig *
_pqos_get_sysconfig(void)
{
        return &m_sysconf;
}

void
_pqos_cap_smba_change(const enum pqos_mba_config cfg)
{
        struct pqos_cap_mba *smba_cap = NULL;
        unsigned i;
#ifdef __linux__
        enum pqos_interface interface = _pqos_get_inter();
#endif
        ASSERT(cfg == PQOS_MBA_DEFAULT || cfg == PQOS_MBA_CTRL ||
               cfg == PQOS_MBA_ANY);
        ASSERT(m_sysconf.cap != NULL);

        if (m_sysconf.cap == NULL)
                return;

        for (i = 0; i < m_sysconf.cap->num_cap && smba_cap == NULL; i++)
                if (m_sysconf.cap->capabilities[i].type == PQOS_CAP_TYPE_SMBA)
                        smba_cap = m_sysconf.cap->capabilities[i].u.smba;

        if (smba_cap == NULL)
                return;

#ifdef __linux__
        /* refresh number of classes */
        if (interface == PQOS_INTER_OS ||
            interface == PQOS_INTER_OS_RESCTRL_MON) {
                int ret;
                unsigned num_classes;

                ret = resctrl_alloc_get_num_closids(&num_classes);
                if (ret == PQOS_RETVAL_OK)
                        smba_cap->num_classes = num_classes;
        }
#endif

        if (cfg == PQOS_MBA_DEFAULT)
                smba_cap->ctrl_on = 0;
        else if (cfg == PQOS_MBA_CTRL) {
#ifdef __linux__
                if (interface != PQOS_INTER_MSR)
                        smba_cap->ctrl = 1;
#endif
                smba_cap->ctrl_on = 1;
        }
}

const struct pqos_cap *
_pqos_get_cap(void)
{
        ASSERT(m_sysconf.cap != NULL);
        return m_sysconf.cap;
}

const struct pqos_cpuinfo *
_pqos_get_cpu(void)
{
        ASSERT(m_sysconf.cpu != NULL);
        return m_sysconf.cpu;
}

const struct pqos_devinfo *
_pqos_get_dev(void)
{
        return m_sysconf.dev;
}

const struct pqos_erdt_info *
_pqos_get_erdt(void)
{
        return m_sysconf.erdt;
}

const struct pqos_mrrm_info *
_pqos_get_mrrm(void)
{
        return m_sysconf.mrrm;
}

const struct pqos_cores_domains *
_pqos_get_cores_domains(void)
{
        return m_sysconf.cores_domains;
}

const struct pqos_channels_domains *
_pqos_get_channels_domains(void)
{
        return m_sysconf.channels_domains;
}

int
cap_get_l3ca_non_contignous(void)
{
        const struct pqos_capability *l3cap =
            _pqos_cap_get_type(PQOS_CAP_TYPE_L3CA);

        /* If L3 capability is not available, return contiguous CBM support */
        if (l3cap == NULL)
                return !PQOS_CPUID_CAT_NON_CONTIGUOUS_CBM_SUPPORT;

        return l3cap->u.l3ca->non_contiguous_cbm;
}

int
cap_get_l2ca_non_contignous(void)
{
        const struct pqos_capability *l2cap =
            _pqos_cap_get_type(PQOS_CAP_TYPE_L2CA);

        /* If L2 capability is not available, return contiguous CBM support */
        if (l2cap == NULL)
                return !PQOS_CPUID_CAT_NON_CONTIGUOUS_CBM_SUPPORT;

        return l2cap->u.l2ca->non_contiguous_cbm;
}

/**
 * =======================================
 * =======================================
 *
 * interface
 *
 * =======================================
 * =======================================
 */

int
pqos_inter_get(enum pqos_interface *interface)
{
        int ret = PQOS_RETVAL_OK;

        if (interface == NULL)
                return PQOS_RETVAL_PARAM;

        lock_get();

        ret = _pqos_check_init(1);
        if (ret != PQOS_RETVAL_OK) {
                lock_release();
                return ret;
        }

        *interface = _pqos_get_inter();

        lock_release();
        return PQOS_RETVAL_OK;
}
