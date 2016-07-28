/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2016 Intel Corporation. All rights reserved.
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

/**
 * @brief Platform QoS API and data structure definition
 *
 * API from this file is implemented by the following:
 * host_cap.c, host_allocation.c, host_monitoring.c and utils.c
 */

#ifndef __PQOS_H__
#define __PQOS_H__

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =======================================
 * Various defines
 * =======================================
 */

#define PQOS_VERSION       105          /**< version 1.05 */
#define PQOS_MAX_L3CA_COS  16           /**< 16xCOS */
#define PQOS_MAX_L2CA_COS  16           /**< 16xCOS */

/*
 * =======================================
 * Return values
 * =======================================
 */
#define PQOS_RETVAL_OK           0      /**< everything OK */
#define PQOS_RETVAL_ERROR        1      /**< generic error */
#define PQOS_RETVAL_PARAM        2      /**< parameter error */
#define PQOS_RETVAL_RESOURCE     3      /**< resource error */
#define PQOS_RETVAL_INIT         4      /**< initialization error */
#define PQOS_RETVAL_TRANSPORT    5      /**< transport error */
#define PQOS_RETVAL_PERF_CTR     6      /**< performance counter error */

/*
 * =======================================
 * Init and fini
 * =======================================
 */

enum pqos_cdp_config {
        PQOS_REQUIRE_CDP_OFF = 0,       /**< app not compatible with CDP */
        PQOS_REQUIRE_CDP_ON,            /**< app requires CDP */
        PQOS_REQUIRE_CDP_ANY            /**< app will work with any CDP
                                           setting */
};

/**
 * PQoS library configuration structure
 */
struct pqos_config {
        int fd_log;                     /**< file descriptor to be used for
                                           library to log messages */
        int verbose;                    /**< if true increases library
                                           verbosity level */
        int free_in_use_rmid;           /**< forces the library to take all
                                           cores and RMIDs in the system even
                                           if cores may seem to be subject of
                                           monitoring activity */
};

/**
 * @brief Initializes PQoS module
 *
 * @param [in] config initialization parameters structure
 *        Note that fd_log in \a config needs to be a valid
 *        file descriptor.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_init(const struct pqos_config *config);

/**
 * @brief Shuts down PQoS module
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_fini(void);

/*
 * =======================================
 * Query capabilities
 * =======================================
 */

/**
 * Types of possible PQoS capabilities
 *
 * For now there are:
 * - monitoring capability
 * - L3 cache allocation capability
 * - L2 cache allocation capability
 */
enum pqos_cap_type {
        PQOS_CAP_TYPE_MON = 0,          /**< QoS monitoring */
        PQOS_CAP_TYPE_L3CA,             /**< L3/LLC cache allocation */
        PQOS_CAP_TYPE_L2CA,             /**< L2 cache allocation */
        PQOS_CAP_TYPE_NUMOF
};

/**
 * L3 Cache Allocation (CA) capability structure
 */
struct pqos_cap_l3ca {
        unsigned mem_size;              /**< byte size of the structure */
        unsigned num_classes;           /**< number of classes of service */
        unsigned num_ways;              /**< number of cache ways */
        unsigned way_size;              /**< way size in bytes */
        uint64_t way_contention;        /**< ways contention bit mask */
        int cdp;                        /**< code data prioratization feature
                                           presence */
        int cdp_on;                     /**< code data prioratization on or
                                           off*/
};

/**
 * L2 Cache Allocation (CA) capability structure
 */
struct pqos_cap_l2ca {
        unsigned mem_size;              /**< byte size of the structure */
        unsigned num_classes;           /**< number of classes of service */
        unsigned num_ways;              /**< number of cache ways */
        unsigned way_size;              /**< way size in bytes */
        uint64_t way_contention;        /**< ways contention bit mask */
};

/**
 * Available types of monitored events
 * (matches CPUID enumeration)
 */
enum pqos_mon_event {
        PQOS_MON_EVENT_L3_OCCUP = 1,    /**< LLC occupancy event */
        PQOS_MON_EVENT_LMEM_BW = 2,     /**< Local memory bandwidth */
        PQOS_MON_EVENT_TMEM_BW = 4,     /**< Total memory bandwidth */
        PQOS_MON_EVENT_RMEM_BW = 8,     /**< Remote memory bandwidth
                                           (virtual event) */
        PQOS_PERF_EVENT_LLC_MISS = 0x4000, /**< LLC misses */
        PQOS_PERF_EVENT_IPC    = 0x8000, /**< instructions per clock */
};

/**
 * Monitoring capabilities structure
 *
 * Few assumptions here:
 * - Data associated with each type of event won't exceed 64bits
 *   This results from MSR register size.
 * - Interpretation of the data is up to application based
 *   on available documentation.
 * - Meaning of the event is well understood by an application
 *   based on available documentation
 */
struct pqos_monitor {
        enum pqos_mon_event type;
        unsigned max_rmid;              /**< max RMID supported for
                                           this event */
        uint32_t scale_factor;          /**< factor to scale RMID value
                                           to bytes */
        unsigned pid_support;           /**< flag to show if PID monitoring
                                           is supported */
};

struct pqos_cap_mon {
        unsigned mem_size;              /**< byte size of the structure */
        unsigned max_rmid;              /**< max RMID supported by socket */
        unsigned l3_size;               /**< L3 cache size in bytes */
        unsigned num_events;            /**< number of supported events */
        struct pqos_monitor events[0];
};

/**
 * Single PQoS capabilities entry structure.
 * Effectively a union of all possible PQoS structures plus type.
 */
struct pqos_capability {
        enum pqos_cap_type type;
        union {
                struct pqos_cap_mon *mon;
                struct pqos_cap_l3ca *l3ca;
                struct pqos_cap_l2ca *l2ca;
                void *generic_ptr;
        } u;
};

/**
 * Structure describing all Platform QoS capabilities
 */
struct pqos_cap {
        unsigned mem_size;              /**< byte size of the structure */
        unsigned version;               /**< version of PQoS library */
        unsigned num_cap;               /**< number of capabilities */
        struct pqos_capability capabilities[0];
};

/**
 * Core information structure
 */
struct pqos_coreinfo {
        unsigned lcore;                 /**< logical core id */
        unsigned socket;                /**< socket id in the system */
        unsigned l3_id;                 /**< L3/LLC cluster id */
        unsigned l2_id;                 /**< L2 cluster id */
};

/**
 * CPU cache information structure
 */
struct pqos_cacheinfo {
        int detected;                   /**< Indicates cache detected & valid */
        unsigned num_ways;              /**< Number of cache ways */
        unsigned num_sets;              /**< Number of sets */
        unsigned num_partitions;        /**< Number of partitions */
        unsigned line_size;             /**< Cache line size in bytes */
        unsigned total_size;            /**< Total cache size in bytes */
        unsigned way_size;              /**< Cache way size in bytes */
};

/**
 * CPU topology structure
 */
struct pqos_cpuinfo {
        unsigned mem_size;              /**< byte size of the structure */
        struct pqos_cacheinfo l2;       /**< L2 cache information */
        struct pqos_cacheinfo l3;       /**< L3 cache information */
        unsigned num_cores;             /**< number of cores in the system */
        struct pqos_coreinfo cores[0];
};

/**
 * @brief Retrieves PQoS capabilities data
 *
 * @param [out] cap location to store PQoS capabilities information at
 * @param [out] cpu location to store CPU information at
 *                  This parameter is optional and when NULL is passed then
 *                  no cpu information is returned.
 *                  CPU information includes data about number of sockets,
 *                  logical cores and their assignment.
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cap_get(const struct pqos_cap **cap,
                 const struct pqos_cpuinfo **cpu);

/*
 * =======================================
 * Monitoring
 * =======================================
 */

/**
 * Resource Monitoring ID (RMID) definition
 */
typedef uint32_t pqos_rmid_t;

/**
 * The structure to store monitoring data for all of the events
 */
struct pqos_event_values {
	uint64_t llc;                   /**< cache occupancy */
	uint64_t mbm_local;             /**< bandwidth local - reading */
	uint64_t mbm_total;             /**< bandwidth total - reading */
	uint64_t mbm_remote;            /**< bandwidth remote - reading */
	uint64_t mbm_local_delta;       /**< bandwidth local - delta */
	uint64_t mbm_total_delta;       /**< bandwidth total - delta */
	uint64_t mbm_remote_delta;      /**< bandwidth remote - delta */
        uint64_t ipc_retired;           /**< instructions retired - reading */
        uint64_t ipc_retired_delta;     /**< instructions retired - delta */
        uint64_t ipc_unhalted;          /**< unhalted cycles - reading */
        uint64_t ipc_unhalted_delta;    /**< unhalted cycles - delta */
        double ipc;                     /**< retired instructions / cycles */
        uint64_t llc_misses;            /**< LLC misses - reading */
        uint64_t llc_misses_delta;      /**< LLC misses - delta */
};

/**
 * Core monitoring poll context
 */
struct pqos_mon_poll_ctx {
        unsigned lcore;
        unsigned cluster;
        pqos_rmid_t rmid;
};

/**
 * Monitoring group data structure
 */
struct pqos_mon_data {
        /**
         * Common section
         */
        int valid;                      /**< structure validity marker */
        enum pqos_mon_event event;      /**< monitored event */
        void *context;                  /**< application specific context
                                           pointer */
        struct pqos_event_values values; /**< RMID events value */

        pid_t pid; /**< if not zero then this group tracks a process */

        /**
         * Task specific section
         */
        int tid_nr;
        pid_t *tid_map;
        int *fds_llc;
        int *fds_mbl;
        int *fds_mbt;
        int **fds_ipc;
        int **fds_misses;

        /**
         * Core specific section
         */
        struct pqos_mon_poll_ctx *poll_ctx; /**< core, cluster & rmid */
        unsigned num_poll_ctx;          /**< number of poll contexts */
        unsigned *cores;                /**< list of cores in the group */
        unsigned num_cores;             /**< number of cores in the group */
        int valid_mbm_read;             /**< flag to discard 1st invalid read */
};

/**
 * @brief Resets monitoring by binding all cores with RMID0
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_reset(void);

/**
 * @brief Reads RMID association of the \a lcore
 *
 * @param [in] lcore CPU logical core id
 * @param [out] rmid place to store resource monitoring id
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_assoc_get(const unsigned lcore,
                       pqos_rmid_t *rmid);

/**
 * @brief Starts resource monitoring data logging on \a lcore
 *
 * Note that \a event cannot select PQOS_PERF_EVENT_IPC or
 * PQOS_PERF_EVENT_L3_MISS events without any PQoS event
 * selected at the same time.
 *
 * @param [in] lcore CPU logical core id
 * @param [in] event monitoring event id
 * @param [in] context application dependent context pointer
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_start(const unsigned num_cores,
                   const unsigned *cores,
                   const enum pqos_mon_event event,
                   void *context,
                   struct pqos_mon_data *group);

/**
 * @brief Starts resource monitoring of \a pid process
 *
 * @param [in] pid process ID
 * @param [in] event monitoring event id
 * @param [in] context application dependent context pointer
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_start_pid(const pid_t pid,
                       const enum pqos_mon_event event,
                       void *context,
                       struct pqos_mon_data *group);

/**
 * @brief Stops resource monitoring data for selected monitoring group
 *
 * @param [in] group monitoring context for selected number of cores
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_stop(struct pqos_mon_data *group);

/**
 * @brief Polls monitoring data from requested cores
 *
 * @param [in] groups table of monitoring group pointers to be be updated
 * @param [in] num_groups number of monitoring groups in the table
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_poll(struct pqos_mon_data **groups,
                  const unsigned num_groups);

/*
 * =======================================
 * Allocation Technology
 * =======================================
 */

/**
 * @brief Associates \a lcore with given class of service
 *
 * @param [in] lcore CPU logical core id
 * @param [in] class_id class of service
 *
 * @return Operations status
 */
int pqos_alloc_assoc_set(const unsigned lcore,
                         const unsigned class_id);

/**
 * @brief Reads association of \a lcore with class of service
 *
 * @param [in] lcore CPU logical core id
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_alloc_assoc_get(const unsigned lcore,
                         unsigned *class_id);

/**
 * @brief Assign first available COS to cores in \a core_array
 *
 * While searching for available COS take technologies it is intended to use
 * with into account.
 *
 * @param [in] technology bit mask selecting technologies
 *             (1 << enum pqos_cap_type)
 * @param [in] core_array list of core ids
 * @param [in] core_num number of core ids in the \a core_array
 * @param [out] classid place to store reserved COS id
 *
 * @return Operations status
 */
int pqos_alloc_assign(const unsigned technology,
                      const unsigned *core_array,
                      const unsigned core_num,
                      unsigned *class_id);

/**
 * @brief Reassign cores in \a core_array to default COS#0
 *
 * @param [in] core_array list of core ids
 * @param [in] core_num number of core ids in the \a core_array
 *
 * @return Operations status
 */
int pqos_alloc_release(const unsigned *core_array,
                       const unsigned core_num);

/**
 * @brief Resets configuration of cache allocation technology
 *
 * Reverts CAT state to the one after reset:
 * - all cores associated with COS0
 * - all COS are set to give access to all cache ways
 *
 * As part of CAT reset CDP reconfiguration can be performed.
 * This can be requested via \a l3_cdp_cfg.
 *
 * @param [in] l3_cdp_cfg requested L3 CAT CDP config
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_alloc_reset(const enum pqos_cdp_config l3_cdp_cfg);

/*
 * =======================================
 * L3 cache allocation
 * =======================================
 */

/**
 * L3 cache allocation class of service data structure
 */
struct pqos_l3ca {
        unsigned class_id;              /**< class of service */
        int cdp;                        /**< data & code masks used if true */
        union {
                uint64_t ways_mask;     /**< bit mask for L3 cache ways */
                struct {
                        uint64_t data_mask;
                        uint64_t code_mask;
                } s;
        } u;
};

/**
 * @brief Sets classes of service defined by \a ca on \a socket
 *
 * @param [in] socket CPU socket id
 * @param [in] num_cos number of classes of service at \a ca
 * @param [in] ca table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l3ca_set(const unsigned socket,
                  const unsigned num_cos,
                  const struct pqos_l3ca *ca);

/**
 * @brief Reads classes of service from \a socket
 *
 * @param [in] socket CPU socket id
 * @param [in] max_num_cos maximum number of classes of service
 *             that can be accommodated at \a ca
 * @param [out] num_cos number of classes of service read into \a ca
 * @param [out] ca table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l3ca_get(const unsigned socket,
                  const unsigned max_num_ca,
                  unsigned *num_ca,
                  struct pqos_l3ca *ca);

/*
 * =======================================
 * L2 cache allocation
 * =======================================
 */

/**
 * L2 cache allocation class of service data structure
 */
struct pqos_l2ca {
        unsigned class_id;      /**< class of service */
        uint32_t ways_mask;     /**< bit mask for L2 cache ways */
};

/**
 * @brief Sets classes of service defined by \a ca on \a socket
 *
 * @param [in] socket CPU socket id
 * @param [in] num_cos number of classes of service at \a ca
 * @param [in] ca table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l2ca_set(const unsigned socket,
                  const unsigned num_cos,
                  const struct pqos_l2ca *ca);

/**
 * @brief Reads classes of service from \a socket
 *
 * @param [in] socket CPU socket id
 * @param [in] max_num_cos maximum number of classes of service
 *             that can be accommodated at \a ca
 * @param [out] num_cos number of classes of service read into \a ca
 * @param [out] ca table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l2ca_get(const unsigned socket,
                  const unsigned max_num_ca,
                  unsigned *num_ca,
                  struct pqos_l2ca *ca);

/*
 * =======================================
 * Utility API
 * =======================================
 */

/**
 * @brief Retrieves socket id's from cpu info structure
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [out] count place to store actual number of sockets returned
 *
 * @return Allocated array of size \a count populated with socket id's
 * @retval NULL on error
 */
unsigned *
pqos_cpu_get_sockets(const struct pqos_cpuinfo *cpu,
                     unsigned *count);

/**
 * @brief Creates list of cores belonging to given L3 cluster
 *
 * Function allocates memory for the core list that needs to be freed by
 * the caller.
 *
 * @param [in] cpu CPU topology
 * @param [in] l3_id L3 cluster ID
 * @param [out] count place to put number of cores found
 *
 * @return Pointer to list of cores belonging to the L3 cluster
 * @retval NULL on error or if no core found
 */
unsigned *
pqos_cpu_get_cores_l3id(const struct pqos_cpuinfo *cpu, const unsigned l3_id,
                        unsigned *count);

/**
 * @brief Retrieves core id's from cpu info structure for \a socket
 *
 * @param [in] cpu CPU information structure from cpu info module
 * @param [in] socket CPU socket id to enumerate
 * @param [out] count place to store actual number of core id's returned
 *
 * @return Allocated core id array
 * @retval NULL on error
 */
unsigned *
pqos_cpu_get_cores(const struct pqos_cpuinfo *cpu,
                   const unsigned socket,
                   unsigned *count);

/**
 * @brief Retrieves one core id from cpu info structure for \a socket
 *
 * @param [in] cpu CPU information structure from cpu info module
 * @param [in] socket CPU socket id to enumerate
 * @param [out] lcore place to store returned core id
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_cpu_get_one_core(const struct pqos_cpuinfo *cpu,
                      const unsigned socket,
                      unsigned *lcore);

/**
 * @brief Verifies if \a core is a valid logical core id
 *
 * @param [in] cpu CPU information structure from cpu info module
 * @param [in] lcore logical core id
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success (\a lcore is valid)
 */
int
pqos_cpu_check_core(const struct pqos_cpuinfo *cpu,
                    const unsigned lcore);

/**
 * @brief Retrieves socket id for given \logical core id
 *
 * @param [in] cpu CPU information structure from cpu info module
 * @param [in] lcore logical core id
 * @param [out] socket location to store socket id at
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_cpu_get_socketid(const struct pqos_cpuinfo *cpu,
                      const unsigned lcore,
                      unsigned *socket);

/**
 * @brief Retrieves monitoring cluster id for given logical core id
 *
 * @param [in] cpu CPU information structure from cpu info module
 * @param [in] lcore logical core id
 * @param [out] cluster location to store cluster id at
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_cpu_get_clusterid(const struct pqos_cpuinfo *cpu,
                       const unsigned lcore,
                       unsigned *cluster);

/**
 * @brief Retrieves \a type of capability from \a cap structure
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [in] type capability type to look for
 * @param [out] cap_item place to store pointer to selected capability
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_cap_get_type(const struct pqos_cap *cap,
                  const enum pqos_cap_type type,
                  const struct pqos_capability **cap_item);

/**
 * @brief Retrieves \a monitoring event data from \a cap structure
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [in] event monitoring event type to look for
 * @param [out] p_mon place to store pointer to selected event capabilities
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_cap_get_event(const struct pqos_cap *cap,
                   const enum pqos_mon_event event,
                   const struct pqos_monitor **p_mon);

/**
 * @brief Retrieves number of L3 allocation classes of service from
 *        \a cap structure.
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [out] cos_num place to store number of classes of service
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_l3ca_get_cos_num(const struct pqos_cap *cap,
                      unsigned *cos_num);

/**
 * @brief Retrieves number of L2 allocation classes of service from
 *        \a cap structure.
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [out] cos_num place to store number of classes of service
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_l2ca_get_cos_num(const struct pqos_cap *cap,
                      unsigned *cos_num);

/**
 * @brief Retrieves CDP status
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [out] cdp_supported place to store CDP support status
 * @param [out] cdp_enabled place to store CDP enable status
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
pqos_l3ca_cdp_enabled(const struct pqos_cap *cap,
                      int *cdp_supported,
                      int *cdp_enabled);

/**
 * @brief Retrieves a monitoring value from a group for a specific event.
 * @param [out] value monitoring value
 * @param [in] event_id event being monitored
 * @param [in] group monitoring group
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static inline int
pqos_mon_get_event_value(void * const value,
                         const enum pqos_mon_event event_id,
                         const struct pqos_mon_data * const group)
{
        uint64_t * const p_64 = value;
        double * const p_dbl = value;

	if (group == NULL || value == NULL)
		return PQOS_RETVAL_PARAM;

	switch (event_id) {
        case PQOS_MON_EVENT_L3_OCCUP:
                *p_64 = group->values.llc;
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                *p_64 = group->values.mbm_local_delta;
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                *p_64 = group->values.mbm_total_delta;
                break;
        case PQOS_MON_EVENT_RMEM_BW:
                *p_64 = group->values.mbm_remote_delta;
                break;
        case PQOS_PERF_EVENT_IPC:
                *p_dbl = group->values.ipc;
                break;
        case PQOS_PERF_EVENT_LLC_MISS:
                *p_64 = group->values.llc_misses_delta;
                break;
        default:
                return PQOS_RETVAL_PARAM;
	}

	return PQOS_RETVAL_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_H__ */
