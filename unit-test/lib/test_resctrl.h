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

#ifndef TEST_RESCTRL_H_
#define TEST_RESCTRL_H_

#include <pthread.h>
#include <time.h>

/* the resctrl lock's cases wrap these: the linker's names for the originals */
int __wrap_open(const char *path, int flags, ...);
int __real_open(const char *path, int flags, ...);
int __wrap_nanosleep(const struct timespec *req, struct timespec *rem);
int __real_nanosleep(const struct timespec *req, struct timespec *rem);
int __wrap_clock_gettime(clockid_t clk, struct timespec *tp);
int __real_clock_gettime(clockid_t clk, struct timespec *tp);
int __wrap_flock(int fd, int operation);
int __real_flock(int fd, int operation);
int __wrap_pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int __real_pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int __wrap_pthread_setspecific(pthread_key_t key, const void *value);
int __real_pthread_setspecific(pthread_key_t key, const void *value);
int __wrap_pthread_setcancelstate(int state, int *oldstate);
int __real_pthread_setcancelstate(int state, int *oldstate);
pid_t __wrap_getpid(void);
pid_t __real_getpid(void);

#endif /* TEST_RESCTRL_H_ */
