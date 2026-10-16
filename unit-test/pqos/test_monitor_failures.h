/*
 * BSD LICENSE
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
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

#ifndef __TEST_MONITOR_FAILURES_H
#define __TEST_MONITOR_FAILURES_H

#include "pqos.h"

#include <stddef.h>
#include <sys/types.h>
#include <time.h>

/* The allocators test_monitor_failures.c makes fail on demand, through
 * --wrap, and the real ones it hands every other call to; the interface
 * query monitor_setup() and monitor_loop() make first, which it answers
 * itself or fails; the poll and the cache size the loop asks for, which a
 * case supplies or fails; and, on Linux, the loop's making, arming and reads
 * of its timer, which a case makes fail or be cut short, and which go to the
 * real calls otherwise.
 */
void *__real_malloc(size_t size);
void *__real_calloc(size_t nmemb, size_t size);
void *__real_realloc(void *ptr, size_t size);
char *__real_strdup(const char *s);
void *__wrap_malloc(size_t size);
void *__wrap_calloc(size_t nmemb, size_t size);
void *__wrap_realloc(void *ptr, size_t size);
void __real_free(void *ptr);
void __wrap_free(void *ptr);
char *__wrap_strdup(const char *s);
int __wrap_pqos_inter_get(enum pqos_interface *interface);
int __wrap_pqos_mon_poll(struct pqos_mon_data **groups,
                         const unsigned num_groups);
int __wrap_monitor_utils_get_cache_size(unsigned *p_cache_size);
#ifdef __linux__
int __real_timerfd_create(int clockid, int flags);
int __wrap_timerfd_create(int clockid, int flags);
int __real_timerfd_settime(int fd,
                           int flags,
                           const struct itimerspec *new_value,
                           struct itimerspec *old_value);
int __wrap_timerfd_settime(int fd,
                           int flags,
                           const struct itimerspec *new_value,
                           struct itimerspec *old_value);
ssize_t __real_read(int fd, void *buf, size_t count);
ssize_t __wrap_read(int fd, void *buf, size_t count);
#endif

#endif /* __TEST_MONITOR_FAILURES_H */
