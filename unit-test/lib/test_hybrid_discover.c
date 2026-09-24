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
 *
 */

/**
 * pqos_hybrid_discover() and the topology it falls back to.
 *
 * The capabilities are CPUID's answer, but the processors to read them on come
 * from a topology - and the two sources of that topology fail independently.
 * CPUID's own topology needs leaf 0BH and the cache leaves, which a hypervisor
 * may filter; the operating system's needs neither. These cases drive the real
 * discovery with CPUID answering as such a platform does, and require the
 * operating system to be asked.
 *
 * lcpuid() is wrapped rather than the topology builders: everything the
 * discovery reads goes through it, so one fake platform serves the vendor
 * check, the APIC detection and the hybrid read, and the code under test is the
 * code that ships.
 */

#include "cpuinfo.h"
#include "log.h"
#include "machine.h"
#include "os_cpuinfo.h"
#include "pqos.h"
#include "test.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** "GenuineIntel", as the vendor check reads it out of leaf 0 */
#define VENDOR_EBX 0x756e6547
#define VENDOR_ECX 0x6c65746e
#define VENDOR_EDX 0x49656e69

/** and a vendor string that is none of the three the library knows */
#define FOREIGN_EBX 0x4d4d5658
#define FOREIGN_ECX 0x4d4d5658
#define FOREIGN_EDX 0x4d4d5658

/** How many times the operating system was asked for the topology, and what it
 *  should answer
 */
static unsigned g_os_topology_calls;
static int g_os_topology_available = 1;

/** Whether the fake platform reports itself hybrid in leaf 7 */
static int g_hybrid = 1;

/** and whether it names a vendor the library knows */
static int g_intel = 1;

/**
 * The rendezvous the lifecycle cases below use: the topology read is the middle
 * of a discovery, so the wrapper stops there, lets the other thread do the
 * logger operation that pqos_init() or pqos_fini() would do, and only then goes
 * on. Everything the discovery thread observes is recorded for the main thread
 * to assert on, because a failed assertion belongs where cmocka can see it.
 */
static pthread_mutex_t g_sync_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_sync_cond = PTHREAD_COND_INITIALIZER;
static int g_pause_in_topology; /**< whether to stop there at all */
static int g_reached_topology;  /**< the discovery is stopped there now */
static int g_may_continue;      /**< the other thread is done */
static int g_log_alive_inside;  /**< log_is_initialized() after it */
static unsigned g_messages;     /**< messages the application received */
static int g_discover_ret = PQOS_RETVAL_ERROR;
static struct pqos_hybrid_capabilities *g_discover_cap;

/** and the finalization running against it: what it answered, and whether it
 *  has answered at all yet
 */
static pthread_mutex_t g_fini_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_fini_cond = PTHREAD_COND_INITIALIZER;
static int g_fini_done;
static int g_fini_ret = LOG_RETVAL_ERROR;

/**
 * @brief An application's log callback: it only counts
 */
static void
count_message(void *context, const size_t size, const char *message)
{
        (void)context;
        (void)size;
        (void)message;
        g_messages++;
}

/**
 * @brief A platform whose CPUID carries no topology
 *
 * Leaf 0BH answers with EBX zero at every sub-leaf, which is how CPUID says it
 * has no topology to report - detect_apic_core_masks() then finds no thread
 * level and fails, and with it the CPUID topology builder. Leaf 7 still reports
 * a hybrid processor, so the read has something to find once a topology is
 * available from elsewhere.
 */
void
__wrap_lcpuid(const unsigned leaf,
              const unsigned subleaf,
              struct cpuid_out *out)
{
        memset(out, 0, sizeof(*out));

        switch (leaf) {
        case 0:
                out->eax = 0x28;
                out->ebx = g_intel ? VENDOR_EBX : FOREIGN_EBX;
                out->ecx = g_intel ? VENDOR_ECX : FOREIGN_ECX;
                out->edx = g_intel ? VENDOR_EDX : FOREIGN_EDX;
                break;
        case 7:
                if (subleaf == 0 && g_hybrid)
                        out->edx = 1U << 15; /* hybrid */
                break;
        default:
                /* including 0BH, whose zero EBX is the point of this fake */
                break;
        }
}

#ifdef __linux__
int
__wrap_sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask)
{
        (void)pid;
        CPU_ZERO_S(cpusetsize, mask);
        CPU_SET_S(0, cpusetsize, mask);
        return 0;
}

int
__wrap_sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask)
{
        (void)pid;
        (void)cpusetsize;
        (void)mask;
        return 0;
}

/**
 * @brief The operating system's topology: one processor, or none to be had
 */
struct pqos_cpuinfo *
__wrap_os_cpuinfo_topology(int prepare_for_access)
{
        struct pqos_cpuinfo *cpu;

        g_os_topology_calls++;

        /* a read of the topology must not ask for the open file limit to be
         * raised: that is the caller's process state, not this call's
         */
        assert_int_equal(prepare_for_access, 0);

        if (g_pause_in_topology) {
                pthread_mutex_lock(&g_sync_mutex);
                g_reached_topology = 1;
                pthread_cond_broadcast(&g_sync_cond);
                while (!g_may_continue)
                        pthread_cond_wait(&g_sync_cond, &g_sync_mutex);
                pthread_mutex_unlock(&g_sync_mutex);

                /* the other thread has installed or removed the log by now,
                 * and this is the discovery still in the middle of its work
                 */
                g_log_alive_inside = log_is_initialized();
        }

        if (!g_os_topology_available)
                return NULL;

        cpu = calloc(1, sizeof(*cpu) + sizeof(struct pqos_coreinfo));
        assert_non_null(cpu);
        cpu->mem_size = sizeof(*cpu) + sizeof(struct pqos_coreinfo);
        cpu->num_cores = 1;
        cpu->cores[0].lcore = 0;

        return cpu;
}

static void
test_discover_falls_back_to_the_os_topology(void **state)
{
        struct pqos_hybrid_capabilities *cap = NULL;

        g_os_topology_calls = 0;
        g_os_topology_available = 1;

        /* CPUID has no topology to give, so the discovery has to ask the
         * operating system - and answer from what it gets back
         */
        assert_int_equal(pqos_hybrid_discover(&cap), PQOS_RETVAL_OK);
        assert_non_null(cap);
        assert_int_equal(g_os_topology_calls, 1);
        assert_int_equal(cap->status, PQOS_HYBRID_STATUS_YES);
        assert_int_equal(cap->num_cores, 1);
        assert_int_equal(cap->cores[0].lcore, 0);

        /* the cache description CPUID never gave is reported as absent rather
         * than as zeroes that look measured
         */
        pqos_hybrid_free(cap);
        (void)state;
}

static void
test_discover_answers_no_without_the_per_core_array(void **state)
{
        struct pqos_hybrid_capabilities *cap = NULL;

        g_os_topology_calls = 0;
        g_os_topology_available = 1;
        g_hybrid = 0;

        /* a processor that is not hybrid is answered from one read, and the
         * capability handed back is the header alone - the per processor array
         * is fifteen hundred bytes each and nothing would be in it
         */
        assert_int_equal(pqos_hybrid_discover(&cap), PQOS_RETVAL_OK);
        assert_non_null(cap);
        assert_int_equal(cap->status, PQOS_HYBRID_STATUS_NO);
        assert_int_equal(cap->num_cores, 0);
        assert_int_equal(cap->mem_size, sizeof(*cap));

        pqos_hybrid_free(cap);
        g_hybrid = 1;
        (void)state;
}

/**
 * @brief Runs one discovery, for a thread of its own
 */
static void *
discover_thread(void *arg)
{
        (void)arg;
        g_discover_ret = pqos_hybrid_discover(&g_discover_cap);

        return NULL;
}

/**
 * @brief Starts a discovery and waits until it is stopped in the topology read
 */
static pthread_t
start_paused_discovery(void)
{
        pthread_t thread;

        g_pause_in_topology = 1;
        g_reached_topology = 0;
        g_may_continue = 0;
        g_log_alive_inside = -1;
        g_messages = 0;
        g_discover_ret = PQOS_RETVAL_ERROR;
        g_discover_cap = NULL;

        assert_int_equal(pthread_create(&thread, NULL, discover_thread, NULL),
                         0);

        pthread_mutex_lock(&g_sync_mutex);
        while (!g_reached_topology)
                pthread_cond_wait(&g_sync_cond, &g_sync_mutex);
        pthread_mutex_unlock(&g_sync_mutex);

        return thread;
}

/**
 * @brief Lets the stopped discovery finish, and waits for it
 */
static void
finish_paused_discovery(pthread_t thread)
{
        pthread_mutex_lock(&g_sync_mutex);
        g_may_continue = 1;
        pthread_cond_broadcast(&g_sync_cond);
        pthread_mutex_unlock(&g_sync_mutex);

        assert_int_equal(pthread_join(thread, NULL), 0);
        g_pause_in_topology = 0;
}

static void
test_an_initialization_during_discovery_is_untouched(void **state)
{
        pthread_t thread;

        while (log_is_initialized())
                log_fini();

        g_os_topology_available = 1;

        /* a discovery with no log runs and is stopped in the middle of its
         * work; the library is then initialized from another thread, which
         * installs the application's log - exactly what pqos_init() does with
         * it, without the hardware. The discovery must install nothing, so
         * there is nothing of its own for it to take away with it
         */
        thread = start_paused_discovery();

        /* stopped in the middle of its work, and there is still no log: nothing
         * of this call's is installed in a process-wide object it does not own
         */
        assert_int_equal(log_is_initialized(), 0);

        assert_int_equal(log_init(-1, count_message, NULL, LOG_VER_DEFAULT),
                         LOG_RETVAL_OK);
        finish_paused_discovery(thread);

        assert_int_equal(g_discover_ret, PQOS_RETVAL_OK);
        pqos_hybrid_free(g_discover_cap);

        /* the application's log is there and receiving, and it received nothing
         * from the discovery that ran across it
         */
        assert_int_equal(log_is_initialized(), 1);
        assert_int_equal(g_messages, 0);
        LOG_ERROR("after the discovery\n");
        assert_int_equal(g_messages, 1);

        assert_int_equal(log_fini(), LOG_RETVAL_OK);
        assert_int_equal(log_is_initialized(), 0);
        (void)state;
}

/**
 * @brief Takes the log down, for a thread of its own, and says when it returned
 */
static void *
finalize_thread(void *arg)
{
        int ret = log_fini();

        (void)arg;
        pthread_mutex_lock(&g_fini_mutex);
        g_fini_ret = ret;
        g_fini_done = 1;
        pthread_cond_broadcast(&g_fini_cond);
        pthread_mutex_unlock(&g_fini_mutex);

        return NULL;
}

/**
 * @brief Whether the finalization has returned within \a millis milliseconds
 */
static int
finalization_returned_within(unsigned millis)
{
        struct timespec deadline;
        int done;

        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_nsec += (long)millis * 1000000L;
        deadline.tv_sec += deadline.tv_nsec / 1000000000L;
        deadline.tv_nsec %= 1000000000L;

        pthread_mutex_lock(&g_fini_mutex);
        while (!g_fini_done) {
                if (pthread_cond_timedwait(&g_fini_cond, &g_fini_mutex,
                                           &deadline) == ETIMEDOUT)
                        break;
        }
        done = g_fini_done;
        pthread_mutex_unlock(&g_fini_mutex);

        return done;
}

static void
test_a_finalization_during_discovery_is_not_delayed(void **state)
{
        pthread_t thread;
        pthread_t finalizer;

        while (log_is_initialized())
                log_fini();
        assert_int_equal(log_init(-1, count_message, NULL, LOG_VER_DEFAULT),
                         LOG_RETVAL_OK);

        g_os_topology_available = 1;
        g_fini_done = 0;
        g_fini_ret = LOG_RETVAL_ERROR;

        /* a discovery is stopped in the middle of its work and the library is
         * finalized from another thread. The finalization must not be made to
         * wait for it: pqos_fini() holds the API lock, and a read that may call
         * back into the library cannot be waited for from under it
         */
        thread = start_paused_discovery();
        assert_int_equal(
            pthread_create(&finalizer, NULL, finalize_thread, NULL), 0);
        assert_int_equal(finalization_returned_within(200), 1);
        assert_int_equal(pthread_join(finalizer, NULL), 0);
        assert_int_equal(g_fini_ret, LOG_RETVAL_OK);
        assert_int_equal(log_is_initialized(), 0);

        finish_paused_discovery(thread);
        assert_int_equal(g_discover_ret, PQOS_RETVAL_OK);
        pqos_hybrid_free(g_discover_cap);

        /* and the log the discovery ran beside was never written to by it,
         * before the finalization or after it: the application's callback heard
         * nothing from this call
         */
        assert_int_equal(g_messages, 0);
        (void)state;
}

static void
test_a_vendor_the_library_does_not_know_is_answered(void **state)
{
        struct pqos_hybrid_capabilities *cap = NULL;

        g_os_topology_calls = 0;
        g_os_topology_available = 1;
        g_intel = 0;

        /* an unknown vendor has no cache leaf configuration in this library,
         * and that configuration is what the CPUID topology is built from - not
         * what the operating system's topology needs. The platform must still
         * get its answer: unknown, because the leaves this reads are Intel
         * definitions and another vendor is free to use those bits for
         * something else
         */
        assert_int_equal(pqos_hybrid_discover(&cap), PQOS_RETVAL_OK);
        assert_non_null(cap);
        assert_int_equal(cap->status, PQOS_HYBRID_STATUS_UNKNOWN);
        assert_int_equal(cap->num_cores, 0);
        assert_int_equal(g_os_topology_calls, 1);

        pqos_hybrid_free(cap);
        g_intel = 1;
        (void)state;
}

static void
test_discover_reports_no_topology_at_all(void **state)
{
        struct pqos_hybrid_capabilities *cap = NULL;

        g_os_topology_calls = 0;
        g_os_topology_available = 0;

        /* neither source answers: a resource error, and no capability handed
         * back for a caller to read
         */
        assert_int_equal(pqos_hybrid_discover(&cap), PQOS_RETVAL_RESOURCE);
        assert_null(cap);
        assert_int_equal(g_os_topology_calls, 1);

        g_os_topology_available = 1;
        (void)state;
}
#endif

static void
test_discover_checks_its_parameter(void **state)
{
        assert_int_equal(pqos_hybrid_discover(NULL), PQOS_RETVAL_PARAM);
        pqos_hybrid_free(NULL);
        (void)state;
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_discover_checks_its_parameter),
#ifdef __linux__
            cmocka_unit_test(test_discover_falls_back_to_the_os_topology),
            cmocka_unit_test(
                test_discover_answers_no_without_the_per_core_array),
            cmocka_unit_test(test_discover_reports_no_topology_at_all),
            cmocka_unit_test(
                test_an_initialization_during_discovery_is_untouched),
            cmocka_unit_test(
                test_a_finalization_during_discovery_is_not_delayed),
            cmocka_unit_test(
                test_a_vendor_the_library_does_not_know_is_answered),
#endif
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
