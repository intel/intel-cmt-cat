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

/**
 * @brief monitor.c's failures, each a return to the caller that decides
 *
 * The setup, the descriptions of its default groups, the conversion of a
 * device group to channels and the arrays the monitoring loop polls are all
 * made after the library is initialised, so a failure in them has to return:
 * only that way does main() reach pqos_fini(), and monitor_stop() as well once
 * the loop has run. The helpers the option handlers call before pqos_init() -
 * the parse of a group's list, the statistics the top-pids mode gathers -
 * return as well, and the handler ends the process, having freed what they
 * allocated. Allocation is what fails there, so malloc(), calloc(), realloc()
 * and strdup() are wrapped to fail on demand - the call a case names fails -
 * and the cases call monitor.c's static functions directly, which this file
 * includes, as test_alloc.c does with alloc.c.
 *
 * The loop's result is what main() exits with, so the loop is driven to each
 * of its ends - a poll, its arrays or the timer failing, a stop, a timeout and
 * more intervals passing than it counts - through wraps of the poll and of the
 * timer's arming and reads. The timer is a timerfd on Linux and a signal
 * elsewhere, and the cases that drive it, like the count of the descriptors
 * the loop leaves open, are Linux's only,
 * as is the case that reads the processes' statistics from /proc.
 */

#include "test_monitor_failures.h"

#include "output.h"
#include "pqos.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

#include "monitor.c"

/** how many allocations from now the failing one is; 0 for none */
static int m_fail_malloc;
static int m_fail_calloc;
static int m_fail_realloc;
static int m_fail_strdup;

/** With m_track_list set, the allocations the size of a top-pids list element
 * or of its statistics are recorded, and a free() takes one off again: what is
 * left once the handler has ended the process is what it did not free.
 * m_tracked_ever counts what was recorded, so that a case can tell an empty
 * list from one that was freed.
 */
static int m_track_list;
static void **m_tracked;
static size_t m_tracked_num;
static size_t m_tracked_max;
static size_t m_tracked_ever;

static void
track(void *ptr)
{
        if (m_tracked_num == m_tracked_max) {
                const size_t max = m_tracked_max != 0 ? 2 * m_tracked_max : 64;
                void **grown = __real_realloc(m_tracked, max * sizeof(*grown));

                assert_non_null(grown);
                m_tracked = grown;
                m_tracked_max = max;
        }
        m_tracked[m_tracked_num++] = ptr;
        m_tracked_ever++;
}

#ifdef __linux__
/* only the top-pids case, which reads /proc, tracks a list */
static void
track_done(void)
{
        m_track_list = 0;
        __real_free(m_tracked);
        m_tracked = NULL;
        m_tracked_num = 0;
        m_tracked_max = 0;
        m_tracked_ever = 0;
}
#endif

void *
__wrap_malloc(size_t size)
{
        void *ptr;

        if (m_fail_malloc > 0 && --m_fail_malloc == 0)
                return NULL;
        ptr = __real_malloc(size);
        if (ptr != NULL && m_track_list &&
            (size == sizeof(struct slist) || size == sizeof(struct proc_stats)))
                track(ptr);
        return ptr;
}

void
__wrap_free(void *ptr)
{
        size_t i;

        for (i = 0; m_track_list && i < m_tracked_num; i++)
                if (m_tracked[i] == ptr) {
                        m_tracked[i] = m_tracked[--m_tracked_num];
                        break;
                }
        __real_free(ptr);
}

void *
__wrap_calloc(size_t nmemb, size_t size)
{
        if (m_fail_calloc > 0 && --m_fail_calloc == 0)
                return NULL;
        return __real_calloc(nmemb, size);
}

void *
__wrap_realloc(void *ptr, size_t size)
{
        if (m_fail_realloc > 0 && --m_fail_realloc == 0)
                return NULL;
        return __real_realloc(ptr, size);
}

char *
__wrap_strdup(const char *s)
{
        if (m_fail_strdup > 0 && --m_fail_strdup == 0)
                return NULL;
        return __real_strdup(s);
}

/** 1 where a case has the interface query, or the cache size, fail */
static int m_fail_inter_get;
static int m_fail_cache_size;

int
__wrap_pqos_inter_get(enum pqos_interface *interface)
{
        if (m_fail_inter_get)
                return PQOS_RETVAL_ERROR;
        *interface = PQOS_INTER_MSR;
        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_mon_poll(struct pqos_mon_data **groups, const unsigned num_groups)
{
        UNUSED_ARG(groups);
        UNUSED_ARG(num_groups);
        return mock_type(int);
}

int
__wrap_monitor_utils_get_cache_size(unsigned *p_cache_size)
{
        if (m_fail_cache_size)
                return PQOS_RETVAL_ERROR;
        *p_cache_size = 1024 * 1024;
        return PQOS_RETVAL_OK;
}

#ifdef __linux__
/** the loop's timer, 1 where a case has it so: its making, its arming or its
 * read fails; a stop signal arrives while the read waits, which then returns
 * the next interval, as the handlers monitoring_catch_signals() sets with
 * SA_RESTART have it, or fails with EINTR, as a handler without SA_RESTART
 * would have it; or the read reports more intervals than the loop counts
 */
static int m_fail_timer_create;
static int m_fail_timer_arming;
static int m_fail_timer_read;
static int m_stop_during_timer_read;
static int m_stop_in_timer_read;
static int m_timer_overrun;

int
__wrap_timerfd_create(int clockid, int flags)
{
        if (m_fail_timer_create) {
                errno = EMFILE;
                return -1;
        }
        return __real_timerfd_create(clockid, flags);
}

int
__wrap_timerfd_settime(int fd,
                       int flags,
                       const struct itimerspec *new_value,
                       struct itimerspec *old_value)
{
        if (m_fail_timer_arming) {
                errno = EINVAL;
                return -1;
        }
        return __real_timerfd_settime(fd, flags, new_value, old_value);
}

ssize_t
__wrap_read(int fd, void *buf, size_t count)
{
        uint64_t intervals = 1;

        if (m_stop_during_timer_read || m_stop_in_timer_read)
                /* the handler ran while the read waited */
                __atomic_store_n(&stop_monitoring_loop, 1, __ATOMIC_RELAXED);
        if (m_stop_in_timer_read) {
                errno = EINTR;
                return -1;
        }
        if (m_fail_timer_read) {
                errno = EIO;
                return -1;
        }
        if (!m_stop_during_timer_read && !m_timer_overrun)
                return __real_read(fd, buf, count);

        if (m_timer_overrun)
                intervals = 101;
        if (count < sizeof(intervals)) {
                errno = EINVAL;
                return -1;
        }
        memcpy(buf, &intervals, sizeof(intervals));
        return (ssize_t)sizeof(intervals);
}
#endif

static void
test_a_device_group_without_memory_returns(void **state)
{
        struct mon_group grp;
        char desc[] = "dev";
        union pqos_device *dev = calloc(1, sizeof(*dev));
        int ret = PQOS_RETVAL_OK;

        UNUSED_ARG(state);
        assert_non_null(dev);

        /* one device with one virtual channel: the conversion allocates the
         * channel table itself, and that is the allocation that fails
         */
        memset(&grp, 0, sizeof(grp));
        dev->vc = 0;
        grp.devices = dev;
        grp.num_res = 1;
        grp.desc = desc;

        m_fail_calloc = 1;
        run_function(grp_device_to_channel, ret, &grp, NULL);
        m_fail_calloc = 0;

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_true(output_has_text("Error allocating memory"));
        /* the conversion leaves the group as it was */
        assert_ptr_equal(grp.devices, dev);

        free(dev);
}

static void
test_monitoring_arrays_without_memory_return(void **state)
{
        struct pqos_mon_data **p1 = NULL, **p2 = NULL;
        int ret = 0;

        UNUSED_ARG(state);

        sel_monitor_num = 2;
        m_fail_malloc = 1;
        run_function(get_mon_arrays, ret, &p1, &p2);
        m_fail_malloc = 0;
        sel_monitor_num = 0;

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_null(p1);
        assert_null(p2);
        assert_true(output_has_text("Error with memory allocation"));
}

static void
test_a_default_group_without_memory_fails_the_setup(void **state)
{
        static const struct {
                enum mon_group_type type;
                const char *said;
        } groups[] = {{(enum mon_group_type)0, "Core group setup error!"},
                      {MON_GROUP_TYPE_UNCORE, "Uncore group setup error!"}};
        struct pqos_cpuinfo *cpu =
            calloc(1, sizeof(*cpu) + sizeof(struct pqos_coreinfo));
        struct pqos_cap_mon mon;
        struct pqos_capability cap;
        unsigned i;

        UNUSED_ARG(state);
        assert_non_null(cpu);

        /* no group selected, so the setup makes one for each core, or with
         * the uncore type for each socket. Its description is a copy, which
         * succeeds, and the first allocation wrapped here is the group's
         * table of cores or sockets, which fails
         */
        cpu->num_cores = 1;
        cpu->cores[0].lcore = 0;
        cpu->cores[0].socket = 0;
        memset(&mon, 0, sizeof(mon));
        memset(&cap, 0, sizeof(cap));
        cap.type = PQOS_CAP_TYPE_MON;
        cap.u.mon = &mon;

        for (i = 0; i < DIM(groups); i++) {
                int ret = 0;

                sel_monitor_num = 0;
                sel_monitor_type = groups[i].type;
                m_fail_malloc = 1;
                run_function(monitor_setup, ret, cpu, &cap, NULL);
                m_fail_malloc = 0;

                assert_int_equal(output_exit_was_called(), 0);
                assert_int_equal(ret, -1);
                assert_true(output_has_text(groups[i].said));
        }

        free(cpu);
}

static void
test_a_default_description_without_memory_fails_the_setup(void **state)
{
        static const enum mon_group_type types[] = {(enum mon_group_type)0,
                                                    MON_GROUP_TYPE_UNCORE};
        struct pqos_cpuinfo *cpu =
            calloc(1, sizeof(*cpu) + sizeof(struct pqos_coreinfo));
        struct pqos_cap_mon mon;
        struct pqos_capability cap;
        unsigned i;

        UNUSED_ARG(state);
        assert_non_null(cpu);

        /* no group selected: the setup makes one for each core, or with the
         * uncore type for each socket, and each one's description is a
         * string of its number, the allocation made to fail here
         */
        cpu->num_cores = 1;
        cpu->cores[0].lcore = 0;
        cpu->cores[0].socket = 0;
        memset(&mon, 0, sizeof(mon));
        memset(&cap, 0, sizeof(cap));
        cap.type = PQOS_CAP_TYPE_MON;
        cap.u.mon = &mon;
        if (sel_output_type == NULL)
                sel_output_type = __real_strdup("text");

        for (i = 0; i < DIM(types); i++) {
                int ret = 0;

                sel_monitor_num = 0;
                sel_monitor_type = types[i];
                m_fail_strdup = 1;
                run_function(monitor_setup, ret, cpu, &cap, NULL);
                m_fail_strdup = 0;

                assert_int_equal(output_exit_was_called(), 0);
                assert_int_equal(ret, -1);
                assert_true(output_has_text("String duplicate error!"));
        }
        sel_monitor_type = 0;

        free(cpu);
}

/** the one group monitor_loop() polls, its data never read: the loop prints
 * nothing before its second poll, and every case ends it before that
 */
static struct mon_group m_loop_group;
#ifdef __linux__
/** the descriptors open before the loop, its timer not yet among them */
static int m_loop_descriptors;

/** the descriptors the process has open, counted the same way each time */
static int
open_descriptors(void)
{
        DIR *dir = opendir("/proc/self/fd");
        int count = 0;

        if (dir == NULL)
                return -1;
        while (readdir(dir) != NULL)
                count++;
        closedir(dir);
        return count;
}
#endif

static void
loop_ready(void)
{
        memset(&m_loop_group, 0, sizeof(m_loop_group));
        sel_monitor_group = &m_loop_group;
        sel_monitor_num = 1;
        fp_monitor = stdout;
        if (sel_output_type == NULL)
                sel_output_type = __real_strdup("text");
#ifdef __linux__
        m_loop_descriptors = open_descriptors();
#endif
}

static void
loop_done(void)
{
        sel_monitor_group = NULL;
        sel_monitor_num = 0;
        sel_timeout = TIMEOUT_INFINITE;
        __atomic_store_n(&stop_monitoring_loop, 0, __ATOMIC_RELAXED);

#ifdef __linux__
        /* the timer is released whichever way the loop ended */
        assert_int_not_equal(m_loop_descriptors, -1);
        assert_int_equal(open_descriptors(), m_loop_descriptors);
#endif
}

static void
test_a_monitoring_loop_that_cannot_start_says_so(void **state)
{
        static const struct {
                int *fail;
                const char *said;
        } starts[] = {
            {&m_fail_inter_get, "Unable to retrieve PQoS interface!"},
            {&m_fail_cache_size, "Error during getting L3 cache size"},
#ifdef __linux__
            {&m_fail_timer_create, "Failed to create timer"},
#endif
        };
        unsigned i;

        UNUSED_ARG(state);

        /* what the loop needs before it first polls: the interface, the cache
         * size and its timer. Without one it polls nothing, and the case
         * makes sure no timer is left open
         */
        for (i = 0; i < DIM(starts); i++) {
                int ret = 0;

                loop_ready();
                *starts[i].fail = 1;
                run_function(monitor_loop, ret);
                *starts[i].fail = 0;
                loop_done();

                assert_int_equal(output_exit_was_called(), 0);
                assert_int_equal(ret, -1);
                assert_true(output_has_text(starts[i].said));
        }
}

static void
test_a_monitoring_loop_whose_poll_fails_says_so(void **state)
{
        int ret = 0;

        UNUSED_ARG(state);

        loop_ready();
        will_return(__wrap_pqos_mon_poll, PQOS_RETVAL_ERROR);
        run_function(monitor_loop, ret);
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("Failed to poll monitoring data!"));
}

#ifdef __linux__
static void
test_a_monitoring_loop_whose_timer_fails_says_so(void **state)
{
        int ret = 0;

        UNUSED_ARG(state);

        /* a timer that cannot be armed: the loop does not poll */
        loop_ready();
        m_fail_timer_arming = 1;
        run_function(monitor_loop, ret);
        m_fail_timer_arming = 0;
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("Failed to setup timer"));

        /* one that cannot be read, after the first poll */
        ret = 0;
        loop_ready();
        will_return(__wrap_pqos_mon_poll, PQOS_RETVAL_OK);
        m_fail_timer_read = 1;
        run_function(monitor_loop, ret);
        m_fail_timer_read = 0;
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("Failed to read timer"));
}

static void
test_a_monitoring_loop_told_to_stop_succeeds(void **state)
{
        unsigned interrupted;

        UNUSED_ARG(state);

        /* ctrl-c raises the flag the loop checks while it waits for its timer.
         * The read is restarted and returns the next interval, and the loop
         * ends at its condition - or, with a handler that does not restart it,
         * the read fails with EINTR. Either is a stop, not a failure, and is
         * not reported as one
         */
        for (interrupted = 0; interrupted <= 1; interrupted++) {
                int ret = -1;

                loop_ready();
                will_return(__wrap_pqos_mon_poll, PQOS_RETVAL_OK);
                m_stop_during_timer_read = !interrupted;
                m_stop_in_timer_read = (int)interrupted;
                run_function(monitor_loop, ret);
                m_stop_during_timer_read = 0;
                m_stop_in_timer_read = 0;
                loop_done();

                assert_int_equal(output_exit_was_called(), 0);
                assert_int_equal(ret, 0);
                assert_false(output_has_text("Failed to read timer"));
        }
}

static void
test_a_timer_overrun_ends_the_loop_without_failing(void **state)
{
        int ret = -1;

        UNUSED_ARG(state);

        /* more than 100 intervals between two reads - the process was stopped,
         * or its output blocked - end the loop as they always did, with the
         * message it always printed, but the run did not fail
         */
        loop_ready();
        will_return(__wrap_pqos_mon_poll, PQOS_RETVAL_OK);
        m_timer_overrun = 1;
        run_function(monitor_loop, ret);
        m_timer_overrun = 0;
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, 0);
        assert_true(output_has_text("Failed to read timer"));
}
#endif

static void
test_a_monitoring_loop_without_memory_for_its_arrays_returns(void **state)
{
        int ret = 0;

        UNUSED_ARG(state);

        /* nothing the loop does before it allocates the arrays it polls goes
         * through malloc(), so the allocation made to fail is theirs: the
         * loop returns before its first poll, its timer released
         */
        loop_ready();
        m_fail_malloc = 1;
        run_function(monitor_loop, ret);
        m_fail_malloc = 0;
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("Error with memory allocation"));
}

static void
test_a_monitoring_loop_that_times_out_succeeds(void **state)
{
        int ret = -1;

        UNUSED_ARG(state);

        /* a timeout of no seconds ends the loop after its first poll */
        loop_ready();
        sel_timeout = 0;
        will_return(__wrap_pqos_mon_poll, PQOS_RETVAL_OK);
        run_function(monitor_loop, ret);
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, 0);
}

/* What a number without digits, such as "0x", is told with. glibc leaves
 * errno alone where strtoull() converts nothing, and the check that follows
 * says "No digits were found"; FreeBSD's sets EINVAL, and the perror() before
 * that check reports on stderr, which the capture does not see. Elsewhere a
 * case asserts only the return and that the process was not ended.
 */
#ifdef __GLIBC__
#define NO_DIGITS_SAID "No digits were found"
#else
#define NO_DIGITS_SAID NULL
#endif

static void
test_a_group_list_that_cannot_be_used_is_refused_by_returning(void **state)
{
        static const struct {
                const char *option;
                enum mon_group_type type;
                const char *said;
        } bad[] = {
            {"all:5-", MON_GROUP_TYPE_PID, "Incomplete PID range format"},
            {"all:5-x", MON_GROUP_TYPE_PID, "Invalid PID range format"},
            {"all:0x", MON_GROUP_TYPE_PID, NO_DIGITS_SAID},
            /* a range whose start, or whose end, is no number */
            {"all:x-5", MON_GROUP_TYPE_PID, NO_DIGITS_SAID},
            {"all:5-5x", MON_GROUP_TYPE_PID,
             "Error converting '5x' to unsigned number!"},
            {"all:1-300000", MON_GROUP_TYPE_PID,
             "Requested PID range expansion is too large"},
            {"all:[5-]", MON_GROUP_TYPE_PID, "Incomplete PID range format"},
            {"all:1-x", MON_GROUP_TYPE_CORE,
             "Error converting '1-x' to unsigned number!"},
            {"foo:1", MON_GROUP_TYPE_CORE,
             "Unrecognized monitoring event type"},
        };
        unsigned i;

        UNUSED_ARG(state);

        for (i = 0; i < DIM(bad); i++) {
                char option[64];
                int ret = 0;

                strncpy(option, bad[i].option, sizeof(option) - 1);
                option[sizeof(option) - 1] = '\0';

                run_function(parse_monitor_group, ret, option, bad[i].type);

                assert_int_equal(output_exit_was_called(), 0);
                assert_int_equal(ret, -1);
                if (bad[i].said != NULL)
                        assert_true(output_has_text(bad[i].said));
        }
}

static void
test_a_group_without_memory_is_refused_by_returning(void **state)
{
        static const enum mon_group_type types[] = {MON_GROUP_TYPE_CORE,
                                                    MON_GROUP_TYPE_CHANNEL};
        /* the copy of the list kept for a message, then the description of
         * the group, a string of its number: decimal for a core, hex for a
         * channel
         */
        static const char *const said[] = {
            "Failed to allocate memory for argument copy!",
            "String duplicate error!"};
        unsigned i, which;

        UNUSED_ARG(state);

        for (i = 0; i < DIM(types); i++)
                for (which = 1; which <= DIM(said); which++) {
                        char option[] = "all:1";
                        int ret = 0;

                        m_fail_strdup = (int)which;
                        run_function(parse_monitor_group, ret, option,
                                     types[i]);
                        m_fail_strdup = 0;

                        assert_int_equal(output_exit_was_called(), 0);
                        assert_int_equal(ret, -1);
                        assert_true(output_has_text(said[which - 1]));
                }
}

static void
test_a_group_copy_without_memory_is_refused_by_returning(void **state)
{
        static const struct {
                const char *option;
                enum mon_group_type type;
                const char *said;
        } copies[] = {
            /* a bracketed group's description is a copy of its list */
            {"all:[1]", MON_GROUP_TYPE_CORE, "String duplicate error!"},
            /* a PID list is copied first, to see how far it expands */
            {"all:1", MON_GROUP_TYPE_PID,
             "Failed to allocate memory for argument copy!"},
        };
        unsigned i;

        UNUSED_ARG(state);

        for (i = 0; i < DIM(copies); i++) {
                char option[64];
                int ret = 0;

                strncpy(option, copies[i].option, sizeof(option) - 1);
                option[sizeof(option) - 1] = '\0';

                m_fail_strdup = 1;
                run_function(parse_monitor_group, ret, option, copies[i].type);
                m_fail_strdup = 0;

                assert_int_equal(output_exit_was_called(), 0);
                assert_int_equal(ret, -1);
                assert_true(output_has_text(copies[i].said));
        }
}

static void
test_a_list_whose_value_set_cannot_grow_returns(void **state)
{
        /* the set of the values stored so far is two allocations, made at
         * the first value that is not part of a leading range: a single
         * value, or, after a leading range, a value inside a later range.
         * Either allocation failing, on either path, refuses the list
         */
        static const char *const lists[] = {"1,2", "1-2,4-5"};
        unsigned l, which;

        UNUSED_ARG(state);

        for (l = 0; l < DIM(lists); l++) {
                for (which = 1; which <= 2; which++) {
                        char list[16];
                        unsigned max = 4;
                        uint64_t *tab = calloc(max, sizeof(*tab));
                        int ret = 0;

                        assert_non_null(tab);
                        strncpy(list, lists[l], sizeof(list) - 1);
                        list[sizeof(list) - 1] = '\0';
                        m_fail_calloc = (int)which;
                        run_function(strlisttotabrealloc_checked, ret, list,
                                     &tab, &max);
                        m_fail_calloc = 0;

                        assert_int_equal(output_exit_was_called(), 0);
                        assert_int_equal(ret, -1);
                        assert_true(output_has_text("Reallocation error!"));
                        free(tab);
                }
        }
}

static void
test_a_list_whose_table_cannot_grow_keeps_it(void **state)
{
        /* the three places the table grows: for a leading range longer than
         * it, and after a value or a range that fills it
         */
        static const struct {
                const char *list;
                unsigned max;
        } lists[] = {{"1-5", 1}, {"1,2", 1}, {"1,3-4", 2}};
        unsigned i;

        UNUSED_ARG(state);

        for (i = 0; i < DIM(lists); i++) {
                char list[16];
                unsigned max = lists[i].max;
                uint64_t *tab = calloc(max, sizeof(*tab));
                const uint64_t *const given = tab;
                int ret = 0;

                assert_non_null(tab);
                strncpy(list, lists[i].list, sizeof(list) - 1);
                list[sizeof(list) - 1] = '\0';

                m_fail_realloc = 1;
                run_function(strlisttotabrealloc_checked, ret, list, &tab,
                             &max);
                m_fail_realloc = 0;

                assert_int_equal(output_exit_was_called(), 0);
                assert_int_equal(ret, -1);
                assert_true(output_has_text("Reallocation error!"));
                /* still the caller's to free, at the size it had */
                assert_ptr_equal(tab, given);
                assert_int_equal(max, lists[i].max);
                free(tab);
        }
}

static void
test_a_table_given_past_what_an_int_counts_is_refused(void **state)
{
        /* a table said to hold more than INT_MAX elements: a list that fills
         * it would count past what the int return holds, so it is refused
         * before anything is read or written
         */
        char list[] = "1";
        unsigned max = (unsigned)INT_MAX + 1;
        uint64_t *tab = calloc(1, sizeof(*tab));
        const uint64_t *const given = tab;
        int ret = 0;

        UNUSED_ARG(state);
        assert_non_null(tab);

        run_function(strlisttotabrealloc_checked, ret, list, &tab, &max);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("Too large group items."));
        assert_ptr_equal(tab, given);
        assert_int_equal(max, (unsigned)INT_MAX + 1);
        assert_int_equal(tab[0], 0);
        free(tab);
}

static void
test_a_table_past_what_an_int_counts_is_refused(void **state)
{
        /* a table said to hold more than INT_MAX / 2 elements, and a first
         * range longer still: doubling it would let the count pass INT_MAX,
         * so it is neither grown nor written, and the list is refused
         */
        char list[] = "0-1073741824";
        unsigned max = (unsigned)INT_MAX / 2 + 1;
        uint64_t *tab = calloc(1, sizeof(*tab));
        const uint64_t *const given = tab;
        int ret = 0;

        UNUSED_ARG(state);
        assert_non_null(tab);

        run_function(strlisttotabrealloc_checked, ret, list, &tab, &max);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("Too large group items."));
        assert_ptr_equal(tab, given);
        assert_int_equal(max, (unsigned)INT_MAX / 2 + 1);
        free(tab);
}

static void
test_a_process_entry_without_memory_leaves_the_list(void **state)
{
        const time_t now = time(NULL);
        struct slist *head = NULL, *got = NULL;

        UNUSED_ARG(state);

        head = add_proc_cpu_stat(NULL, 1, 10, now);
        assert_non_null(head);

        /* the entry, then the list element that would carry it */
        m_fail_malloc = 1;
        run_function(add_proc_cpu_stat, got, head, 2, 10, now);
        m_fail_malloc = 0;
        assert_int_equal(output_exit_was_called(), 0);
        assert_null(got);
        assert_true(output_has_text("Error with memory allocation for pstat!"));

        got = head;
        m_fail_malloc = 2;
        run_function(add_proc_cpu_stat, got, head, 3, 10, now);
        m_fail_malloc = 0;
        assert_int_equal(output_exit_was_called(), 0);
        assert_null(got);
        assert_true(
            output_has_text("Error with memory allocation for slist element!"));

        /* the list the caller passed is still the caller's to free */
        free(head->data);
        free(head);
}

#ifdef __linux__
/* the statistics are read from /proc/<pid>/stat, which only Linux has */
static void
test_process_statistics_without_memory_say_so(void **state)
{
        struct slist *list = NULL;
        int ret = 0;
        int fds[2];
        pid_t child;

        UNUSED_ARG(state);

        /* this process at least is listed in /proc, so an entry is made, and
         * the first allocation wrapped here is that entry
         */
        m_fail_malloc = 1;
        run_function(get_proc_pids_stats, ret, &list);
        m_fail_malloc = 0;

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PROC_STATS_NO_MEMORY);
        assert_null(list);

        /* each entry is two allocations, itself and the element that carries
         * it, so the third is the second entry's: by then one entry is in the
         * list, and it is left there for the caller to free. A child that
         * waits on a pipe is a second process for the scan to meet, even
         * where this one is alone in its PID namespace
         */
        assert_int_equal(pipe(fds), 0);
        child = fork();
        assert_true(child >= 0);
        if (child == 0) {
                char c;

                close(fds[1]);
                (void)read(fds[0], &c, 1);
                _exit(0);
        }
        close(fds[0]);

        m_fail_malloc = 3;
        run_function(get_proc_pids_stats, ret, &list);
        m_fail_malloc = 0;

        close(fds[1]);
        waitpid(child, NULL, 0);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, PROC_STATS_NO_MEMORY);
        assert_non_null(list);
        assert_null(list->next);
        free(list->data);
        free(list);
}

/**
 * @brief Clears the selections selfn_monitor_top_pids() makes
 *
 * All but one: the handler also narrows the interface to OS, in main.c's
 * mask, which is static there and which a case cannot reset. So the top-pids
 * cases run after every case that calls a handler narrowing to MSR or MMIO.
 */
static void
top_pids_done(void)
{
        sel_monitor_opts = 0;
        sel_mon_top_like = 0;
        sel_mon_top_pids = 0;
}

static void
test_top_pids_without_memory_end_the_process(void **state)
{
        UNUSED_ARG(state);

        /* the first scan of /proc runs out of memory at its first entry: the
         * helpers return, and the handler frees what they gathered and ends
         * the process, without the pause before the second scan
         */
        m_fail_malloc = 1;
        run_void_function(selfn_monitor_top_pids);
        m_fail_malloc = 0;
        top_pids_done();

        assert_true(output_exit_was_called());
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_true(output_has_text("Error with memory allocation for pstat!"));
}

static void
test_top_pids_whose_pick_cannot_be_made_end_the_process(void **state)
{
        const unsigned groups = sel_monitor_num;

        UNUSED_ARG(state);

        /* both scans complete, after the handler's pause, and the first pick's
         * description, a string of its PID, is the first copy made: it
         * failing makes fill_top_procs() return, and the handler, with the
         * statistics freed, ends the process. Every element of the list the
         * scans gathered, and its statistics, is to be freed by then
         */
        m_track_list = 1;
        m_fail_strdup = 1;
        run_void_function(selfn_monitor_top_pids);
        m_fail_strdup = 0;
        m_track_list = 0;
        top_pids_done();

        assert_true(output_exit_was_called());
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_true(output_has_text("String duplicate error!"));
        assert_int_equal(sel_monitor_num, groups);
        assert_true(m_tracked_ever > 0);
        assert_int_equal(m_tracked_num, 0);
        track_done();
}
#endif

static void
test_a_top_pid_that_overlaps_a_named_group_returns(void **state)
{
        const unsigned groups = sel_monitor_num;
        const uint64_t named[] = {(uint64_t)getpid(), (uint64_t)getpid() + 1};
        char *desc = __real_strdup("named");
        struct slist *head;
        int ret = 0;

        UNUSED_ARG(state);
        assert_non_null(desc);

        /* a group the user named, of this process and another, and a top-pids
         * pick of this process alone: the pick's group would partly overlap
         * the named one, which grp_add() refuses, and fill_top_procs() returns
         */
        assert_non_null(grp_add(MON_GROUP_TYPE_PID,
                                (enum pqos_mon_event)PQOS_MON_EVENT_ALL, desc,
                                named, 2));
        head = add_proc_cpu_stat(NULL, getpid(), 10, time(NULL));
        assert_non_null(head);
        ((struct proc_stats *)head->data)->valid = 1;

        run_function(fill_top_procs, ret, head);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_true(
            output_has_text("cannot monitor same pids in different groups"));
        assert_int_equal(sel_monitor_num, groups + 1);

        /* the named group, the one this case added, is taken off again */
        sel_monitor_num--;
        grp_free(&sel_monitor_group[sel_monitor_num]);
        free(head->data);
        free(head);
}

static void
test_a_top_pid_without_memory_for_its_description_returns(void **state)
{
        const unsigned groups = sel_monitor_num;
        struct slist *head;
        int ret = 0;

        UNUSED_ARG(state);

        /* one process with its statistics complete: the description of its
         * group, a string of its PID, is the allocation made to fail
         */
        head = add_proc_cpu_stat(NULL, getpid(), 10, time(NULL));
        assert_non_null(head);
        ((struct proc_stats *)head->data)->valid = 1;

        m_fail_strdup = 1;
        run_function(fill_top_procs, ret, head);
        m_fail_strdup = 0;

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("String duplicate error!"));
        assert_int_equal(sel_monitor_num, groups);

        free(head->data);
        free(head);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_a_device_group_without_memory_returns),
            cmocka_unit_test(test_monitoring_arrays_without_memory_return),
            cmocka_unit_test(
                test_a_default_group_without_memory_fails_the_setup),
            cmocka_unit_test(
                test_a_default_description_without_memory_fails_the_setup),
            cmocka_unit_test(test_a_monitoring_loop_that_cannot_start_says_so),
            cmocka_unit_test(test_a_monitoring_loop_whose_poll_fails_says_so),
            cmocka_unit_test(
                test_a_monitoring_loop_without_memory_for_its_arrays_returns),
#ifdef __linux__
            cmocka_unit_test(test_a_monitoring_loop_whose_timer_fails_says_so),
            cmocka_unit_test(test_a_monitoring_loop_told_to_stop_succeeds),
            cmocka_unit_test(
                test_a_timer_overrun_ends_the_loop_without_failing),
#endif
            cmocka_unit_test(test_a_monitoring_loop_that_times_out_succeeds),
            cmocka_unit_test(
                test_a_group_list_that_cannot_be_used_is_refused_by_returning),
            cmocka_unit_test(
                test_a_group_without_memory_is_refused_by_returning),
            cmocka_unit_test(
                test_a_group_copy_without_memory_is_refused_by_returning),
            cmocka_unit_test(test_a_list_whose_value_set_cannot_grow_returns),
            cmocka_unit_test(test_a_list_whose_table_cannot_grow_keeps_it),
            cmocka_unit_test(
                test_a_table_given_past_what_an_int_counts_is_refused),
            cmocka_unit_test(test_a_table_past_what_an_int_counts_is_refused),
            cmocka_unit_test(
                test_a_process_entry_without_memory_leaves_the_list),
#ifdef __linux__
            cmocka_unit_test(test_process_statistics_without_memory_say_so),
            cmocka_unit_test(test_top_pids_without_memory_end_the_process),
            cmocka_unit_test(
                test_top_pids_whose_pick_cannot_be_made_end_the_process),
#endif
            cmocka_unit_test(
                test_a_top_pid_without_memory_for_its_description_returns),
            cmocka_unit_test(
                test_a_top_pid_that_overlaps_a_named_group_returns)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
