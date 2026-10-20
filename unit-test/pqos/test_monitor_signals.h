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

#ifndef TEST_MONITOR_SIGNALS_H_
#define TEST_MONITOR_SIGNALS_H_

#include "pqos.h"

#include <signal.h>
#ifdef __linux__
#include <sys/timerfd.h>
#endif
#include <sys/types.h>
#include <time.h>

/* The wrapped library entry points monitoring calls: the interface is the OS
 * one, and the start and the poll raise the signal a case has chosen
 */
int __wrap_pqos_inter_get(enum pqos_interface *interface);
int __wrap_pqos_cap_get(const struct pqos_cap **cap,
                        const struct pqos_cpuinfo **cpu);
int __wrap_pqos_mon_start_pids2(const unsigned num_pids,
                                const pid_t *pids,
                                const enum pqos_mon_event event,
                                void *context,
                                struct pqos_mon_data **group);
int __wrap_pqos_mon_poll(struct pqos_mon_data **groups,
                         const unsigned num_groups);
int __wrap_pqos_mon_stop(struct pqos_mon_data *group);

/* the handlers' installation, which a case can make refuse one signal */
int __wrap_sigaction(int signum,
                     const struct sigaction *act,
                     struct sigaction *oldact);
int __real_sigaction(int signum,
                     const struct sigaction *act,
                     struct sigaction *oldact);

/* and the loop's timer, counted to show the loop got as far as setting it:
 * a timerfd on Linux and a POSIX timer elsewhere, as monitor_loop() has them
 */
#ifdef __linux__
int __wrap_timerfd_settime(int fd,
                           int flags,
                           const struct itimerspec *new_value,
                           struct itimerspec *old_value);
int __real_timerfd_settime(int fd,
                           int flags,
                           const struct itimerspec *new_value,
                           struct itimerspec *old_value);
#else
int __wrap_timer_settime(timer_t timerid,
                         int flags,
                         const struct itimerspec *new_value,
                         struct itimerspec *old_value);
int __real_timer_settime(timer_t timerid,
                         int flags,
                         const struct itimerspec *new_value,
                         struct itimerspec *old_value);
#endif

#endif /* TEST_MONITOR_SIGNALS_H_ */
