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
 * of its ends - a poll, its arrays or the timer failing, a stop, a timeout,
 * more intervals passing than it counts and an output that takes no more of
 * what is written, after a round or a counter's overflow - through wraps of
 * the poll and of the timer's arming and reads; and an output that takes all
 * of it is seen to keep the loop going to its timeout, as a stdout that takes
 * no more of an overflow's message, where -o named a file, is seen to leave it
 * going, without the message. After an overflow the loop is seen to poll again
 * at once, but where the overflow repeats, to wait for the timer, as after a
 * round, and so to meet its timeout. A failed write is said once, and by
 * monitor_flush(), not the loop: a message to a stderr that blocks would hold
 * the groups and the library lock. The end of the output, which the loop
 * leaves for monitor_flush() to write, is seen to reach the output only then,
 * whether its stream is fully buffered, line buffered or unbuffered, and to
 * fail there; and a loop told to stop, before its first poll, at an overflow
 * or at a round, to flush nothing more, leaving what the stream holds with
 * the end, and an overflow's message in stdout, where -o named a file, for
 * monitor_flush() to flush and check. The timer is a timerfd on Linux and a
 * signal elsewhere, and the cases that drive it, like the count of the
 * descriptors the loop leaves open, are Linux's only, as are the cases whose
 * output is a pseudo-terminal, /dev/full, a temporary file or a pipe that XML
 * goes to - but for the two whose stdout fails, which wait on no timer and
 * run everywhere - and the case that reads the processes' statistics from
 * /proc.
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

/** the polls the loop has made since loop_ready(); and, in the cases whose
 * output fails, the poll that writes to it (1 for the first, 0 for none), as
 * the output's functions would after it: the capture keeps what they print
 * from the stream, so this write is what reaches it. Where m_pty_master is
 * open, that poll hangs the output's terminal up first, by closing the
 * master side of the pseudo-terminal it is
 */
static unsigned m_polls;
static unsigned m_output_poll;
static int m_pty_master = -1;

/** the poll that leaves a line in stdout (0 for none), as the message of a
 * counter's overflow would be, the capture keeping the message itself from
 * the stream. It has no newline, so that a flush - the loop's, or after a
 * stop monitor_flush()'s - is what writes it, whether stdout is line buffered
 * or fully buffered
 */
static unsigned m_stdout_poll;

/** the poll during which a stop is requested (0 for none), as the handler of
 * a stop signal that came while the poll ran would request it
 */
static unsigned m_stop_poll;

/** the poll at which the output is read through its descriptor, past its
 * stream's buffer (0 for none), and what it then held: what the loop had
 * flushed to it
 */
static unsigned m_read_poll;
static char m_read[64];

int
__wrap_pqos_mon_poll(struct pqos_mon_data **groups, const unsigned num_groups)
{
        UNUSED_ARG(groups);
        UNUSED_ARG(num_groups);

        if (++m_polls == m_read_poll) {
                const ssize_t len =
                    pread(fileno(fp_monitor), m_read, sizeof(m_read) - 1, 0);

                m_read[len > 0 ? len : 0] = '\0';
        }
        if (m_polls == m_output_poll) {
                if (m_pty_master != -1) {
                        close(m_pty_master);
                        m_pty_master = -1;
                }
                (void)fputs("TIME 2026-10-20 16:00:00\n", fp_monitor);
        }
        if (m_polls == m_stdout_poll)
                (void)fputs("MBM counter overflow", stdout);
        if (m_polls == m_stop_poll)
                __atomic_store_n(&stop_monitoring_loop, 1, __ATOMIC_RELAXED);
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

/** the output whose close fails, with EIO, once the real call has closed it,
 * as a close on a file system such as NFS fails where the write-back of what
 * was flushed to the file does; NULL for none
 */
static FILE *m_close_fails;

int
__wrap_pqos_fclose(FILE *stream)
{
        const int fails = stream != NULL && stream == m_close_fails;
        const int ret = __real_pqos_fclose(stream);

        if (!fails)
                return ret;
        m_close_fails = NULL;
        errno = EIO;
        return EOF;
}

#ifdef __linux__
/** the loop's timer, 1 where a case has it so: its making, its arming or its
 * read fails; a stop signal arrives while the read waits, which then returns
 * the next interval, as the handlers monitoring_catch_signals() sets with
 * SA_RESTART have it, or fails with EINTR, as a handler without SA_RESTART
 * would have it; the read reports more intervals than the loop counts; or it
 * returns the next interval at once, so that the loop goes on to its next
 * poll without waiting for it
 */
static int m_fail_timer_create;
static int m_fail_timer_arming;
static int m_fail_timer_read;
static int m_stop_during_timer_read;
static int m_stop_in_timer_read;
static int m_timer_overrun;
static int m_timer_at_once;

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
        if (!m_stop_during_timer_read && !m_timer_overrun && !m_timer_at_once)
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

/** the one group monitor_loop() polls. The loop prints no rows before its
 * second poll, and a case whose loop gets that far gives the group data to
 * read first, with loop_data_ready()
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
        m_polls = 0;
        m_read[0] = '\0';
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
        /* an end that a case left unwritten is dropped with its output, and
         * a failure left unsaid with it
         */
        output_end = NULL;
        output_failure.failed = 0;
        stdout_failure.failed = 0;
        m_close_fails = NULL;

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

/**
 * @brief How many times the run said something
 *
 * A failed output is said once, and by monitor_flush(), once the monitoring
 * is stopped: the loop records the write or the flush that failed and ends
 * there. The cases that count it run the loop and monitor_flush() in one
 * capture, with loop_and_flush(), so that the count takes in both. A count
 * of one cannot show that the output failed only once: only the first
 * failure of a stream is recorded, and so said. That monitor_flush() wrote
 * no end to an output that failed in the loop, output_done() tells.
 *
 * @param [in] text what was said
 *
 * @return how many times the capture holds it
 */
static unsigned
output_count(const char *text)
{
        const char *at = output_get();
        const size_t len = strlen(text);
        unsigned count = 0;

        assert_int_not_equal(len, 0);
        while ((at = strstr(at, text)) != NULL) {
                count++;
                at += len;
        }
        return count;
}

/** what monitor_flush() returned in loop_and_flush(); whether the loop
 * said a failed write itself, which it leaves to monitor_flush(): a message
 * on stderr could block it, with the groups and the library lock held; and
 * whether the loop had recorded a failure of stdout, which it flushes nothing
 * of once a stop is requested
 */
static int m_flushed;
static int m_loop_said;
static int m_loop_stdout_failed;

/**
 * @brief Runs the loop and then monitor_flush(), as main() does
 *
 * Called with run_function(), whose capture then holds what both said: one
 * capture is begun anew by each run_function(). What the loop said is the
 * capture when it returns.
 *
 * @return what the loop returned
 */
static int
loop_and_flush(void)
{
        const int ret = monitor_loop();

        m_loop_said = strstr(output_get(), "Failed to write") != NULL;
        m_loop_stdout_failed = stdout_failure.failed;
        m_flushed = monitor_flush();
        return ret;
}

/**
 * @brief Closes a case's output, unless monitor_flush() did, and goes back
 *        to stdout
 *
 * monitor_flush() closes an output other than stdout once it has written
 * the end to it, and leaves fp_monitor NULL. It writes no end to an output
 * that failed in the loop, and leaves that open, for monitor_cleanup().
 *
 * @param [in] fp the output
 *
 * @return 1 where monitor_flush() left the output open, as it does where it
 *         wrote no end to it, 0 where it closed it
 */
static int
output_done(FILE *fp)
{
        const int left_open = fp_monitor == fp;

        if (fp_monitor != NULL)
                fclose(fp);
        fp_monitor = stdout;
        return left_open;
}

static void
test_the_flush_leaves_stdout_open(void **state)
{
        int ret = -1;

        UNUSED_ARG(state);

        /* an output that -o did not name is stdout: monitor_flush() writes
         * the end to it and flushes it, but leaves it open, for exit() to
         * close, and for what main() prints after it
         */
        loop_ready();
        sel_timeout = 0;
        m_flushed = -1;
        will_return(__wrap_pqos_mon_poll, PQOS_RETVAL_OK);
        run_function(loop_and_flush, ret);
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, 0);
        assert_int_equal(m_flushed, 0);
        assert_ptr_equal(fp_monitor, stdout);
}

static void
test_an_output_that_a_write_failed_on_ends_the_loop_unpolled(void **state)
{
        FILE *fp;
        int left_open;
        int ret = 0;

        UNUSED_ARG(state);

        /* a write that failed before the loop, as the start's messages fail
         * on a terminal that hung up while pqos initialised: the stream's
         * error indicator is set, and there is nothing left for a flush to
         * fail on - which is how a terminal's line-buffered stream is once a
         * write to it failed. A stream opened for reading is in that state
         * after any write. The loop ends before its first poll, and fails,
         * leaving monitor_flush() no end to write, only the failure to say
         */
        loop_ready();
        fp = fopen("/dev/null", "r");
        assert_non_null(fp);
        assert_int_equal(fputc('x', fp), EOF);
        assert_true(ferror(fp));
        fp_monitor = fp;
        m_flushed = 0;
        m_loop_said = 1;

        run_function(loop_and_flush, ret);

        left_open = output_done(fp);
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_int_equal(m_polls, 0);
        assert_false(m_loop_said);
        assert_int_equal(output_count("Failed to write monitoring output"), 1);
        assert_int_equal(m_flushed, -1);
        assert_true(left_open);
}

/**
 * @brief Has SIGPIPE ignored, as monitoring_catch_signals() has it
 *
 * A write to a pipe whose reader has gone then fails with EPIPE, rather than
 * ending the process.
 *
 * @param [out] old what it was, for the case to put back
 */
static void
ignore_sigpipe(struct sigaction *old)
{
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SIG_IGN;
        sigemptyset(&sa.sa_mask);
        assert_int_equal(sigaction(SIGPIPE, &sa, old), 0);
}

static void
test_an_output_that_fails_at_an_overflow_ends_the_loop(void **state)
{
        struct sigaction sigpipe;
        int fds[2];
        FILE *fp;
        int left_open;
        int ret = 0;

        UNUSED_ARG(state);

        /* a poll that overflows says so, and prints no round, nor reaches
         * the check after it: an overflow that repeats, as an MMIO
         * counter's does while its overflow bit stays set, would keep the
         * loop going on an output that takes no more. Here the reader of the
         * output's pipe has gone, and the second overflow's write - of the
         * line the mocked poll leaves, the message itself being captured -
         * fails with EPIPE: the loop ends there, and fails, leaving
         * monitor_flush() no end to write, only the failure to say
         */
        ignore_sigpipe(&sigpipe);
        loop_ready();
        assert_int_equal(pipe(fds), 0);
        close(fds[0]);
        fp = fdopen(fds[1], "w");
        if (fp == NULL)
                close(fds[1]);
        assert_non_null(fp);
        fp_monitor = fp;
        m_output_poll = 2;
        m_flushed = 0;
        m_loop_said = 1;
        will_return_count(__wrap_pqos_mon_poll, PQOS_RETVAL_OVERFLOW, 2);

        run_function(loop_and_flush, ret);

        m_output_poll = 0;
        left_open = output_done(fp);
        loop_done();
        assert_int_equal(sigaction(SIGPIPE, &sigpipe, NULL), 0);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_int_equal(m_polls, 2);
        assert_int_equal(output_count("MBM counter overflow"), 2);
        assert_false(m_loop_said);
        assert_true(output_has_text("Failed to write monitoring output: %s",
                                    strerror(EPIPE)));
        assert_int_equal(output_count("Failed to write"), 1);
        assert_int_equal(m_flushed, -1);
        assert_true(left_open);
}

/** stdout's descriptor while a case has stdout on a pipe, -1 otherwise */
static int m_stdout_saved = -1;

/**
 * @brief Puts stdout back where a case had it on a pipe
 *
 * The case's teardown as well, so that a case that fails with stdout on the
 * pipe does not leave the report of the cases after it there.
 *
 * @param [in] state unused
 *
 * @return 0 where stdout is back, -1 where it could not be put back
 */
static int
stdout_back(void **state)
{
        int ret = 0;

        UNUSED_ARG(state);

        m_stdout_poll = 0;
        if (m_stdout_saved == -1)
                return 0;
        if (dup2(m_stdout_saved, STDOUT_FILENO) == -1)
                ret = -1;
        close(m_stdout_saved);
        m_stdout_saved = -1;
        clearerr(stdout);
        return ret;
}

/**
 * @brief Has a stdout whose reader has gone fail at a counter's overflow
 *
 * -o named a file, and the message of an overflow goes to stdout, which a
 * pipe whose reader has gone stands in for: the first overflow's write to
 * it - of what the mocked poll leaves there - fails with EPIPE. The output
 * took all of it, so monitor_flush() writes its end, which the capture
 * takes, closes it, and says how stdout failed, failing.
 *
 * @param [in] stop 0 where the loop flushes stdout at that overflow: it
 *             records the failure and goes on, its output working, to a
 *             second overflow, whose message it does not write, and at which,
 *             the overflow repeating, its timeout of no seconds ends it
 *             without failing; ended at the failure, it would cut short a
 *             session whose output works, and unchecked, it would write the
 *             message again. 1 where a stop is requested during that
 *             overflow's poll: the loop flushes nothing from then on, stdout
 *             included, and ends without failing, leaving the message to
 *             monitor_flush(), where unchecked it would leave it to exit(),
 *             with nothing to see that write fail
 */
static void
a_stdout_that_fails(const int stop)
{
        struct sigaction sigpipe;
        int fds[2];
        int closed;
        FILE *fp;
        int ret = -1;

        ignore_sigpipe(&sigpipe);
        loop_ready();
        fp = tmpfile();
        if (fp == NULL) {
                loop_done();
                assert_int_equal(sigaction(SIGPIPE, &sigpipe, NULL), 0);
                skip();
        }
        free(sel_output_type);
        sel_output_type = __real_strdup("xml");
        assert_non_null(sel_output_type);
        fp_monitor = fp;
        assert_int_equal(pipe(fds), 0);
        close(fds[0]);
        /* what stdout holds goes where it was going, before the pipe */
        (void)fflush(stdout);
        m_stdout_saved = dup(STDOUT_FILENO);
        assert_int_not_equal(m_stdout_saved, -1);
        assert_int_not_equal(dup2(fds[1], STDOUT_FILENO), -1);
        close(fds[1]);
        m_stdout_poll = 1;
        m_stop_poll = stop ? 1 : 0;
        m_flushed = 0;
        m_loop_said = 1;
        m_loop_stdout_failed = stop;
        sel_timeout = 0;
        will_return_count(__wrap_pqos_mon_poll, PQOS_RETVAL_OVERFLOW,
                          stop ? 1 : 2);

        run_function(loop_and_flush, ret);

        m_stop_poll = 0;
        assert_int_equal(stdout_back(NULL), 0);
        closed = fp_monitor == NULL;
        output_done(fp);
        free(sel_output_type);
        sel_output_type = __real_strdup("text");
        loop_done();
        assert_int_equal(sigaction(SIGPIPE, &sigpipe, NULL), 0);

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, 0);
        assert_int_equal(m_polls, stop ? 1 : 2);
        assert_int_equal(output_count("MBM counter overflow"), 1);
        assert_false(m_loop_said);
        assert_int_equal(m_loop_stdout_failed, !stop);
        assert_true(output_has_text("Failed to write standard output: %s",
                                    strerror(EPIPE)));
        assert_int_equal(output_count("Failed to write"), 1);
        assert_int_equal(output_count("</records>"), 1);
        assert_true(closed);
        assert_int_equal(m_flushed, -1);
}

static void
test_a_stdout_that_fails_at_an_overflow_leaves_the_loop_going(void **state)
{
        UNUSED_ARG(state);

        a_stdout_that_fails(0);
}

static void
test_a_stdout_that_fails_after_a_stop_fails_the_flush(void **state)
{
        UNUSED_ARG(state);

        a_stdout_that_fails(1);
}

static void
test_an_overflow_that_repeats_meets_the_timeout(void **state)
{
        int ret = -1;

        UNUSED_ARG(state);

        /* the loop polls again at once after an overflow, but where the
         * overflow repeats, as an MMIO counter's does while its overflow bit
         * stays set, it goes on as after a round, to its timeout: one of no
         * seconds ends it there, without failing. Polling again at once each
         * time, it would poll a third time, and never end
         */
        loop_ready();
        sel_timeout = 0;
        will_return_count(__wrap_pqos_mon_poll, PQOS_RETVAL_OVERFLOW, 2);
        run_function(monitor_loop, ret);
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, 0);
        assert_int_equal(m_polls, 2);
        assert_int_equal(output_count("MBM counter overflow"), 2);
}

#ifdef __linux__
/**
 * @brief Opens a pseudo-terminal, as an output that a case hangs up
 *
 * @param [out] master its master side, whose close hangs the terminal up
 *
 * @return its other side as a stream, or NULL where it could not be opened
 */
static FILE *
open_terminal(int *master)
{
        unsigned int pty = 0;
        int unlock = 0;
        char path[64];
        int slave;
        FILE *fp;

        *master = open("/dev/ptmx", O_RDWR | O_NOCTTY);
        if (*master == -1)
                return NULL;
        if (ioctl(*master, TIOCSPTLCK, &unlock) == 0 &&
            ioctl(*master, TIOCGPTN, &pty) == 0) {
                snprintf(path, sizeof(path), "/dev/pts/%u", pty);
                slave = open(path, O_RDWR | O_NOCTTY);
                if (slave != -1) {
                        fp = fdopen(slave, "w");
                        if (fp != NULL)
                                return fp;
                        close(slave);
                }
        }
        close(*master);
        *master = -1;
        return NULL;
}

/** the data of the loop's group in the cases that print a second poll, read
 * as the output's rows are: nothing monitored, so they print no values
 */
static struct pqos_mon_data m_loop_data;
static char m_loop_context[] = "0";

/** gives the loop's group that data */
static void
loop_data_ready(void)
{
        memset(&m_loop_data, 0, sizeof(m_loop_data));
        m_loop_data.context = m_loop_context;
        m_loop_group.data = &m_loop_data;
}

static void
test_a_working_output_keeps_the_loop_going(void **state)
{
        FILE *fp;
        int failed;
        int ret = -1;

        UNUSED_ARG(state);

        /* a file that takes what is written to it: the output of the second
         * poll - the line the mocked poll leaves in the stream, and the blank
         * line that ends the round in a file, written with fputs(), which the
         * capture does not take - is flushed there, as the third poll finds,
         * reading the file past the stream's buffer. The loop goes on to that
         * poll, after which its timeout of two seconds ends it, and succeeds,
         * with no failure recorded for monitor_flush() to say
         */
        loop_ready();
        fp = tmpfile();
        if (fp == NULL) {
                loop_done();
                skip();
        }
        fp_monitor = fp;
        sel_timeout = 2;
        m_output_poll = 2;
        m_read_poll = 3;
        m_timer_at_once = 1;
        loop_data_ready();
        will_return_count(__wrap_pqos_mon_poll, PQOS_RETVAL_OK, 3);

        run_function(monitor_loop, ret);

        failed = output_failure.failed;
        m_timer_at_once = 0;
        m_read_poll = 0;
        m_output_poll = 0;
        fclose(fp);
        fp_monitor = stdout;
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, 0);
        assert_int_equal(m_polls, 3);
        assert_int_equal(failed, 0);
        assert_false(output_has_text("Failed to read timer"));
        assert_string_equal(m_read, "TIME 2026-10-20 16:00:00\n\n");
}

static void
test_only_an_overflow_that_repeats_waits_for_the_timer(void **state)
{
        FILE *fp;
        int ret = -1;

        UNUSED_ARG(state);

        /* two overflows, a measurement, two overflows and a measurement,
         * each interval of the timer passing at once. The loop polls again
         * at once after the first overflow of each pair, but waits for the
         * timer after the second, as after a measurement, so that a timeout
         * of three seconds ends it at the second measurement. An overflow
         * is not a measurement, and prints no round: the first measurement
         * is the one after the first pair, and the second the only one
         * printed
         */
        loop_ready();
        fp = tmpfile();
        if (fp == NULL) {
                loop_done();
                skip();
        }
        fp_monitor = fp;
        sel_timeout = 3;
        m_timer_at_once = 1;
        loop_data_ready();
        will_return_count(__wrap_pqos_mon_poll, PQOS_RETVAL_OVERFLOW, 2);
        will_return(__wrap_pqos_mon_poll, PQOS_RETVAL_OK);
        will_return_count(__wrap_pqos_mon_poll, PQOS_RETVAL_OVERFLOW, 2);
        will_return(__wrap_pqos_mon_poll, PQOS_RETVAL_OK);

        run_function(monitor_loop, ret);

        m_timer_at_once = 0;
        fclose(fp);
        fp_monitor = stdout;
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, 0);
        assert_int_equal(m_polls, 6);
        assert_int_equal(output_count("MBM counter overflow"), 4);
        assert_int_equal(output_count("TIME "), 1);
}

static void
test_an_output_whose_terminal_hangs_up_ends_the_loop(void **state)
{
        FILE *fp;
        int failed_write;
        int left_open;
        int ret = 0;

        UNUSED_ARG(state);

        /* the output is a terminal, a pseudo-terminal here, which hangs up
         * at the second poll: the writes after it fail with EIO. Its stream
         * is line buffered, as a terminal's is, set so before the hangup
         * rather than left to the C library, which decides at the first
         * write: each write fails as it is made, setting the stream's error
         * indicator, and the flush after the round finds nothing left to
         * fail on, so the failure is said without a reason. With SIGHUP
         * ignored nothing else ends a session without a timeout - the loop
         * ends at the output of that poll, and fails, leaving
         * monitor_flush() no end to write, only the failure to say
         */
        loop_ready();
        fp = open_terminal(&m_pty_master);
        if (fp == NULL) {
                loop_done();
                skip();
        }
        assert_int_equal(setvbuf(fp, NULL, _IOLBF, 0), 0);
        fp_monitor = fp;
        m_output_poll = 2;
        m_timer_at_once = 1;
        m_flushed = 0;
        m_loop_said = 1;
        loop_data_ready();
        will_return_count(__wrap_pqos_mon_poll, PQOS_RETVAL_OK, 2);

        run_function(loop_and_flush, ret);

        m_timer_at_once = 0;
        m_output_poll = 0;
        if (m_pty_master != -1)
                close(m_pty_master);
        m_pty_master = -1;
        /* asked of the stream only where monitor_flush() left it open */
        failed_write = fp_monitor != NULL && ferror(fp);
        left_open = output_done(fp);
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_int_equal(m_polls, 2);
        assert_true(failed_write);
        assert_false(m_loop_said);
        assert_true(output_has_text("Failed to write monitoring output\n"));
        assert_int_equal(output_count("Failed to write monitoring output"), 1);
        assert_int_equal(m_flushed, -1);
        assert_true(left_open);
}

static void
test_an_output_that_cannot_be_flushed_ends_the_loop(void **state)
{
        FILE *fp;
        int left_open;
        int ret = 0;

        UNUSED_ARG(state);

        /* a file on a file system that is full, /dev/full: its stream is
         * fully buffered, and the flush after the second poll's output fails
         * with ENOSPC. The loop ends there, and fails, and leaves
         * monitor_flush() no end to write, only the failure to say
         */
        loop_ready();
        fp = fopen("/dev/full", "w");
        if (fp == NULL) {
                loop_done();
                skip();
        }
        fp_monitor = fp;
        m_output_poll = 2;
        m_timer_at_once = 1;
        m_flushed = 0;
        m_loop_said = 1;
        loop_data_ready();
        will_return_count(__wrap_pqos_mon_poll, PQOS_RETVAL_OK, 2);

        run_function(loop_and_flush, ret);

        m_timer_at_once = 0;
        m_output_poll = 0;
        left_open = output_done(fp);
        loop_done();

        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(ret, -1);
        assert_int_equal(m_polls, 2);
        assert_false(m_loop_said);
        assert_true(output_has_text("Failed to write monitoring output: %s",
                                    strerror(ENOSPC)));
        assert_int_equal(output_count("Failed to write monitoring output"), 1);
        assert_int_equal(m_flushed, -1);
        assert_true(left_open);
}

/**
 * @brief Runs a loop to its timeout, with XML for its output, on a pipe
 *
 * The loop runs outside the capture, so that what the output's functions
 * print reaches the pipe: the start, which opens the root, and the row of the
 * second poll, which names its group's core, after which a timeout of one
 * second ends the loop. The end, which closes the root, the loop leaves for
 * monitor_flush() to write.
 *
 * @param [in] mode how the output's stream is buffered: _IOFBF, as a pipe's
 *             is by default, _IOLBF, as under "stdbuf -oL", or _IONBF
 * @param [out] reader the pipe's read end, which does not block
 * @param [out] got what the pipe held when the loop returned
 * @param [in] size the size of got
 *
 * @return the output, a stream on the pipe's write end
 */
static FILE *
xml_loop_on_a_pipe(const int mode, int *reader, char *got, const size_t size)
{
        int fds[2];
        ssize_t len;
        FILE *fp;
        int ret;

        free(sel_output_type);
        sel_output_type = __real_strdup("xml");
        assert_non_null(sel_output_type);
        sel_monitor_type = MON_GROUP_TYPE_CORE;
        loop_ready();
        assert_int_equal(pipe(fds), 0);
        *reader = fds[0];
        assert_int_equal(fcntl(fds[0], F_SETFL, O_NONBLOCK), 0);
        fp = fdopen(fds[1], "w");
        if (fp == NULL)
                close(fds[1]);
        assert_non_null(fp);
        assert_int_equal(setvbuf(fp, NULL, mode, 0), 0);
        fp_monitor = fp;
        sel_timeout = 1;
        m_timer_at_once = 1;
        loop_data_ready();
        will_return_count(__wrap_pqos_mon_poll, PQOS_RETVAL_OK, 2);

        ret = monitor_loop();

        m_timer_at_once = 0;
        assert_int_equal(ret, 0);
        assert_int_equal(m_polls, 2);
        len = read(fds[0], got, size - 1);
        got[len > 0 ? len : 0] = '\0';

        return fp;
}

/** closes what xml_loop_on_a_pipe() opened and monitor_flush() did not, and
 * goes back to text
 */
static void
xml_loop_done(FILE *fp, const int reader)
{
        output_done(fp);
        if (reader != -1)
                close(reader);
        free(sel_output_type);
        sel_output_type = __real_strdup("text");
        sel_monitor_type = 0;
        loop_done();
}

/**
 * @brief Has monitor_flush() write the end of the output to its stream
 *
 * The capture takes every write made while it runs, whichever stream it is
 * addressed to, and would take the end before it reached the pipe. So
 * monitor_flush() is called outside it, and what it says on stderr is read
 * from a pipe that stands in for stderr while it runs.
 *
 * @param [out] said what monitor_flush() wrote to stderr
 * @param [in] size the size of said
 *
 * @return what monitor_flush() returned
 */
static int
flush_outside_the_capture(char *said, const size_t size)
{
        int fds[2];
        ssize_t len;
        int saved;
        int ret;

        assert_int_equal(pipe(fds), 0);
        saved = dup(STDERR_FILENO);
        assert_int_not_equal(saved, -1);
        (void)fflush(stderr);
        assert_int_not_equal(dup2(fds[1], STDERR_FILENO), -1);
        close(fds[1]);

        ret = monitor_flush();

        (void)fflush(stderr);
        assert_int_not_equal(dup2(saved, STDERR_FILENO), -1);
        close(saved);
        /* stderr is back, and the pipe has no writer left: the read does not
         * wait
         */
        len = read(fds[0], said, size - 1);
        said[len > 0 ? len : 0] = '\0';
        close(fds[0]);

        return ret;
}

/**
 * @brief Sees the end of an output on a pipe reach it only when
 *        monitor_flush() writes it
 *
 * The loop flushes its start and its rows as it goes, but leaves its end to
 * monitor_flush(): a pipe whose reader does not read blocks a write - the
 * flush of a fully buffered stream, the write itself of a line buffered or
 * unbuffered one - and the groups and the library lock would be held until it
 * did. main() has monitor_flush() write it, once it has stopped the
 * monitoring and finalised the library. monitor_flush() then closes the
 * output, and checks the close, where a file system such as NFS can report a
 * write that failed. Where the close fails - the wrap of pqos_fclose() has it
 * fail with EIO once the stream is closed - monitor_flush() says why, and
 * fails, the end having reached the pipe all the same.
 *
 * @param [in] mode how the output's stream is buffered: _IOFBF, _IOLBF or
 *             _IONBF
 * @param [in] close_fails 1 for the output's close to fail
 */
static void
an_end_left_to_the_flush(const int mode, const int close_fails)
{
        char expected[128] = "";
        char got[1024];
        char said[256];
        char end[64];
        int reader = -1;
        ssize_t len;
        int closed;
        FILE *fp;
        int ret;

        fp = xml_loop_on_a_pipe(mode, &reader, got, sizeof(got));
        if (close_fails) {
                m_close_fails = fp;
                (void)snprintf(expected, sizeof(expected),
                               "Failed to write monitoring output: %s\n",
                               strerror(EIO));
        }

        ret = flush_outside_the_capture(said, sizeof(said));
        closed = fp_monitor == NULL;

        len = read(reader, end, sizeof(end) - 1);
        end[len > 0 ? len : 0] = '\0';
        xml_loop_done(fp, reader);

        assert_non_null(strstr(got, "<records>\n<record>\n"));
        assert_non_null(strstr(got, "\t<core>0</core>\n"));
        assert_non_null(strstr(got, "</record>\n"));
        assert_null(strstr(got, "</records>"));
        assert_int_equal(ret, close_fails ? -1 : 0);
        assert_string_equal(said, expected);
        assert_string_equal(end, "</records>\n");
        assert_true(closed);
}

static void
test_the_end_of_an_output_is_left_to_the_flush(void **state)
{
        UNUSED_ARG(state);

        an_end_left_to_the_flush(_IOFBF, 0);
}

static void
test_the_end_of_a_line_buffered_output_is_left_to_the_flush(void **state)
{
        UNUSED_ARG(state);

        an_end_left_to_the_flush(_IOLBF, 0);
}

static void
test_the_end_of_an_unbuffered_output_is_left_to_the_flush(void **state)
{
        UNUSED_ARG(state);

        an_end_left_to_the_flush(_IONBF, 0);
}

static void
test_a_failed_close_of_an_output_fails_the_flush(void **state)
{
        UNUSED_ARG(state);

        an_end_left_to_the_flush(_IOFBF, 1);
}

/**
 * @brief Sees monitor_flush() fail where the end of an output on a pipe does
 *
 * What the loop flushes, its start and its row, reaches the reader, which
 * then goes, as it may at a Ctrl-C, and the write that closes the root fails
 * with EPIPE: at the flush, which says why, where the stream is fully
 * buffered, and at once otherwise, which leaves the stream's error for the
 * flush to find, without a reason. monitor_flush() says so, and fails, and so
 * does the run. It closes the output all the same, and its close, which the
 * wrap of pqos_fclose() fails as well, it does not say a second time.
 *
 * @param [in] mode how the output's stream is buffered: _IOFBF, _IOLBF or
 *             _IONBF
 * @param [in] expected what monitor_flush() is to say on stderr
 */
static void
a_failed_end(const int mode, const char *expected)
{
        struct sigaction sigpipe;
        char got[1024];
        char said[256];
        int reader = -1;
        int closed;
        FILE *fp;
        int ret;

        ignore_sigpipe(&sigpipe);
        fp = xml_loop_on_a_pipe(mode, &reader, got, sizeof(got));
        close(reader);
        reader = -1;
        m_close_fails = fp;

        ret = flush_outside_the_capture(said, sizeof(said));
        closed = fp_monitor == NULL;

        xml_loop_done(fp, reader);
        assert_int_equal(sigaction(SIGPIPE, &sigpipe, NULL), 0);

        assert_non_null(strstr(got, "</record>\n"));
        assert_null(strstr(got, "</records>"));
        assert_int_equal(ret, -1);
        assert_string_equal(said, expected);
        assert_true(closed);
}

static void
test_a_failed_end_of_an_output_fails_the_flush(void **state)
{
        char expected[128];

        UNUSED_ARG(state);

        (void)snprintf(expected, sizeof(expected),
                       "Failed to write monitoring output: %s\n",
                       strerror(EPIPE));
        a_failed_end(_IOFBF, expected);
}

static void
test_a_failed_end_of_a_line_buffered_output_fails_the_flush(void **state)
{
        UNUSED_ARG(state);

        a_failed_end(_IOLBF, "Failed to write monitoring output\n");
}

static void
test_a_failed_end_of_an_unbuffered_output_fails_the_flush(void **state)
{
        UNUSED_ARG(state);

        a_failed_end(_IONBF, "Failed to write monitoring output\n");
}

/**
 * @brief Sees a loop told to stop flush nothing more
 *
 * A stop signal's handler, which the setup sets before the groups start,
 * requests the stop, and from then on the loop flushes nothing: a pipe whose
 * reader has stopped reading would block the flush, which the signal would
 * not cut short, with the groups and the library lock held. What the stream
 * still holds reaches the pipe with the end, which monitor_flush() writes
 * once the monitoring is stopped. The stream is fully buffered, as a pipe's
 * is by default: a line buffered or unbuffered one writes what is printed as
 * it is printed.
 *
 * @param [in] poll the poll during which the stop is requested, the mocked
 *             poll leaving a line in the stream; or 0 for a stop that came
 *             while the groups started, which ends the loop before its first
 *             poll, once it has begun the output
 * @param [in] result what that poll returns: after an overflow the loop polls
 *             again at once, and after any other result prints the round
 */
static void
a_loop_told_to_stop(const unsigned poll, const int result)
{
        static const char start[] =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<records>\n";
        static const char line[] = "TIME 2026-10-20 16:00:00\n";
        static const char end[] = "</records>\n";
        char early[256];
        char got[1024];
        char said[256];
        int fds[2];
        ssize_t len;
        size_t size;
        int flushed;
        int closed;
        FILE *fp;
        int ret;

        free(sel_output_type);
        sel_output_type = __real_strdup("xml");
        assert_non_null(sel_output_type);
        sel_monitor_type = MON_GROUP_TYPE_CORE;
        loop_ready();
        assert_int_equal(pipe(fds), 0);
        assert_int_equal(fcntl(fds[0], F_SETFL, O_NONBLOCK), 0);
        fp = fdopen(fds[1], "w");
        if (fp == NULL)
                close(fds[1]);
        assert_non_null(fp);
        assert_int_equal(setvbuf(fp, NULL, _IOFBF, 0), 0);
        fp_monitor = fp;
        if (poll == 0) {
                __atomic_store_n(&stop_monitoring_loop, 1, __ATOMIC_RELAXED);
        } else {
                m_stop_poll = poll;
                m_output_poll = poll;
                m_timer_at_once = 1;
                loop_data_ready();
                if (poll > 1)
                        will_return_count(__wrap_pqos_mon_poll, PQOS_RETVAL_OK,
                                          (int)poll - 1);
                will_return(__wrap_pqos_mon_poll, result);
        }

        ret = monitor_loop();

        m_timer_at_once = 0;
        m_output_poll = 0;
        m_stop_poll = 0;
        /* what the loop flushed to the pipe */
        len = read(fds[0], early, sizeof(early) - 1);
        early[len > 0 ? len : 0] = '\0';

        flushed = flush_outside_the_capture(said, sizeof(said));
        closed = fp_monitor == NULL;
        len = read(fds[0], got, sizeof(got) - 1);
        got[len > 0 ? len : 0] = '\0';
        xml_loop_done(fp, fds[0]);

        assert_int_equal(ret, 0);
        assert_int_equal(m_polls, poll);
        assert_string_equal(early, poll == 0 ? "" : start);
        assert_int_equal(flushed, 0);
        assert_string_equal(said, "");
        assert_true(closed);
        /* and what monitor_flush() wrote: what the loop had not flushed - the
         * start, or the poll's line and any round printed after it - and the
         * end
         */
        size = strlen(poll == 0 ? start : line);
        assert_int_equal(strncmp(got, poll == 0 ? start : line, size), 0);
        assert_int_equal(strstr(got, "<record>\n") != NULL,
                         poll != 0 && result != PQOS_RETVAL_OVERFLOW);
        size = strlen(got);
        assert_true(size >= strlen(end));
        assert_string_equal(got + size - strlen(end), end);
        /* once */
        assert_ptr_equal(strstr(got, end), got + size - strlen(end));
}

static void
test_a_loop_told_to_stop_before_it_polls_flushes_nothing(void **state)
{
        UNUSED_ARG(state);

        a_loop_told_to_stop(0, PQOS_RETVAL_OK);
}

static void
test_a_loop_told_to_stop_at_an_overflow_flushes_nothing(void **state)
{
        UNUSED_ARG(state);

        a_loop_told_to_stop(2, PQOS_RETVAL_OVERFLOW);
}

static void
test_a_loop_told_to_stop_at_a_round_flushes_nothing(void **state)
{
        UNUSED_ARG(state);

        a_loop_told_to_stop(2, PQOS_RETVAL_OK);
}
#endif

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
            cmocka_unit_test(test_the_flush_leaves_stdout_open),
            cmocka_unit_test(
                test_an_output_that_a_write_failed_on_ends_the_loop_unpolled),
            cmocka_unit_test(
                test_an_output_that_fails_at_an_overflow_ends_the_loop),
            cmocka_unit_test_teardown(
                test_a_stdout_that_fails_at_an_overflow_leaves_the_loop_going,
                stdout_back),
            cmocka_unit_test_teardown(
                test_a_stdout_that_fails_after_a_stop_fails_the_flush,
                stdout_back),
            cmocka_unit_test(test_an_overflow_that_repeats_meets_the_timeout),
#ifdef __linux__
            cmocka_unit_test(test_a_working_output_keeps_the_loop_going),
            cmocka_unit_test(
                test_only_an_overflow_that_repeats_waits_for_the_timer),
            cmocka_unit_test(
                test_an_output_whose_terminal_hangs_up_ends_the_loop),
            cmocka_unit_test(
                test_an_output_that_cannot_be_flushed_ends_the_loop),
            cmocka_unit_test(test_the_end_of_an_output_is_left_to_the_flush),
            cmocka_unit_test(
                test_the_end_of_a_line_buffered_output_is_left_to_the_flush),
            cmocka_unit_test(
                test_the_end_of_an_unbuffered_output_is_left_to_the_flush),
            cmocka_unit_test(test_a_failed_close_of_an_output_fails_the_flush),
            cmocka_unit_test(test_a_failed_end_of_an_output_fails_the_flush),
            cmocka_unit_test(
                test_a_failed_end_of_a_line_buffered_output_fails_the_flush),
            cmocka_unit_test(
                test_a_failed_end_of_an_unbuffered_output_fails_the_flush),
            cmocka_unit_test(
                test_a_loop_told_to_stop_before_it_polls_flushes_nothing),
            cmocka_unit_test(
                test_a_loop_told_to_stop_at_an_overflow_flushes_nothing),
            cmocka_unit_test(
                test_a_loop_told_to_stop_at_a_round_flushes_nothing),
#endif
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
