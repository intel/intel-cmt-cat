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
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.O
 *
 */

#ifndef __MONITOR_UTILS_H__
#define __MONITOR_UTILS_H__

#include "pqos.h"

/**
 * @brief Function to safely translate an unsigned int
 *        value to a string without memory allocation
 *
 * @param buf buffer string will be copied into
 * @param buf_len length of buffer
 * @param val value to be translated
 *
 * @return length of generated string
 * @retval < 0 on error
 */
int monitor_utils_uinttostr(char *buf, const int buf_len, const unsigned val);

/**
 * @brief Function to safely translate an unsigned int value to a hex string
 *
 * @param buf buffer string will be copied into
 * @param buf_len length of buffer
 * @param val value to be translated
 *
 * @return length of generated string
 * @retval < 0 on error
 */
int
monitor_utils_uinttohexstr(char *buf, const int buf_len, const unsigned val);

/**
 * @brief Get monitoring value to be displayed for the event
 *
 * @param data monitoring data
 * @param event monitoring event ID
 *
 * @return value to be displayed
 */
double monitor_utils_get_value(const struct pqos_mon_data *const data,
                               const enum pqos_mon_event event);

/**
 * @brief Get memory region monitoring value to be displayed for the event
 *
 * @param data monitoring data
 * @param event monitoring event ID
 * @param region_num memory region number to be monitoring
 *
 * @return value to be displayed
 */
double monitor_utils_get_region_value(const struct pqos_mon_data *const data,
                                      const enum pqos_mon_event event,
                                      int region_num);

/**
 * @brief Gets total l3 cache value
 *
 * @param[in] p_cache_size pointer to cache-size value to be filled.
 *            It must not be NULL.
 *
 * @return PQOS_RETVAL_OK on success or error code in case of failure
 */
int monitor_utils_get_cache_size(unsigned *p_cache_size);

/**
 * @brief Function to return a comma separated list of all cores that PIDs
 *        in \a mon_data last ran on.
 *
 * @param mon_data struct with info on the group of pids to be monitored
 * @param cores_s[out] char pointer to hold string of cores the PIDs last ran on
 * @param len length of cores_s
 *
 * @param retval 0 in case of success, -1 for error
 */
int monitor_utils_get_pid_cores(const struct pqos_mon_data *mon_data,
                                char *cores_s,
                                const int len);

/**
 * @brief Returns value in /proc/<pid>/stat file at user defined column
 *
 * @param proc_pid_dir_name name of target PID directory e.g, "1234"
 * @param column value of the requested column number in
 *        the /proc/<pid>/stat file
 * @param len_val length of buffer user is going to pass the value into
 * @param val[out] value in column of the /proc/<pid>/stat file
 *
 * @return operation status
 * @retval 0 in case of success
 * @retval -1 in case of error
 */
int monitor_utils_get_pid_stat(const char *proc_pid_dir_name,
                               const int column,
                               const unsigned len_val,
                               char *val);

#endif /* __MONITOR_UTILS_H__ */
