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
 *
 */

/**
 * @brief Platform QoS API and data structure definition
 *
 * API from this file is implemented by the following:
 * cap.c, allocation.c, monitoring.c and utils.c
 */

#ifndef __PQOS_H__
#define __PQOS_H__

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SWIG
#define PQOS_DEPRECATED __attribute__((deprecated))
#else
#define PQOS_DEPRECATED
#endif

/*
 * =======================================
 * Various defines
 * =======================================
 */

#define PQOS_VERSION      60001 /**< version 6.0.1 */
#define PQOS_MAX_COS      16    /**< 16 x COS */
#define PQOS_MAX_L3CA_COS PQOS_MAX_COS
#define PQOS_MAX_L2CA_COS PQOS_MAX_COS
#define IMH_MAX_PATH      256

#define CPU_AGENTS_PER_SOCKET    4
#define DEVICE_AGENTS_PER_SOCKET 2

/*
 * =======================================
 * Return values
 * =======================================
 */
#define PQOS_RETVAL_OK          0  /**< everything OK */
#define PQOS_RETVAL_ERROR       1  /**< generic error */
#define PQOS_RETVAL_PARAM       2  /**< parameter error */
#define PQOS_RETVAL_RESOURCE    3  /**< resource error */
#define PQOS_RETVAL_INIT        4  /**< initialization error */
#define PQOS_RETVAL_TRANSPORT   5  /**< transport error */
#define PQOS_RETVAL_PERF_CTR    6  /**< performance counter error */
#define PQOS_RETVAL_BUSY        7  /**< resource busy error */
#define PQOS_RETVAL_INTER       8  /**< Interface not supported */
#define PQOS_RETVAL_OVERFLOW    9  /**< Data overflow */
#define PQOS_RETVAL_UNAVAILABLE 10 /**< Data not available */

/*
 * =======================================
 * MBA
 * =======================================
 */
#define PQOS_MAX_MEM_REGIONS      4
#define PQOS_BW_CTRL_TYPE_COUNT   3
#define PQOS_BW_CTRL_TYPE_OPT_IDX 0
#define PQOS_BW_CTRL_TYPE_MIN_IDX 1
#define PQOS_BW_CTRL_TYPE_MAX_IDX 2

/*
 * =======================================
 * MMIO Dump defines
 * =======================================
 */
/* Calculate offset of member in structure */
#define OFFSET_IN_STRUCT(type, member) ((size_t) & (((type *)0)->member))

/* Page size */
#define PAGE_SIZE 4096

/* Buffer sizes */
#define BUF_SIZE_64  64
#define BUF_SIZE_128 128
#define BUF_SIZE_256 256

/*
 * =======================================
 * Interface values
 * =======================================
 */
enum pqos_interface {
        PQOS_INTER_MSR = 0,            /**< MSR */
        PQOS_INTER_OS = 1,             /**< OS */
        PQOS_INTER_OS_RESCTRL_MON = 2, /**< OS with resctrl monitoring */
        PQOS_INTER_AUTO = 3,           /**< Auto detection */
        PQOS_INTER_MMIO = 4            /**< MMIO */
};

/*
 * =======================================
 * Init and fini
 * =======================================
 */

enum pqos_feature_cfg {
        PQOS_FEATURE_ANY = 0, /**< feature status does not matter */
        PQOS_FEATURE_OFF,     /**< feature disabled */
        PQOS_FEATURE_ON,      /**< feature enabled */
};

enum pqos_cdp_config {
        PQOS_REQUIRE_CDP_OFF = PQOS_FEATURE_OFF, /**< app not compatible
                                                    with CDP */
        PQOS_REQUIRE_CDP_ON = PQOS_FEATURE_ON,   /**< app requires CDP */
        PQOS_REQUIRE_CDP_ANY = PQOS_FEATURE_ANY  /**< app will work with any
                                                    CDP setting */
};

/**
 * Memory Bandwidth Allocation configuration enumeration
 */
enum pqos_mba_config {
        PQOS_MBA_ANY,     /**< currently enabled configuration */
        PQOS_MBA_DEFAULT, /**< direct MBA hardware configuration
                             (percentage) */
        PQOS_MBA_CTRL     /**< MBA controller configuration (MBps) */
};

enum pqos_iordt_config {
        PQOS_REQUIRE_IORDT_ANY = 0, /**< currently enabled configuration */
        PQOS_REQUIRE_IORDT_OFF,     /**< I/O RDT disabled */
        PQOS_REQUIRE_IORDT_ON,      /**< I/O RDT enabled */
};

enum pqos_snc_mode {
        PQOS_SNC_LOCAL = 0, /**< Monitor local numa node only */
        PQOS_SNC_TOTAL,     /**< Monitor all numa nodes */
};

enum pqos_snc_config {
        PQOS_REQUIRE_SNC_ANY = 0,   /**< currently enabled configuration */
        PQOS_REQUIRE_SNC_LOCAL = 1, /**< SNC monitoring local mode */
        PQOS_REQUIRE_SNC_TOTAL = 2, /**< SNC monitoring total mode */
};

enum pqos_mbm_mba_modes {
        PQOS_REGION_AWARE_MBM_MBA_MODE = 0, /**< Region-aware MBM & MBA mode */
        PQOS_TOTAL_MBM_MBA_MODE = 1,        /**< Total MBM & MBA mode */
};

/**
 * Resource Monitoring ID (RMID) definition
 */
typedef uint32_t pqos_rmid_t;

/**
 * PQoS library configuration structure
 *
 * @param fd_log file descriptor to be used as library log
 * @param callback_log pointer to an application callback function
 *         void *       - An application context - it can point to a structure
 *                        or an object that an application may find useful
 *                        when receiving the callback
 *         const size_t - the size of the log message
 *         const char * - the log message
 * @param context_log application specific data that is provided
 *                    to the callback function. It can be NULL if application
 *                    doesn't require it.
 * @param verbose logging options
 *         LOG_VER_SILENT         - no messages
 *         LOG_VER_DEFAULT        - warning and error messages
 *         LOG_VER_VERBOSE        - warning, error and info messages
 *         LOG_VER_SUPER_VERBOSE  - warning, error, info and debug messages
 *
 * @param interface preference
 *         PQOS_INTER_MSR            - MSR interface or nothing
 *         PQOS_INTER_OS             - OS interface or nothing
 *         PQOS_INTER_OS_RESCTRL_MON - OS interface with resctrl monitoring
 *                                     or nothing
 */
struct pqos_config {
        int fd_log;
        void (*callback_log)(void *context,
                             const size_t size,
                             const char *message);
        void *context_log;
        int verbose;
        enum pqos_interface interface;
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
 * @note   If you require system wide interface enforcement you can do so by
 *         setting the "RDT_IFACE" environment variable.
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
 * - Memory Bandwidth Allocation
 * - Slow Memory Bandwidth Allocation
 */
enum pqos_cap_type {
        PQOS_CAP_TYPE_MON = 0, /**< QoS monitoring */
        PQOS_CAP_TYPE_L3CA,    /**< L3/LLC cache allocation */
        PQOS_CAP_TYPE_L2CA,    /**< L2 cache allocation */
        PQOS_CAP_TYPE_MBA,     /**< Memory Bandwidth Allocation */
        PQOS_CAP_TYPE_SMBA,    /**< Slow Memory Bandwidth Allocation */
        PQOS_CAP_TYPE_NUMOF
};

/**
 * L3 Cache Allocation (CA) capability structure
 */
struct pqos_cap_l3ca {
        unsigned mem_size;           /**< byte size of the structure */
        unsigned num_classes;        /**< number of classes of service */
        unsigned num_ways;           /**< number of cache ways */
        unsigned way_size;           /**< way size in bytes */
        uint64_t way_contention;     /**< ways contention bit mask */
        int cdp;                     /**< code data prioritization feature
                                        support */
        int cdp_on;                  /**< code data prioritization on or off */
        unsigned non_contiguous_cbm; /**< Non-Contiguous CBM support */
        int iordt;                   /**< I/O RDT feature support */
        int iordt_on;                /**< I/O RDT on or off */
};

/**
 * L2 Cache Allocation (CA) capability structure
 */
struct pqos_cap_l2ca {
        unsigned mem_size;           /**< byte size of the structure */
        unsigned num_classes;        /**< number of classes of service */
        unsigned num_ways;           /**< number of cache ways */
        unsigned way_size;           /**< way size in bytes */
        uint64_t way_contention;     /**< ways contention bit mask */
        int cdp;                     /**< code data prioritization feature
                                        support */
        int cdp_on;                  /**< code data prioritization on or off */
        unsigned non_contiguous_cbm; /**< Non-Contiguous CBM support */
};

/**
 * Memory Bandwidth Allocation capability structure
 */
struct pqos_cap_mba {
        unsigned mem_size;      /**< byte size of the structure */
        unsigned num_classes;   /**< number of classes of service */
        unsigned throttle_max;  /**< the max MBA can be throttled */
        unsigned throttle_step; /**< MBA granularity */
        int is_linear;          /**< the type of MBA linear/nonlinear */
        int ctrl;               /**< MBA controller support */
        int ctrl_on;            /**< MBA controller enabled */
        int mba40;              /**< MBA 4.0 extensions support */
        int mba40_on;           /**< MBA 4.0 extensions enabled */
};

/**
 * Available types of monitored events
 * (matches CPUID enumeration)
 */
enum pqos_mon_event {
        PQOS_MON_EVENT_L3_OCCUP = 1, /**< LLC occupancy event */
        PQOS_MON_EVENT_LMEM_BW = 2,  /**< Local memory bandwidth */
        PQOS_MON_EVENT_TMEM_BW = 4,  /**< Total memory bandwidth */
        PQOS_MON_EVENT_RMEM_BW = 8,  /**< Remote memory bandwidth
                                          (virtual event) */

        PQOS_MON_EVENT_IO_L3_OCCUP = 0x10,     /**< LLC occupancy event */
        PQOS_MON_EVENT_IO_TOTAL_MEM_BW = 0x20, /**< IO total memory bandwidth */
        PQOS_MON_EVENT_IO_MISS_MEM_BW = 0x40,  /**< IO miss memory bandwidth */

        RESERVED1 = 0x1000,
        RESERVED2 = 0x2000,
        PQOS_PERF_EVENT_LLC_MISS = 0x4000, /**< LLC misses */
        PQOS_PERF_EVENT_IPC = 0x8000,      /**< instructions per clock */
        PQOS_PERF_EVENT_LLC_REF = 0x10000, /**< LLC references */

        PQOS_PERF_EVENT_LLC_MISS_PCIE_READ =
            0x100000, /**< DDIO LLC read misses */
        PQOS_PERF_EVENT_LLC_MISS_PCIE_WRITE =
            0x200000, /**< DDIO LLC write misses */
        PQOS_PERF_EVENT_LLC_REF_PCIE_READ =
            0x400000, /**< DDIO LLC read references */
        PQOS_PERF_EVENT_LLC_REF_PCIE_WRITE =
            0x800000, /**< DDIO LLC write references */
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
        enum pqos_mon_event type; /**< event type */
        unsigned max_rmid;        /**< max RMID supported for this event */
        uint32_t scale_factor;    /**< factor to scale RMID value to bytes */
        unsigned counter_length;  /**< counter bit length */
        int iordt;                /**< I/O RDT monitoring is supported */
};

struct pqos_cap_mon {
        unsigned mem_size;           /**< byte size of the structure */
        unsigned max_rmid;           /**< max RMID supported by socket */
        unsigned l3_size;            /**< L3 cache size in bytes */
        unsigned num_events;         /**< number of supported events */
        int iordt;                   /**< I/O RDT monitoring is supported */
        int iordt_on;                /**< I/O RDT monitoring is enabled */
        unsigned snc_num;            /**< Number of monitoring clusters */
        enum pqos_snc_mode snc_mode; /**< SNC mode */
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
                struct pqos_cap_mba *mba;
                struct pqos_cap_mba *smba;
                void *generic_ptr;
        } u;
};

/**
 * Structure describing all Platform QoS capabilities
 */
struct pqos_cap {
        unsigned mem_size; /**< byte size of the structure */
        unsigned version;  /**< version of PQoS library */
        unsigned num_cap;  /**< number of capabilities */
        struct pqos_capability capabilities[0];
};

/**
 * Core information structure
 */
struct pqos_coreinfo {
        unsigned lcore;    /**< logical core id */
        unsigned socket;   /**< socket id in the system */
        unsigned l3_id;    /**< L3/LLC cluster id */
        unsigned l2_id;    /**< L2 cluster id */
        unsigned l3cat_id; /**< L3 CAT classes id */
        unsigned mba_id;   /**< MBA id */
        unsigned numa;     /**< numa node in the system */
        unsigned smba_id;  /**< SMBA id */
};

/**
 * CPU cache information structure
 */
struct pqos_cacheinfo {
        int detected;            /**< Indicates cache detected & valid */
        unsigned num_ways;       /**< Number of cache ways */
        unsigned num_sets;       /**< Number of sets */
        unsigned num_partitions; /**< Number of partitions */
        unsigned line_size;      /**< Cache line size in bytes */
        unsigned total_size;     /**< Total cache size in bytes */
        unsigned way_size;       /**< Cache way size in bytes */
};

/**
 * Vendor values
 */
enum pqos_vendor {
        PQOS_VENDOR_UNKNOWN = 0, /**< UNKNOWN */
        PQOS_VENDOR_INTEL = 1,   /**< INTEL */
        PQOS_VENDOR_AMD = 2,     /**< AMD */
        PQOS_VENDOR_HYGON = 3    /**< HYGON */
};

/**
 * CPU topology structure
 */
struct pqos_cpuinfo {
        unsigned mem_size;        /**< byte size of the structure */
        struct pqos_cacheinfo l2; /**< L2 cache information */
        struct pqos_cacheinfo l3; /**< L3 cache information */
        enum pqos_vendor vendor;  /**< vendor Intel/AMD/Hygon */
        unsigned num_cores;       /**< number of cores in the system */
        struct pqos_coreinfo cores[0];
};

#define PQOS_DEV_MAX_CHANNELS 8

/**
 * I/O RDT channel ID
 */
typedef uint64_t pqos_channel_t;

/**
 * I/O RDT channel information
 */
struct pqos_channel {
        pqos_channel_t channel_id; /**< Channel ID */
        int rmid_tagging;          /**< RMID tagging is supported */
        int clos_tagging;          /**< CLOS tagging is supported */
        uint64_t mmio_addr;        /**< MMIO physical address */
        unsigned numa;             /**< numa id in the system */
};

/**
 * Device type
 */
enum pqos_dev_type {
        PQOS_DEVICE_TYPE_PCI = 1,
        PQOS_DEVICE_TYPE_PCI_BRIDGE = 2,
};

/**
 * I/O RDT device information
 */
struct pqos_dev {
        enum pqos_dev_type type; /**< Device type */
        uint16_t segment;        /**< PCI segment */
        uint16_t bdf;            /**< Bus Device Function */
        /**
         * Channel enabled/valid if value > 0
         */
        pqos_channel_t channel[PQOS_DEV_MAX_CHANNELS];
};

/**
 * Device information
 */
struct pqos_devinfo {
        unsigned num_channels;         /**< Number of I/O RDT channels */
        struct pqos_channel *channels; /**< List of I/O RDT channels */
        unsigned num_devs;             /**< Number of I/O devices */
        struct pqos_dev *devs;         /**< List of I/O devices */
};

/**
 * Cache Allocation Registers for Device Agents Description Structure
 */
struct pqos_erdt_card {
        uint32_t contention_bitmask_valid;
        uint32_t non_contiguous_cbm;
        uint32_t zero_length_bitmask;
        uint32_t contention_bitmask;
        uint8_t reg_index_func_ver;
        uint64_t reg_base_addr;
        uint32_t reg_block_size;
        uint16_t cat_reg_offset;
        uint16_t cat_reg_block_size;
};

/**
 * IO Bandwidth Monitoring Registers for Device Agents Description Structure
 */
struct pqos_erdt_ibrd {
        uint32_t flags;
        uint8_t reg_index_func_ver;
        uint64_t reg_base_addr;
        uint32_t reg_block_size;
        uint16_t bw_reg_offset;
        uint16_t miss_bw_reg_offset;
        uint16_t bw_reg_clump_size;
        uint16_t miss_reg_clump_size;
        uint8_t counter_width;
        uint64_t upscaling_factor;
        uint32_t correction_factor_length;
        uint32_t *correction_factor;
};

/**
 * Cache Monitoring Registers for Device Agents Description Structure
 */
struct pqos_erdt_cmrd {
        uint32_t flags;
        uint8_t reg_index_func_ver;
        uint64_t reg_base_addr;
        uint32_t reg_block_size;
        uint16_t offset;
        uint16_t clump_size;
        uint64_t upscaling_factor;
};

/**
 * Memory-bandwidth Allocation Registers for CPU Agents Description Structure
 */
struct pqos_erdt_marc {
        uint16_t flags;
        uint8_t reg_index_func_ver;
        uint64_t opt_bw_reg_block_base_addr;
        uint64_t min_bw_reg_block_base_addr;
        uint64_t max_bw_reg_block_base_addr;
        uint32_t reg_block_size;
        uint32_t control_window_range;
};

/**
 * Memory-bandwidth Monitoring Registers for CPU Agents Description Structure
 */
struct pqos_erdt_mmrc {
        uint32_t flags;
        uint8_t reg_index_func_ver;
        uint64_t reg_block_base_addr;
        uint32_t reg_block_size;
        uint8_t counter_width;
        uint64_t upscaling_factor;
        uint32_t correction_factor_length;
        uint32_t *correction_factor;
};

/**
 * Cache Monitoring Registers for CPU Agents Description Structure
 */
struct pqos_erdt_cmrc {
        uint32_t flags;
        uint8_t reg_index_func_ver;
        uint64_t block_base_addr;
        uint32_t block_size;
        uint16_t clump_size;
        uint16_t clump_stride;
        uint64_t upscaling_factor;
};

/**
 * Device Agent Scope Entry (DASE) Structure
 */
struct pqos_erdt_dase {
        uint8_t type;
        uint16_t segment_number;
        uint8_t start_bus_number;
        uint8_t *path;
        uint16_t path_length;
};

/**
 * Device Agent Collection Description Structure
 */
struct pqos_erdt_dacd {
        uint16_t type;
        uint16_t length;
        uint16_t rmdd_domain_id;
        uint32_t num_dases;
        struct pqos_erdt_dase *dase;
};

/**
 * CPU Agent Collection Description Structure
 */
struct pqos_erdt_cacd {
        uint16_t rmdd_domain_id;
        uint32_t *enumeration_ids;
        uint32_t enum_ids_length;
};

/**
 * Resource Management Domain Description Structure
 */
struct pqos_erdt_rmdd {
        uint16_t flags;
        uint16_t num_io_l3_slices;
        uint8_t num_io_l3_sets;
        uint8_t num_io_l3_ways;
        uint16_t domain_id;
        uint32_t max_rmids;
        uint64_t control_reg_base_addr;
        uint16_t control_reg_size;
};

/**
 * ERDT CPU Agents Structure
 */
struct pqos_cpu_agent_info {
        struct pqos_erdt_rmdd rmdd;
        struct pqos_erdt_cacd cacd;
        struct pqos_erdt_cmrc cmrc;
        struct pqos_erdt_mmrc mmrc;
        struct pqos_erdt_marc marc;
};

/**
 * ERDT Device Agents Structure
 */
struct pqos_device_agent_info {
        struct pqos_erdt_rmdd rmdd;
        struct pqos_erdt_dacd dacd;
        struct pqos_erdt_cmrd cmrd;
        struct pqos_erdt_ibrd ibrd;
        struct pqos_erdt_card card;
};

/**
 * ERDT Top-Level ACPI Structure
 */
struct pqos_erdt_info {
        uint32_t max_clos;
        uint32_t num_cpu_agents;
        uint32_t num_dev_agents;
        struct pqos_cpu_agent_info *cpu_agents;
        struct pqos_device_agent_info *dev_agents;
};

/**
 * Memory Range Entry (MRE) Structure
 */
struct pqos_mre_info {
        uint32_t base_address_low;
        uint32_t base_address_high;
        uint32_t length_low;
        uint32_t length_high;
        uint16_t region_id_flags;
        uint8_t local_region_id;
        uint8_t remote_region_id;
        uint32_t regs_length;
        uint8_t *programming_regs;
};

/**
 * Memory Range and Region Mapping (MRRM) Structure
 */
struct pqos_mrrm_info {
        uint8_t max_memory_regions_supported;
        uint8_t flags;
        uint32_t num_mres;
        struct pqos_mre_info *mre;
};

/**
 * Cores to Domains Mapping Structure
 */
struct pqos_cores_domains {
        unsigned int num_cores;
        uint16_t *domains;
};

/**
 * Channels to Domains Mapping Structure
 */
struct pqos_channels_domains {
        unsigned int num_channel_ids;
        pqos_channel_t *channel_ids;
        uint16_t *domain_ids;
        uint16_t *domain_id_idxs;
};

/**
 * System configuration structure
 */
struct pqos_sysconfig {
        struct pqos_cap *cap;        /**< CPU capabilities */
        struct pqos_cpuinfo *cpu;    /**< CPU topology */
        struct pqos_devinfo *dev;    /**< PCI device info */
        struct pqos_erdt_info *erdt; /**< ERDT ACPI table info */
        struct pqos_mrrm_info *mrrm; /**< Memory range & Region IDs info */
        struct pqos_cores_domains *cores_domains; /**< Cores to domains info */
        struct pqos_channels_domains *channels_domains; /**< Channels to domains
                                                           info */
};

/**
 * Dump interface data structures
 */

/* Domain type */
enum pqos_domain_type {
        DM_TYPE_CPU,    /**< Domain type cpu */
        DM_TYPE_DEVICE, /**< Domain type device */
};

/* MMIO space */
enum pqos_mmio_dump_space {
        MMIO_DUMP_SPACE_CMRC,     /**< ERDT Sub-structure CMRC */
        MMIO_DUMP_SPACE_MMRC,     /**< ERDT Sub-structure MMRC */
        MMIO_DUMP_SPACE_MARC_OPT, /**< ERDT Sub-structure MARC - Optimal BW */
        MMIO_DUMP_SPACE_MARC_MIN, /**< ERDT Sub-structure MARC - Minimum BW */
        MMIO_DUMP_SPACE_MARC_MAX, /**< ERDT Sub-structure MARC - Maximum BW */
        MMIO_DUMP_SPACE_CMRD,     /**< ERDT Sub-structure CMRD */
        MMIO_DUMP_SPACE_IBRD,     /**< ERDT Sub-structure IBRD */
        MMIO_DUMP_SPACE_CARD,     /**< ERDT Sub-structure CARD */
        MMIO_DUMP_SPACE_ERROR,    /**< Wrong ERDT Sub-structure */
};

/* Entry in the map between ACPI structures and appropriate MMIO space */
struct pqos_mmio_dump_space_map_entry {
        enum pqos_mmio_dump_space space;   /**< Dump space */
        enum pqos_domain_type domain_type; /**< Domain type */
        const char *space_name;            /**< Dump space name */
        size_t struct_offset;   /**< Offset in cpu_agent or device_agent */
        size_t base_offset;     /**< Offset of MMMIO base address in the
                                   structure */
        size_t size_offset;     /**< Offset of MMMIO size in the structure */
        size_t size_adjustment; /**< Adjustments to bytes */
};

/* Dump topology */
struct pqos_mmio_dump_topology {
        uint32_t num_domain_ids;         /**< Number of domain ids */
        uint16_t *domain_ids;            /**< List of domain ids */
        enum pqos_mmio_dump_space space; /**< ERDT Sub-structure types */
};

/* Dump Format */

/* Hex dump width */
enum pqos_mmio_dump_width {
        MMIO_DUMP_WIDTH_64BIT, /**< 8 bytes hex value */
        MMIO_DUMP_WIDTH_8BIT,  /**< 1 byte hex value */
};

/* Dump format */
struct pqos_mmio_dump_format {
        enum pqos_mmio_dump_width width; /**< Default: 64 bits */
        unsigned int le;                 /**< Little endian flag.
                                            default: 0, means big endian */
        unsigned int bin;                /**< Binary flag.
                                            default: 0, means hexadecimal */
};

/* View window */
struct pqos_mmio_dump_view {
        unsigned long offset; /**< Default: 0 */
                              /**< Meaning the beginning of the MMIO space */
        unsigned long length; /**< Default 0. Meaning from the offset to end of
                                 the MMIO space */
};

/**
 * Main dump data structure
 */
struct pqos_mmio_dump {
        struct pqos_mmio_dump_topology topology;
        struct pqos_mmio_dump_format fmt;
        struct pqos_mmio_dump_view view;
};

/**
 * RMID Dump interface data structures
 */

/* MMIO RMID types */
enum pqos_mmio_dump_rmid_type {
        MMIO_DUMP_RMID_TYPE_MBM, /**< RMID for MBM */
        MMIO_DUMP_RMID_TYPE_CMT, /**< RMID for CMT */
        MMIO_DUMP_RMID_IO_L3,    /**< RMID for IO L3 */
        MMIO_DUMP_RMID_IO_TOTAL, /**< RMID for IO TOTAL */
        MMIO_DUMP_RMID_IO_MISS,  /**< RMID for IO MISS */
};

/**
 * Main RMID dump data structure
 */
struct pqos_mmio_dump_rmids {
        uint32_t num_domain_ids;              /**< Number of domain ids */
        uint16_t *domain_ids;                 /**< List of domain ids */
        int num_mem_regions;                  /**< Number of memory regions */
        int region_num[PQOS_MAX_MEM_REGIONS]; /**< List of memory regions */
        unsigned int num_rmids;               /**< Number of RMIDs */
        pqos_rmid_t *rmids;                   /**< List of RMIDs */
        enum pqos_mmio_dump_rmid_type rmid_type; /**< RMIDs type. Default: 0,
                                                    means MBM */
        unsigned int bin;                        /**< Binary flag. Default: 0,
                                                    means hexadecimal */
        unsigned int upscale;                    /**< Upscale raw value.
                                                    Default: 0 */
};

struct pqos_pci_info {
        unsigned int revision;
        int numa;
        int is_pcie;
        char vendor_name[BUF_SIZE_256];
        char device_name[BUF_SIZE_256];
        char subclass_name[BUF_SIZE_256];
        char kernel_driver[BUF_SIZE_128];
        char pcie_type[BUF_SIZE_64];
        unsigned int num_channels;
        pqos_channel_t channels[PQOS_DEV_MAX_CHANNELS];
        uint64_t mmio_addr[PQOS_DEV_MAX_CHANNELS];
        uint16_t domain_id;
};

/**
 * @brief Retrieves PQoS capabilities data
 *
 * @param [out] cap location to store PQoS capabilities information at
 * @param [out] cpu location to store CPU information at
 *              This parameter is optional and when NULL is passed then
 *              no cpu information is returned.
 *              CPU information includes data about number of sockets,
 *              logical cores and their assignment.
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cap_get(const struct pqos_cap **cap, const struct pqos_cpuinfo **cpu);

/**
 * @brief Retrieves PQoS system configuration data
 *
 * @param sysconfig [out] Location to store PQoS system configuration data
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_sysconfig_get(const struct pqos_sysconfig **sysconf);

/*
 * @brief Retrieves PQoS interface
 *
 * @param [out] interface PQoS interface
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success, error code otherwise
 */
int pqos_inter_get(enum pqos_interface *interface);

/*
 * =======================================
 * Monitoring
 * =======================================
 */

/**
 * The structure to store monitoring data for all of the events
 */
struct pqos_event_values {
        uint64_t llc;                  /**< cache occupancy */
        uint64_t mbm_local;            /**< bandwidth local - reading */
        uint64_t mbm_total;            /**< bandwidth total - reading */
        uint64_t mbm_remote;           /**< bandwidth remote - reading */
        uint64_t mbm_local_delta;      /**< bandwidth local - delta */
        uint64_t mbm_total_delta;      /**< bandwidth total - delta */
        uint64_t mbm_remote_delta;     /**< bandwidth remote - delta */
        uint64_t ipc_retired;          /**< instructions retired - reading */
        uint64_t ipc_retired_delta;    /**< instructions retired - delta */
        uint64_t ipc_unhalted;         /**< unhalted cycles - reading */
        uint64_t ipc_unhalted_delta;   /**< unhalted cycles - delta */
        double ipc;                    /**< retired instructions / cycles */
        uint64_t llc_misses;           /**< LLC misses - reading */
        uint64_t llc_misses_delta;     /**< LLC misses - delta */
        uint64_t llc_references;       /**< LLC references - reading */
        uint64_t llc_references_delta; /**< LLC references - delta */
};

/**
 * The structure to store region aware monitoring data for all of the events
 */
struct pqos_region_aware_event_values {
        uint64_t mbm_total[PQOS_MAX_MEM_REGIONS];
        uint64_t mbm_total_delta[PQOS_MAX_MEM_REGIONS];
        uint64_t io_llc;
        uint64_t io_total;
        uint64_t io_total_delta;
        uint64_t io_miss;
        uint64_t io_miss_delta;
};

/**
 * Memory Regions information data structure
 */
struct pqos_mon_mem_region {
        /**
         * Region Aware MBM specific section
         */
        int region_num[PQOS_MAX_MEM_REGIONS];
        int num_mem_regions;
};

struct pqos_mon_data_internal;

/**
 * Monitoring group data structure
 */
struct pqos_mon_data {
        /**
         * Common section
         */
        int valid;                       /**< structure validity marker */
        enum pqos_mon_event event;       /**< monitored event */
        void *context;                   /**< application specific context
                                            pointer */
        struct pqos_event_values values; /**< RMID events value */

        /**
         * Task specific section
         */
        unsigned num_pids; /**< number of pids in the group */
        pid_t *pids;       /**< list of pids in the group */
        unsigned tid_nr;
        pid_t *tid_map;

        /**
         * Core specific section
         */
        unsigned *cores;    /**< list of cores in the group */
        unsigned num_cores; /**< number of cores in the group */

        /**
         * I/O RDT specific section
         */
        pqos_channel_t *channels; /**< list of channels in the group */
        unsigned num_channels;    /**< number of channels in the group */

        struct pqos_mon_data_internal *intl; /**< internal data */
        /**
         * RMID events' values for memory regions
         */
        struct pqos_region_aware_event_values region_values;
        struct pqos_mon_mem_region regions; /**< memory regions information */
};

/**
 * @brief Resets monitoring by binding all cores with RMID0
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_reset(void);

/**
 * Monitoring configuration
 */
struct pqos_mon_config {
        enum pqos_iordt_config l3_iordt; /**< requested I/O RDT configuration */
        enum pqos_snc_config snc;        /**< requested SNC configuration */
};

/**
 * @brief Resets monitoring by binding all cores with RMID0
 *
 * As part of monitoring reset I/O RDT reconfiguration can be performed. This
 * can be requested via \a cfg.
 *
 * @param [in] cfg requested configuration
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_reset_config(const struct pqos_mon_config *cfg);

/**
 * @brief Reads RMID association of the \a lcore
 *
 * @param [in] lcore CPU logical core id
 * @param [out] rmid place to store resource monitoring id
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_assoc_get(const unsigned lcore, pqos_rmid_t *rmid);

/**
 * @brief Reads RMID association of \a channel
 *
 * @param [in] channel_id Control channel
 * @param [out] rmid associated RMID
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_assoc_get_channel(const pqos_channel_t channel_id,
                               pqos_rmid_t *rmid);

/**
 * @brief Reads RMID association of device channel
 *
 * @param [in] segment Device segment/domain
 * @param [in] bdf Device ID
 * @param [in] vc Device virtual channel
 * @param [out] rmid associated RMID
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_assoc_get_dev(const uint16_t segment,
                           const uint16_t bdf,
                           const unsigned vc,
                           pqos_rmid_t *rmid);

/**
 * @brief Starts resource monitoring on selected group of cores
 *
 * The function sets up content of the \a group structure.
 *
 * Note that \a event cannot select PQOS_PERF_EVENT_IPC or
 * PQOS_PERF_EVENT_L3_MISS events without any PQoS event
 * selected at the same time.
 *
 * @param [in] num_cores number of cores in \a cores array
 * @param [in] cores array of logical core id's
 * @param [in] event combination of monitoring events
 * @param [in] context a pointer for application's convenience
 *            (unused by the library)
 * @param [in,out] group a pointer to monitoring structure
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 *
 * @deprecated since 5.0.0
 * @see pqos_mon_start_cores()
 *
 * @note As of Kernel 4.10, Intel(R) RDT perf results per core are found to
 *       be incorrect.
 */
#if !defined(SWIG) && !defined(SWIGPERL)
PQOS_DEPRECATED
#endif
int pqos_mon_start(const unsigned num_cores,
                   const unsigned *cores,
                   const enum pqos_mon_event event,
                   void *context,
                   struct pqos_mon_data *group);

/**
 * @brief Starts resource monitoring on selected group of cores
 *
 * The function sets up content of the \a group structure.
 *
 * Note that \a event cannot select PQOS_PERF_EVENT_IPC or
 * PQOS_PERF_EVENT_L3_MISS events without any PQoS event
 * selected at the same time.
 *
 * @param [in] num_cores number of cores in \a cores array
 * @param [in] cores array of logical core id's
 * @param [in] event combination of monitoring events
 * @param [in] context a pointer for application's convenience
 *            (unused by the library)
 * @param [out] group a pointer to monitoring structure
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 *
 * @note As of Kernel 4.10, Intel(R) RDT perf results per core are found to
 *       be incorrect.
 */
int pqos_mon_start_cores(const unsigned num_cores,
                         const unsigned *cores,
                         const enum pqos_mon_event event,
                         void *context,
                         struct pqos_mon_mem_region *mem_region,
                         struct pqos_mon_data **group);

/**
 * @brief Starts resource monitoring of selected \a pid (process)
 *
 * @param [in] pid process ID
 * @param [in] event monitoring event id
 * @param [in] context a pointer for application's convenience
 *             (unused by the library)
 * @param [in,out] group a pointer to monitoring structure
 *
 * @deprecated since 5.0.0
 * @see pqos_mon_start_pids2()
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
#if !defined(SWIG) && !defined(SWIGPERL)
PQOS_DEPRECATED
#endif
int pqos_mon_start_pid(const pid_t pid,
                       const enum pqos_mon_event event,
                       void *context,
                       struct pqos_mon_data *group);

/**
 * @brief Starts resource monitoring of selected \a pids (processes)
 *
 * @param [in] num_pids number of pids in \a pids array
 * @param [in] pids array of process ID
 * @param [in] event monitoring event id
 * @param [in] context a pointer for application's convenience
 *             (unused by the library)
 * @param [in,out] group a pointer to monitoring structure
 *
 * @deprecated since 5.0.0
 * @see pqos_mon_start_pids2()
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
#if !defined(SWIG) && !defined(SWIGPERL)
PQOS_DEPRECATED
#endif
int pqos_mon_start_pids(const unsigned num_pids,
                        const pid_t *pids,
                        const enum pqos_mon_event event,
                        void *context,
                        struct pqos_mon_data *group);

/**
 * @brief Starts resource monitoring of selected \a pids (processes)
 *
 * @param [in] num_pids number of pids in \a pids array
 * @param [in] pids array of process ID
 * @param [in] event monitoring event id
 * @param [in] context a pointer for application's convenience
 *             (unused by the library)
 * @param [out] group a pointer to monitoring structure
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_start_pids2(const unsigned num_pids,
                         const pid_t *pids,
                         const enum pqos_mon_event event,
                         void *context,
                         struct pqos_mon_data **group);

/**
 * @brief Adds pids to the resource monitoring grpup
 *
 * @param [in] num_pids number of pids in \a pids array
 * @param [in] pids array of process ID
 * @param [in,out] group a pointer to monitoring structure
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_add_pids(const unsigned num_pids,
                      const pid_t *pids,
                      struct pqos_mon_data *group);

/**
 * @brief Remove pids from the resource monitoring grpup
 *
 * @param [in] num_pids number of pids in \a pids array
 * @param [in] pids array of process ID
 * @param [in,out] group a pointer to monitoring structure
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_remove_pids(const unsigned num_pids,
                         const pid_t *pids,
                         struct pqos_mon_data *group);

/**
 * @brief Starts uncore monitoring of selected \a sockets
 *
 * @param [in] num_sockets number of sockets in \a sockets array
 * @param [in] sockets array of socket IDs
 * @param [in] event monitoring event id
 * @param [in] context a pointer for application's convenience
 *             (unused by the library)
 * @param [out] group a pointer to monitoring structure
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_start_uncore(const unsigned num_sockets,
                          const unsigned *sockets,
                          const enum pqos_mon_event event,
                          void *context,
                          struct pqos_mon_data **group);

/**
 * @brief Starts resource monitoring on selected group of channels
 *
 * The function sets up content of the \a group structure.
 *
 * @param [in] num_channels number of channels in \a channels array
 * @param [in] channels array of channel id's
 * @param [in] event combination of monitoring events
 * @param [in] context a pointer for application's convenience
 *            (unused by the library)
 * @param [out] group a pointer to monitoring structure
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_start_channels(const unsigned num_channels,
                            const pqos_channel_t *channels,
                            const enum pqos_mon_event event,
                            void *context,
                            struct pqos_mon_data **group);

/**
 * @brief Starts resource monitoring on selected device channel
 *
 * The function sets up content of the \a group structure.
 *
 * @param [in] segment PCI segment
 * @param [in] bdf Device BDF
 * @param [in] channel Device virtual channel
 * @param [in] event combination of monitoring events
 * @param [in] context a pointer for application's convenience
 *            (unused by the library)
 * @param [out] group a pointer to monitoring structure
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_start_dev(const uint16_t segment,
                       const uint16_t bdf,
                       const uint8_t channel,
                       const enum pqos_mon_event event,
                       void *context,
                       struct pqos_mon_data **group);

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
 * @param [in] groups table of monitoring group pointers to be updated
 * @param [in] num_groups number of monitoring groups in the table
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OVERFLOW MBM counter overflow
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_poll(struct pqos_mon_data **groups, const unsigned num_groups);

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
int pqos_alloc_assoc_set(const unsigned lcore, const unsigned class_id);

/**
 * @brief Reads association of \a lcore with class of service
 *
 * @param [in] lcore CPU logical core id
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_alloc_assoc_get(const unsigned lcore, unsigned *class_id);

/**
 * @brief OS interface to associate \a task
 *        with given class of service
 *
 * @param [in] task task ID to be associated
 * @param [in] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_alloc_assoc_set_pid(const pid_t task, const unsigned class_id);

/**
 * @brief OS interface to read association
 *        of \a task with class of service
 *
 * @param [in] task ID to find association
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_alloc_assoc_get_pid(const pid_t task, unsigned *class_id);

/**
 * @brief Assign first available COS to cores in \a core_array
 *
 * While searching for available COS take technologies it is intended to use
 * with into account.
 * Note on \a technology and \a core_array selection:
 * - if L2 CAT technology is requested then cores need to belong to
 *   one L2 cluster (same L2ID)
 * - if only L3 CAT is requested then cores need to belong to one socket
 * - if only MBA is selected then cores need to belong to one socket
 *
 * @param [in] technology bit mask selecting technologies
 *             (1 << enum pqos_cap_type)
 * @param [in] core_array list of core ids
 * @param [in] core_num number of core ids in the \a core_array
 * @param [out] class_id place to store reserved COS id
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
int pqos_alloc_release(const unsigned *core_array, const unsigned core_num);

/**
 * @brief Assign first available COS to tasks in \a task_array
 *        Searches all COS directories from highest to lowest
 *
 * While searching for available COS take technologies it is intended to use
 * with into account.
 * Note on \a technology parameter:
 * - this parameter is currently reserved for future use
 * - resctrl (Linux interface) will only provide the highest class id common
 *   to all supported technologies
 *
 * @param [in] technology bit mask selecting technologies
 *             (1 << enum pqos_cap_type)
 * @param [in] task_array list of task ids
 * @param [in] task_num number of task ids in the \a task_array
 * @param [out] class_id place to store reserved COS id
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_alloc_assign_pid(const unsigned technology,
                          const pid_t *task_array,
                          const unsigned task_num,
                          unsigned *class_id);

/**
 * @brief Reassign tasks in \a task_array to default COS#0
 *
 * @param [in] task_array list of task ids
 * @param [in] task_num number of task ids in the \a task_array
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_alloc_release_pid(const pid_t *task_array, const unsigned task_num);

/**
 * @brief Resets configuration of allocation technologies
 *
 * Reverts CAT/MBA state to the one after reset:
 * - all cores associated with COS0
 * - all COS are set to give access to entire resource
 * - all device channels associated with COS0
 *
 * As part of allocation reset CDP reconfiguration can be performed.
 * This can be requested via \a l3_cdp_cfg, \a l2_cdp_cfg and \a mba_cfg.
 *
 * @param [in] l3_cdp_cfg requested L3 CAT CDP config
 * @param [in] l2_cdp_cfg requested L2 CAT CDP config
 * @param [in] mba_cfg requested MBA config
 *
 * @deprecated since 5.0.0
 * @see pqos_alloc_reset_config()
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
#if !defined(SWIG) && !defined(SWIGPERL)
PQOS_DEPRECATED
#endif
int pqos_alloc_reset(const enum pqos_cdp_config l3_cdp_cfg,
                     const enum pqos_cdp_config l2_cdp_cfg,
                     const enum pqos_mba_config mba_cfg);

/**
 * Configuration of allocation
 */
struct pqos_alloc_config {
        enum pqos_cdp_config l3_cdp;     /**< requested L3 CAT CDP config */
        enum pqos_cdp_config l2_cdp;     /**< requested L2 CAT CDP config */
        enum pqos_mba_config mba;        /**< requested MBA config */
        enum pqos_feature_cfg mba40;     /**< requested MBA 4.0 config */
        enum pqos_iordt_config l3_iordt; /**< requested L3 I/O RDT config */
        enum pqos_mba_config smba;       /**< requested SMBA config */
        int reserved[4];                 /**< reserved for future use */
};

/**
 * @brief Resets configuration of allocation technologies
 *
 * Reverts CAT/MBA state to the one after reset:
 * - all cores associated with COS0
 * - all COS are set to give access to entire resource
 *
 * As part of allocation reset CDP, MBA can be performed.
 * This can be requested via \a cfg.
 *
 * @param [in] cfg requested configuration
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_alloc_reset_config(const struct pqos_alloc_config *cfg);

/*
 * =======================================
 * L3 cache allocation
 * =======================================
 */

/**
 * L3 cache allocation class of service data structure
 */
struct pqos_l3ca {
        uint16_t domain_id; /**< RMDD, resource management domain id */
        unsigned class_id;  /**< class of service */
        int cdp;            /**< data & code masks used if true */
        union {
                uint64_t ways_mask; /**< bit mask for L3 cache ways */
                struct {
                        uint64_t data_mask;
                        uint64_t code_mask;
                } s;
        } u;
};

/**
 * @brief Sets classes of service defined by \a ca on \a l3cat_id
 *
 * @param [in] l3cat_id L3 CAT resource id
 * @param [in] num_cos number of classes of service at \a ca
 * @param [in] ca table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l3ca_set(const unsigned l3cat_id,
                  const unsigned num_cos,
                  const struct pqos_l3ca *ca);

/**
 * @brief Reads classes of service from \a socket
 *
 * @param [in] l3cat_id L3 CAT resource id
 * @param [in] max_num_ca maximum number of classes of service
 *             that can be accommodated at \a ca
 * @param [out] num_ca number of classes of service read into \a ca
 * @param [out] ca table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l3ca_get(const unsigned l3cat_id,
                  const unsigned max_num_ca,
                  unsigned *num_ca,
                  struct pqos_l3ca *ca);

/**
 * @brief Get minimum number of bits which must be set in L3 way mask when
 *        updating a class of service
 *
 * @param [out] min_cbm_bits minimum number of bits that must be set
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE if unable to determine
 */
int pqos_l3ca_get_min_cbm_bits(unsigned *min_cbm_bits);

/*
 * =======================================
 * L2 cache allocation
 * =======================================
 */

/**
 * L2 cache allocation class of service data structure
 */
struct pqos_l2ca {
        unsigned class_id; /**< class of service */
        int cdp;           /**< data & code masks used if true */
        union {
                uint64_t ways_mask; /**< bit mask for L2 cache ways */
                struct {
                        uint64_t data_mask;
                        uint64_t code_mask;
                } s;
        } u;
};

/**
 * @brief Sets classes of service defined by \a ca on \a l2id
 *
 * @param [in] l2id unique L2 cache identifier
 * @param [in] num_cos number of classes of service at \a ca
 * @param [in] ca table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l2ca_set(const unsigned l2id,
                  const unsigned num_cos,
                  const struct pqos_l2ca *ca);

/**
 * @brief Reads classes of service from \a l2id
 *
 * @param [in] l2id unique L2 cache identifier
 * @param [in] max_num_ca maximum number of classes of service
 *             that can be accommodated at \a ca
 * @param [out] num_ca number of classes of service read into \a ca
 * @param [out] ca table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l2ca_get(const unsigned l2id,
                  const unsigned max_num_ca,
                  unsigned *num_ca,
                  struct pqos_l2ca *ca);

/**
 * @brief Get minimum number of bits which must be set in L2 way mask when
 *        updating a class of service
 *
 * @param [out] min_cbm_bits minimum number of bits that must be set
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE if unable to determine
 */
int pqos_l2ca_get_min_cbm_bits(unsigned *min_cbm_bits);

/*
 * =======================================
 * Memory Bandwidth Allocation
 * =======================================
 */

/**
 * Memory regions data structure with bandwidth control type
 */
struct pqos_mba_mem_region {
        int bw_ctrl_val[PQOS_BW_CTRL_TYPE_COUNT]; /**< Q value: 0 - 0x1FF.
                                                     Invalid value is -1 */
        int region_num; /**< Memory region number: 0 or 1 or 2 or 3.
                           Invalid value is -1 */
};

/**
 * MBA class of service data structure
 */
struct pqos_mba {
        unsigned class_id; /**< class of service */
        unsigned mb_max;   /**< maximum available bandwidth in percentage
                              (without MBA controller) or in MBps
                              (with MBA controller), depending on ctrl
                              flag */
        int ctrl;          /**< MBA controller flag */
        int smba;          /**< SMBA clos */
        /* MMIO section */
        uint16_t domain_id;  /**< RMDD, resource management domain id */
        int num_mem_regions; /**< Total regions from command line */
        /**< Region number & BW Ctrl type */
        struct pqos_mba_mem_region mem_regions[PQOS_MAX_MEM_REGIONS];
};

/**
 * @brief Sets classes of service defined by \a mba on \a mba id
 *
 * @param [in]  mba_id MBA resource id
 * @param [in]  num_cos number of classes of service at \a ca
 * @param [in]  requested table with class of service definitions
 * @param [out] actual table with class of service definitions
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mba_set(const unsigned mba_id,
                 const unsigned num_cos,
                 const struct pqos_mba *requested,
                 struct pqos_mba *actual);

/**
 * @brief Reads MBA from \a mba_id
 *
 * @param [in]  mba_id MBA resource id
 * @param [in]  max_num_cos maximum number of classes of service
 *              that can be accommodated at \a mba_tab
 * @param [out] num_cos number of classes of service read into \a mba_tab
 * @param [out] mba_tab table with read classes of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mba_get(const unsigned mba_id,
                 const unsigned max_num_cos,
                 unsigned *num_cos,
                 struct pqos_mba *mba_tab);

/*
 * =======================================
 * IO RDT Allocation
 * =======================================
 */

/**
 * @brief Reads association of \a channel with class of service
 *
 * @param [in] channel Control channel
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_alloc_assoc_get_channel(const pqos_channel_t channel,
                                 unsigned *class_id);

/**
 * @brief Reads association of device channel with class of service
 *
 * @param [in] segment Device segment/domain
 * @param [in] bdf Device ID
 * @param [in] vc Device virtual channel
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_alloc_assoc_get_dev(const uint16_t segment,
                             const uint16_t bdf,
                             const unsigned vc,
                             unsigned *class_id);

/**
 * @brief Associates \a channel with given class of service
 *
 * @param [in] channel Control channel id
 * @param [in] class_id class of service
 *
 * @return Operations status
 */
int pqos_alloc_assoc_set_channel(const pqos_channel_t channel,
                                 const unsigned class_id);

/**
 * @brief Associates device with given class of service
 *
 * @param [in] segment Device segment/domain
 * @param [in] bdf Device ID
 * @param [in] vc Device virtual channel
 * @param [in] class_id class of service
 *
 * @return Operations status
 */
int pqos_alloc_assoc_set_dev(const uint16_t segment,
                             const uint16_t bdf,
                             const unsigned vc,
                             const unsigned class_id);

/*
 * =======================================
 * Utility API
 * =======================================
 */

/**
 * @brief Reads control channel for device's virtual channel
 *
 * @param [in] segment Device segment/domain
 * @param [in] bdf Device ID
 * @param [in] vc Device virtual channel
 * @param [out] channel control channel
 *
 * @return Channel
 * @retval 0 on error
 */
pqos_channel_t pqos_devinfo_get_channel_id(const struct pqos_devinfo *devinfo,
                                           const uint16_t segment,
                                           const uint16_t bdf,
                                           const unsigned vc);

/**
 * @brief Reads control channels for device
 *
 * @param [in] segment Device segment/domain
 * @param [in] bdf Device ID
 * @param [out] num_channels number of control channels
 *
 * @return Pointer to control channels' array
 * @retval NULL on error
 */
pqos_channel_t *pqos_devinfo_get_channel_ids(const struct pqos_devinfo *devinfo,
                                             const uint16_t segment,
                                             const uint16_t bdf,
                                             unsigned *num_channels);

/**
 * @brief Retrieves channel information from dev info structure
 *
 * @param [in] dev Device information structure
 * @param [in] channel_id channel id
 *
 * @return Channel information structure
 * @retval NULL on error
 */
const struct pqos_channel *
pqos_devinfo_get_channel(const struct pqos_devinfo *dev,
                         const pqos_channel_t channel_id);

/**
 * @brief Retrieves mba id's from cpu info structure
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [out] count place to store actual number of mba ids returned
 *
 * @return Allocated array of size \a count populated with mba id's
 * @retval NULL on error
 */
unsigned *pqos_cpu_get_mba_ids(const struct pqos_cpuinfo *cpu, unsigned *count);

/**
 * @brief Retrieves smba id's from cpu info structure
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [out] count place to store actual number of smba ids returned
 *
 * @return Allocated array of size \a count populated with mba id's
 * @retval NULL on error
 */
unsigned *pqos_cpu_get_smba_ids(const struct pqos_cpuinfo *cpu,
                                unsigned *count);

/**
 * @brief Retrieves l3cat id's from cpu info structure
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [out] count place to store actual number of l3cat ids returned
 *
 * @return Allocated array of size \a count populated with l3cat id's
 * @retval NULL on error
 */
unsigned *pqos_cpu_get_l3cat_ids(const struct pqos_cpuinfo *cpu,
                                 unsigned *count);

/**
 * @brief Retrieves socket id's from cpu info structure
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [out] count place to store actual number of sockets returned
 *
 * @return Allocated array of size \a count populated with socket id's
 * @retval NULL on error
 */
unsigned *pqos_cpu_get_sockets(const struct pqos_cpuinfo *cpu, unsigned *count);

/**
 * @brief Retrieves numa id's from cpu info structure
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [out] count place to store actual number of sockets returned
 *
 * @return Allocated array of size \a count populated with numa id's
 * @retval NULL on error
 */
unsigned *pqos_cpu_get_numa(const struct pqos_cpuinfo *cpu, unsigned *count);

/**
 * @brief Retrieves L2 id's from cpu info structure
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [out] count place to store actual number of L2 id's returned
 *
 * @return Allocated array of size \a count populated with L2 id's
 * @retval NULL on error
 */
unsigned *pqos_cpu_get_l2ids(const struct pqos_cpuinfo *cpu, unsigned *count);

/**
 * @brief Creates list of cores belonging to given L3 cluster
 *
 * Function allocates memory for the core list that needs to be freed by
 * the caller.
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] l3_id L3 cluster ID
 * @param [out] count place to put number of cores found
 *
 * @return Pointer to list of cores belonging to the L3 cluster
 * @retval NULL on error or if no core found
 */
unsigned *pqos_cpu_get_cores_l3id(const struct pqos_cpuinfo *cpu,
                                  const unsigned l3_id,
                                  unsigned *count);

/**
 * @brief Retrieves core id's from cpu info structure for \a socket
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] socket CPU socket id to enumerate
 * @param [out] count place to store actual number of core id's returned
 *
 * @return Allocated core id array
 * @retval NULL on error
 */
unsigned *pqos_cpu_get_cores(const struct pqos_cpuinfo *cpu,
                             const unsigned socket,
                             unsigned *count);

/**
 * @brief Retrieves task id's from resctrl task file for a given COS
 *
 * @param [in] class_id Class of Service ID
 * @param [out] count place to store actual number of task id's returned
 *
 * @return Allocated task id array
 * @retval NULL on error
 */
unsigned *pqos_pid_get_pid_assoc(const unsigned class_id, unsigned *count);

/**
 * @brief Retrieves one core id from cpu info structure for \a mba_id
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] mba_id to enumerate
 * @param [out] lcore place to store returned core id
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cpu_get_one_by_mba_id(const struct pqos_cpuinfo *cpu,
                               const unsigned mba_id,
                               unsigned *lcore);
/**
 * @brief Retrieves one core id from cpu info structure for \a smba_id
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] mba to enumerate
 * @param [out] lcore place to store returned core id
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cpu_get_one_by_smba_id(const struct pqos_cpuinfo *cpu,
                                const unsigned smba_id,
                                unsigned *lcore);
/**
 * @brief Retrieves core information from cpu info structure for \a lcore
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] lcore logical core ID to retrieve information for
 *
 * @return Pointer to core information structure
 * @retval NULL on error
 */
const struct pqos_coreinfo *
pqos_cpu_get_core_info(const struct pqos_cpuinfo *cpu, unsigned lcore);

/**
 * @brief Retrieves one core id from cpu info structure for \a socket
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] socket CPU socket id to enumerate
 * @param [out] lcore place to store returned core id
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cpu_get_one_core(const struct pqos_cpuinfo *cpu,
                          const unsigned socket,
                          unsigned *lcore);

/**
 * @brief Retrieves one core id from cpu info structure for \a numaid
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] numaid id to enumerate
 * @param [out] lcore place to store returned core id
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cpu_get_one_by_numaid(const struct pqos_cpuinfo *cpu,
                               const unsigned numaid,
                               unsigned *lcore);

/**
 * @brief Retrieves one core id from cpu info structure for \a l3cat id
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] l3cat_id id to enumerate
 * @param [out] lcore place to store returned core id
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cpu_get_one_by_l3cat_id(const struct pqos_cpuinfo *cpu,
                                 const unsigned l3cat_id,
                                 unsigned *lcore);
/**
 * @brief Retrieves one core id from cpu info structure for \a l2id
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] l2id unique L2 cache identifier
 * @param [out] lcore place to store returned core id
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cpu_get_one_by_l2id(const struct pqos_cpuinfo *cpu,
                             const unsigned l2id,
                             unsigned *lcore);

/**
 * @brief Verifies if \a core is a valid logical core id
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] lcore logical core id
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success (\a lcore is valid)
 */
int pqos_cpu_check_core(const struct pqos_cpuinfo *cpu, const unsigned lcore);

/**
 * @brief Retrieves socket id for given logical core id
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] lcore logical core id
 * @param [out] socket location to store socket id at
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cpu_get_socketid(const struct pqos_cpuinfo *cpu,
                          const unsigned lcore,
                          unsigned *socket);

/**
 * @brief Retrieves NUMA cluster id for given logical core id
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] lcore logical core id
 * @param [out] numa location to store numa id at
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cpu_get_numaid(const struct pqos_cpuinfo *cpu,
                        const unsigned lcore,
                        unsigned *numa);

/**
 * @brief Retrieves monitoring cluster id for given logical core id
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @param [in] lcore logical core id
 * @param [out] cluster location to store cluster id at
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_cpu_get_clusterid(const struct pqos_cpuinfo *cpu,
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
int pqos_cap_get_type(const struct pqos_cap *cap,
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
int pqos_cap_get_event(const struct pqos_cap *cap,
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
int pqos_l3ca_get_cos_num(const struct pqos_cap *cap, unsigned *cos_num);

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
int pqos_l2ca_get_cos_num(const struct pqos_cap *cap, unsigned *cos_num);

/**
 * @brief Retrieves number of memory B/W allocation classes of service from
 *        \a cap structure.
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [out] cos_num place to store number of classes of service
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mba_get_cos_num(const struct pqos_cap *cap, unsigned *cos_num);

/**
 * @brief Retrieves number of "slow" memory B/W allocation classes of service
 * from \a cap structure.
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [out] cos_num place to store number of classes of service
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_smba_get_cos_num(const struct pqos_cap *cap, unsigned *cos_num);

/**
 * @brief Retrieves L3 CDP status
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [out] cdp_supported place to store L3 CDP support status
 * @param [out] cdp_enabled place to store L3 CDP enable status
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l3ca_cdp_enabled(const struct pqos_cap *cap,
                          int *cdp_supported,
                          int *cdp_enabled);

/**
 * @brief Retrieves L3 I/O RDT status
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [out] supported place to store L3 I/O RDT support status
 * @param [out] enabled place to store L3 I/O RDT enable status
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l3ca_iordt_enabled(const struct pqos_cap *cap,
                            int *supported,
                            int *enabled);

/**
 * @brief Retrieves L2 CDP status
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [out] cdp_supported place to store L2 CDP support status
 * @param [out] cdp_enabled place to store L2 CDP enable status
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_l2ca_cdp_enabled(const struct pqos_cap *cap,
                          int *cdp_supported,
                          int *cdp_enabled);

/**
 * @brief Retrieves MBA controller configuration status
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [out] ctrl_supported place to store MBA controller support status
 * @param [out] ctrl_enabled place to store MBA controller enable status
 *
 */
int pqos_mba_ctrl_enabled(const struct pqos_cap *cap,
                          int *ctrl_supported,
                          int *ctrl_enabled);

/**
 * @brief returns the CPU vendor identification
 *
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 * @retval 0 if vendor unknown
 * @return 1 if the vendor is Intel
 * @return 2 if the vendor is AMD
 * @return 3 if the vendor is Hygon
 */
enum pqos_vendor pqos_get_vendor(const struct pqos_cpuinfo *cpu);

/*
 * @brief Retrieves a monitoring value from a group for a specific event.
 *
 * @note Update event values using \a pqos_mon_poll
 *
 * @param [in] group monitoring group
 * @param [in] event_id event being monitored
 * @param [out] value monitoring counter value
 * @param [out] delta monitoring counter delta
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_get_value(const struct pqos_mon_data *const group,
                       const enum pqos_mon_event event_id,
                       uint64_t *value,
                       uint64_t *delta);

/*
 * @brief Retrieves a memory region monitoring value from a group
 * for a specific event.
 *
 * @note Update event values using \a pqos_mon_poll
 *
 * @param [in] group monitoring group
 * @param [in] event_id event being monitored
 * @param [in] region_num memory region being monitored
 * @param [out] value memory region monitoring counter value
 * @param [out] delta memory region monitoring counter delta
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_get_region_value(const struct pqos_mon_data *const group,
                              const enum pqos_mon_event event_id,
                              int region_num,
                              uint64_t *value,
                              uint64_t *delta);

/*
 * @brief Retrieves a IPC value from a monitoring group.
 *
 * @note Update event values using \a pqos_mon_poll
 *
 * @param [in] group monitoring group
 * @param [out] value IPC value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_mon_get_ipc(const struct pqos_mon_data *const group, double *value);

/**
 * @brief Frees memory previously allocated and returned by the library
 * functions.
 *
 * Some pqos functions return memory buffers to their callers.
 * It's the responsibility of the caller to free this memory when no longer
 * needed.
 *
 * Call this function from Foreign Function Interfaces to avoid memory leaks.
 *
 * @param [in] ptr to memory area allocated and returned by pqos functions.
 *             When NULL, the function does nothing.
 *
 * @return Void.
 */
void pqos_free(void *ptr);

/**
 *  @brief Shows the hex dumps of MMIO spaces according to given parameters
 *
 *  @param [in] dump defines dump output
 *
 *  @retval PQOS_RETVAL_OK success
 **/

int pqos_dump(const struct pqos_mmio_dump *dump_cfg);

/**
 *  @brief Shows the RMID dumps of a particular type
 *
 *  @param [in] dump defines dump output
 *
 *  @retval PQOS_RETVAL_OK success
 **/
int pqos_dump_rmids(const struct pqos_mmio_dump_rmids *dump_cfg);

/**
 * @brief Get I/O devices' PCI information
 *
 * @param [out] pci_info pqos_pci_info structure instance
 * @param [in] segment PCI segment
 * @param [in] bdf Bus Device Function
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pqos_io_devs_get(struct pqos_pci_info *pci_info,
                     uint16_t segment,
                     uint16_t bdf);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_H__ */
