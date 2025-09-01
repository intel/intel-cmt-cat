/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2025 Intel Corporation. All rights reserved.
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

#ifndef MMIO_H
#define MMIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"

/* MMIO constants */
#define RDT_REG_SIZE         8
#define PAGE_SIZE            4096
#define BYTES_PER_REGION_SET 512
#define BYTES_PER_CLOS_ENTRY 8
#define BYTES_PER_RMID_ENTRY 8
#define MBM_REGION_SIZE      2048
#define MBA_MAX_BW           0x1FF

/* MMIO registers bit fields */

/* RDT_CTRL register */
#define RDT_CTRL_TME_MASK       0x0000000000000004ULL
#define RDT_CTRL_TME_RESET_MASK ~RDT_CTRL_TME_MASK
#define RDT_CTRL_TME_SHIFT      2

/* L3_CMT_RMID register */
#define L3_CMT_RMID_COUNT_MASK 0x7fffffffffffffffULL
#define L3_CMT_RMID_U_MASK     ~L3_CMT_RMID_COUNT_MASK

/* MBM_REGION_RMID register */
#define MBM_REGION_RMID_COUNT_MASK 0x3fffffffffffffffULL
#define MBM_REGION_RMID_FLAGS_MASK ~MBM_REGION_RMID_COUNT_MASK
#define MBM_REGION_RMID_U_MASK     0x8000000000000000ULL
#define MBM_REGION_RMID_O_MASK     0x4000000000000000ULL

/* MBA_BW register for MIN, MAX,OPTIMAL */
#define MBA_BW_ALL_BR_MASK        0x01ff01ff01ff01ffULL
#define MBA_BW_ALL_BR_RESET_MASK  ~MBA_BW_ALL_BR_MASK
#define MBA_BW_ALL_BR0_MASK       0x00000000000001ffULL
#define MBA_BW_ALL_BR0_RESET_MASK ~MBA_BW_ALL_BR0_MASK
#define MBA_BW_ALL_BR1_MASK       0x0000000001ff0000ULL
#define MBA_BW_ALL_BR1_RESET_MASK ~MBA_BW_ALL_BR1_MASK
#define MBA_BW_ALL_BR1_SHIFT      0x10
#define MBA_BW_ALL_BR2_MASK       0x000001ff00000000ULL
#define MBA_BW_ALL_BR2_RESET_MASK ~MBA_BW_ALL_BR2_MASK
#define MBA_BW_ALL_BR2_SHIFT      0x20
#define MBA_BW_ALL_BR3_MASK       0x01ff000000000000ULL
#define MBA_BW_ALL_BR3_RESET_MASK ~MBA_BW_ALL_BR3_MASK
#define MBA_BW_ALL_BR3_SHIFT      0x30

/* IOL3_CMT_RMID register */
#define IOL3_CMT_RMID_COUNT_MASK 0x7fffffffffffffffULL
#define IOL3_CMT_RMID_U_MASK     ~IOL3_CMT_RMID_COUNT_MASK

/* TOTAL_IO_BW_RMID register */
#define TOTAL_IO_BW_RMID_COUNT_MASK 0x3fffffffffffffffULL
#define TOTAL_IO_BW_RMID_FLAGS_MASK ~TOTAL_IO_BW_RMID_COUNT_MASK
#define TOTAL_IO_BW_RMID_U_MASK     0x8000000000000000ULL
#define TOTAL_IO_BW_RMID_O_MASK     0x4000000000000000ULL

/* IO_MISS_BW_RMID register */
#define IO_MISS_BW_RMID_COUNT_MASK 0x3fffffffffffffffULL
#define IO_MISS_BW_RMID_FLAGS_MASK ~IO_MISS_BW_RMID_COUNT_MASK
#define IO_MISS_BW_RMID_U_MASK     0x8000000000000000ULL
#define IO_MISS_BW_RMID_O_MASK     0x4000000000000000ULL

/* IOL3_MASK register */
#define IOL3_CBM_MASK       0x0fffffff00000000ULL
#define IOL3_CBM_RESET_MASK ~IOL3_CBM_MASK
#define IOL3_CBM_SHIFT      0x20
/* First CAT Register Block of MMIO registers for CLOS */
#define REG_BLOCK_SIZE_ZERO 0

/* MMIO RMID types */
typedef uint64_t l3_cmt_rmid_t;
typedef uint64_t l3_mbm_rmid_t;
typedef uint64_t iol3_cmt_rmid_t;

/* Describes both TOTAL_IO_BW_RMID and IO_MISS_BW_RMID registers */
typedef uint64_t iol3_mbm_rmid_t;

/* MMIO data retrieval functions */

/**
 * @brief Returns current MBM/MBA mode
 *
 * @param[in] rmdd structure containing MMIO description for RDT_CTRL register
 * @param[out] value RDT_CTRL register value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int get_mba_mode_v1(const struct pqos_erdt_rmdd *rmdd, uint64_t *value);

/**
 * @brief Set current MBM/MBA mode
 *
 * @param[in] rmdd structure, containing MMIO description for RDT_CTRL register
 * @param[in] value MBM/MBA mode:
 *            0 - Region mode for MBM/MBA with MMIO interface enabled
 *            1 - Total mode for MBM/MBA with MSR interface enabled
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int set_mbm_mba_mode_v1(const struct pqos_erdt_rmdd *rmdd, unsigned int value);

/**
 * @brief Returns L3 cache RMID subrange n_first...n_last for a given resource
 * management domain (CBB for DMR)
 *
 * @param[in] cmrc structure under given RMDD containing MMIO
 *            description for L3_CMT_RMID_n register set
 * @param[in] n_first first RMID to extract
 * @param[in] n_last last RMID to extract
 *            0 <= n_first <= n_last < RMDD.MAX_RMID
 * @param[out] rmid_vals array of RMID range. Array must be allocated on the
 *             caller side.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */

int get_l3_cmt_rmid_range_v1(const struct pqos_erdt_cmrc *cmrc,
                             unsigned int rmid_first,
                             unsigned int rmid_last,
                             l3_cmt_rmid_t *rmids_val);

/**
 * @brief Returns memory bandwidth RMID subrange n_first...n_last for a region
 * under a given resource management domain (CBB for DMR)
 *
 * @param[in] mmrc structure under given RMDD containing MMIO
 * description for MBM_Region_m_RMID_n register set
 * @param[in] region_number region number to which RMID sub-range belongs to.
 * @param[in] n_first first RMID to extract
 * @param[in] n_last last RMID to extract
 *            0 <=  n_first <= n_last < RMDD.MAX_RMID
 * @param[out] rmid_vals array of RMID range. Array must be allocated on the
 *             caller side.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */

int get_l3_mbm_region_rmid_range_v1(const struct pqos_erdt_mmrc *mmrc,
                                    unsigned int region_number,
                                    unsigned int rmid_first,
                                    unsigned int rmid_last,
                                    l3_mbm_rmid_t *rmids_val);

/**
 * @brief Get MBA optimal bandwidth value for a particular CLOS for a region
 * under a given resource management domain (CBB for DMR)
 *
 * @param[in] marc structure under given RMDD containing MMIO
 *            description for MBA_OPTIMAL_BW_n register set
 * @param[in] region_number region number to which RMID sub-range belongs to.
 * @param[in] clos_number region number to which RMID sub-range belongs to.
 * @param[out] value CLOS value. Allocated on the caller's side.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int get_mba_optimal_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                      unsigned int region_number,
                                      unsigned int clos_number,
                                      unsigned int *value);

/**
 * @brief Setup MBA optimal bandwidth value for a particular CLOS for a region
 * under a given resource management domain (CBB for DMR)
 *
 * @param[in] marc structure under given RMDD containing MMIO
 *            description for MBA_OPTIMAL_BW_n register set
 * @param[in] region_number region number to which RMID sub-range belongs to.
 * @param[in] clos_number region number to which RMID sub-range belongs to.
 * @param[in] value CLOS value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int set_mba_optimal_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                      unsigned int region_number,
                                      unsigned int clos_number,
                                      unsigned int value);

/**
 * @brief Get MBA minimum bandwidth value for a particular CLOS for a region
 * under a given resource management domain (CBB for DMR)
 *
 * @param[in] marc structure under given RMDD containing MMIO
 *            description for MBA_OPTIMAL_BW_n register set
 * @param[in] region_number region number to which RMID sub-range belongs to.
 * @param[in] clos_number region number to which RMID sub-range belongs to.
 * @param[out] value CLOS value. Allocates on the caller side.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int get_mba_min_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                  unsigned int region_number,
                                  unsigned int clos_number,
                                  unsigned int *value);

/**
 * @brief Setup MBA optimal bandwidth value for a particular CLOS for a region
 * under a given resource management domain (CBB for DMR)
 *
 * @param[in] marc structure under given RMDD containing MMIO
 *            description for MBA_OPTIMAL_BW_n register set
 * @param[in] region_number region number to which RMID sub-range belongs to.
 * @param[in] clos_number region number to which RMID sub-range belongs to.
 * @param[in] value CLOS value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int set_mba_min_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                  unsigned int region_number,
                                  unsigned int clos_number,
                                  unsigned int value);

/**
 * @brief Get MBA maximum bandwidth value for a particular CLOS for a region
 * under a given resource management domain (CBB for DMR)
 *
 * @param[in] marc structure under given RMDD containing MMIO
 *            description for MBA_OPTIMAL_BW_n register set
 * @param[in] region_number region number to which RMID sub-range belongs to.
 * @param[in] clos_number clos number to which RMID sub-range belongs to.
 * @param[out] value CLOS value. Allocates on the caller side.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int get_mba_max_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                  unsigned int region_number,
                                  unsigned int clos_number,
                                  unsigned int *value);

/**
 * @brief Setup MBA maximum bandwidth value for a particular CLOS for a region
 * under a given resource management domain (CBB for DMR)
 *
 * @param[in] marc structure under given RMDD containing MMIO
 *            description for MBA_OPTIMAL_BW_n register set
 * @param[in] region_number region number to which RMID sub-range belongs to.
 * @param[in] clos_number region number to which RMID sub-range belongs to.
 * @param[in] value CLOS value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int set_mba_max_bw_region_clos_v1(const struct pqos_erdt_marc *marc,
                                  unsigned int region_number,
                                  unsigned int clos_number,
                                  unsigned int value);

/**
 * @brief Returns IO L3 cache RMID subrange n_first...n_last for a given
 * resource management domain (CBB for DMR)
 *
 * @param[in] cmrd structure under given RMDD containing MMIO
 *            description for IOL3_CMT_RMID_n register set
 * @param[in] n_first  first RMID to extract
 * @param[in] n_last last RMID to extract
 *            0 <=  n_first <= n_last < RMDD.MAX_RMID
 * @param[out] rmid_vals array of RMID range. Array must be allocated on the
 *             caller side.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int get_iol3_cmt_rmid_range_v1(const struct pqos_erdt_cmrd *cmrd,
                               unsigned int rmid_first,
                               unsigned int rmid_last,
                               iol3_cmt_rmid_t *rmids_val);

/**
 * @brief Returns total IO bandwidth RMID subrange n_first...n_last for a given
 * resource management domain (CBB for DMR)
 *
 * @param[in] ibrd structure under given RMDD containing MMIO
 *            description for Total_IO_BW_RMID_nregister set
 * @param[in] n_first first RMID to extract
 * @param[in] n_last last RMID to extract
 *            0 <=  n_first <= n_last < RMDD.MAX_RMID
 * @param[out] rmid_vals array of RMID range. Array must be allocated on the
 *             caller side.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int get_total_iol3_mbm_rmid_range_v1(const struct pqos_erdt_ibrd *ibrd,
                                     unsigned int rmid_first,
                                     unsigned int rmid_last,
                                     iol3_mbm_rmid_t *rmids_val);

/**
 * @brief Returns miss IO bandwidth RMID subrange n_first...n_last for a given
 * resource management domain (CBB for DMR)
 *
 * @param[in] ibrd structure under given RMDD containing MMIO
 *            description for IO_MISS_BW_RMID_n register set
 * @param[in] n_first first RMID to extract
 * @param[in] n_last last RMID to extract
 *            0 <=  n_first <= n_last < RMDD.MAX_RMID
 * @param[out] rmid_vals array of RMID range. Array must be allocated on the
 *             caller side.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int get_miss_iol3_mbm_rmid_range_v1(const struct pqos_erdt_ibrd *ibrd,
                                    unsigned int rmid_first,
                                    unsigned int rmid_last,
                                    iol3_mbm_rmid_t *rmids_val);

/**
 * @brief Get cache bit mask value for a particular CLOS under a given resource
 * management domain (CBB for DMR)
 *
 * @param[in] card structure under given RMDD containing MMIO
 *            description for IOL3_MASK_n register set
 * @param[in] clos_number clos number to extract bit mask
 * @param[in] block_number block number in which clos resided.
 *            Has exactly the same value in the each CAT register
 *            block according to the spec
 * @param[out] value CLOS value. Allocates on the caller side.
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int get_iol3_cbm_clos_v1(const struct pqos_erdt_card *card,
                         unsigned int clos_number,
                         unsigned int block_number,
                         uint64_t *value);

/**
 * @brief Set cache bit mask value for a particular CLOS under a given
 * resource management domain (CBB for DMR).
 *
 * Note: Unlike get_iol3_cbm_clos_v1 function does not provide a
 *       'block_number' parameter as all the similar CLOSes in all the
 *       CAT register blocks must be set to the similar value according
 *       to the specification
 *
 * @param[in] card structure under given RMDD containing MMIO
 *            description for IOL3_MASK_n register set
 * @param[in] clos_number region number to which RMID sub-range belongs to.
 * @param[in] value CLOS value
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if error occurs
 */
int set_iol3_cbm_clos_v1(const struct pqos_erdt_card *card,
                         unsigned int clos_number,
                         uint64_t value);

/* Helper functions */

/**
 * @brief  Converts l3_cmt_rmid_t to uint64_t
 *
 * @param[in] value to convert
 *
 * @return converted value with cleared flags bits
 * @retval uint64_t
 */
uint64_t l3_cmt_rmid_to_uint64(l3_cmt_rmid_t value);

/**
 * @brief  Checks if value of type l3_cmt_rmid_t is valid by analyzing
 * 'available' bit
 *
 * @param[in] value to check
 *
 * @return 'available' flag status
 * @retval 0 if 'unavailable' bit set
 * @retval 1 if 'unavailable' bit cleared
 */
int is_available_l3_cmt_rmid(l3_cmt_rmid_t value);

/**
 * @brief  Converts l3_mbm_rmid_t to uint64_t
 *
 * @param[in] value to convert
 *
 * @return converted value with cleared flags bits
 * @retval uint_64t
 */
uint64_t l3_mbm_rmid_to_uint64(l3_mbm_rmid_t value);

/**
 * @brief  Checks if value of type l3_mbm_rmid_t is valid by analyzing
 * 'available' bit
 *
 * @param[in] value to check
 *
 * @return 'available' flag status
 * @retval 0 if 'unavailable' bit set
 * @retval 1 if 'unavailable' bit cleared
 */
int is_available_l3_mbm_rmid(l3_mbm_rmid_t value);

/**
 * @brief Checks if value of type l3_mbm_rmid_t is valid by analyzing
 * 'overflow' bit
 *
 * @param[in] value to check
 *
 * @return 'overflow' flag status
 * @retval 1 if 'overflow' bit set
 * @retval 0 if 'overflow' bit cleared
 */
int is_overflow_l3_mbm_rmid(l3_mbm_rmid_t value);

/**
 * @brief Converts iol3_cmt_rmid_t to uint64_t
 *
 * @param[in] value to convert
 *
 * @return converted value with cleared flags bits
 * @retval uint_64t
 */
uint64_t iol3_cmt_rmid_to_uint64(iol3_cmt_rmid_t value);

/**
 * @brief  Checks if value of type iol3_cmt_rmid_t is valid by analyzing
 * 'available' bit
 *
 * @param[in] value to check
 *
 * @return 'available' flag status
 * @retval 0 if 'unavailable' bit set
 * @retval 1 if 'unavailable' bit cleared
 */
int is_available_iol3_cmt_rmid(iol3_cmt_rmid_t value);

/**
 * @brief  Converts iol3_mbm_rmid_t to uint64_t
 * Note: Applicable to both TOTAL_IO_BW_RMID and IO_MISS_BW_RMID registers
 *
 * @param[in] value to convert
 *
 * @return converted value with cleared flags bits
 * @retval uint_64t
 */
uint64_t iol3_mbm_rmid_to_uint64(iol3_mbm_rmid_t value);

/**
 * @brief  Checks if value of type iol3_mbm_rmid_t is valid by analyzing
 * 'available' bit
 * Note: Applicable to both TOTAL_IO_BW_RMID and IO_MISS_BW_RMID registers
 *
 * @param[in] value to check
 *
 * @return 'available' flag status
 * @retval 0 if 'unavailable' bit set
 * @retval 1 if 'unavailable' bit cleared
 */
int is_available_iol3_mbm_rmid(iol3_mbm_rmid_t value);

/**
 * @brief  Checks if value of type iol3_mbm_rmid_t is valid by analyzing
 * 'overflow' bit
 * Note: Applicable to both TOTAL_IO_BW_RMID and IO_MISS_BW_RMID registers
 *
 * @param[in] value to check
 *
 * @return 'overflow' flag status
 * @retval 1 if 'overflow' bit set
 * @retval 0 if 'overflow' bit cleared
 */
int is_overflow_iol3_mbm_rmid(iol3_mbm_rmid_t value);

#ifdef __cplusplus
}
#endif

#endif /* MMIO_H */
