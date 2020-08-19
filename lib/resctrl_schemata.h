/*
 * BSD LICENSE
 *
 * Copyright(c) 2019-2020 Intel Corporation. All rights reserved.
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

/**
 * @brief Internal header file for resctrl schemata
 */

#ifndef __PQOS_RESCTRL_SCHEMATA_H__
#define __PQOS_RESCTRL_SCHEMATA_H__

#include "pqos.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * @brief Structure to hold schemata
 */
struct resctrl_schemata;

/**
 * @brief Allocates memory of schemata struct
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [in] cpu CPU information structure from \a pqos_cap_get
 *
 * @return Operational status
 * @retval pointer to allocated data
 */
struct resctrl_schemata *resctrl_schemata_alloc(const struct pqos_cap *cap,
                                                const struct pqos_cpuinfo *cpu);

/*
 * @brief Deallocate memory of schemata struct
 *
 * @param[in] schemata Schemata structure
 */
void resctrl_schemata_free(struct resctrl_schemata *schemata);

/*
 * @brief Reset schemata to default values
 *
 * @param [out] schemata Schemata structure
 * @param [in] l3ca_cap l3 cache capability
 * @param [in] l2ca_cap l2 cache capability
 * @param [in] mba_cap mba capability
 *
 * @return Operation status
 */
int resctrl_schemata_reset(struct resctrl_schemata *schemata,
                           const struct pqos_cap_l3ca *l3ca_cap,
                           const struct pqos_cap_l2ca *l2ca_cap,
                           const struct pqos_cap_mba *mba_cap);

/*
 * @brief Reads L2 class of service from schemata
 *
 * @param [in] schemata Schemata structure
 * @param [in] resource_id unique L2 cache identifier
 * @param [out] ca class of service definition
 *
 * @return Operation status
 */
int resctrl_schemata_l2ca_get(const struct resctrl_schemata *schemata,
                              unsigned resource_id,
                              struct pqos_l2ca *ca);

/*
 * @brief Updates L2 class of service in schemata
 *
 * @param [in,out] schemata Schemata structure
 * @param [in] resource_id unique L2 cache identifier
 * @param [in] ca class of service definition
 *
 * @return Operation status
 */
int resctrl_schemata_l2ca_set(struct resctrl_schemata *schemata,
                              unsigned resource_id,
                              const struct pqos_l2ca *ca);

/*
 * @brief Reads L3 class of service from schemata
 *
 * @param [in] schemata Schemata structure
 * @param [in] resource_id unique L3 cache identifier
 * @param [out] ca class of service definition
 *
 * @return Operation status
 */
int resctrl_schemata_l3ca_get(const struct resctrl_schemata *schemata,
                              unsigned resource_id,
                              struct pqos_l3ca *ca);

/*
 * @brief Updates L3 class of service in schemata
 *
 * @param [in,out] schemata Schemata structure
 * @param [in] resource_id unique L3 cache identifier
 * @param [in] ca class of service definition
 *
 * @return Operation status
 */
int resctrl_schemata_l3ca_set(struct resctrl_schemata *schemata,
                              unsigned resource_id,
                              const struct pqos_l3ca *ca);

/*
 * @brief Reads MBA class of service from schemata
 *
 * @param [in] schemata Schemata structure
 * @param [in] resource_id unique L3 cache identifier
 * @param [out] ca class of service definition
 *
 * @return Operation status
 */
int resctrl_schemata_mba_get(const struct resctrl_schemata *schemata,
                             unsigned resource_id,
                             struct pqos_mba *ca);

/*
 * @brief Updates MBA class of service in schemata
 *
 * @param [in,out] schemata Schemata structure
 * @param [in] resource_id unique L3 cache identifier
 * @param [in] ca class of service definition
 *
 * @return Operation status
 */
int resctrl_schemata_mba_set(struct resctrl_schemata *schemata,
                             unsigned resource_id,
                             const struct pqos_mba *ca);

/**
 * @brief Read schemata from file
 *
 * @param [in] fd read file descriptor
 * @param [out] schemata schemata struct to store values
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_schemata_read(FILE *fd, struct resctrl_schemata *schemata);

/**
 * @brief Write schemata to file
 *
 * @param [in] fd write file descriptor
 * @param [in] schemata schemata to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_schemata_write(FILE *fd, const struct resctrl_schemata *schemata);

/**
 * @brief Write l3ca schemata to file
 *
 * @param [in] fd write file descriptor
 * @param [in] schemata schemata to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_schemata_l3ca_write(FILE *fd,
                                const struct resctrl_schemata *schemata);

/**
 * @brief Write l2ca schemata to file
 *
 * @param [in] fd write file descriptor
 * @param [in] schemata schemata to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_schemata_l2ca_write(FILE *fd,
                                const struct resctrl_schemata *schemata);

/**
 * @brief Write mba schemata to file
 *
 * @param [in] fd write file descriptor
 * @param [in] schemata schemata to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_schemata_mba_write(FILE *fd,
                               const struct resctrl_schemata *schemata);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_RESCTRL_SCHEMATA_H__ */
