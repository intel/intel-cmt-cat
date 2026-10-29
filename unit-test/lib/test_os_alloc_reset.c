/*
 * BSD LICENSE
 *
 * Copyright(c) 2020-2026 Intel Corporation. All rights reserved.
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

#include "allocation.h"
#include "mock_cap.h"
#include "mock_resctrl.h"
#include "mock_resctrl_alloc.h"
#include "mock_resctrl_monitoring.h"
#include "mock_resctrl_schemata.h"
#include "os_allocation.h"
#include "resctrl_alloc.h"
#include "test.h"

/* ======== os_alloc_reset_cores ======== */

static void
test_os_alloc_reset_cores(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;
        unsigned i;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        expect_value(__wrap_resctrl_alloc_cpumask_read, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_read, PQOS_RETVAL_OK);

        for (i = 0; i < data->cpu->num_cores; ++i) {
                unsigned lcore = data->cpu->cores[i].lcore;

                expect_value(__wrap_resctrl_cpumask_set, lcore, lcore);
        }

        expect_value(__wrap_resctrl_alloc_cpumask_write, class_id, 0);
        will_return(__wrap_resctrl_alloc_cpumask_write, PQOS_RETVAL_OK);

        ret = os_alloc_reset_cores();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_alloc_reset_schematas ======== */

static void
test_os_alloc_reset_schematas(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const struct pqos_cpuinfo *cpu = data->cpu;
        enum pqos_vendor vendor;
        int ret;
        unsigned grps;
        unsigned i;

        resctrl_alloc_get_grps_num(data->cap, &grps);

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        will_return_maybe(__wrap__pqos_get_cpu, data->cpu);

        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);

        for (i = 0; i < grps; ++i) {
                expect_value(__wrap_resctrl_alloc_schemata_write, class_id, i);
                expect_value(__wrap_resctrl_alloc_schemata_write, technology,
                             (unsigned)PQOS_TECHNOLOGY_ALL);
                will_return(__wrap_resctrl_alloc_schemata_write,
                            PQOS_RETVAL_OK);
        }

        vendor = pqos_get_vendor(cpu);

        if (vendor != PQOS_VENDOR_AMD)
                ret = os_alloc_reset_schematas(&data->cap_l3ca, &data->cap_l2ca,
                                               &data->cap_mba, NULL);
        else
                ret = os_alloc_reset_schematas(&data->cap_l3ca, &data->cap_l2ca,
                                               &data->cap_mba, &data->cap_smba);

        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* ======== os_alloc_reset_tasks ======== */

/* the tasks that the tasks file of a class lists, which a test queues as
 * their number and an array of them: a negative number is a file that cannot
 * be read
 */
/* the reads of a class's tasks file so far, which a case that has a task
 * take the TID of one gone counts from
 */
static unsigned reads;

/* the TID a case has another task take from read reused_from on, 0 for none */
static pid_t reused_tid;
static unsigned reused_from;

/* when a task started: ten times its TID, so that a TID listed again is the
 * same task, unless the case has another task take it since
 */
int
os_alloc_task_start(const pid_t tid, unsigned long long *start)
{
        *start = (unsigned long long)tid * 10;
        if (tid == reused_tid && reads >= reused_from)
                *start += 1;

        return PQOS_RETVAL_OK;
}

static int
tid_back(void **state __attribute__((unused)))
{
        reused_tid = 0;
        reused_from = 0;

        return 0;
}

unsigned *
__wrap_resctrl_alloc_task_read(unsigned class_id, unsigned *count)
{
        const int num = mock_type(int);
        const unsigned *listed = mock_ptr_type(const unsigned *);
        unsigned *tasks;

        reads++;

        check_expected(class_id);
        if (num < 0)
                return NULL;

        tasks = calloc(num + 1, sizeof(*tasks));
        assert_non_null(tasks);
        if (num > 0)
                memcpy(tasks, listed, num * sizeof(*tasks));
        *count = num;

        return tasks;
}

/* the tasks file of \a class_id read, under a shared lock */
static void
expect_class_read(const unsigned class_id, const unsigned *tasks, const int num)
{
        will_return(__wrap_resctrl_lock_shared, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_alloc_task_read, class_id, class_id);
        will_return(__wrap_resctrl_alloc_task_read, num);
        will_return(__wrap_resctrl_alloc_task_read, tasks);
}

/* \a task written to the tasks file of class 0, which answers \a ret, and
 * put back in its monitoring group, "test", if \a monitored and the write
 * succeeded
 */
static void
expect_task_moved(const pid_t task, const int monitored, const int ret)
{
        expect_value(__wrap_resctrl_alloc_task_validate, task, task);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_assoc_get_pid, task, task);
        will_return(__wrap_resctrl_mon_assoc_get_pid,
                    monitored ? PQOS_RETVAL_OK : PQOS_RETVAL_RESOURCE);
        expect_value(__wrap_resctrl_alloc_assoc_set_pid, task, task);
        expect_value(__wrap_resctrl_alloc_assoc_set_pid, class_id, 0);
        will_return(__wrap_resctrl_alloc_assoc_set_pid, ret);
        if (monitored && ret == PQOS_RETVAL_OK) {
                expect_value(__wrap_resctrl_mon_assoc_set_pid, task, task);
                expect_string(__wrap_resctrl_mon_assoc_set_pid, name, "test");
                will_return(__wrap_resctrl_mon_assoc_set_pid, PQOS_RETVAL_OK);
        }
}

/* task \a task written and not moved, and still there when looked at again,
 * as an exiting task is on some kernels
 */
static void
expect_task_not_moved(const pid_t task)
{
        expect_task_moved(task, 0, PQOS_RETVAL_PARAM);
        expect_value(__wrap_resctrl_alloc_task_validate, task, task);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
}

/* the write of task \a value, the TID \a tid, at which another task takes
 * the TID: from that read of the class on, its start time is the other
 * task's
 */
static int
tid_taken_at_write(const LargestIntegralType value,
                   const LargestIntegralType tid)
{
        if (value != tid)
                return 0;
        reused_tid = (pid_t)tid;
        reused_from = reads;
        return 1;
}

/* task \a task written and not moved, and reaped at the write, where
 * another task takes its TID, which is there when looked at again
 */
static void
expect_task_taken_at_write(const pid_t task)
{
        expect_value(__wrap_resctrl_alloc_task_validate, task, task);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_exclusive, PQOS_RETVAL_OK);
        will_return(__wrap_resctrl_lock_release, PQOS_RETVAL_OK);
        expect_value(__wrap_resctrl_mon_assoc_get_pid, task, task);
        will_return(__wrap_resctrl_mon_assoc_get_pid, PQOS_RETVAL_RESOURCE);
        expect_check(__wrap_resctrl_alloc_assoc_set_pid, task,
                     tid_taken_at_write, task);
        expect_value(__wrap_resctrl_alloc_assoc_set_pid, class_id, 0);
        will_return(__wrap_resctrl_alloc_assoc_set_pid, PQOS_RETVAL_PARAM);
        expect_value(__wrap_resctrl_alloc_task_validate, task, task);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_OK);
}

/* the classes from \a first on, read and found to list no task */
static void expect_classes_empty(void **state, const unsigned first);

/* class 2, after one that fails, listing task 21, which is moved all the
 * same, and the classes after it listing none
 */
static void
expect_class_2_reset(void **state)
{
        static const unsigned cos2[] = {21};

        expect_class_read(2, cos2, DIM(cos2));
        expect_task_moved(21, 0, PQOS_RETVAL_OK);
        expect_class_read(2, NULL, 0);
        expect_classes_empty(state, 3);
}

static void
expect_classes_empty(void **state, const unsigned first)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned grps;
        unsigned class_id;

        assert_int_equal(resctrl_alloc_get_grps_num(data->cap, &grps),
                         PQOS_RETVAL_OK);
        for (class_id = first; class_id < grps; class_id++)
                expect_class_read(class_id, NULL, 0);
}

/* the tasks of classes 1 and 2 are moved, a thread as a process is, and the
 * tasks of class 0, which nearly every task on the system is in, are not
 * looked at: no task is validated or written that a class does not list
 */
static void
test_os_alloc_reset_tasks_other_classes(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned cos1[] = {11, 12};
        const unsigned cos2[] = {2101};
        unsigned grps;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        assert_int_equal(resctrl_alloc_get_grps_num(data->cap, &grps),
                         PQOS_RETVAL_OK);
        assert_true(grps >= 3);

        expect_class_read(1, cos1, DIM(cos1));
        expect_task_moved(11, 0, PQOS_RETVAL_OK);
        expect_task_moved(12, 1, PQOS_RETVAL_OK);
        expect_class_read(1, NULL, 0);
        expect_class_read(2, cos2, DIM(cos2));
        expect_task_moved(2101, 0, PQOS_RETVAL_OK);
        expect_class_read(2, NULL, 0);
        expect_classes_empty(state, 3);

        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* no class but the default one has a task: nothing is written */
static void
test_os_alloc_reset_tasks_none(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        expect_classes_empty(state, 1);

        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

/* a task that has exited since the read is skipped, before it is written or
 * after, and the reset goes on. The one gone at the write is the class's last,
 * so that its PARAM, if not cleared, would end class 1 before its second read
 * and fail the reset
 */
static void
test_os_alloc_reset_tasks_exited(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned cos1[] = {11, 13, 12};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        expect_class_read(1, cos1, DIM(cos1));
        expect_value(__wrap_resctrl_alloc_task_validate, task, 11);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_ERROR);
        expect_task_moved(13, 0, PQOS_RETVAL_OK);
        expect_task_moved(12, 0, PQOS_RETVAL_PARAM);
        expect_value(__wrap_resctrl_alloc_task_validate, task, 12);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_ERROR);
        expect_class_read(1, NULL, 0);
        expect_classes_empty(state, 2);

        /* a race the reset takes in its stride, with no message of its own.
         * resctrl_alloc_fclose() logs one for the task gone at the write,
         * which the wrap of resctrl_alloc_assoc_set_pid() leaves out here
         * (PSWD-20371)
         */
        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* a task that task 11 forked after the read, before 11 was moved, is in class
 * 1 too, and not in the list read: the next read of the class lists it, and
 * it is moved, and the read after lists none
 */
static void
test_os_alloc_reset_tasks_forked_since_read(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned read1[] = {11};
        const unsigned read2[] = {12};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        expect_class_read(1, read1, DIM(read1));
        expect_task_moved(11, 0, PQOS_RETVAL_OK);
        expect_class_read(1, read2, DIM(read2));
        expect_task_moved(12, 0, PQOS_RETVAL_OK);
        expect_class_read(1, NULL, 0);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* a class whose every read lists a task forked since the one before keeps it
 * from being emptied: after ten passes that each moved one, the reset fails it,
 * which it says once, and moves on to the classes after it, then fails
 */
static void
test_os_alloc_reset_tasks_class_never_empty(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        static const unsigned forked[] = {11, 12, 13, 14, 15, 16,
                                          17, 18, 19, 20, 21};
        unsigned grps;
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        assert_int_equal(resctrl_alloc_get_grps_num(data->cap, &grps),
                         PQOS_RETVAL_OK);
        /* a class after the one that fails, which the reset still reads */
        assert_true(grps >= 3);

        for (i = 0; i < 10; i++) {
                expect_class_read(1, &forked[i], 1);
                expect_task_moved((pid_t)forked[i], 0, PQOS_RETVAL_OK);
        }
        expect_class_read(1, &forked[10], 1);
        expect_class_2_reset(state);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
}

/* a class that its tenth pass empties is done: ten passes each move a task,
 * the eleventh read lists none, and the reset succeeds
 */
static void
test_os_alloc_reset_tasks_class_emptied_by_its_tenth_pass(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        static const unsigned forked[] = {11, 12, 13, 14, 15,
                                          16, 17, 18, 19, 20};
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        for (i = 0; i < DIM(forked); i++) {
                expect_class_read(1, &forked[i], 1);
                expect_task_moved((pid_t)forked[i], 0, PQOS_RETVAL_OK);
        }
        expect_class_read(1, NULL, 0);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* a task the tenth pass cannot move is written once more, past the limit:
 * nine passes each move a task, the tenth cannot move task 20, and the
 * eleventh read, which lists only it, has its write fail again, which ends
 * the class, and the reset succeeds
 */
static void
test_os_alloc_reset_tasks_task_kept_by_the_tenth_pass(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        static const unsigned forked[] = {11, 12, 13, 14, 15, 16, 17, 18, 19};
        static const unsigned kept[] = {20};
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        for (i = 0; i < DIM(forked); i++) {
                expect_class_read(1, &forked[i], 1);
                expect_task_moved((pid_t)forked[i], 0, PQOS_RETVAL_OK);
        }
        expect_class_read(1, kept, DIM(kept));
        expect_task_not_moved(20);
        expect_class_read(1, kept, DIM(kept));
        expect_task_not_moved(20);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the write past the limit is the last: of the two tasks the tenth pass
 * could not move, the same two by their start times, one moves at that
 * write, and the read after, which lists only the other, fails the class,
 * which is said once
 */
static void
test_os_alloc_reset_tasks_limit_after_the_kept_tasks_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        static const unsigned forked[] = {11, 12, 13, 14, 15, 16, 17, 18, 19};
        static const unsigned kept[] = {20, 22};
        static const unsigned after[] = {22};
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        for (i = 0; i < DIM(forked); i++) {
                expect_class_read(1, &forked[i], 1);
                expect_task_moved((pid_t)forked[i], 0, PQOS_RETVAL_OK);
        }
        expect_class_read(1, kept, DIM(kept));
        expect_task_not_moved(20);
        expect_task_not_moved(22);
        expect_class_read(1, kept, DIM(kept));
        expect_task_moved(20, 0, PQOS_RETVAL_OK);
        expect_task_not_moved(22);
        expect_class_read(1, after, DIM(after));
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
}

/* the limit is for each class: class 1 needs six passes and class 2 six more,
 * twelve in all, and the reset succeeds
 */
static void
test_os_alloc_reset_tasks_passes_count_per_class(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        static const unsigned forked1[] = {11, 12, 13, 14, 15, 16};
        static const unsigned forked2[] = {21, 22, 23, 24, 25, 26};
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        for (i = 0; i < DIM(forked1); i++) {
                expect_class_read(1, &forked1[i], 1);
                expect_task_moved((pid_t)forked1[i], 0, PQOS_RETVAL_OK);
        }
        expect_class_read(1, NULL, 0);
        for (i = 0; i < DIM(forked2); i++) {
                expect_class_read(2, &forked2[i], 1);
                expect_task_moved((pid_t)forked2[i], 0, PQOS_RETVAL_OK);
        }
        expect_class_read(2, NULL, 0);
        expect_classes_empty(state, 3);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the only task listed goes at its write (PARAM), and is gone when looked at
 * again, after it forked a child into the class: the class is read again, and
 * the child, listed then, is moved
 */
static void
test_os_alloc_reset_tasks_task_gone_at_write_reads_class_again(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned read1[] = {11};
        const unsigned read2[] = {12};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        expect_class_read(1, read1, DIM(read1));
        expect_task_moved(11, 0, PQOS_RETVAL_PARAM);
        expect_value(__wrap_resctrl_alloc_task_validate, task, 11);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_ERROR);
        expect_class_read(1, read2, DIM(read2));
        expect_task_moved(12, 0, PQOS_RETVAL_OK);
        expect_class_read(1, NULL, 0);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the only task listed has gone before its write, after it forked a child
 * into the class: the class is read again, and the child, listed then, is
 * moved
 */
static void
test_os_alloc_reset_tasks_gone_task_reads_class_again(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned read1[] = {11};
        const unsigned read2[] = {12};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        expect_class_read(1, read1, DIM(read1));
        expect_value(__wrap_resctrl_alloc_task_validate, task, 11);
        will_return(__wrap_resctrl_alloc_task_validate, PQOS_RETVAL_ERROR);
        expect_class_read(1, read2, DIM(read2));
        expect_task_moved(12, 0, PQOS_RETVAL_OK);
        expect_class_read(1, NULL, 0);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* a task that cannot be moved is left as it is: the class is read again, the
 * read lists only it, and its write fails again, which ends the class, so
 * that the task cannot fail the reset
 */
static void
test_os_alloc_reset_tasks_unmovable_task_ends_class(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned cos1[] = {11};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        expect_class_read(1, cos1, DIM(cos1));
        expect_task_not_moved(11);
        expect_class_read(1, cos1, DIM(cos1));
        expect_task_not_moved(11);
        expect_classes_empty(state, 2);

        /* no message of the reset's own. resctrl_alloc_fclose() logs one for
         * the ESRCH at the write, which the wrap of
         * resctrl_alloc_assoc_set_pid() leaves out here (PSWD-20371)
         */
        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the only task listed cannot be moved, as a zombie cannot on some kernels,
 * after it forked a child into the class: the class is read again, the child,
 * listed then and first, is moved, and the read after lists only the zombie,
 * whose write fails again, which ends the class
 */
static void
test_os_alloc_reset_tasks_unmovable_task_reads_class_again(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned read1[] = {11};
        const unsigned read2[] = {12, 11};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        expect_class_read(1, read1, DIM(read1));
        expect_task_not_moved(11);
        expect_class_read(1, read2, DIM(read2));
        expect_task_moved(12, 0, PQOS_RETVAL_OK);
        expect_task_not_moved(11);
        expect_class_read(1, read1, DIM(read1));
        expect_task_not_moved(11);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the only task listed cannot be moved, nor can the child it forked into the
 * class, which exits too: the read that lists only the child, which the pass
 * before did not keep, is not the end of the class, and the read after it,
 * whose write fails again, is
 */
static void
test_os_alloc_reset_tasks_unmovable_child_reads_class_again(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned read1[] = {11};
        const unsigned read2[] = {12};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        expect_class_read(1, read1, DIM(read1));
        expect_task_not_moved(11);
        expect_class_read(1, read2, DIM(read2));
        expect_task_not_moved(12);
        expect_class_read(1, read2, DIM(read2));
        expect_task_not_moved(12);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the task listed could not be moved, and is reaped; the next read lists a
 * task that has its TID since, a live one forked into the class, which its
 * start time tells from the task kept, and which the write moves: the class
 * is read again, and lists none
 */
static void
test_os_alloc_reset_tasks_task_with_a_kept_tid_is_moved(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned cos1[] = {11};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        reads = 0;
        reused_tid = 11;
        reused_from = 2;

        expect_class_read(1, cos1, DIM(cos1));
        expect_task_not_moved(11);
        expect_class_read(1, cos1, DIM(cos1));
        expect_task_moved(11, 0, PQOS_RETVAL_OK);
        expect_class_read(1, NULL, 0);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* task 11 cannot be moved, is reaped, and another task takes its TID before
 * the next read, then forks a child into the class and cannot be moved either:
 * the read that lists TID 11 again is not of the task kept, by its start time,
 * so that the class is read again, and the child is moved
 */
static void
test_os_alloc_reset_tasks_kept_tid_taken_by_another_task(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned read1[] = {11};
        const unsigned read3[] = {11, 12};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        reads = 0;
        reused_tid = 11;
        reused_from = 2;

        expect_class_read(1, read1, DIM(read1));
        expect_task_not_moved(11);
        expect_class_read(1, read1, DIM(read1));
        expect_task_not_moved(11);
        expect_class_read(1, read3, DIM(read3));
        expect_task_not_moved(11);
        expect_task_moved(12, 0, PQOS_RETVAL_OK);
        expect_class_read(1, read1, DIM(read1));
        expect_task_not_moved(11);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* task 11 cannot be moved, and is reaped at its write, where another task
 * forked into the class takes its TID: the task is not kept, as the start
 * time after the write is not the one before it, so the next read, which
 * lists TID 11 again, has a pass of its own. The other task cannot be moved
 * either, and forks a child into the class at its write, which the read
 * after lists, and moves
 */
static void
test_os_alloc_reset_tasks_tid_taken_at_the_write(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned read1[] = {11};
        const unsigned read3[] = {11, 12};
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        expect_class_read(1, read1, DIM(read1));
        expect_task_taken_at_write(11);
        expect_class_read(1, read1, DIM(read1));
        expect_task_not_moved(11);
        expect_class_read(1, read3, DIM(read3));
        expect_task_not_moved(11);
        expect_task_moved(12, 0, PQOS_RETVAL_OK);
        expect_class_read(1, read1, DIM(read1));
        expect_task_not_moved(11);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* the tasks a pass keeps are looked up by TID whatever order a read lists
 * them in: a read that lists the three kept, in another order, has their
 * writes fail again, which ends the class
 */
static void
test_os_alloc_reset_tasks_kept_tasks_in_another_order(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned read1[] = {15, 11, 13};
        const unsigned read2[] = {13, 15, 11};
        unsigned i;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);

        expect_class_read(1, read1, DIM(read1));
        for (i = 0; i < DIM(read1); i++)
                expect_task_not_moved((pid_t)read1[i]);
        expect_class_read(1, read2, DIM(read2));
        for (i = 0; i < DIM(read2); i++)
                expect_task_not_moved((pid_t)read2[i]);
        expect_classes_empty(state, 2);

        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(test_log_count_stop(), 0);
}

/* a class whose tasks cannot be read fails the reset, which is said once,
 * after the classes after it are reset
 */
static void
test_os_alloc_reset_tasks_unreadable(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned grps;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        assert_int_equal(resctrl_alloc_get_grps_num(data->cap, &grps),
                         PQOS_RETVAL_OK);
        /* a class after the one that fails, which the reset still reads */
        assert_true(grps >= 3);

        expect_class_read(1, NULL, -1);
        expect_class_2_reset(state);

        /* the reset's own message: what fails to read the file, such as
         * resctrl_alloc_fopen(), or the lock, logs one of its own first, which
         * the wraps leave out here
         */
        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
}

/* a write that fails, other than for a task gone or exiting, fails the
 * reset, which is said once: the tasks of its class after it are left, and
 * the classes after it are reset
 */
static void
test_os_alloc_reset_tasks_move_fails(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        const unsigned cos1[] = {11, 12};
        unsigned grps;
        int ret;

        will_return_maybe(__wrap__pqos_get_cap, data->cap);
        assert_int_equal(resctrl_alloc_get_grps_num(data->cap, &grps),
                         PQOS_RETVAL_OK);
        /* a class after the one that fails, which the reset still reads */
        assert_true(grps >= 3);

        expect_class_read(1, cos1, DIM(cos1));
        expect_task_moved(11, 0, PQOS_RETVAL_ERROR);
        expect_class_2_reset(state);

        /* the reset's own message: the open, write or close that fails, or
         * the lock, logs one of its own first, which the wraps leave out here
         */
        test_log_count_start();
        ret = os_alloc_reset_tasks();
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
        assert_int_equal(test_log_count_stop(), 1);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_l3ca[] = {};

        const struct CMUnitTest tests_l2ca[] = {};

        const struct CMUnitTest tests_mba[] = {};

        const struct CMUnitTest tests_all[] = {
            cmocka_unit_test(test_os_alloc_reset_cores),
            cmocka_unit_test(test_os_alloc_reset_schematas),
            cmocka_unit_test(test_os_alloc_reset_tasks_other_classes),
            cmocka_unit_test(test_os_alloc_reset_tasks_none),
            cmocka_unit_test(test_os_alloc_reset_tasks_exited),
            cmocka_unit_test(test_os_alloc_reset_tasks_forked_since_read),
            cmocka_unit_test(test_os_alloc_reset_tasks_class_never_empty),
            cmocka_unit_test(test_os_alloc_reset_tasks_passes_count_per_class),
            cmocka_unit_test(
                test_os_alloc_reset_tasks_task_kept_by_the_tenth_pass),
            cmocka_unit_test(
                test_os_alloc_reset_tasks_limit_after_the_kept_tasks_write),
            cmocka_unit_test(
                test_os_alloc_reset_tasks_gone_task_reads_class_again),
            cmocka_unit_test(
                test_os_alloc_reset_tasks_task_gone_at_write_reads_class_again),
            cmocka_unit_test(
                test_os_alloc_reset_tasks_class_emptied_by_its_tenth_pass),
            cmocka_unit_test(
                test_os_alloc_reset_tasks_unmovable_task_ends_class),
            cmocka_unit_test(
                test_os_alloc_reset_tasks_unmovable_task_reads_class_again),
            cmocka_unit_test(
                test_os_alloc_reset_tasks_unmovable_child_reads_class_again),
            cmocka_unit_test_teardown(
                test_os_alloc_reset_tasks_task_with_a_kept_tid_is_moved,
                tid_back),
            cmocka_unit_test_teardown(
                test_os_alloc_reset_tasks_kept_tid_taken_by_another_task,
                tid_back),
            cmocka_unit_test_teardown(
                test_os_alloc_reset_tasks_tid_taken_at_the_write, tid_back),
            cmocka_unit_test(
                test_os_alloc_reset_tasks_kept_tasks_in_another_order),
            cmocka_unit_test(test_os_alloc_reset_tasks_unreadable),
            cmocka_unit_test(test_os_alloc_reset_tasks_move_fails)};

        const struct CMUnitTest tests_unsupported[] = {};

        result += cmocka_run_group_tests(tests_l3ca, test_init_l3ca, test_fini);
        result += cmocka_run_group_tests(tests_l2ca, test_init_l2ca, test_fini);
        result += cmocka_run_group_tests(tests_mba, test_init_mba, test_fini);
        result += cmocka_run_group_tests(tests_all, test_init_all, test_fini);
        result += cmocka_run_group_tests(tests_unsupported,
                                         test_init_unsupported, test_fini);

        return result;
}
