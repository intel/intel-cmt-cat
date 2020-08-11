/*
 * BSD LICENSE
 *
 * Copyright(c) 2017-2020 Intel Corporation. All rights reserved.
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
 * @brief Internal header file for resctrl allocation
 */

#ifndef __PQOS_RESCTRL_ALLOC_H__
#define __PQOS_RESCTRL_ALLOC_H__

#include "pqos.h"
#include "resctrl.h"
#include "resctrl_schemata.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes resctrl allocation sub-module
 *
 * @param cpu cpu topology structure
 * @param cap capabilities structure
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
int resctrl_alloc_init(const struct pqos_cpuinfo *cpu,
                       const struct pqos_cap *cap);

/**
 * @brief Shuts down resctrl allocation sub-module for OS allocation
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK success
 */
int resctrl_alloc_fini(void);

/**
 * @brief Retrieves number of resctrl groups allowed
 *
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param grps_num place to store number of groups
 *
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_get_grps_num(const struct pqos_cap *cap, unsigned *grps_num);

/**
 * @brief Opens COS file in resctl filesystem
 *
 * @param [in] class_id COS id
 * @param [in] name File name
 * @param [in] mode fopen mode
 *
 * @return Pointer to the stream
 * @retval Pointer on success
 * @retval NULL on error
 */
FILE *resctrl_alloc_fopen(const unsigned class_id,
                          const char *name,
                          const char *mode);

/**
 * @brief Write CPU mask to file
 *
 * @param [in] class_id COS id
 * @param [in] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_cpumask_write(const unsigned class_id,
                                const struct resctrl_cpumask *mask);

/**
 * @brief Read CPU mask from file
 *
 * @param [in] class_id COS id
 * @param [out] mask CPU mask to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_cpumask_read(const unsigned class_id,
                               struct resctrl_cpumask *mask);

/**
 * @brief Read resctrl schemata from file
 *
 * @param [in] class_id COS id
 * @param [out] schemata Parsed schemata
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_schemata_read(const unsigned class_id,
                                struct resctrl_schemata *schemata);

/**
 * @brief Write resctrl schemata to file
 *
 * @param [in] class_id COS id
 * @param [in] technology bit mask selecting technologies
 *             (1 << enum pqos_cap_type)
 * @param [in] schemata Schemata to write
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_schemata_write(const unsigned class_id,
                                 const unsigned technology,
                                 const struct resctrl_schemata *schemata);

/**
 * @brief Function to validate if \a task is a valid task ID
 *
 * @param task task ID to validate
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_task_validate(const pid_t task);

/**
 * @brief Function to write task ID to resctrl COS tasks file
 *        Used to associate a task with COS
 *
 * @param class_id COS tasks file to write to
 * @param task task ID to write to tasks file
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_task_write(const unsigned class_id, const pid_t task);

/**
 * @brief Reads task id's from resctrl task file for a given COS
 *
 * @param [in] class_id Class of Service ID
 * @param [out] count place to store actual number of task id's returned
 *
 * @return Allocated task id array
 * @retval NULL on error
 */
unsigned *resctrl_alloc_task_read(unsigned class_id, unsigned *count);

/**
 * @brief Function to search a COS tasks file for a task ID
 *
 * @param [out] class_id COS containing task ID
 * @param [in] cap platform QoS capabilities structure
 *                 returned by \a pqos_cap_get
 * @param [in] task task ID to search for
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_task_search(unsigned *class_id,
                              const struct pqos_cap *cap,
                              const pid_t task);

/**
 * @brief Function to search a COS tasks file and check if this file is blank
 *
 * @param [in] class_id COS containing task ID
 * @param [out] found flag
 *                    0 if no Task ID is found
 *                    1 if a Task ID is found
 *
 * @return Operation status
 */
int resctrl_alloc_task_file_check(const unsigned class_id, unsigned *found);

/**
 * @brief Resctrl interface to associate \a lcore
 *        with given class of service
 *
 * @param [in] lcore CPU logical core id
 * @param [in] class_id class of service
 *
 * @return Operations status
 */
int resctrl_alloc_assoc_set(const unsigned lcore, const unsigned class_id);

/**
 * @brief Resctrl interface to read association
 *        of \a lcore with class of service
 *
 * @param [in] lcore CPU logical core id
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_assoc_get(const unsigned lcore, unsigned *class_id);

/**
 * @brief Resctrl interface to associate \a task
 *        with given class of service
 *
 * @param [in] task task id to be associated
 * @param [in] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_assoc_set_pid(const pid_t task, const unsigned class_id);

/**
 * @brief Resctrl interface to read association
 *        of \a task with class of service
 *
 * @param [in] task task id to find association
 * @param [out] class_id class of service
 *
 * @return Operations status
 * @retval PQOS_RETVAL_OK on success
 */
int resctrl_alloc_assoc_get_pid(const pid_t task, unsigned *class_id);

/**
 * @brief Gets unused resctrl group
 *
 * The lowest acceptable group is 1, as 0 is a default one
 *
 * @param [in] grps_num number of resctrl groups
 * @param [out] group_id unused group
 *
 * @return Operation status
 */
int resctrl_alloc_get_unused_group(const unsigned grps_num, unsigned *group_id);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_RESCTRL_ALLOC_H__ */
