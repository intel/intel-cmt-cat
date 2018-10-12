/*
 * BSD LICENSE
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
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

#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <getopt.h>

#ifdef __linux__
#include <sched.h>
#endif

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/cpuset.h>
#endif

/**
 * MACROS
 */

#ifdef __linux__
#define PAGE_SIZE       (4 * 1024)
#endif

#define MEMCHUNK_SIZE   (PAGE_SIZE * 32 * 1024) /* 128MB chunk */
#define CL_SIZE         (64)

#ifdef DEBUG
#include <assert.h>
#define ALWAYS_INLINE inline
#else
#define assert(x)
#define ALWAYS_INLINE inline __attribute__((always_inline))
#endif

#define MAX_OPTARG_LEN  64

/**
 * DATA STRUCTURES
 */

/**
 * Define read and write types
 */
enum cl_type {
        CL_WRITE_TYPE_INVALID,
        CL_WRITE_TYPE_WB,
        CL_WRITE_TYPE_READ_MOD_WRITE,
        CL_READ_TYPE_WB,
};

/**
 * COMMON DATA
 */

static int stop_loop = 0;

/**
 * UTILS
 */

/**
 * @brief Function to bind thread to a cpu
 *
 * @param cpuid cpu to bind thread to
 */
static void set_thread_affinity(const unsigned cpuid)
{
#ifdef __linux__
        cpu_set_t cpuset;
#endif
#ifdef __FreeBSD__
        cpuset_t cpuset;
#endif
        int res = -1;

        CPU_ZERO(&cpuset);
        CPU_SET((int)cpuid, &cpuset);

#ifdef __linux__
        res = sched_setaffinity(0, sizeof(cpuset), &cpuset);
#endif
#ifdef __FreeBSD__
        res = cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                                 sizeof(cpuset), &cpuset);
#endif
        if (res != 0)
                perror("Error setting core affinity ");

}

/**
 * @brief Function to flush cache
 *
 * @param p line of cache to flush
 */
ALWAYS_INLINE void cl_flush(void *p)
{
        asm volatile("clflush (%0)\n\t"
                     :
                     : "r"(p)
                     : "memory");
}

/**
 * @brief Serialize store operations, prevent reordering of writes
 */
ALWAYS_INLINE void sb(void)
{
        asm volatile("sfence\n\t"
                     :
                     :
                     : "memory");
}

/**
 * @brief Flush memory
 *
 * @param p memory allocated
 * @param s size of memory to flush
 */
ALWAYS_INLINE void
mem_flush(void *p, size_t s)
{
        char *cp = (char *)p;
        size_t i = 0;

        s = s / CL_SIZE; /* mem size in cache lines */
        for (i = 0; i < s; i++)
                cl_flush(&cp[i * CL_SIZE]);
        sb();
}

/**
 * @brief Function to initialize and allocate memory to thread
 *
 * @param s size of memory to allocate to thread
 *
 * @retval p allocated memory
 */
void *malloc_and_init_memory(size_t s)
{
        void *p = NULL;
        int ret;

        ret = posix_memalign(&p, PAGE_SIZE, s - s % PAGE_SIZE);

        if (ret != 0 || p == NULL) {
                printf("ERROR: Failed to allocate %lu bytes\n",
                       (unsigned long) s - s % PAGE_SIZE);
                stop_loop = 1;
                return NULL;
        }

        uint64_t *p64 = (uint64_t *)p;
        size_t s64 = s / sizeof(uint64_t);

        while (s64 > 0) {
                *p64 = (uint64_t) rand();
                p64 += (CL_SIZE / sizeof(uint64_t));
                s64 -= (CL_SIZE / sizeof(uint64_t));
        }

        mem_flush(p, MEMCHUNK_SIZE);
        return p;
}

/**
 * MEMORY OPERATIONS
 */

/**
 * @brief Load XOR writes
 *
 * @param p pointer to memory
 * @param v value to xor with value in memory location and write back
 */
ALWAYS_INLINE void
cl_read_mod_write(void *p, const uint64_t v)
{
        asm volatile("xor %0, (%1)\n\t"
                     "xor %0, 8(%1)\n\t"
                     "xor %0, 16(%1)\n\t"
                     "xor %0, 24(%1)\n\t"
                     "xor %0, 32(%1)\n\t"
                     "xor %0, 40(%1)\n\t"
                     "xor %0, 48(%1)\n\t"
                     "xor %0, 56(%1)\n\t"
                     :
                     : "r"(v), "r"(p)
                     : "memory");
}

/**
 * @brief Perform write operation to specified cache line
 *
 * @param p pointer to memory location to be written
 * @param v value to overwrite memory location
 */
ALWAYS_INLINE void
cl_write(void *p, const uint64_t v)
{
#ifdef __x86_64__
        asm volatile("movq %0, (%1)\n\t"
                     "movq %0, 8(%1)\n\t"
                     "movq %0, 16(%1)\n\t"
                     "movq %0, 24(%1)\n\t"
                     "movq %0, 32(%1)\n\t"
                     "movq %0, 40(%1)\n\t"
                     "movq %0, 48(%1)\n\t"
                     "movq %0, 56(%1)\n\t"
                     :
                     : "r"(v), "r"(p)
                     : "memory");
#else
        asm volatile("movl %0, (%1)\n\t"
                     "movl %0, 4(%1)\n\t"
                     "movl %0, 8(%1)\n\t"
                     "movl %0, 12(%1)\n\t"
                     "movl %0, 16(%1)\n\t"
                     "movl %0, 20(%1)\n\t"
                     "movl %0, 24(%1)\n\t"
                     "movl %0, 28(%1)\n\t"
                     "movl %0, 32(%1)\n\t"
                     "movl %0, 36(%1)\n\t"
                     "movl %0, 40(%1)\n\t"
                     "movl %0, 44(%1)\n\t"
                     "movl %0, 48(%1)\n\t"
                     "movl %0, 52(%1)\n\t"
                     "movl %0, 56(%1)\n\t"
                     "movl %0, 64(%1)\n\t"
                     :
                     : "r"(v), "r"(p)
                     : "memory");
#endif
}

/**
 * @brief Function to perform read operation to specified memory location
 *
 * @param p pointer to memory location to read from
 */
ALWAYS_INLINE void
cl_read(void *p)
{
        register uint64_t v = 0;
#ifdef __x86_64__
        asm volatile("movq (%1), %0\n\t"
                     "movq 8(%1), %0\n\t"
                     "movq 16(%1), %0\n\t"
                     "movq 24(%1), %0\n\t"
                     "movq 32(%1), %0\n\t"
                     "movq 40(%1), %0\n\t"
                     "movq 48(%1), %0\n\t"
                     "movq 56(%1), %0\n\t"
                     :
                     : "r"(v), "r"(p)
                     : "memory");
#else
        asm volatile("movl (%1), %0\n\t"
                     "movl 4(%1), %0\n\t"
                     "movl 8(%1), %0\n\t"
                     "movl 12(%1), %0\n\t"
                     "movl 16(%1), %0\n\t"
                     "movl 20(%1), %0\n\t"
                     "movl 24(%1), %0\n\t"
                     "movl 28(%1), %0\n\t"
                     "movl 32(%1), %0\n\t"
                     "movl 36(%1), %0\n\t"
                     "movl 40(%1), %0\n\t"
                     "movl 44(%1), %0\n\t"
                     "movl 48(%1), %0\n\t"
                     "movl 52(%1), %0\n\t"
                     "movl 56(%1), %0\n\t"
                     "movl 64(%1), %0\n\t"
                     :
                     : "r"(v), "r"(p)
                     : "memory");
#endif
}

/**
 * @brief Function to find selected operation and execute it
 *
 * @param p memory allocated
 * @param s size of memory
 * @param bw amount of bandwidth
 * @param type operation type to perform on core
 */
ALWAYS_INLINE void
mem_execute(void *p, size_t s, const unsigned bw, const enum cl_type type)
{
        const uint64_t val = (uint64_t) rand();
        char *cp = (char *)p;
        unsigned i = 0;
        size_t n = 0;

        s = s / CL_SIZE; /* mem size in cache lines */

        for (n = 0, i = 0; i < bw; i++) {
                char *ptr = &cp[n * CL_SIZE];

                switch (type) {
                case CL_WRITE_TYPE_WB:
                        cl_write(ptr, val);
                        break;
                case CL_WRITE_TYPE_READ_MOD_WRITE:
                        cl_read_mod_write(ptr, val);
                        break;
                case CL_READ_TYPE_WB:
                        cl_read(ptr);
                        break;
                default:
                        assert(0);
                        break;
                }
                if (++n >= s)
                        n = 0;
        }
        sb();
}

/**
 * MAIN
 */

 /**
 * @brief Function to print Membw command line usage
 *
 * @param argv list of arguments supplied by user
 */
static void usage(char **argv)
{
        printf("Usage: %s -c <cpuid> -b <BW [MB/s]>"
               " (-w | -r | -m)\n"
               "Operation types:\n"
               "  -w, --write          write - x86 stores\n"
               "  -r, --read           read - x86 loads\n"
               "  -m, --read-mod-write read-mod-write - load XOR write\n",
               argv[0]);
}

/**
 * @brief Calculate microseconds to the nearest measurement interval
 *
 * @param tv_s start time of memory operation
 * @param tv_e end time of memory operation
 *
 * @retval long time taken to execute operation
 */
static long get_usec_diff(struct timeval *tv_s, struct timeval *tv_e)
{
        long usec_start, usec_end = 0;

        usec_start = ((long)tv_s->tv_usec) + ((long)tv_s->tv_sec*1000000L);
        usec_end = ((long)tv_e->tv_usec) + ((long)tv_e->tv_sec*1000000L);

        return usec_end - usec_start;
}

/**
 * @brief Sleep before executing operation
 *
 * @param usec_diff time taken to execute operation
 * @param interval maximum time operation should take
 */
static void nano_sleep(const long interval, long usec_diff)
{
        struct timespec req, rem;

        memset(&rem, 0, sizeof(rem));
        memset(&req, 0, sizeof(req));
        req.tv_sec = (interval - usec_diff) / 1000000L;
        req.tv_nsec = ((interval - usec_diff) % 1000000L) * 1000L;
        if (nanosleep(&req, &rem) == -1) {
                req = rem;
                memset(&rem, 0, sizeof(rem));
                nanosleep(&req, &rem);
        }
}

/**
 * @brief Converts string str to UINT
 *
 * @param [in] str string
 * @param [in] base numerical base
 * @param [out] value UINT value
 *
 * @return number of parsed characters
 * @retval positive on success
 * @retval negative on error (-errno)
 */
static int
str_to_uint(const char *str, const unsigned base, unsigned *value)
{
        const char *str_start = str;
        char *str_end = NULL;

        if (NULL == str || NULL == value)
                return -EINVAL;

        while (isblank(*str_start))
                str_start++;

        if (base == 10 && !isdigit(*str_start))
                return -EINVAL;

        if (base == 16 && !isxdigit(*str_start))
                return -EINVAL;

        errno = 0;
        *value = strtoul(str_start, &str_end, base);
        if (errno != 0 || str_end == NULL || str_end == str_start)
                return -EINVAL;

        return str_end - str;
}

int main(int argc, char **argv)
{
        int cmd = EXIT_SUCCESS;
        enum cl_type type = CL_WRITE_TYPE_INVALID;
        unsigned mem_bw = 0;
        unsigned cpuid = UINT_MAX;
        void *private = NULL;
        int option_index;
        int ret;

        struct option options[] = {
            {"cpuid",          required_argument, 0, 'c'},
            {"bandwidth",      required_argument, 0, 'b'},
            {"write",          no_argument,       0, 'w'},
            {"read",           no_argument,       0, 'r'},
            {"read-mod-write", no_argument,       0, 'm'},
            {0, 0, 0, 0}
        };

        /* Process command line arguments */
        while ((cmd = getopt_long(argc, argv, ":c:b:wrm", options,
                &option_index)) != -1) {
                switch (cmd) {
                case 'c':
                        ret = str_to_uint(optarg, 10, &cpuid);
                        if (ret != (int)strnlen(optarg, MAX_OPTARG_LEN)) {
                                printf("Invalid cpuid specified!\n");
                                return EXIT_FAILURE;
                        }
                        break;
                case 'b':
                        ret = str_to_uint(optarg, 10, &mem_bw);
                        if (ret != (int)strnlen(optarg, MAX_OPTARG_LEN)
                                        || !mem_bw) {
                                printf("Invalid B/W specified!\n");
                                return EXIT_FAILURE;
                        }
                        break;
                case 'w':
                        type = CL_WRITE_TYPE_WB;
                        break;
                case 'r':
                        type = CL_READ_TYPE_WB;
                        break;
                case 'm':
                        type = CL_WRITE_TYPE_READ_MOD_WRITE;
                        break;
                default:
                        usage(argv);
                        return EXIT_FAILURE;
                        break;
                }
        }

        /* Check if user has supplied all required arguments */
        if (type == CL_WRITE_TYPE_INVALID || cpuid == UINT_MAX || !mem_bw
                        || optind < argc) {
                usage(argv);
                return EXIT_FAILURE;
        }

        printf("- THREAD logical core id: %u, "
               " memory bandwidth [MB]: %u, starting...\n",
               cpuid, mem_bw);

        /* Bind thread to cpu */
        set_thread_affinity(cpuid);

        /* Allocate memory */
        private = malloc_and_init_memory(MEMCHUNK_SIZE);
        if (private == NULL) {
                printf("Failed to allocate memory!\n");
                return EXIT_FAILURE;
        }

        /* Calculate memory bandwidth to use */
        mem_bw *= (((1024 * 1024) / CL_SIZE));

        /* Stress memory bandwidth */
        while (stop_loop == 0) {

                struct timeval tv_s, tv_e;
                long usec_diff = 0;
                const long interval = 1000000L; /* 1s interval in [us] */

                /* Get time before executing operation in loop */
                gettimeofday(&tv_s, NULL);

                /* Execute operation */
                mem_execute(private, MEMCHUNK_SIZE, mem_bw, type);

                /* Get time after executing operation */
                gettimeofday(&tv_e, NULL);

                usec_diff = get_usec_diff(&tv_s, &tv_e);

                if (usec_diff < interval) {
                        /* Sleep before executing operation again */
                        nano_sleep(interval, usec_diff);
                }

        }

        /* Terminate thread */
        free(private);
        printf("\nexiting...\n");

        return 0;
}
