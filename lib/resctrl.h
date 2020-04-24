/*
 * BSD LICENSE
 *
 * Copyright(c) 2018-2020 Intel Corporation. All rights reserved.
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
 * @brief Internal header file for resctrl common functions
 */

#ifndef __PQOS_RESCTRL_H__
#define __PQOS_RESCTRL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h> /**< CHAR_BIT*/

#ifndef RESCTRL_PATH
#define RESCTRL_PATH "/sys/fs/resctrl"
#endif
#define RESCTRL_PATH_INFO        RESCTRL_PATH "/info"
#define RESCTRL_PATH_INFO_L3_MON RESCTRL_PATH_INFO "/L3_MON"
#define RESCTRL_PATH_INFO_L3     RESCTRL_PATH_INFO "/L3"
#define RESCTRL_PATH_INFO_L3CODE RESCTRL_PATH_INFO "/L3CODE"
#define RESCTRL_PATH_INFO_L3DATA RESCTRL_PATH_INFO "/L3DATA"
#define RESCTRL_PATH_INFO_L2     RESCTRL_PATH_INFO "/L2"
#define RESCTRL_PATH_INFO_L2CODE RESCTRL_PATH_INFO "/L2CODE"
#define RESCTRL_PATH_INFO_L2DATA RESCTRL_PATH_INFO "/L2DATA"
#define RESCTRL_PATH_INFO_MB     RESCTRL_PATH_INFO "/MB"

/**
 * Max supported number of CPU's
 */
#define RESCTRL_MAX_CPUS 4096

/**
 * @brief Obtain shared lock on resctrl filesystem
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_lock_shared(void);

/**
 * @brief Obtain exclusive lock on resctrl filesystem
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_lock_exclusive(void);

/**
 * @brief Release lock on resctrl filesystem
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_lock_release(void);

/**
 * @brief Mount the resctrl file system with given CDP option
 *
 * @param l3_cdp_cfg L3 CDP option
 * @param l2_cdp_cfg L2 CDP option
 * @param mba_cfg requested MBA config
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_mount(const enum pqos_cdp_config l3_cdp_cfg,
                  const enum pqos_cdp_config l2_cdp_cfg,
                  const enum pqos_mba_config mba_cfg);

/**
 * @brief Unmount the resctrl file system
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_umount(void);

/**
 * @brief Structure to hold parsed cpu mask
 *
 * Structure contains table with cpu bit mask. Each table item holds
 * information about 8 bit in mask.
 *
 * Example bitmask tables:
 *  - cpus file contains 'ABC' mask = [ ..., 0x0A, 0xBC ]
 *  - cpus file contains 'ABCD' mask = [ ..., 0xAB, 0xCD ]
 */
struct resctrl_cpumask {
        uint8_t tab[RESCTRL_MAX_CPUS / CHAR_BIT]; /**< bit mask table */
};

/**
 * @brief Set lcore bit in cpu mask
 *
 * @param [in] lcore Core number
 * @param [in] cpumask Modified cpu mask
 */
void resctrl_cpumask_set(const unsigned lcore, struct resctrl_cpumask *mask);

/**
 * @brief Unset lcore bit in cpu mask
 *
 * @param [in] lcore Core number
 * @param [in] cpumask Modified cpu mask
 */
void resctrl_cpumask_unset(const unsigned lcore, struct resctrl_cpumask *mask);

/**
 * @brief Check if lcore is set in cpu mask
 *
 * @param [in] lcore Core number
 * @param [in] cpumask Cpu mask
 *
 * @return Returns 1 when bit corresponding to lcore is set in mask
 * @retval 1 if cpu bit is set in mask
 * @retval 0 if cpu bit is not set in mask
 */
int resctrl_cpumask_get(const unsigned lcore,
                        const struct resctrl_cpumask *mask);

/**
 * @brief Write CPU mask to file
 *
 * @param [in] fd write file descriptor
 * @param [in] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_cpumask_write(FILE *fd, const struct resctrl_cpumask *mask);

/**
 * @brief Read CPU mask from file
 *
 * @param [in] fd read file descriptor
 * @param [out] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_cpumask_read(FILE *fd, struct resctrl_cpumask *mask);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_RESCTRL_H__ */
