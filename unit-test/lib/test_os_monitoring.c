/*
 * BSD LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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

#include "monitoring.h"
#include "os_monitoring.h"
#include "test.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void __real_free(void *ptr);

/* ======== mock ======== */

/* A core group's start made in a child process and cancelled there - the
 * child's one thread has cmocka's state, which is each thread's own - and what
 * the child shows its parent, in a page both share. NULL outside those cases.
 */
struct cancelled_start {
        struct pqos_mon_data *group; /**< the group, as the child made it */
        /** its internal data, where it is made apart from the group */
        struct pqos_mon_data_internal *intl;
        unsigned *cores;    /**< its cores, as the start made them */
        int cancel_in_wait; /**< a wait has the thread cancelled */
        unsigned group_freed;
        unsigned intl_freed;
        unsigned cores_freed;
        int said;     /**< the thread was cancelled in a message */
        int returned; /**< the start returned */
};

static struct cancelled_start *cancelled;

void
__wrap_free(void *ptr)
{
        if (cancelled != NULL && ptr != NULL) {
                if (ptr == cancelled->group)
                        cancelled->group_freed++;
                else if (ptr == cancelled->intl)
                        cancelled->intl_freed++;
                else if (ptr == cancelled->cores)
                        cancelled->cores_freed++;
        }
        __real_free(ptr);
}

int
os_mon_stop_events(struct pqos_mon_data *group)
{

        assert_non_null(group);
        check_expected_ptr(group);

        free(group->intl->perf.ctx);
        group->intl->perf.ctx = NULL;

        return mock_type(int);
}

int
os_mon_start_events(struct pqos_mon_data *group)
{
        unsigned num_ctrs = 0;
        int ret;

        assert_non_null(group);
        check_expected_ptr(group);
        if (cancelled != NULL)
                cancelled->cores = group->cores;

        if (group->num_cores > 0)
                num_ctrs = group->num_cores;
        else if (group->tid_nr > 0)
                num_ctrs = group->tid_nr;

        group->intl->perf.ctx =
            calloc(num_ctrs, sizeof(group->intl->perf.ctx[0]));

        /* a start that fails rolls itself back, as the real one does */
        ret = mock_type(int);
        if (ret != PQOS_RETVAL_OK) {
                free(group->intl->perf.ctx);
                group->intl->perf.ctx = NULL;
        }

        return ret;
}

/* the waits between the tries of a start, which take no time here: what each
 * asked for, and the first waits_cut_short of them end half way, as a signal
 * ends them
 */
static unsigned waits;
static long wait_ns[4];
static unsigned waits_cut_short;

int
__wrap_nanosleep(const struct timespec *req, struct timespec *rem)
{
        assert_non_null(req);
        assert_int_equal(req->tv_sec, 0);
        if (waits < DIM(wait_ns))
                wait_ns[waits] = req->tv_nsec;
        waits++;
        if (waits <= waits_cut_short) {
                assert_non_null(rem);
                rem->tv_sec = 0;
                rem->tv_nsec = req->tv_nsec / 2;
                errno = EINTR;
                return -1;
        }
        if (cancelled != NULL && cancelled->cancel_in_wait) {
                /* as another thread would: nanosleep() is a cancellation
                 * point
                 */
                (void)pthread_cancel(pthread_self());
                pthread_testcancel();
        }

        return 0;
}

int
os_mon_tid_exists(const pid_t pid)
{
        return pid != 0xDEAD;
}

int
resctrl_mon_is_supported(void)
{
        return mock_type(int);
}

/* PIDs whose /proc/<pid>/task the scandir() mock fails to read */
#define TASK_DIR_GONE       0xE0D
#define TASK_DIR_UNREADABLE 0xACC

int
__wrap_scandir(const char *restrict dirp,
               struct dirent ***restrict namelist,
               int (*filter)(const struct dirent *) __attribute__((unused)),
               int (*compar)(const struct dirent **, const struct dirent **)
                   __attribute__((unused)))
{
        if (strncmp(dirp, "/proc", 5) == 0) {
                int ret;
                pid_t pid = 0;

                ret = sscanf(dirp, "/proc/%d/task", &pid);
                if (ret != 1 || pid == 0 || pid == 0xDEAD)
                        return -1;
                /* the process ended after it was found to exist */
                if (pid == TASK_DIR_GONE) {
                        errno = ENOENT;
                        return -1;
                }
                /* or its directory cannot be read for another reason */
                if (pid == TASK_DIR_UNREADABLE) {
                        errno = EACCES;
                        return -1;
                }

                *namelist = malloc(sizeof(struct dirent *));
                *namelist[0] = malloc(sizeof(struct dirent) + 10);
                sprintf((*namelist)[0]->d_name, "%d", pid);

                return 1;
        }

        return -1;
}

/* ======== os_mon_init ======== */

/**
 * @brief Test \a os_mon_init function behaviour when perf monitoring
 *        is supported for RDT events
 */
static void
test_os_mon_init_perf(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        expect_value(__wrap_perf_mon_init, cpu, data->cpu);
        expect_value(__wrap_perf_mon_init, cap, data->cap);
        will_return(__wrap_perf_mon_init, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_init, cpu, data->cpu);
        expect_value(__wrap_resctrl_mon_init, cap, data->cap);
        will_return(__wrap_resctrl_mon_init, PQOS_RETVAL_RESOURCE);

        ret = os_mon_init(data->cpu, data->cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/**
 * @brief Test \a os_mon_init function behaviour when resctrl monitoring
 *        is supported for RDT events
 */
static void
test_os_mon_init_resctrl(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        expect_value(__wrap_perf_mon_init, cpu, data->cpu);
        expect_value(__wrap_perf_mon_init, cap, data->cap);
        will_return(__wrap_perf_mon_init, PQOS_RETVAL_RESOURCE);
        expect_value(__wrap_resctrl_mon_init, cpu, data->cpu);
        expect_value(__wrap_resctrl_mon_init, cap, data->cap);
        will_return(__wrap_resctrl_mon_init, PQOS_RETVAL_OK);
        will_return(resctrl_mon_is_supported, 1);

        ret = os_mon_init(data->cpu, data->cap);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/**
 * @brief Test \a os_mon_init function behaviour when RDT monitoring
 *        is not supported by kernel
 */
static void
test_os_mon_init_unsupported(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        expect_value(__wrap_perf_mon_init, cpu, data->cpu);
        expect_value(__wrap_perf_mon_init, cap, data->cap);
        will_return(__wrap_perf_mon_init, PQOS_RETVAL_RESOURCE);
        expect_value(__wrap_resctrl_mon_init, cpu, data->cpu);
        expect_value(__wrap_resctrl_mon_init, cap, data->cap);
        will_return(__wrap_resctrl_mon_init, PQOS_RETVAL_RESOURCE);

        ret = os_mon_init(data->cpu, data->cap);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

/**
 * @brief Test \a os_mon_init function invalid parameters handling
 */
static void
test_os_mon_init_param(void **state)
{
        SKIP_IF_ASSERTS_ARE_LIVE();

        struct test_data *data = (struct test_data *)*state;
        int ret;

        ret = os_mon_init(data->cpu, NULL);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);

        ret = os_mon_init(NULL, data->cap);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== os_mon_fini ======== */

/**
 * @brief Test \a os_mon_init function deinicalizes all sub-modules
 */
static void
test_os_mon_fini(void **state __attribute__((unused)))
{
        int ret;

        will_return(__wrap_perf_mon_fini, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_mon_fini, PQOS_RETVAL_OK);

        ret = os_mon_fini();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_mon_reset ======== */

static void
test_os_mon_reset(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_config cfg;

        memset(&cfg, 0, sizeof(cfg));

        will_return(__wrap_resctrl_mon_reset, PQOS_RETVAL_OK);

        ret = os_mon_reset(&cfg);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_mon_stop ======== */

static void
test_os_mon_stop_core(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        group.num_cores = 1;
        group.cores = calloc(1, sizeof(*group.cores));
        group.cores[0] = 1;

        expect_value(os_mon_stop_events, group, &group);
        will_return(os_mon_stop_events, PQOS_RETVAL_OK);

        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_null(group.cores);
}

static void
test_os_mon_stop_pid(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        group.tid_nr = 1;
        group.tid_map = calloc(1, sizeof(*group.tid_map));
        group.tid_map[0] = 1;

        expect_value(os_mon_stop_events, group, &group);
        will_return(os_mon_stop_events, PQOS_RETVAL_OK);

        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_null(group.tid_map);
}

static void
test_os_mon_stop_param(void **state __attribute__((unused)))
{
        struct pqos_mon_data group;
        int ret;

        memset(&group, 0, sizeof(group));

        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

/* ======== os_mon_start_cores ======== */

static void
test_os_mon_start_cores_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));

        /* invalid event */
        {
                unsigned num_cores = 1;
                unsigned cores[] = {1};
                enum pqos_mon_event event = 0xDEAD;

                ret = os_mon_start_cores(num_cores, cores, event, NULL, NULL,
                                         &group, NULL);
                assert_int_equal(ret, PQOS_RETVAL_PARAM);
        }

        /* invalid core */
        {
                unsigned num_cores = 1;
                unsigned cores[] = {1024};
                enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

                ret = os_mon_start_cores(num_cores, cores, event, NULL, NULL,
                                         &group, NULL);
                assert_int_equal(ret, PQOS_RETVAL_PARAM);
        }
}

static void
test_os_mon_start_cores_already_started(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, cores[0]);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_OK);

        ret = os_mon_start_cores(num_cores, cores, event, NULL, NULL, &group,
                                 NULL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
}

static void
test_os_mon_start_cores(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;
        unsigned num_cores = 1;
        unsigned cores[] = {1};
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, cores[0]);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_RESOURCE);
        expect_value(os_mon_start_events, group, &group);
        will_return(os_mon_start_events, PQOS_RETVAL_OK);

        ret = os_mon_start_cores(num_cores, cores, event, NULL, NULL, &group,
                                 NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.num_cores, num_cores);
        assert_int_equal(group.event, event);
        assert_non_null(group.cores);
        expect_value(os_mon_stop_events, group, &group);
        will_return(os_mon_stop_events, PQOS_RETVAL_OK);
        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, cores[0]);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_RESOURCE);
        expect_value(os_mon_start_events, group, &group);
        will_return(os_mon_start_events, PQOS_RETVAL_ERROR);

        ret = os_mon_start_cores(num_cores, cores, event, NULL, NULL, &group,
                                 NULL);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_null(group.cores);
}

/* the cores of the starts tried below */
static const unsigned tried_cores[] = {1};

/* sets up a core group's start whose tries find every free RMID in limbo:
 * each try finds the cores free again, and its BUSY, which leaves nothing of
 * it, has the start made again after a wait, tries times in all, ending with
 * last
 */
static void
start_cores_set_up(struct test_data *data,
                   struct pqos_mon_data *group,
                   const unsigned tries,
                   const int last)
{
        unsigned i;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        for (i = 0; i < tries; i++) {
                expect_value(__wrap_resctrl_mon_assoc_get, lcore,
                             tried_cores[0]);
                will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_RESOURCE);
                expect_value(os_mon_start_events, group, group);
                will_return(os_mon_start_events,
                            i + 1 < tries ? PQOS_RETVAL_BUSY : last);
        }

        waits = 0;
}

/* makes that start; says how many messages it logged */
static int
start_cores_tried(struct test_data *data,
                  struct pqos_mon_data *group,
                  const unsigned tries,
                  const int last,
                  unsigned *logged)
{
        int ret;

        start_cores_set_up(data, group, tries, last);
        test_log_count_start();
        ret = os_mon_start_cores(DIM(tried_cores), tried_cores,
                                 PQOS_MON_EVENT_L3_OCCUP, NULL, NULL, group,
                                 NULL);
        *logged = test_log_count_stop();

        return ret;
}

static void
test_os_mon_start_cores_busy_then_started(void **state)
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned logged;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        ret = start_cores_tried((struct test_data *)*state, &group, 3,
                                PQOS_RETVAL_OK, &logged);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(waits, 2);
        assert_int_equal(wait_ns[0], 100000000L);
        assert_int_equal(logged, 0);
        assert_non_null(group.cores);

        expect_value(os_mon_stop_events, group, &group);
        will_return(os_mon_stop_events, PQOS_RETVAL_OK);
        assert_int_equal(os_mon_stop(&group), PQOS_RETVAL_OK);
}

static void
test_os_mon_start_cores_busy_throughout(void **state)
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned logged;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        ret = start_cores_tried((struct test_data *)*state, &group, 30,
                                PQOS_RETVAL_BUSY, &logged);
        assert_int_equal(ret, PQOS_RETVAL_BUSY);
        assert_int_equal(waits, 29);
        assert_int_equal(logged, 1);
        assert_null(group.cores);
}

/* a failure other than BUSY is the start's: not tried again */
static void
test_os_mon_start_cores_error_not_tried_again(void **state)
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned logged;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        ret = start_cores_tried((struct test_data *)*state, &group, 1,
                                PQOS_RETVAL_ERROR, &logged);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(waits, 0);
        assert_null(group.cores);
}

/* a core that another process started during a wait is its: the start fails
 * as it would have before its first try, and is not made again
 */
static void
test_os_mon_start_cores_core_taken_during_a_wait(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {1};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        expect_value(__wrap_resctrl_mon_assoc_get, lcore, cores[0]);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_RESOURCE);
        expect_value(os_mon_start_events, group, &group);
        will_return(os_mon_start_events, PQOS_RETVAL_BUSY);
        expect_value(__wrap_resctrl_mon_assoc_get, lcore, cores[0]);
        will_return(__wrap_resctrl_mon_assoc_get, PQOS_RETVAL_OK);

        waits = 0;
        ret = os_mon_start_cores(DIM(cores), cores, PQOS_MON_EVENT_L3_OCCUP,
                                 NULL, NULL, &group, NULL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        assert_int_equal(waits, 1);
        assert_null(group.cores);
}

/* a signal that ends a wait early has the rest of it waited */
static void
test_os_mon_start_cores_wait_after_a_signal(void **state)
{
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        unsigned logged;
        int ret;

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;
        waits_cut_short = 1;

        ret = start_cores_tried((struct test_data *)*state, &group, 2,
                                PQOS_RETVAL_OK, &logged);
        waits_cut_short = 0;
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(waits, 2);
        assert_int_equal(wait_ns[0], 100000000L);
        assert_int_equal(wait_ns[1], 50000000L);

        expect_value(os_mon_stop_events, group, &group);
        will_return(os_mon_stop_events, PQOS_RETVAL_OK);
        assert_int_equal(os_mon_stop(&group), PQOS_RETVAL_OK);
}

/* a log whose message has the thread cancelled, as another thread would one
 * blocked in a write() to a reader that has stopped
 */
static void
cancel_in_the_message(void *context, const size_t size, const char *message)
{
        (void)context;
        (void)size;
        (void)message;
        cancelled->said = 1;
        (void)pthread_cancel(pthread_self());
        pthread_testcancel();
}

/* makes, in a child process, the start start_cores_set_up() sets up, on a
 * group made as pqos_mon_start_cores_ext() makes one, with its internal data
 * in it, or as pqos_mon_start() has a caller's own, with its internal data
 * made apart; the thread is cancelled in a wait or in a message. Returns the
 * child's status, with what it showed in seen
 */
static int
start_cancelled(struct test_data *data,
                const unsigned tries,
                const int last,
                const int library_made,
                const int cancel_in_wait,
                struct cancelled_start *seen)
{
        struct cancelled_start *shared;
        int status = -1;
        pid_t pid;

        shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        assert_true(shared != MAP_FAILED);
        memset(shared, 0, sizeof(*shared));
        shared->cancel_in_wait = cancel_in_wait;
        fflush(stdout);
        fflush(stderr);

        pid = fork();
        assert_true(pid >= 0);
        if (pid == 0) {
                struct pqos_mon_data *group;
                const int null = open("/dev/null", O_WRONLY);

                /* a failure would have cmocka go on with the cases left in
                 * this process too: they print nowhere, and its status says
                 * it
                 */
                if (null >= 0) {
                        (void)dup2(null, STDOUT_FILENO);
                        (void)dup2(null, STDERR_FILENO);
                }
                if (library_made) {
                        group =
                            calloc(1, sizeof(*group) + sizeof(*group->intl));
                        if (group == NULL)
                                _exit(4);
                        group->intl =
                            (struct pqos_mon_data_internal *)&group[1];
                        group->intl->manage_memory = 1;
                } else {
                        group = calloc(1, sizeof(*group));
                        if (group == NULL)
                                _exit(4);
                        group->intl = calloc(1, sizeof(*group->intl));
                        if (group->intl == NULL)
                                _exit(4);
                        shared->intl = group->intl;
                }
                shared->group = group;
                cancelled = shared;
                start_cores_set_up(data, group, tries, last);
                if (!cancel_in_wait)
                        (void)log_init(-1, cancel_in_the_message, NULL,
                                       LOG_VER_DEFAULT);
                (void)os_mon_start_cores(DIM(tried_cores), tried_cores,
                                         PQOS_MON_EVENT_L3_OCCUP, NULL, NULL,
                                         group, NULL);
                shared->returned = 1;
                _exit(3);
        }

        assert_int_equal(waitpid(pid, &status, 0), pid);
        *seen = *shared;
        assert_int_equal(munmap(shared, sizeof(*shared)), 0);

        return status;
}

/* a thread cancelled in a wait between two tries gives the group back, as a
 * failed start does: the try before it has left nothing else. A cancelled
 * thread that is a process's only one ends the process with status 0
 */
static void
test_os_mon_start_cores_cancelled_in_a_wait(void **state)
{
        struct cancelled_start seen;
        int status;

        status = start_cancelled((struct test_data *)*state, 2, PQOS_RETVAL_OK,
                                 1, 1, &seen);
        assert_true(WIFEXITED(status));
        assert_int_equal(WEXITSTATUS(status), 0);
        assert_false(seen.returned);
        assert_int_equal(seen.cores_freed, 1);
        assert_int_equal(seen.group_freed, 1);
}

/* a group of the caller's own, as pqos_mon_start() starts, is the caller's to
 * free; its cores, and the internal data the library made for it apart, are
 * given back
 */
static void
test_os_mon_start_cores_cancelled_in_a_wait_own_group(void **state)
{
        struct cancelled_start seen;
        int status;

        status = start_cancelled((struct test_data *)*state, 2, PQOS_RETVAL_OK,
                                 0, 1, &seen);
        assert_true(WIFEXITED(status));
        assert_int_equal(WEXITSTATUS(status), 0);
        assert_false(seen.returned);
        assert_int_equal(seen.cores_freed, 1);
        assert_int_equal(seen.intl_freed, 1);
        assert_int_equal(seen.group_freed, 0);
}

/* and one cancelled in the message that gives the start up, a write() that
 * another thread can cancel as it can any write the library makes
 */
static void
test_os_mon_start_cores_cancelled_saying_it_gave_up(void **state)
{
        struct cancelled_start seen;
        int status;

        status = start_cancelled((struct test_data *)*state, 30,
                                 PQOS_RETVAL_BUSY, 1, 0, &seen);
        assert_true(WIFEXITED(status));
        assert_int_equal(WEXITSTATUS(status), 0);
        assert_true(seen.said);
        assert_false(seen.returned);
        assert_int_equal(seen.cores_freed, 1);
        assert_int_equal(seen.group_freed, 1);
}

/* ======== os_mon_start_pids ======== */

static void
test_os_mon_start_pids_param(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));

        /* invalid event */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {1};
                enum pqos_mon_event event = 0xDEAD;

                ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
                assert_int_equal(ret, PQOS_RETVAL_PARAM);
        }

        /* invalid pid */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {0xDEAD};
                enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

                ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
                assert_int_equal(ret, PQOS_RETVAL_PARAM);
        }
}

static void
test_os_mon_start_pids(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;
        unsigned num_pids = 1;
        pid_t pids[] = {1};
        void *context = (void *)0xDEAD;
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        expect_value(os_mon_start_events, group, &group);
        will_return(os_mon_start_events, PQOS_RETVAL_OK);

        ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(group.num_pids, num_pids);
        assert_int_equal(group.tid_nr, num_pids);
        assert_int_equal(group.event, event);
        assert_non_null(group.tid_map);
        assert_non_null(group.pids);
        expect_value(os_mon_stop_events, group, &group);
        will_return(os_mon_stop_events, PQOS_RETVAL_OK);
        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        expect_value(os_mon_start_events, group, &group);
        will_return(os_mon_start_events, PQOS_RETVAL_ERROR);

        ret = os_mon_start_pids(num_pids, pids, event, context, &group);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_null(group.tid_map);
        assert_null(group.pids);
        assert_int_equal(group.tid_nr, 0);
        assert_int_equal(group.num_pids, 0);
        assert_int_equal(group.event, 0);
        assert_null(group.context);

        ret = os_mon_stop(&group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
}

static void
test_os_mon_start_pids_task_dir(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t gone = TASK_DIR_GONE;
        pid_t unreadable = TASK_DIR_UNREADABLE;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        /* a process that ended after it was found to exist is one that does
         * not exist, which a caller can tell from a failure
         */
        ret =
            os_mon_start_pids(1, &gone, PQOS_MON_EVENT_L3_OCCUP, NULL, &group);
        assert_int_equal(ret, PQOS_RETVAL_PARAM);
        assert_null(group.tid_map);

        /* and one whose threads cannot be read stays an error */
        ret = os_mon_start_pids(1, &unreadable, PQOS_MON_EVENT_L3_OCCUP, NULL,
                                &group);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_null(group.tid_map);
}

/* a task group's start is not tried again on BUSY: a task found before a
 * wait may have ended, and its ID gone to another, by the next try
 */
static void
test_os_mon_start_pids_busy_not_tried_again(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        pid_t pids[] = {1};
        unsigned logged;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);
        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        expect_value(os_mon_start_events, group, &group);
        will_return(os_mon_start_events, PQOS_RETVAL_BUSY);

        waits = 0;
        test_log_count_start();
        ret = os_mon_start_pids(DIM(pids), pids, PQOS_MON_EVENT_L3_OCCUP, NULL,
                                &group);
        logged = test_log_count_stop();
        assert_int_equal(ret, PQOS_RETVAL_BUSY);
        assert_int_equal(waits, 0);
        assert_int_equal(logged, 1);
        assert_null(group.tid_map);
}

/* ======== os_mon_add_pids ======== */

static void
test_os_mon_add_pids(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        /* init monitoring group */
        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        /* start monitoring */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {1};
                enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

                expect_value(os_mon_start_events, group, &group);
                will_return(os_mon_start_events, PQOS_RETVAL_OK);

                ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
        }

        /* add non-existent */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {0xDEAD};

                ret = os_mon_add_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_PARAM);
                assert_int_equal(group.num_pids, 1);
                assert_int_equal(group.tid_nr, 1);
        }

        /* add old pid */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {1};

                expect_any(os_mon_start_events, group);
                will_return(os_mon_start_events, PQOS_RETVAL_OK);

                ret = os_mon_add_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(group.num_pids, 1);
                assert_int_equal(group.tid_nr, 1);
        }

        /* add new pid */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {2};

                ret = os_mon_add_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(group.num_pids, 2);
                assert_int_equal(group.tid_nr, 2);
        }

        /* add error */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {3};

                expect_any(os_mon_start_events, group);
                will_return(os_mon_start_events, PQOS_RETVAL_ERROR);

                ret = os_mon_add_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_ERROR);
                assert_int_equal(group.num_pids, 2);
                assert_int_equal(group.tid_nr, 2);
        }

        /* a start that fails with RESOURCE, as one finding no RMID left does,
         * has rolled itself back, and is not stopped again: that is for an
         * add whose memory runs out after its start. os_mon_stop_events() is
         * not expected
         */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {4};

                expect_any(os_mon_start_events, group);
                will_return(os_mon_start_events, PQOS_RETVAL_RESOURCE);

                ret = os_mon_add_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
                assert_int_equal(group.num_pids, 2);
                assert_int_equal(group.tid_nr, 2);
        }

        /* stop monitoring */
        {
                expect_value(os_mon_stop_events, group, &group);
                will_return(os_mon_stop_events, PQOS_RETVAL_OK);

                ret = os_mon_stop(&group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_null(group.tid_map);
                assert_null(group.pids);
        }
}

/* ======== os_mon_remove_pids ======== */

static void
test_os_mon_remove_pids(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        struct pqos_mon_data group;
        struct pqos_mon_data_internal intl;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        /* init monitoring group */
        memset(&group, 0, sizeof(group));
        memset(&intl, 0, sizeof(intl));
        group.intl = &intl;

        /* start monitoring */
        {
                unsigned num_pids = 2;
                pid_t pids[] = {1, 2};
                enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

                expect_value(os_mon_start_events, group, &group);
                will_return(os_mon_start_events, PQOS_RETVAL_OK);

                ret = os_mon_start_pids(num_pids, pids, event, NULL, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
        }

        /* remove pids */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {1};

                expect_any(os_mon_stop_events, group);
                will_return(os_mon_stop_events, PQOS_RETVAL_OK);

                ret = os_mon_remove_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(group.num_pids, 1);
                assert_int_equal(group.tid_nr, 1);
        }

        /* remove non-existent */
        {
                unsigned num_pids = 1;
                pid_t pids[] = {3};

                expect_any(os_mon_stop_events, group);
                will_return(os_mon_stop_events, PQOS_RETVAL_OK);

                ret = os_mon_remove_pids(num_pids, pids, &group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_int_equal(group.num_pids, 1);
                assert_int_equal(group.tid_nr, 1);
        }

        /* stop monitoring */
        {
                expect_value(os_mon_stop_events, group, &group);
                will_return(os_mon_stop_events, PQOS_RETVAL_OK);

                ret = os_mon_stop(&group);
                assert_int_equal(ret, PQOS_RETVAL_OK);
                assert_null(group.tid_map);
                assert_null(group.pids);
        }
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_os_mon_init_perf),
            cmocka_unit_test(test_os_mon_init_resctrl),
            cmocka_unit_test(test_os_mon_init_unsupported),
            cmocka_unit_test(test_os_mon_init_param),
            cmocka_unit_test(test_os_mon_fini),
            cmocka_unit_test(test_os_mon_reset),
            cmocka_unit_test(test_os_mon_stop_core),
            cmocka_unit_test(test_os_mon_stop_pid),
            cmocka_unit_test(test_os_mon_stop_param),
            cmocka_unit_test(test_os_mon_start_cores_param),
            cmocka_unit_test(test_os_mon_start_cores_already_started),
            cmocka_unit_test(test_os_mon_start_cores),
            cmocka_unit_test(test_os_mon_start_cores_busy_then_started),
            cmocka_unit_test(test_os_mon_start_cores_busy_throughout),
            cmocka_unit_test(test_os_mon_start_cores_error_not_tried_again),
            cmocka_unit_test(test_os_mon_start_cores_core_taken_during_a_wait),
            cmocka_unit_test(test_os_mon_start_cores_wait_after_a_signal),
            cmocka_unit_test(test_os_mon_start_cores_cancelled_in_a_wait),
            cmocka_unit_test(
                test_os_mon_start_cores_cancelled_in_a_wait_own_group),
            cmocka_unit_test(
                test_os_mon_start_cores_cancelled_saying_it_gave_up),
            cmocka_unit_test(test_os_mon_start_pids_param),
            cmocka_unit_test(test_os_mon_start_pids),
            cmocka_unit_test(test_os_mon_start_pids_busy_not_tried_again),
            cmocka_unit_test(test_os_mon_start_pids_task_dir),
            cmocka_unit_test(test_os_mon_add_pids),
            cmocka_unit_test(test_os_mon_remove_pids),
        };

        result += cmocka_run_group_tests(tests, test_init_mon, test_fini);

        return result;
}
