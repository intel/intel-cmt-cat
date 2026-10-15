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
 * Which PID groups may be left out when their process has ended before its
 * monitoring starts: the ones top-pids mode chose, and not one the user named
 * - whether it sits beside the top-pids selection or names a process top-pids
 * chose as well, in either order.
 *
 * monitor.c is included rather than linked, so that a case can build its
 * selection through the functions the options call and read the groups back.
 * The library's start is mocked; whether a process has ended is asked of the
 * running system. So a process that ends is a child: running while the options
 * are read, which refuse a PID that is not there, and ended - reaped, or left a
 * zombie - before the monitoring starts, which is the window this is about. A
 * child whose start fails while it still runs is the other side of it: that
 * failure is the run's.
 */

/* for fopencookie(), which makes the /proc file a reap leaves behind */
#define _GNU_SOURCE

#include "test_monitor_top_pids.h"

#include "monitor.c"
#include "output.h"

#include <errno.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/** the process whose monitoring start fails, as one that has ended does */
static pid_t g_ended;

/** a process still running whose monitoring start fails all the same */
static pid_t g_start_fails;

/** how many groups the mocked start was asked for */
static unsigned g_starts;

/** the process whose /proc stat file reads as one reaped after its open */
static pid_t g_reaped_on_read;

/** the process whose /proc stat file reads as g_stat instead */
static pid_t g_stat_of;
static char g_stat[512];

/** the process whose /proc stat file is not there, as hidepid hides one */
static pid_t g_hidden;

/** the process reaped as its /proc stat file is opened */
static pid_t g_reaped_on_open;

/** the process this one may not signal (EPERM) */
static pid_t g_no_signal;

/** the process whose /proc stat file reads as g_cut_text and then fails */
static pid_t g_cut_short;
static char g_cut_text[512];

/** a real top-pids selection: every pick but g_kept has ended */
static int g_others_ended;
static pid_t g_kept;

/** the children a case started, which the teardown ends if the case did not */
static pid_t g_children[4];
static unsigned g_num_children;

/**
 * @brief Whether \a pid is a top-pids pick that g_others_ended has ended
 */
static int
pretends_ended(const pid_t pid)
{
        unsigned i;

        if (!g_others_ended || pid == g_kept)
                return 0;
        for (i = 0; i < sel_monitor_num; i++)
                if (sel_monitor_group[i].type == MON_GROUP_TYPE_PID &&
                    sel_monitor_group[i].top_pid &&
                    sel_monitor_group[i].pids[0] == pid)
                        return 1;

        return 0;
}

int
__wrap_kill(pid_t pid, int sig)
{
        if (sig == 0 && pid > 0) {
                if (pid == g_no_signal) {
                        errno = EPERM;
                        return -1;
                }
                if (pretends_ended(pid)) {
                        errno = ESRCH;
                        return -1;
                }
        }

        return __real_kill(pid, sig);
}

int
__wrap_pqos_inter_get(enum pqos_interface *interface)
{
        *interface = PQOS_INTER_OS;
        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_mon_start_pids2(const unsigned num_pids,
                            const pid_t *pids,
                            const enum pqos_mon_event event,
                            void *context,
                            struct pqos_mon_data **group)
{
        (void)event;
        (void)context;

        assert_int_equal(num_pids, 1);
        g_starts++;
        *group = NULL;

        if (pids[0] == g_ended || pretends_ended(pids[0]))
                return PQOS_RETVAL_PARAM;
        if (pids[0] == g_start_fails)
                return PQOS_RETVAL_RESOURCE;
        return PQOS_RETVAL_OK;
}

int
__wrap_pqos_mon_stop(struct pqos_mon_data *group)
{
        (void)group;
        return PQOS_RETVAL_OK;
}

/* what a read of a reaped process's /proc file answers */
static ssize_t
read_esrch(void *cookie, char *buf, size_t size)
{
        (void)cookie;
        (void)buf;
        (void)size;
        errno = ESRCH;
        return -1;
}

/* a read of g_cut_text, and then a failure */
static ssize_t
read_cut_short(void *cookie, char *buf, size_t size)
{
        int *done = (int *)cookie;
        const size_t len = strlen(g_cut_text);

        if (*done) {
                errno = EIO;
                return -1;
        }
        *done = 1;
        if (size > len)
                size = len;
        memcpy(buf, g_cut_text, size);

        return (ssize_t)size;
}

/**
 * @brief Marks a child reaped, so that the teardown does not wait for it
 */
static void
reaped(const pid_t pid)
{
        unsigned i;

        for (i = 0; i < g_num_children; i++)
                if (g_children[i] == pid)
                        g_children[i] = 0;
}

FILE *
__wrap_pqos_fopen(const char *name, const char *mode)
{
        static int cut_done;
        char path[64];

        snprintf(path, sizeof(path), "/proc/%d/stat", (int)g_hidden);
        if (g_hidden > 0 && strcmp(name, path) == 0) {
                errno = ENOENT;
                return NULL;
        }
        snprintf(path, sizeof(path), "/proc/%d/stat", (int)g_reaped_on_open);
        if (g_reaped_on_open > 0 && strcmp(name, path) == 0) {
                assert_int_equal(waitpid(g_reaped_on_open, NULL, 0),
                                 g_reaped_on_open);
                reaped(g_reaped_on_open);
                errno = ENOENT;
                return NULL;
        }
        snprintf(path, sizeof(path), "/proc/%d/stat", (int)g_cut_short);
        if (g_cut_short > 0 && strcmp(name, path) == 0) {
                cookie_io_functions_t io = {.read = read_cut_short};

                cut_done = 0;
                return fopencookie(&cut_done, mode, io);
        }

        snprintf(path, sizeof(path), "/proc/%d/stat", (int)g_reaped_on_read);
        if (g_reaped_on_read > 0 && strcmp(name, path) == 0) {
                cookie_io_functions_t io = {.read = read_esrch};

                return fopencookie(NULL, mode, io);
        }
        snprintf(path, sizeof(path), "/proc/%d/stat", (int)g_stat_of);
        if (g_stat_of > 0 && strcmp(name, path) == 0)
                return fmemopen(g_stat, strlen(g_stat), mode);

        return __real_pqos_fopen(name, mode);
}

/**
 * @brief Forks a child of the case, which the teardown ends if the case did not
 *
 * The child also ends with this process, should it be killed before the
 * teardown, and holds none of the run's output open.
 *
 * @return the child's PID in this process, 0 in the child
 */
static pid_t
child(void)
{
        const pid_t parent = getpid();
        pid_t pid;

        assert_true(g_num_children < DIM(g_children));
        pid = fork();
        assert_true(pid >= 0);
        if (pid == 0) {
                if (prctl(PR_SET_PDEATHSIG, SIGKILL) != 0 ||
                    getppid() != parent)
                        _exit(0);
                close(STDOUT_FILENO);
                close(STDERR_FILENO);
                return 0;
        }
        g_children[g_num_children++] = pid;

        return pid;
}

/**
 * @brief A process that runs until end_process() ends it
 */
static pid_t
running_process(void)
{
        const pid_t pid = child();

        if (pid == 0)
                for (;;)
                        pause();

        return pid;
}

/**
 * @brief A running process named \a name, once it has taken the name
 */
static pid_t
running_process_named(const char *name)
{
        int ready[2];
        pid_t pid;
        char c;

        assert_int_equal(pipe(ready), 0);
        pid = child();
        if (pid == 0) {
                if (prctl(PR_SET_NAME, name) != 0)
                        _exit(1);
                (void)write(ready[1], "x", 1);
                for (;;)
                        pause();
        }
        close(ready[1]);
        assert_int_equal(read(ready[0], &c, 1), 1);
        close(ready[0]);

        return pid;
}

static void *
waits(void *arg)
{
        (void)arg;
        for (;;)
                pause();

        return NULL;
}

/**
 * @brief Reads the state letter and thread count of \a pid, from the status
 *        file rather than the stat file process_has_ended() parses
 */
static void
proc_state(const pid_t pid, char *state, long *threads)
{
        char path[64];
        char line[256];
        FILE *fp;

        *state = '?';
        *threads = -1;
        snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
        fp = fopen(path, "r");
        if (fp == NULL)
                return;
        while (fgets(line, sizeof(line), fp) != NULL) {
                const char *v = strchr(line, ':');

                if (v == NULL)
                        continue;
                for (v++; *v == ' ' || *v == '\t'; v++)
                        ;
                if (strncmp(line, "State:", 6) == 0)
                        *state = *v;
                else if (strncmp(line, "Threads:", 8) == 0)
                        *threads = strtol(v, NULL, 10);
        }
        fclose(fp);
}

/**
 * @brief A process whose leader has called pthread_exit() and whose other
 *        thread runs on: a zombie leader, and a process that has not ended
 */
static pid_t
process_whose_leader_exited(void)
{
        const struct timespec ms = {0, 1000000};
        pid_t pid = child();
        char state = '?';
        long threads = -1;
        int i;

        if (pid == 0) {
                pthread_t thread;

                if (pthread_create(&thread, NULL, waits, NULL) != 0)
                        _exit(1);
                pthread_exit(NULL);
        }
        for (i = 0; i < 5000; i++) {
                proc_state(pid, &state, &threads);
                if (state == 'Z')
                        break;
                nanosleep(&ms, NULL);
        }
        assert_int_equal(state, 'Z');
        assert_int_equal(threads, 2);

        return pid;
}

/**
 * @brief Ends a process from running_process(), and reaps it
 *
 * It is then the process whose monitoring start fails.
 */
static void
end_process(const pid_t pid)
{
        unsigned i;

        assert_int_equal(kill(pid, SIGKILL), 0);
        assert_int_equal(waitpid(pid, NULL, 0), pid);
        g_ended = pid;
        for (i = 0; i < g_num_children; i++)
                if (g_children[i] == pid)
                        g_children[i] = 0;
}

/**
 * @brief Selects \a pid the way top-pids mode does, as its only pick
 */
static void
top_pids_pick(const pid_t pid)
{
        struct proc_stats stats;
        struct slist elem;
        int ret = 0;

        memset(&stats, 0, sizeof(stats));
        stats.pid = pid;
        stats.ticks_delta = 1;
        stats.valid = 1;
        elem.data = &stats;
        elem.next = NULL;

        sel_mon_top_pids = 1;
        run_function(fill_top_procs, ret, &elem);
        assert_false(output_exit_was_called());
        assert_int_equal(ret, 1);
}

/**
 * @brief Selects \a pid the way "-p llc:<pid>" does, or "-p llc:[<pid>]"
 */
static void
user_names_as(const char *format, const pid_t pid)
{
        char arg[32];

        snprintf(arg, sizeof(arg), format, (int)pid);
        run_void_function(selfn_monitor_pids, arg);
        assert_false(output_exit_was_called());
}

static void
user_names(const pid_t pid)
{
        user_names_as("llc:%d", pid);
}

static struct mon_group *
group_of(const pid_t pid)
{
        unsigned i;

        for (i = 0; i < sel_monitor_num; i++)
                if (sel_monitor_group[i].type == MON_GROUP_TYPE_PID &&
                    sel_monitor_group[i].pids[0] == pid)
                        return &sel_monitor_group[i];

        return NULL;
}

/**
 * @brief Starts the selection with a platform that monitors LLC occupancy
 *
 * @return what monitor_setup() returned
 */
static int
setup_events(const enum pqos_mon_event *events, const unsigned num_events)
{
        struct pqos_cpuinfo cpu;
        struct pqos_capability cap;
        struct pqos_cap_mon *mon;
        unsigned i;
        int ret = 0;

        memset(&cpu, 0, sizeof(cpu));
        mon = calloc(1, sizeof(*mon) + num_events * sizeof(mon->events[0]));
        assert_non_null(mon);
        mon->max_rmid = 64;
        mon->num_events = num_events;
        for (i = 0; i < num_events; i++)
                mon->events[i].type = events[i];
        cap.type = PQOS_CAP_TYPE_MON;
        cap.u.mon = mon;

        run_function(monitor_setup, ret, &cpu, &cap, NULL);
        free(mon);

        return ret;
}

static int
setup(void)
{
        const enum pqos_mon_event llc = PQOS_MON_EVENT_L3_OCCUP;

        return setup_events(&llc, 1);
}

static int
reset(void **state)
{
        unsigned i;

        (void)state;
        for (i = 0; i < g_num_children; i++)
                if (g_children[i] > 0) {
                        kill(g_children[i], SIGKILL);
                        waitpid(g_children[i], NULL, 0);
                }
        g_num_children = 0;
        for (i = 0; i < sel_monitor_num; i++)
                grp_free(&sel_monitor_group[i]);
        free(sel_monitor_group);
        sel_monitor_group = NULL;
        sel_monitor_num = 0;
        sel_monitor_type = 0;
        sel_mon_top_pids = 0;
        monitor_cleanup();
        g_ended = 0;
        g_start_fails = 0;
        g_starts = 0;
        g_reaped_on_read = 0;
        g_stat_of = 0;
        g_hidden = 0;
        g_reaped_on_open = 0;
        g_no_signal = 0;
        g_cut_short = 0;
        g_others_ended = 0;
        g_kept = 0;
        sel_mon_top_like = 0;
        sel_monitor_opts = 0;
        sel_events_max = (enum pqos_mon_event)0;

        return 0;
}

static void
test_an_ended_top_pid_is_left_out(void **state)
{
        const pid_t self = getpid();
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(self);
        end_process(pid);

        assert_int_equal(setup(), 0);
        assert_int_equal(g_starts, 2);
        assert_true(output_has_text("PID %d ended before its monitoring "
                                    "could start, so it is left out",
                                    (int)g_ended));
        assert_int_equal(sel_monitor_num, 1);
        assert_int_equal(sel_monitor_group[0].pids[0], self);
}

static void
test_a_named_pid_beside_top_pids_stays_an_error(void **state)
{
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(getpid());
        user_names(pid);
        assert_false(group_of(pid)->top_pid);
        end_process(pid);

        assert_int_equal(setup(), -1);
        assert_true(
            output_has_text("PID %d monitoring start error", (int)g_ended));
        assert_false(output_has_text("left out"));
}

static void
test_a_top_pid_the_user_names_after_is_the_users(void **state)
{
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        assert_true(group_of(pid)->top_pid);
        user_names(pid);
        assert_false(group_of(pid)->top_pid);
        end_process(pid);

        assert_int_equal(setup(), -1);
        assert_true(
            output_has_text("PID %d monitoring start error", (int)g_ended));
        assert_false(output_has_text("left out"));
}

static void
test_a_top_pid_the_user_names_as_a_group_is_the_users(void **state)
{
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        user_names_as("llc:[%d]", pid);
        assert_int_equal(sel_monitor_num, 1);
        assert_false(group_of(pid)->top_pid);
        end_process(pid);

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d monitoring start error", (int)pid));
        assert_false(output_has_text("left out"));
}

static void
test_a_named_pid_top_pids_picks_after_is_the_users(void **state)
{
        const pid_t pid = running_process();

        (void)state;
        user_names(pid);
        top_pids_pick(pid);
        assert_int_equal(sel_monitor_num, 1);
        assert_false(group_of(pid)->top_pid);
        end_process(pid);

        assert_int_equal(setup(), -1);
        assert_true(
            output_has_text("PID %d monitoring start error", (int)g_ended));
        assert_false(output_has_text("left out"));
}

static void
test_a_named_pid_is_kept_when_top_pids_are_left_out(void **state)
{
        const pid_t self = getpid();
        const pid_t pid = running_process();

        (void)state;
        user_names(self);
        top_pids_pick(pid);
        end_process(pid);

        assert_int_equal(setup(), 0);
        assert_true(output_has_text("PID %d ended before its monitoring "
                                    "could start, so it is left out",
                                    (int)g_ended));
        assert_int_equal(sel_monitor_num, 1);
        assert_int_equal(sel_monitor_group[0].pids[0], self);
        assert_false(sel_monitor_group[0].top_pid);
}

static void
test_a_pid_top_pids_picks_twice_stays_top_pids(void **state)
{
        const pid_t self = getpid();
        const pid_t pid = running_process();

        (void)state;
        /* "pqos -p -p": two scans, which may pick the same process */
        top_pids_pick(pid);
        top_pids_pick(pid);
        top_pids_pick(self);
        assert_true(group_of(pid)->top_pid);
        end_process(pid);

        assert_int_equal(setup(), 0);
        assert_true(output_has_text("PID %d ended before its monitoring "
                                    "could start, so it is left out",
                                    (int)pid));
        assert_int_equal(sel_monitor_num, 1);
}

static void
test_a_zombie_reaped_before_its_stat_is_read_has_ended(void **state)
{
        const pid_t self = getpid();
        const pid_t pid = running_process();
        siginfo_t info;

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(self);
        /* a zombie when its start fails, so kill() still answers for it, and
         * reaped by the time its stat file is read: waited for and left
         * unreaped, which the teardown does
         */
        assert_int_equal(kill(pid, SIGKILL), 0);
        memset(&info, 0, sizeof(info));
        assert_int_equal(waitid(P_PID, pid, &info, WEXITED | WNOWAIT), 0);
        g_ended = pid;
        g_reaped_on_read = pid;

        assert_int_equal(setup(), 0);
        assert_true(output_has_text("PID %d ended before its monitoring "
                                    "could start, so it is left out",
                                    (int)pid));
        assert_int_equal(sel_monitor_num, 1);
}

static void
test_a_running_top_pid_whose_start_fails_stays_an_error(void **state)
{
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(getpid());
        /* refused as a process that does not exist - every thread the library
         * listed ended, say, and one started since runs on: the process is
         * there, and the failure is real
         */
        g_ended = pid;

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d monitoring start error", (int)pid));
        assert_false(output_has_text("left out"));
}

static void
test_a_zombie_top_pid_has_ended(void **state)
{
        const pid_t self = getpid();
        const pid_t pid = running_process();
        siginfo_t info;

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(self);
        /* exited and not reaped: kill() still answers for it, and its stat
         * file, read for real, says Z with no thread but the leader
         */
        assert_int_equal(kill(pid, SIGKILL), 0);
        memset(&info, 0, sizeof(info));
        assert_int_equal(waitid(P_PID, pid, &info, WEXITED | WNOWAIT), 0);
        g_ended = pid;

        assert_int_equal(setup(), 0);
        assert_true(output_has_text("PID %d ended before its monitoring "
                                    "could start, so it is left out",
                                    (int)pid));
        assert_int_equal(sel_monitor_num, 1);
        assert_int_equal(sel_monitor_group[0].pids[0], self);
}

static void
test_a_top_pid_named_like_a_zombie_stays_an_error(void **state)
{
        /* A name anyone can give a process. Taken to end at its first ')', it
         * reads as state Z, and the thread count as the children's user time,
         * which is 0 for a child that has none - wherever the case runs, which
         * a shorter name would not make so: one field fewer is the nice value
         */
        const pid_t pid = running_process_named("x) Z a b c");

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(getpid());
        g_ended = pid;

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d monitoring start error", (int)pid));
        assert_false(output_has_text("left out"));
}

static void
test_a_top_pid_whose_leader_exited_stays_an_error(void **state)
{
        const pid_t pid = process_whose_leader_exited();

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(getpid());
        /* the leader's TID is still listed, and refused as a thread that has
         * ended: the start fails, for a process that is still running
         */
        g_ended = pid;

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d monitoring start error", (int)pid));
        assert_false(output_has_text("left out"));
}

static void
test_a_dead_top_pid_has_ended(void **state)
{
        const pid_t self = getpid();
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(self);
        /* X, dead: the state a process passes through as it is reaped, too
         * brief to catch, so its stat file is made to say it
         */
        snprintf(g_stat, sizeof(g_stat),
                 "%d (dying) X 1 %d %d 0 -1 4194560 0 0 0 0 0 0 0 0 20 0 1 0 "
                 "100 0 0\n",
                 (int)pid, (int)pid, (int)pid);
        g_stat_of = pid;
        g_ended = pid;

        assert_int_equal(setup(), 0);
        assert_true(output_has_text("PID %d ended before its monitoring "
                                    "could start, so it is left out",
                                    (int)pid));
        assert_int_equal(sel_monitor_num, 1);
}

static void
test_every_top_pid_ended_fails_the_run(void **state)
{
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        end_process(pid);

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d ended before its monitoring "
                                    "could start, so it is left out",
                                    (int)pid));
        assert_true(output_has_text("Every top-pids process ended before its "
                                    "monitoring could start"));
        assert_int_equal(sel_monitor_num, 0);
}

/**
 * @brief Makes \a pid a zombie: exited and waited for, but not reaped
 */
static void
make_zombie(const pid_t pid)
{
        siginfo_t info;

        assert_int_equal(kill(pid, SIGKILL), 0);
        memset(&info, 0, sizeof(info));
        assert_int_equal(waitid(P_PID, pid, &info, WEXITED | WNOWAIT), 0);
}

static void
test_another_failure_of_an_ended_top_pid_stays_an_error(void **state)
{
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(getpid());
        /* ended, and its start failed for a reason of the library's own, not
         * the one a process that is gone gets: that failure is the run's
         */
        end_process(pid);
        g_ended = 0;
        g_start_fails = pid;

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d monitoring start error", (int)pid));
        assert_false(output_has_text("left out"));
}

static void
test_the_columns_are_those_of_the_groups_left(void **state)
{
        const enum pqos_mon_event events[] = {PQOS_MON_EVENT_L3_OCCUP,
                                              PQOS_MON_EVENT_LMEM_BW};
        const pid_t self = getpid();
        const pid_t pid = running_process();
        enum pqos_mon_event shown;

        (void)state;
        /* the user's group asks for LLC only, the pick for every event */
        user_names(self);
        top_pids_pick(pid);
        end_process(pid);

        assert_int_equal(setup_events(events, DIM(events)), 0);
        assert_int_equal(sel_monitor_num, 1);
        /* so with the pick left out there is no column for MBL */
        shown = monitor_get_events();
        assert_int_equal(shown, sel_monitor_group[0].events);
        assert_false(shown & PQOS_MON_EVENT_LMEM_BW);
}

static void
test_a_top_pid_hidden_from_proc_stays_an_error(void **state)
{
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(getpid());
        /* the library cannot see it, and neither can its stat file be
         * opened - /proc mounted hidepid - while kill() finds it: it is there
         */
        g_ended = pid;
        g_hidden = pid;

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d monitoring start error", (int)pid));
        assert_false(output_has_text("left out"));
}

static void
test_a_zombie_reaped_before_its_stat_is_opened_has_ended(void **state)
{
        const pid_t self = getpid();
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(self);
        /* a zombie when its start fails, so kill() still answers for it, and
         * reaped by the time its stat file is opened, which is not there then
         */
        make_zombie(pid);
        g_ended = pid;
        g_reaped_on_open = pid;

        assert_int_equal(setup(), 0);
        assert_true(output_has_text("PID %d ended before its monitoring "
                                    "could start, so it is left out",
                                    (int)pid));
        assert_int_equal(sel_monitor_num, 1);
        assert_int_equal(sel_monitor_group[0].pids[0], self);
}

static void
test_a_zombie_that_may_not_be_signalled_has_ended(void **state)
{
        const pid_t self = getpid();
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(self);
        /* another user's zombie, say: kill() gives EPERM, and its stat file,
         * read for real, says Z with no thread but the leader
         */
        make_zombie(pid);
        g_ended = pid;
        g_no_signal = pid;

        assert_int_equal(setup(), 0);
        assert_true(output_has_text("PID %d ended before its monitoring "
                                    "could start, so it is left out",
                                    (int)pid));
        assert_int_equal(sel_monitor_num, 1);
        assert_int_equal(sel_monitor_group[0].pids[0], self);
}

static void
test_a_stat_read_that_fails_tells_nothing(void **state)
{
        const pid_t pid = running_process();

        (void)state;
        top_pids_pick(pid);
        top_pids_pick(getpid());
        /* a zombie's stat file as far as a thread count of 1, and then a
         * failed read: what came before the failure is not taken for the file,
         * so the start's failure stands
         */
        g_ended = pid;
        g_cut_short = pid;
        snprintf(g_cut_text, sizeof(g_cut_text),
                 "%d (worker) Z 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 1 ",
                 (int)pid);

        assert_int_equal(setup(), -1);
        assert_true(output_has_text("PID %d monitoring start error", (int)pid));
        assert_false(output_has_text("left out"));
}

static void
test_a_thread_count_that_is_not_a_whole_field_tells_nothing(void **state)
{
        /* where a zombie's stat file ends before its thread count, inside
         * it, and what is not a field of digits of its own
         */
        const char *const counts[] = {"", "1", " 1 ", "-1 "};
        const pid_t pid = running_process();
        unsigned i;

        (void)state;
        g_stat_of = pid;
        for (i = 0; i < DIM(counts); i++) {
                snprintf(g_stat, sizeof(g_stat),
                         "%d (worker) Z 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 "
                         "16 %s",
                         (int)pid, counts[i]);
                assert_int_equal(process_has_ended(pid), 0);
        }
        /* and a whole one tells */
        snprintf(g_stat, sizeof(g_stat),
                 "%d (worker) Z 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 1 ",
                 (int)pid);
        assert_int_equal(process_has_ended(pid), 1);
}

static void
test_top_pids_from_its_option_leaves_the_ended_picks_out(void **state)
{
        unsigned num_picks;

        (void)state;
        /* a child of the case, so that there are two processes to pick even
         * where this one is alone in its PID namespace
         */
        (void)running_process();
        /* the -p option itself: real picks, of the busiest processes */
        run_void_function(selfn_monitor_top_pids);
        assert_false(output_exit_was_called());
        assert_int_equal(sel_mon_top_pids, 1);
        num_picks = sel_monitor_num;
        assert_true(num_picks >= 2);

        /* every pick but the first has ended before its start */
        g_kept = sel_monitor_group[0].pids[0];
        g_others_ended = 1;

        assert_int_equal(setup(), 0);
        assert_int_equal(g_starts, num_picks);
        assert_int_equal(sel_monitor_num, 1);
        assert_int_equal(sel_monitor_group[0].pids[0], g_kept);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup_teardown(test_an_ended_top_pid_is_left_out,
                                            reset, reset),
            cmocka_unit_test_setup_teardown(
                test_a_named_pid_beside_top_pids_stays_an_error, reset, reset),
            cmocka_unit_test_setup_teardown(
                test_a_top_pid_the_user_names_after_is_the_users, reset, reset),
            cmocka_unit_test_setup_teardown(
                test_a_top_pid_the_user_names_as_a_group_is_the_users, reset,
                reset),
            cmocka_unit_test_setup_teardown(
                test_a_named_pid_top_pids_picks_after_is_the_users, reset,
                reset),
            cmocka_unit_test_setup_teardown(
                test_a_named_pid_is_kept_when_top_pids_are_left_out, reset,
                reset),
            cmocka_unit_test_setup_teardown(
                test_a_pid_top_pids_picks_twice_stays_top_pids, reset, reset),
            cmocka_unit_test_setup_teardown(
                test_a_zombie_reaped_before_its_stat_is_read_has_ended, reset,
                reset),
            cmocka_unit_test_setup_teardown(
                test_a_running_top_pid_whose_start_fails_stays_an_error, reset,
                reset),
            cmocka_unit_test_setup_teardown(test_a_zombie_top_pid_has_ended,
                                            reset, reset),
            cmocka_unit_test_setup_teardown(
                test_a_top_pid_named_like_a_zombie_stays_an_error, reset,
                reset),
            cmocka_unit_test_setup_teardown(
                test_a_top_pid_whose_leader_exited_stays_an_error, reset,
                reset),
            cmocka_unit_test_setup_teardown(test_a_dead_top_pid_has_ended,
                                            reset, reset),
            cmocka_unit_test_setup_teardown(
                test_every_top_pid_ended_fails_the_run, reset, reset),
            cmocka_unit_test_setup_teardown(
                test_another_failure_of_an_ended_top_pid_stays_an_error, reset,
                reset),
            cmocka_unit_test_setup_teardown(
                test_the_columns_are_those_of_the_groups_left, reset, reset),
            cmocka_unit_test_setup_teardown(
                test_a_top_pid_hidden_from_proc_stays_an_error, reset, reset),
            cmocka_unit_test_setup_teardown(
                test_a_zombie_reaped_before_its_stat_is_opened_has_ended, reset,
                reset),
            cmocka_unit_test_setup_teardown(
                test_a_zombie_that_may_not_be_signalled_has_ended, reset,
                reset),
            cmocka_unit_test_setup_teardown(
                test_a_stat_read_that_fails_tells_nothing, reset, reset),
            cmocka_unit_test_setup_teardown(
                test_a_thread_count_that_is_not_a_whole_field_tells_nothing,
                reset, reset),
            cmocka_unit_test_setup_teardown(
                test_top_pids_from_its_option_leaves_the_ended_picks_out, reset,
                reset),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
