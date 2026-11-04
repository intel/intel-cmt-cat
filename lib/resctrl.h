/*
 * BSD LICENSE
 *
 * Copyright(c) 2018-2026 Intel Corporation. All rights reserved.
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
 * @brief Internal header file for resctrl common functions
 */

#ifndef PQOS_RESCTRL_H_
#define PQOS_RESCTRL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"

#include <limits.h> /**< CHAR_BIT*/

#ifndef RESCTRL_PATH
#define RESCTRL_PATH "/sys/fs/resctrl"
#endif
#define RESCTRL_PATH_INFO              RESCTRL_PATH "/info"
#define RESCTRL_PATH_INFO_L3_MON       RESCTRL_PATH_INFO "/L3_MON"
#define RESCTRL_PATH_INFO_L3           RESCTRL_PATH_INFO "/L3"
#define RESCTRL_PATH_INFO_L3CODE       RESCTRL_PATH_INFO "/L3CODE"
#define RESCTRL_PATH_INFO_L3DATA       RESCTRL_PATH_INFO "/L3DATA"
#define RESCTRL_PATH_INFO_L2           RESCTRL_PATH_INFO "/L2"
#define RESCTRL_PATH_INFO_L2CODE       RESCTRL_PATH_INFO "/L2CODE"
#define RESCTRL_PATH_INFO_L2DATA       RESCTRL_PATH_INFO "/L2DATA"
#define RESCTRL_PATH_INFO_MB           RESCTRL_PATH_INFO "/MB"
#define RESCTRL_PATH_INFO_SMBA         RESCTRL_PATH_INFO "/SMBA"
#define RESCTRL_PATH_INFO_PERF_PKG_MON RESCTRL_PATH_INFO "/PERF_PKG_MON"

/**
 * Max supported number of CPU's
 */
#define RESCTRL_MAX_CPUS 4096

/**
 * @brief Obtain shared lock on resctrl filesystem
 *
 * A thread that ends holding the lock, by pthread_exit() or a cancellation
 * acted on in a locked section, gives it up as it ends, as it would with
 * resctrl_lock_release(); and so does resctrl_lock_exclusive()'s. That needs a
 * thread-specific key: where one cannot be created or set for the thread,
 * the lock is taken all the same, a warning is logged once for the thread,
 * and a thread that then ends holding it leaves it held; each later lock
 * tries the key again. A child forked while the lock is held, releasing it or
 * ending the thread that inherited it, closes its copy of the descriptor
 * without unlocking it, so the lock stays the parent's, even where the child
 * has since tried for the lock and failed to take it.
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int resctrl_lock_shared(void);

/**
 * @brief Obtain exclusive lock on resctrl filesystem
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int resctrl_lock_exclusive(void);

/**
 * @brief Release lock on resctrl filesystem
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
PQOS_LOCAL int resctrl_lock_release(void);

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
PQOS_LOCAL int resctrl_mount(const enum pqos_cdp_config l3_cdp_cfg,
                             const enum pqos_cdp_config l2_cdp_cfg,
                             const enum pqos_mba_config mba_cfg);

/**
 * @brief Unmount the resctrl file system
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE where nothing is mounted there to take down
 * @retval PQOS_RETVAL_ERROR where the mount could not be taken down
 */
PQOS_LOCAL int resctrl_umount(void);

/**
 * @brief Asks the kernel whether resctrl takes a mount option, without
 *        mounting or unmounting anything
 *
 * The option is set on a filesystem context opened for resctrl and never
 * created, which the kernel parses as it would a mount's. A refusal there is
 * the kernel's answer; a kernel that cannot be asked this way, or whose
 * resctrl parses its options only when it mounts, leaves the answer unknown.
 *
 * @param [in] option mount option, a flag such as "mba_MBps"
 * @param [out] supported 1 taken, 0 refused, -1 not known
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success, whatever the answer
 * @retval PQOS_RETVAL_PARAM on a NULL parameter
 */
PQOS_LOCAL int resctrl_mount_option_supported(const char *option,
                                              int *supported);

/**
 * @brief Whether resctrl is mounted with a mount option
 *
 * The resctrl lines of /proc/mounts are read, and their options compared one
 * by one, so that neither another filesystem's line nor an option whose name
 * begins with this one counts.
 *
 * @param [in] option mount option, such as "mba_MBps"
 * @param [out] found 1 where a resctrl mount has it, 0 where none has, or
 *              /proc/mounts cannot be read
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_PARAM on a NULL parameter
 */
PQOS_LOCAL int resctrl_mounted_with(const char *option, int *found);

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
 * A core of RESCTRL_MAX_CPUS or more, which the mask has no bit for, leaves
 * it as it is.
 *
 * @param [in] lcore Core number
 * @param [in] mask Modified cpu mask
 */
PQOS_LOCAL void resctrl_cpumask_set(const unsigned lcore,
                                    struct resctrl_cpumask *mask);

/**
 * @brief Unset lcore bit in cpu mask
 *
 * A core of RESCTRL_MAX_CPUS or more, which the mask has no bit for, leaves
 * it as it is.
 *
 * @param [in] lcore Core number
 * @param [in] mask Modified cpu mask
 */
PQOS_LOCAL void resctrl_cpumask_unset(const unsigned lcore,
                                      struct resctrl_cpumask *mask);

/**
 * @brief Check if lcore is set in cpu mask
 *
 * @param [in] lcore Core number
 * @param [in] mask Cpu mask
 *
 * @return Returns 1 when bit corresponding to lcore is set in mask
 * @retval 1 if cpu bit is set in mask
 * @retval 0 if cpu bit is not set in mask, or lcore is RESCTRL_MAX_CPUS or
 *         more, which the mask has no bit for
 */
PQOS_LOCAL int resctrl_cpumask_get(const unsigned lcore,
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
PQOS_LOCAL int resctrl_cpumask_write(FILE *fd,
                                     const struct resctrl_cpumask *mask);

/**
 * @brief Read CPU mask from file
 *
 * @param [in] fd read file descriptor
 * @param [out] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_ERROR if the file cannot be read, or holds a mask
 *         longer than one of RESCTRL_MAX_CPUS
 */
PQOS_LOCAL int resctrl_cpumask_read(FILE *fd, struct resctrl_cpumask *mask);

/**
 * @brief Check if resctrl is supported
 *
 * @return Support status
 * @retval PQOS_RETVAL_OK if supported, error code otherwise
 */
int resctrl_is_supported(void);

#ifdef __cplusplus
}
#endif

#endif /* PQOS_RESCTRL_H_ */
