/*
 * BSD LICENSE
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
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
 *
 */

#include "common.h"
#include "log.h"
#include "mock_common.h"
#include "monitoring.h"
#include "perf.h"
#include "perf_monitoring.h"
#include "test.h"
#include "utils.h"

#include <dirent.h> /**< scandir() */
#include <linux/perf_event.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static enum pqos_mon_event rdt_events[] = {
    PQOS_MON_EVENT_L3_OCCUP,
    PQOS_MON_EVENT_TMEM_BW,
    PQOS_MON_EVENT_LMEM_BW,
};

/* ======== helper functions ======== */

static const char *
get_event_name(enum pqos_mon_event event)
{
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                return "llc_occupancy";
        case PQOS_MON_EVENT_LMEM_BW:
                return "local_bytes";
        case PQOS_MON_EVENT_TMEM_BW:
                return "total_bytes";
        default:
                return NULL;
        }
}

static const char *
get_event_path(enum pqos_mon_event event)
{
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                return PERF_MON_EVENTS "/llc_occupancy";
        case PQOS_MON_EVENT_LMEM_BW:
                return PERF_MON_EVENTS "/local_bytes";
        case PQOS_MON_EVENT_TMEM_BW:
                return PERF_MON_EVENTS "/total_bytes";
        default:
                return NULL;
        }
}

static const char *
get_event_scale(enum pqos_mon_event event)
{
        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                return PERF_MON_EVENTS "/llc_occupancy.scale";
        case PQOS_MON_EVENT_LMEM_BW:
                return PERF_MON_EVENTS "/local_bytes.scale";
        case PQOS_MON_EVENT_TMEM_BW:
                return PERF_MON_EVENTS "/total_bytes.scale";
        default:
                return NULL;
        }
}

static void
test_perf_mon_init(enum pqos_mon_event events)
{
        int ret;
        unsigned i;

        for (i = 0; i < DIM(rdt_events); ++i) {
                enum pqos_mon_event evt = rdt_events[i];

                assert_int_equal(perf_mon_is_event_supported(evt), 0);
        }

        expect_string(__wrap_pqos_file_exists, path,
                      "/proc/sys/kernel/perf_event_paranoid");
        will_return(__wrap_pqos_file_exists, 1);

        /* read perf mon type */
        expect_string(__wrap_pqos_fopen, name, PERF_MON_TYPE);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, 0xDEAD);
        expect_function_call(__wrap_pqos_fclose);

        /* list rdt events */
        expect_string(__wrap_scandir, dirp, PERF_MON_EVENTS);
        will_return(__wrap_scandir, events);

        /* read event details */
        for (i = 0; i < DIM(rdt_events); ++i) {
                const enum pqos_mon_event evt = rdt_events[i];

                if ((evt & events) == 0)
                        continue;

                expect_string(__wrap_pqos_fopen, name, get_event_path(evt));
                expect_string(__wrap_pqos_fopen, mode, "r");
                will_return(__wrap_pqos_fopen, 0xDEAD);
                expect_function_call(__wrap_pqos_fclose);

                expect_string(__wrap_pqos_fopen, name, get_event_scale(evt));
                expect_string(__wrap_pqos_fopen, mode, "r");
                will_return(__wrap_pqos_fopen, 0xDEAD);
                expect_function_call(__wrap_pqos_fclose);
        }

        ret = perf_mon_init(NULL, NULL);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        for (i = 0; i < DIM(rdt_events); ++i) {
                enum pqos_mon_event evt = rdt_events[i];

                assert_int_equal(perf_mon_is_event_supported(evt),
                                 (events & evt) != 0);
        }
}

/* ======== mock ======== */

int
__wrap_perf_setup_counter(struct perf_event_attr *attr,
                          const pid_t pid,
                          const int cpu,
                          const int group_fd,
                          const unsigned long flags,
                          int *counter_fd)
{
        int ret;

        check_expected(attr);
        check_expected(pid);
        check_expected(cpu);
        check_expected(group_fd);
        check_expected(flags);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *counter_fd = mock_type(int);

        return ret;
}

int
__wrap_perf_shutdown_counter(int counter_fd)
{
        check_expected(counter_fd);

        return mock_type(int);
}

int
__wrap_perf_read_counter(int counter_fd, uint64_t *value)
{
        int ret;

        check_expected(counter_fd);
        assert_non_null(value);

        ret = mock_type(int);
        if (ret == PQOS_RETVAL_OK)
                *value = mock_type(uint64_t);

        return ret;
}

static int
_perf_mon_init(void **state __attribute__((unused)))
{
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP |
                                    PQOS_MON_EVENT_LMEM_BW |
                                    PQOS_MON_EVENT_TMEM_BW;

        test_perf_mon_init(event);

        return 0;
}

static int
_perf_mon_fini(void **state __attribute__((unused)))
{
        perf_mon_fini();

        return 0;
}

int
__wrap_scandir(const char *restrict dirp,
               struct dirent ***restrict namelist,
               int (*filter)(const struct dirent *) __attribute__((unused)),
               int (*compar)(const struct dirent **, const struct dirent **)
                   __attribute__((unused)))
{
        int ret;

        check_expected(dirp);

        ret = mock_type(int);
        if (ret == 0)
                *namelist = NULL;
        else if (strcmp(dirp, PERF_MON_EVENTS) == 0) {
                unsigned i;
                enum pqos_mon_event events = ret;
                unsigned event_num = 0;

                *namelist = calloc(DIM(rdt_events), sizeof(struct dirent *));

                for (i = 0; i < DIM(rdt_events); ++i) {
                        const enum pqos_mon_event evt = rdt_events[i];
                        const char *name = get_event_name(evt);

                        if ((evt & events) == 0)
                                continue;

                        (*namelist)[event_num] =
                            malloc(sizeof(struct dirent) + strlen(name) + 1);
                        strcpy((*namelist)[event_num]->d_name, name);
                        ++event_num;
                }

                return event_num;
        }

        return ret;
}

char *
__wrap_fgets(char *str, int n, FILE *stream)
{
        char *data;
        int len;

        assert_non_null(stream);
        assert_non_null(str);

        data = mock_ptr_type(char *);
        len = strlen(data);
        if (len == 0)
                return NULL;

        strncpy(str, data, n);

        return str;
}

FILE *
__wrap_pqos_fopen(const char *name, const char *mode)
{
        FILE *fd = mock_ptr_type(FILE *);

        check_expected(name);
        check_expected(mode);

        if (fd != (FILE *)0xDEAD || strcmp(mode, "r") != 0)
                return fd;

        fd = tmpfile();
        assert_non_null(fd);

        if (strcmp(name, PERF_MON_TYPE) == 0) {
                fprintf(fd, "1\n");

        } else if (strcmp(name, PERF_MON_EVENTS "/llc_occupancy") == 0 ||
                   strcmp(name, PERF_MON_EVENTS "/local_bytes") == 0 ||
                   strcmp(name, PERF_MON_EVENTS "/total_bytes") == 0) {
                fprintf(fd, "config=1\n");

        } else if (strcmp(name, PERF_MON_EVENTS "/llc_occupancy.scale") == 0 ||
                   strcmp(name, PERF_MON_EVENTS "/local_bytes.scale") == 0 ||
                   strcmp(name, PERF_MON_EVENTS "/total_bytes.scale") == 0) {
                fprintf(fd, "1\n");
        }

        fseek(fd, 0, SEEK_SET);
        return fd;
}

int
__wrap_pqos_fclose(FILE *fd)
{
        function_called();
        assert_non_null(fd);

        if (fd == (FILE *)0xDEAD)
                return mock_type(int);

        return __real_pqos_fclose(fd);
}

/* ======== perf_mon_init ======== */

static void
test_perf_mon_init_neg(void **state __attribute__((unused)))
{
        int ret;
        unsigned i;

        /* perf monitoring not supported */
        expect_string(__wrap_pqos_file_exists, path,
                      "/proc/sys/kernel/perf_event_paranoid");
        will_return(__wrap_pqos_file_exists, 0);

        ret = perf_mon_init(NULL, NULL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        for (i = 0; i < DIM(rdt_events); ++i)
                assert_int_equal(perf_mon_is_event_supported(rdt_events[i]), 0);

        /* RDT monitoring not supported */
        expect_string(__wrap_pqos_file_exists, path,
                      "/proc/sys/kernel/perf_event_paranoid");
        will_return(__wrap_pqos_file_exists, 1);
        expect_string(__wrap_pqos_fopen, name, PERF_MON_TYPE);
        expect_string(__wrap_pqos_fopen, mode, "r");
        will_return(__wrap_pqos_fopen, NULL);

        ret = perf_mon_init(NULL, NULL);
        assert_int_equal(ret, PQOS_RETVAL_RESOURCE);
        for (i = 0; i < DIM(rdt_events); ++i)
                assert_int_equal(perf_mon_is_event_supported(rdt_events[i]), 0);
        perf_mon_fini();
}

static void
test_perf_mon_init_llc_occupancy(void **state __attribute__((unused)))
{
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP;

        test_perf_mon_init(event);
        perf_mon_fini();
}

static void
test_perf_mon_init_local_bytes(void **state __attribute__((unused)))
{
        enum pqos_mon_event event = PQOS_MON_EVENT_LMEM_BW;

        test_perf_mon_init(event);
        perf_mon_fini();
}

static void
test_perf_mon_init_total_bytes(void **state __attribute__((unused)))
{
        enum pqos_mon_event event = PQOS_MON_EVENT_TMEM_BW;

        test_perf_mon_init(event);
        perf_mon_fini();
}

static void
test_perf_mon_init_all(void **state __attribute__((unused)))
{
        enum pqos_mon_event event = PQOS_MON_EVENT_L3_OCCUP |
                                    PQOS_MON_EVENT_LMEM_BW |
                                    PQOS_MON_EVENT_TMEM_BW;

        test_perf_mon_init(event);
        perf_mon_fini();
}

/* ======== perf_mon_start ======== */

static void
test_perf_mon_start_param(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_perf_ctx ctx;

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        memset(&ctx, 0, sizeof(ctx));
        grp.intl = &intl;
        grp.intl->perf.ctx = &ctx;

        /* No cores or pids selected for monitoring*/
        ret = perf_mon_start(&grp, PQOS_PERF_EVENT_LLC_MISS);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_perf_mon_start_core_event(enum pqos_mon_event event)
{
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {1};
        const unsigned cores_num = DIM(cores);
        struct pqos_mon_perf_ctx ctx[cores_num];

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        memset(ctx, 0, sizeof(struct pqos_mon_perf_ctx) * cores_num);
        grp.intl = &intl;
        grp.num_cores = cores_num;
        grp.cores = cores;
        grp.intl->perf.ctx = ctx;

        expect_not_value(__wrap_perf_setup_counter, attr, 0);
        expect_value(__wrap_perf_setup_counter, pid, -1);
        expect_value(__wrap_perf_setup_counter, cpu, cores[0]);
        expect_value(__wrap_perf_setup_counter, group_fd, -1);
        expect_value(__wrap_perf_setup_counter, flags, 0);
        will_return(__wrap_perf_setup_counter, PQOS_RETVAL_OK);
        will_return(__wrap_perf_setup_counter, 0xDEAD);

        ret = perf_mon_start(&grp, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ctx->fd_llc,
                         event == PQOS_MON_EVENT_L3_OCCUP ? 0xDEAD : 0);
        assert_int_equal(ctx->fd_mbl,
                         event == PQOS_MON_EVENT_LMEM_BW ? 0xDEAD : 0);
        assert_int_equal(ctx->fd_mbt,
                         event == PQOS_MON_EVENT_TMEM_BW ? 0xDEAD : 0);
        assert_int_equal(ctx->fd_llc_misses,
                         event == PQOS_PERF_EVENT_LLC_MISS ? 0xDEAD : 0);
        assert_int_equal(ctx->fd_llc_references,
                         event == PQOS_PERF_EVENT_LLC_REF ? 0xDEAD : 0);
        assert_int_equal(
            ctx->fd_cyc,
            event == (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES ? 0xDEAD : 0);
        assert_int_equal(
            ctx->fd_inst,
            event == (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS ? 0xDEAD
                                                                       : 0);

        expect_value(__wrap_perf_shutdown_counter, counter_fd, 0xDEAD);
        will_return(__wrap_perf_shutdown_counter, PQOS_RETVAL_OK);

        ret = perf_mon_stop(&grp, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_perf_mon_start_core(void **state __attribute__((unused)))
{
        test_perf_mon_start_core_event(PQOS_PERF_EVENT_LLC_MISS);
        test_perf_mon_start_core_event(PQOS_PERF_EVENT_LLC_REF);
        test_perf_mon_start_core_event(
            (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES);
        test_perf_mon_start_core_event(
            (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS);
        test_perf_mon_start_core_event(PQOS_MON_EVENT_L3_OCCUP);
        test_perf_mon_start_core_event(PQOS_MON_EVENT_LMEM_BW);
        test_perf_mon_start_core_event(PQOS_MON_EVENT_TMEM_BW);
}

static void
test_perf_mon_start_core_param(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {1};
        const unsigned cores_num = DIM(cores);
        struct pqos_mon_perf_ctx ctx[cores_num];

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        memset(ctx, 0, sizeof(struct pqos_mon_perf_ctx) * cores_num);
        grp.intl = &intl;
        grp.num_cores = cores_num;
        grp.cores = cores;
        grp.intl->perf.ctx = ctx;

        ret = perf_mon_start(&grp, (enum pqos_mon_event)0xDEAD);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_perf_mon_start_pid_event(enum pqos_mon_event event)
{
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        pid_t pids[] = {1};
        const unsigned pid_num = DIM(pids);
        struct pqos_mon_perf_ctx ctx[pid_num];

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        memset(ctx, 0, sizeof(struct pqos_mon_perf_ctx) * pid_num);
        grp.intl = &intl;
        grp.tid_nr = pid_num;
        grp.tid_map = pids;
        grp.intl->perf.ctx = ctx;

        expect_not_value(__wrap_perf_setup_counter, attr, 0);
        expect_value(__wrap_perf_setup_counter, pid, pids[0]);
        expect_value(__wrap_perf_setup_counter, cpu, -1);
        expect_value(__wrap_perf_setup_counter, group_fd, -1);
        expect_value(__wrap_perf_setup_counter, flags, 0);
        will_return(__wrap_perf_setup_counter, PQOS_RETVAL_OK);
        will_return(__wrap_perf_setup_counter, 0xDEAD);

        ret = perf_mon_start(&grp, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);
        assert_int_equal(ctx->fd_llc,
                         event == PQOS_MON_EVENT_L3_OCCUP ? 0xDEAD : 0);
        assert_int_equal(ctx->fd_mbl,
                         event == PQOS_MON_EVENT_LMEM_BW ? 0xDEAD : 0);
        assert_int_equal(ctx->fd_mbt,
                         event == PQOS_MON_EVENT_TMEM_BW ? 0xDEAD : 0);
        assert_int_equal(ctx->fd_llc_misses,
                         event == PQOS_PERF_EVENT_LLC_MISS ? 0xDEAD : 0);
        assert_int_equal(ctx->fd_llc_references,
                         event == PQOS_PERF_EVENT_LLC_REF ? 0xDEAD : 0);
        assert_int_equal(
            ctx->fd_cyc,
            event == (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES ? 0xDEAD : 0);
        assert_int_equal(
            ctx->fd_inst,
            event == (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS ? 0xDEAD
                                                                       : 0);

        expect_value(__wrap_perf_shutdown_counter, counter_fd, 0xDEAD);
        will_return(__wrap_perf_shutdown_counter, PQOS_RETVAL_OK);

        ret = perf_mon_stop(&grp, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);
}

static void
test_perf_mon_start_pid(void **state __attribute__((unused)))
{
        test_perf_mon_start_pid_event(PQOS_PERF_EVENT_LLC_MISS);
        test_perf_mon_start_pid_event(PQOS_PERF_EVENT_LLC_REF);
        test_perf_mon_start_pid_event(
            (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES);
        test_perf_mon_start_pid_event(
            (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS);
        test_perf_mon_start_pid_event(PQOS_MON_EVENT_L3_OCCUP);
        test_perf_mon_start_pid_event(PQOS_MON_EVENT_LMEM_BW);
        test_perf_mon_start_pid_event(PQOS_MON_EVENT_TMEM_BW);
}

static void
test_perf_mon_start_pid_param(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        pid_t pids[] = {1};
        const unsigned pid_num = DIM(pids);
        struct pqos_mon_perf_ctx ctx[pid_num];

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        memset(ctx, 0, sizeof(struct pqos_mon_perf_ctx) * pid_num);
        grp.intl = &intl;
        grp.tid_nr = pid_num;
        grp.tid_map = pids;
        grp.intl->perf.ctx = ctx;

        ret = perf_mon_start(&grp, (enum pqos_mon_event)0xDEAD);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== perf_mon_stop ======== */

static void
test_perf_mon_stop_param(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_perf_ctx ctx;

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        memset(&ctx, 0, sizeof(ctx));
        grp.intl = &intl;
        grp.intl->perf.ctx = &ctx;

        /* No cores or pids selected for monitoring*/
        ret = perf_mon_stop(&grp, PQOS_PERF_EVENT_LLC_MISS);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_perf_mon_stop_pid_param(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        pid_t pids[] = {1};
        const unsigned pid_num = DIM(pids);
        struct pqos_mon_perf_ctx ctx[pid_num];

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        memset(ctx, 0, sizeof(struct pqos_mon_perf_ctx) * pid_num);
        grp.intl = &intl;
        grp.tid_nr = pid_num;
        grp.tid_map = pids;
        grp.intl->perf.ctx = ctx;

        ret = perf_mon_stop(&grp, (enum pqos_mon_event)0xDEAD);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

/* ======== perf_mon_poll ======== */

static void
test_perf_mon_poll_core_event(enum pqos_mon_event event)
{
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        unsigned cores[] = {1};
        const unsigned cores_num = DIM(cores);
        struct pqos_mon_perf_ctx ctx[cores_num];
        uint64_t value = 0xDEAD;

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        memset(ctx, 0, sizeof(struct pqos_mon_perf_ctx) * cores_num);
        grp.intl = &intl;
        grp.num_cores = cores_num;
        grp.cores = cores;
        grp.intl->perf.ctx = ctx;

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                ctx->fd_llc = 0xDEAD;
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                ctx->fd_mbl = 0xDEAD;
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                ctx->fd_mbt = 0xDEAD;
                break;
        case PQOS_PERF_EVENT_LLC_MISS:
                ctx->fd_llc_misses = 0xDEAD;
                break;
        case PQOS_PERF_EVENT_LLC_REF:
                ctx->fd_llc_references = 0xDEAD;
                break;
        case (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES:
                ctx->fd_cyc = 0xDEAD;
                break;
        case (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS:
                ctx->fd_inst = 0xDEAD;
                break;
        default:
                break;
        }

        expect_value(__wrap_perf_read_counter, counter_fd, 0xDEAD);
        will_return(__wrap_perf_read_counter, PQOS_RETVAL_OK);
        will_return(__wrap_perf_read_counter, value);

        ret = perf_mon_poll(&grp, event);
        assert_int_equal(ret, PQOS_RETVAL_OK);

        switch (event) {
        case PQOS_MON_EVENT_L3_OCCUP:
                assert_int_equal(grp.values.llc, value);
                break;
        case PQOS_MON_EVENT_LMEM_BW:
                assert_int_equal(grp.values.mbm_local, value);
                break;
        case PQOS_MON_EVENT_TMEM_BW:
                assert_int_equal(grp.values.mbm_total, value);
                break;
        case PQOS_PERF_EVENT_LLC_MISS:
                assert_int_equal(grp.values.llc_misses, value);
                break;
        case PQOS_PERF_EVENT_LLC_REF:
#if PQOS_VERSION < 50000
                assert_int_equal(grp.intl->values.llc_references, value);
#else
                assert_int_equal(grp.values.llc_references, value);
#endif
                break;
        case (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES:
                assert_int_equal(grp.values.ipc_unhalted, value);
                break;
        case (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS:
                assert_int_equal(grp.values.ipc_retired, value);
                break;
        default:
                break;
        }
}

static void
test_perf_mon_poll_core(void **state __attribute__((unused)))
{
        test_perf_mon_poll_core_event(PQOS_PERF_EVENT_LLC_MISS);
        test_perf_mon_poll_core_event(PQOS_PERF_EVENT_LLC_REF);
        test_perf_mon_poll_core_event(
            (enum pqos_mon_event)PQOS_PERF_EVENT_CYCLES);
        test_perf_mon_poll_core_event(
            (enum pqos_mon_event)PQOS_PERF_EVENT_INSTRUCTIONS);
        test_perf_mon_poll_core_event(PQOS_MON_EVENT_L3_OCCUP);
        test_perf_mon_poll_core_event(PQOS_MON_EVENT_LMEM_BW);
        test_perf_mon_poll_core_event(PQOS_MON_EVENT_TMEM_BW);
}

static void
test_perf_mon_poll_param(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        struct pqos_mon_perf_ctx ctx;

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        memset(&ctx, 0, sizeof(ctx));
        grp.intl = &intl;
        grp.intl->perf.ctx = &ctx;

        /* No cores or pids selected for monitoring*/
        ret = perf_mon_poll(&grp, PQOS_PERF_EVENT_LLC_MISS);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

static void
test_perf_mon_poll_pid_param(void **state __attribute__((unused)))
{
        int ret;
        struct pqos_mon_data grp;
        struct pqos_mon_data_internal intl;
        pid_t pids[] = {1};
        const unsigned pid_num = DIM(pids);
        struct pqos_mon_perf_ctx ctx[pid_num];

        memset(&grp, 0, sizeof(grp));
        memset(&intl, 0, sizeof(intl));
        memset(ctx, 0, sizeof(struct pqos_mon_perf_ctx) * pid_num);
        grp.intl = &intl;
        grp.tid_nr = pid_num;
        grp.tid_map = pids;
        grp.intl->perf.ctx = ctx;

        ret = perf_mon_poll(&grp, (enum pqos_mon_event)0xDEAD);
        assert_int_equal(ret, PQOS_RETVAL_ERROR);
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests_init[] = {
            cmocka_unit_test(test_perf_mon_init_neg),
            cmocka_unit_test(test_perf_mon_init_llc_occupancy),
            cmocka_unit_test(test_perf_mon_init_local_bytes),
            cmocka_unit_test(test_perf_mon_init_total_bytes),
            cmocka_unit_test(test_perf_mon_init_all),

        };

        const struct CMUnitTest tests_core[] = {
            cmocka_unit_test(test_perf_mon_start_param),
            cmocka_unit_test(test_perf_mon_start_core),
            cmocka_unit_test(test_perf_mon_start_core_param),
            cmocka_unit_test(test_perf_mon_stop_param),
            cmocka_unit_test(test_perf_mon_poll_core),
            cmocka_unit_test(test_perf_mon_poll_param),
        };

        const struct CMUnitTest tests_pid[] = {
            cmocka_unit_test(test_perf_mon_start_pid),
            cmocka_unit_test(test_perf_mon_start_pid_param),
            cmocka_unit_test(test_perf_mon_stop_pid_param),
            cmocka_unit_test(test_perf_mon_poll_pid_param),
        };

        result += cmocka_run_group_tests(tests_init, NULL, _perf_mon_fini);
        result +=
            cmocka_run_group_tests(tests_core, _perf_mon_init, _perf_mon_fini);
        result +=
            cmocka_run_group_tests(tests_pid, _perf_mon_init, _perf_mon_fini);

        return result;
}
