/*
 * BSD LICENSE
 *
 * Copyright(c) 2018-2020 Intel Corporation. All rights reserved.
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
#include <cpuid.h>
#endif

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/cpuset.h>
#endif

/**
 * MACROS
 */

#ifdef __linux__
#define PAGE_SIZE (4 * 1024)
#endif

#define MEMCHUNK_SIZE (PAGE_SIZE * 32 * 1024) /* 128MB chunk */
#define CL_SIZE       (64)
#define CHUNKS        (128)

#ifdef DEBUG
#include <assert.h>
#define ALWAYS_INLINE static inline
#else
#define assert(x)
#define ALWAYS_INLINE static inline __attribute__((always_inline))
#endif

#define MAX_OPTARG_LEN 64

#define MAX_MEM_BW 100 * 1000 /* 100GBps */

#define CPU_FEATURE_SSE4_2  (1ULL << 0)
#define CPU_FEATURE_CLWB    (1ULL << 1)
#define CPU_FEATURE_AVX512F (1ULL << 2)

/**
 * DATA STRUCTURES
 */

/**
 * Define read and write types
 */
enum cl_type {
        CL_TYPE_INVALID,
        CL_TYPE_PREFETCH_T0,
        CL_TYPE_PREFETCH_T1,
        CL_TYPE_PREFETCH_T2,
        CL_TYPE_PREFETCH_NTA,
        CL_TYPE_PREFETCH_W,
        CL_TYPE_READ_NTQ,
        CL_TYPE_READ_WB,
        CL_TYPE_READ_WB_DQA,
        CL_TYPE_READ_MOD_WRITE,
#ifdef __x86_64__
        CL_TYPE_WRITE_DQA,
        CL_TYPE_WRITE_DQA_FLUSH,
#endif
        CL_TYPE_WRITE_WB,
#ifdef __x86_64__
        CL_TYPE_WRITE_WB_AVX512,
#endif
        CL_TYPE_WRITE_WB_CLWB,
        CL_TYPE_WRITE_WB_FLUSH,
        CL_TYPE_WRITE_NTI,
        CL_TYPE_WRITE_NTI_CLWB,
#ifdef __x86_64__
        CL_TYPE_WRITE_NT512,
        CL_TYPE_WRITE_NTDQ
#endif
};

/* structure to store cpuid values */
struct cpuid_out {
        uint32_t eax;
        uint32_t ebx;
        uint32_t ecx;
        uint32_t edx;
};

static struct cpuid_out cpuid_1_0; /* leaf 1, sub-leaf 0 */
static struct cpuid_out cpuid_7_0; /* leaf 7, sub-leaf 0 */

/**
 * COMMON DATA
 */

static int stop_loop = 0;
static void *memchunk = NULL;
static unsigned memchunk_offset = 0;

/**
 * UTILS
 */

/*
 * A C wrapper for CPUID opcode
 *
 * Parameters:
 *    [in] leaf    - CPUID leaf number (EAX)
 *    [in] subleaf - CPUID sub-leaf number (ECX)
 *    [out] out    - registers structure to store results of CPUID into
 */
static void
lcpuid(const unsigned leaf, const unsigned subleaf, struct cpuid_out *out)
{
        if (out == NULL)
                return;

#ifdef __x86_64__
        asm volatile("mov %4, %%eax\n\t"
                     "mov %5, %%ecx\n\t"
                     "cpuid\n\t"
                     "mov %%eax, %0\n\t"
                     "mov %%ebx, %1\n\t"
                     "mov %%ecx, %2\n\t"
                     "mov %%edx, %3\n\t"
                     : "=g"(out->eax), "=g"(out->ebx), "=g"(out->ecx),
                       "=g"(out->edx)
                     : "g"(leaf), "g"(subleaf)
                     : "%eax", "%ebx", "%ecx", "%edx");
#else
        asm volatile("push %%ebx\n\t"
                     "mov %4, %%eax\n\t"
                     "mov %5, %%ecx\n\t"
                     "cpuid\n\t"
                     "mov %%eax, %0\n\t"
                     "mov %%ebx, %1\n\t"
                     "mov %%ecx, %2\n\t"
                     "mov %%edx, %3\n\t"
                     "pop %%ebx\n\t"
                     : "=g"(out->eax), "=g"(out->ebx), "=g"(out->ecx),
                       "=g"(out->edx)
                     : "g"(leaf), "g"(subleaf)
                     : "%eax", "%ecx", "%edx");
#endif
}

static uint32_t
detect_sse42(void)
{
        /* Check presence of SSE4.2 - bit 20 of ECX */
        return (cpuid_1_0.ecx & (1 << 20));
}

static uint32_t
detect_clwb(void)
{
        /* Check presence of CLWB - bit 24 of EBX */
        return (cpuid_7_0.ebx & (1 << 24));
}

static uint32_t
detect_avx512f(void)
{
        /* Check presence of AVX512F - bit 16 of EBX */
        return (cpuid_7_0.ebx & (1 << 16));
}

/**
 * @brief Function to detect CPU features
 *
 * @return Bitmap of supported features
 */
static uint64_t
cpu_feature_detect(void)
{
        static const struct {
                unsigned req_leaf_number;
                uint64_t feat;
                uint32_t (*detect_fn)(void);
        } feat_tab[] = {
            {1, CPU_FEATURE_SSE4_2, detect_sse42},
            {7, CPU_FEATURE_CLWB, detect_clwb},
            {7, CPU_FEATURE_AVX512F, detect_avx512f},
        };
        struct cpuid_out r;
        unsigned hi_leaf_number = 0;
        uint64_t features = 0;
        unsigned i;

        /* Get highest supported CPUID leaf number */
        lcpuid(0x0, 0x0, &r);
        hi_leaf_number = r.eax;

        /* Get the most common CPUID leafs to speed up the detection */
        if (hi_leaf_number >= 1)
                lcpuid(0x1, 0x0, &cpuid_1_0);

        if (hi_leaf_number >= 7)
                lcpuid(0x7, 0x0, &cpuid_7_0);

        for (i = 0; i < (sizeof(feat_tab) / sizeof(feat_tab[0])); i++) {
                if (hi_leaf_number < feat_tab[i].req_leaf_number)
                        continue;

                if (feat_tab[i].detect_fn() != 0)
                        features |= feat_tab[i].feat;
        }

        return features;
}

/**
 * @brief Function to bind thread to a cpu
 *
 * @param cpuid cpu to bind thread to
 */
static void
set_thread_affinity(const unsigned cpuid)
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
ALWAYS_INLINE void
cl_flush(void *p)
{
        asm volatile("clflush (%0)\n\t" : : "r"(p) : "memory");
}

/**
 * @brief Serialize store operations, prevent reordering of writes
 */
ALWAYS_INLINE void
sb(void)
{
        asm volatile("sfence\n\t" : : : "memory");
}

/**
 * @brief Cache line write back
 *
 * @param p line of cache
 */
ALWAYS_INLINE void
cl_wb(void *p)
{
        asm volatile("clwb (%0)\n\t" : : "r"(p) : "memory");
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
static void *
malloc_and_init_memory(size_t s)
{
        void *p = NULL;
        int ret;

        ret = posix_memalign(&p, PAGE_SIZE, s - s % PAGE_SIZE);

        if (ret != 0 || p == NULL) {
                printf("ERROR: Failed to allocate %lu bytes\n",
                       (unsigned long)s - s % PAGE_SIZE);
                stop_loop = 1;
                return NULL;
        }

        uint64_t *p64 = (uint64_t *)p;
        size_t s64 = s / sizeof(uint64_t);

        while (s64 > 0) {
                *p64 = (uint64_t)rand();
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
 * @brief Perform prefetcht0
 *
 * @param p pointer to memory location
 */
ALWAYS_INLINE void
cl_prefetch_t0(void *p)
{
        asm volatile("prefetcht0 (%0)\n\t" : : "r"(p) : "memory");
}

/**
 * @brief Perform prefetcht1
 *
 * @param p pointer to memory location
 */
ALWAYS_INLINE void
cl_prefetch_t1(void *p)
{
        asm volatile("prefetcht1 (%0)\n\t" : : "r"(p) : "memory");
}

/**
 * @brief Perform prefetcht2
 *
 * @param p pointer to memory location
 */
ALWAYS_INLINE void
cl_prefetch_t2(void *p)
{
        asm volatile("prefetcht2 (%0)\n\t" : : "r"(p) : "memory");
}

/**
 * @brief Perform prefetchnta
 *
 * @param p pointer to memory location
 */
ALWAYS_INLINE void
cl_prefetch_nta(void *p)
{
        asm volatile("prefetchnta (%0)\n\t" : : "r"(p) : "memory");
}

/**
 * @brief Perform prefetchw
 *
 * @param p pointer to memory location
 */
ALWAYS_INLINE void
cl_prefetch_w(void *p)
{
        asm volatile("prefetchw (%0)\n\t" : : "r"(p) : "memory");
}

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

#ifdef __x86_64__
#ifdef __AVX512F__
/**
 * @brief WB store vector version
 *
 * @param p pointer to memory location to be written
 * @param v value to overwrite memory location
 */
ALWAYS_INLINE void
cl_write_avx512(void *p, const uint64_t v)
{
        asm volatile("vmovq   %0, %%xmm1\n\t"
                     "vmovdqa64 %%zmm1, (%1)\n\t"
                     :
                     : "r"(v), "r"(p)
                     : "%zmm1", "memory");
}
#endif

/**
 * @brief WB vector version
 *
 * @param p pointer to memory location to be written
 * @param v value to overwrite memory location
 */
ALWAYS_INLINE void
cl_write_dqa(void *p, const uint64_t v)
{
        asm volatile("movq   %0, %%xmm1\n\t"
                     "movdqa %%xmm1, (%1)\n\t"
                     "movdqa %%xmm1, 16(%1)\n\t"
                     "movdqa %%xmm1, 32(%1)\n\t"
                     "movdqa %%xmm1, 48(%1)\n\t"
                     :
                     : "r"(v), "r"(p)
                     : "%xmm1", "memory");
}

/**
 * @brief Perform SSE write operation to specified cache line with flush
 *
 * @param p pointer to memory location to be written
 * @param v value to overwrite memory location
 */
ALWAYS_INLINE void
cl_write_dqa_flush(void *p, const uint64_t v)
{
        cl_write_dqa(p, v);
        cl_flush(p);
}
#endif

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

#ifdef __CLWB__
/**
 * @brief Perform write operation to specified cache line with clwb
 *
 * @param p pointer to memory location to be written
 * @param v value to overwrite memory location
 */
ALWAYS_INLINE void
cl_write_clwb(void *p, const uint64_t v)
{
        cl_write(p, v);
        cl_wb(p);
}
#endif

/**
 * @brief Perform write operation to specified cache line with flush
 *
 * @param p pointer to memory location to be written
 * @param v value to overwrite memory location
 */
ALWAYS_INLINE void
cl_write_flush(void *p, const uint64_t v)
{

        cl_write(p, v);
        cl_flush(p);
}

/**
 * @brief Perform write operation to memory giving non-temporal hint
 *
 * @param p pointer to memory location to be written
 * @param v value to overwrite memory location
 */
ALWAYS_INLINE void
cl_write_nti(void *p, const uint64_t v)
{
#ifdef __x86_64__
        asm volatile("movnti %0, (%1)\n\t"
                     "movnti %0, 8(%1)\n\t"
                     "movnti %0, 16(%1)\n\t"
                     "movnti %0, 24(%1)\n\t"
                     "movnti %0, 32(%1)\n\t"
                     "movnti %0, 40(%1)\n\t"
                     "movnti %0, 48(%1)\n\t"
                     "movnti %0, 56(%1)\n\t"
                     :
                     : "r"(v), "r"(p)
                     : "memory");
#else
        uint32_t v2 = (uint32_t)v;

        asm volatile("movnti %0, (%1)\n\t"
                     "movnti %0, 4(%1)\n\t"
                     "movnti %0, 8(%1)\n\t"
                     "movnti %0, 12(%1)\n\t"
                     "movnti %0, 16(%1)\n\t"
                     "movnti %0, 20(%1)\n\t"
                     "movnti %0, 24(%1)\n\t"
                     "movnti %0, 28(%1)\n\t"
                     "movnti %0, 32(%1)\n\t"
                     "movnti %0, 36(%1)\n\t"
                     "movnti %0, 40(%1)\n\t"
                     "movnti %0, 44(%1)\n\t"
                     "movnti %0, 48(%1)\n\t"
                     "movnti %0, 52(%1)\n\t"
                     "movnti %0, 56(%1)\n\t"
                     "movnti %0, 64(%1)\n\t"
                     :
                     : "r"(v2), "r"(p)
                     : "memory");
#endif
}

#if defined(__x86_64__) && defined(__AVX512F__)
/**
 * @brief non-temporal store vector version
 *
 * @param p pointer to memory location to be written
 * @param v value to overwrite memory location
 */
ALWAYS_INLINE void
cl_write_nt512(void *p, const uint64_t v)
{
        asm volatile("vmovq   %0, %%xmm1\n\t"
                     "vmovntpd %%zmm1, (%1)\n\t"
                     :
                     : "r"(v), "r"(p)
                     : "%zmm1", "memory");
}
#endif

#ifdef __CLWB__
/**
 * @brief Perform write operation to memory giving non-temporal hint with cache
 * line write back
 *
 * @param p pointer to memory location to be written
 * @param v value to overwrite memory location
 */
ALWAYS_INLINE void
cl_write_nti_clwb(void *p, const uint64_t v)
{
        cl_write_nti(p, v);
        cl_wb(p);
}
#endif

#ifdef __x86_64__
/**
 * @brief Non temporal store vector version
 *
 * @param p pointer to memory location to be written
 * @param v value to overwrite memory location
 */
ALWAYS_INLINE void
cl_write_ntdq(void *p, const uint64_t v)
{
        asm volatile("movq   %0, %%xmm1\n\t"
                     "movntdq %%xmm1, (%1)\n\t"
                     "movntdq %%xmm1, 16(%1)\n\t"
                     "movntdq %%xmm1, 32(%1)\n\t"
                     "movntdq %%xmm1, 48(%1)\n\t"
                     :
                     : "r"(v), "r"(p)
                     : "%xmm1", "memory");
}
#endif

/**
 * @brief Function to perform non-temporal read operation
 *        from specified memory location, vector version
 *
 * @param p pointer to memory location to read from
 */
ALWAYS_INLINE void
cl_read_ntq(void *p)
{
        asm volatile("movntdqa (%0), %%xmm1\n\t"
                     "movntdqa 16(%0), %%xmm1\n\t"
                     "movntdqa 32(%0), %%xmm1\n\t"
                     "movntdqa 48(%0), %%xmm1\n\t"
                     :
                     : "r"(p)
                     : "%xmm1", "memory");
}

/**
 * @brief Function to perform read operation from specified memory location
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
 * @brief Function to perform read operation from specified memory location,
 *        vector version
 *
 * @param p pointer to memory location to read from
 */
ALWAYS_INLINE void
cl_read_dqa(void *p)
{
        asm volatile("movdqa (%0), %%xmm1\n\t"
                     "movdqa 16(%0), %%xmm1\n\t"
                     "movdqa 32(%0), %%xmm1\n\t"
                     "movdqa 48(%0), %%xmm1\n\t"
                     :
                     : "r"(p)
                     : "%xmm1", "memory");
}

/**
 * @brief Function to find selected operation and execute it
 *
 * @param bw amount of bandwidth
 * @param type operation type to perform on core
 */
ALWAYS_INLINE void
mem_execute(const unsigned bw, const enum cl_type type)
{
        const uint64_t val = (uint64_t)rand();
        char *cp = (char *)memchunk;
        unsigned i = 0;
        const size_t s = MEMCHUNK_SIZE / CL_SIZE; /* mem size in cache lines */

        assert(memchunk != NULL);

        for (i = 0; i < bw; i++) {
                char *ptr = cp + (memchunk_offset * CL_SIZE);

                switch (type) {
                case CL_TYPE_PREFETCH_T0:
                        cl_prefetch_t0(ptr);
                        break;
                case CL_TYPE_PREFETCH_T1:
                        cl_prefetch_t1(ptr);
                        break;
                case CL_TYPE_PREFETCH_T2:
                        cl_prefetch_t2(ptr);
                        break;
                case CL_TYPE_PREFETCH_NTA:
                        cl_prefetch_nta(ptr);
                        break;
                case CL_TYPE_PREFETCH_W:
                        cl_prefetch_w(ptr);
                        break;
                case CL_TYPE_READ_NTQ:
                        cl_read_ntq(ptr);
                        break;
                case CL_TYPE_READ_WB:
                        cl_read(ptr);
                        break;
                case CL_TYPE_READ_WB_DQA:
                        cl_read_dqa(ptr);
                        break;
                case CL_TYPE_READ_MOD_WRITE:
                        cl_read_mod_write(ptr, val);
                        break;
#ifdef __x86_64__
                case CL_TYPE_WRITE_DQA:
                        cl_write_dqa(ptr, val);
                        break;
                case CL_TYPE_WRITE_DQA_FLUSH:
                        cl_write_dqa_flush(ptr, val);
                        break;
#endif
                case CL_TYPE_WRITE_WB:
                        cl_write(ptr, val);
                        break;
#if defined(__x86_64__) && defined(__AVX512F__)
                case CL_TYPE_WRITE_WB_AVX512:
                        cl_write_avx512(ptr, val);
                        break;
#endif
#ifdef __CLWB__
                case CL_TYPE_WRITE_WB_CLWB:
                        cl_write_clwb(ptr, val);
                        break;
#endif
                case CL_TYPE_WRITE_WB_FLUSH:
                        cl_write_flush(ptr, val);
                        break;
                case CL_TYPE_WRITE_NTI:
                        cl_write_nti(ptr, val);
                        break;
#ifdef __CLWB__
                case CL_TYPE_WRITE_NTI_CLWB:
                        cl_write_nti_clwb(ptr, val);
                        break;
#endif
#ifdef __x86_64__
#ifdef __AVX512F__
                case CL_TYPE_WRITE_NT512:
                        cl_write_nt512(ptr, val);
                        break;
#endif
                case CL_TYPE_WRITE_NTDQ:
                        cl_write_ntdq(ptr, val);
                        break;
#endif
                default:
                        assert(0);
                        break;
                }
                if (++memchunk_offset >= s)
                        memchunk_offset = 0;
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
static void
usage(char **argv)
{
        printf("Usage: %s -c <cpu> -b <BW [MB/s]> <operation type>\n"
               "Description:\n"
               "  -c, --cpu          cpu to generate B/W\n"
               "  -b, --bandwidth    memory B/W specified in MBps\n"
               "Operation types:\n"
               "  --prefetch-t0      prefetcht0\n"
               "  --prefetch-t1      prefetcht1\n"
               "  --prefetch-t2      prefetcht2\n"
               "  --prefetch-nta     prefetchtnta\n"
               "  --prefetch-w       prefetchw\n"
               "  --read             x86 loads\n"
               "  --read-sse         SSE loads\n"
               "  --nt-read-sse      SSE NT loads\n"
               "  --read-mod-write   x86 load XOR write\n"
               "  --write            x86 stores\n"
#ifdef __x86_64__
               "  --write-avx512     AVX512 stores\n"
#endif
               "  --write-clwb       x86 stores + clwb\n"
               "  --write-flush      x86 stores & clflush (naturally generates "
               "loads & stores)\n"
#ifdef __x86_64__
               "  --write-sse        SSE stores\n"
               "  --write-sse-flush  SSE stores & clflush (naturally generates "
               "loads & stores)\n"
#endif
               "  --nt-write         x86 NT stores\n"
               "  --nt-write-avx512  AVX512 NT stores\n"
               "  --nt-write-clwb    x86 NT stores + clwb\n"
#ifdef __x86_64__
               "  --nt-write-sse     SSE NT stores\n"
#endif
               ,
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
ALWAYS_INLINE long
get_usec_diff(struct timeval *tv_s, struct timeval *tv_e)
{
        long usec_start, usec_end = 0;

        usec_start = ((long)tv_s->tv_usec) + ((long)tv_s->tv_sec * 1000000L);
        usec_end = ((long)tv_e->tv_usec) + ((long)tv_e->tv_sec * 1000000L);

        return usec_end - usec_start;
}

/**
 * @brief Sleep before executing operation
 *
 * @param usec_diff time taken to execute operation
 * @param interval maximum time operation should take
 */
ALWAYS_INLINE void
nano_sleep(const long interval, long usec_diff)
{
        struct timespec req, rem;

        req.tv_sec = (interval - usec_diff) / 1000000L;
        req.tv_nsec = ((interval - usec_diff) % 1000000L) * 1000L;
        if (nanosleep(&req, &rem) == -1)
                nanosleep(&rem, NULL);
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
        unsigned tmp = 0;

        if (NULL == str || NULL == value)
                return -EINVAL;

        while (isblank(*str_start))
                str_start++;

        if (base == 10 && !isdigit(*str_start))
                return -EINVAL;

        if (base == 16 && !isxdigit(*str_start))
                return -EINVAL;

        errno = 0;
        tmp = strtoul(str_start, &str_end, base);
        if (errno != 0 || !(*str_start != '\0' && *str_end == '\0'))
                return -EINVAL;

        *value = tmp;
        return 0;
}

int
main(int argc, char **argv)
{
        int cmd = EXIT_SUCCESS;
        enum cl_type type = CL_TYPE_INVALID;
        unsigned mem_bw = 0;
        unsigned cpu = UINT_MAX;
        int option_index;
        int ret;
        uint64_t features;

        /* clang-format off */
        struct option options[] = {
            {"bandwidth",       required_argument, 0, 'b'},
            {"cpu",             required_argument, 0, 'c'},
            {"prefetch-t0",     no_argument, 0, CL_TYPE_PREFETCH_T0},
            {"prefetch-t1",     no_argument, 0, CL_TYPE_PREFETCH_T1},
            {"prefetch-t2",     no_argument, 0, CL_TYPE_PREFETCH_T2},
            {"prefetch-nta",    no_argument, 0, CL_TYPE_PREFETCH_NTA},
            {"prefetch-w",      no_argument, 0, CL_TYPE_PREFETCH_W},
            {"read",            no_argument, 0, CL_TYPE_READ_WB},
            {"read-sse",        no_argument, 0, CL_TYPE_READ_WB_DQA},
            {"nt-read-sse",     no_argument, 0, CL_TYPE_READ_NTQ},
            {"read-mod-write",  no_argument, 0, CL_TYPE_READ_MOD_WRITE},
            {"write",           no_argument, 0, CL_TYPE_WRITE_WB},
#ifdef __x86_64__
            {"write-avx512",    no_argument, 0, CL_TYPE_WRITE_WB_AVX512},
#endif
            {"write-clwb",      no_argument, 0, CL_TYPE_WRITE_WB_CLWB},
            {"write-flush",     no_argument, 0, CL_TYPE_WRITE_WB_FLUSH},
#ifdef __x86_64__
            {"write-sse",       no_argument, 0, CL_TYPE_WRITE_DQA},
            {"write-sse-flush", no_argument, 0, CL_TYPE_WRITE_DQA_FLUSH},
#endif
            {"nt-write",        no_argument, 0, CL_TYPE_WRITE_NTI},
#ifdef __x86_64__
            {"nt-write-avx512", no_argument, 0, CL_TYPE_WRITE_NT512},
#endif
            {"nt-write-clwb",   no_argument, 0, CL_TYPE_WRITE_NTI_CLWB},
#ifdef __x86_64__
            {"nt-write-sse",    no_argument, 0, CL_TYPE_WRITE_NTDQ},
#endif
            {0, 0, 0, 0}
        };
        /* clang-format on */

        /* Process command line arguments */
        while ((cmd = getopt_long_only(argc, argv, "b:c:", options,
                                       &option_index)) != -1) {

                switch (cmd) {
                case 'c':
                        ret = str_to_uint(optarg, 10, &cpu);
                        if (ret != 0) {
                                printf("Invalid CPU specified!\n");
                                return EXIT_FAILURE;
                        }
                        break;
                case 'b':
                        ret = str_to_uint(optarg, 10, &mem_bw);
                        if (ret != 0 || mem_bw == 0 || mem_bw > MAX_MEM_BW) {
                                printf("Invalid B/W specified!\n");
                                return EXIT_FAILURE;
                        }
                        break;
                case CL_TYPE_PREFETCH_T0:
                case CL_TYPE_PREFETCH_T1:
                case CL_TYPE_PREFETCH_T2:
                case CL_TYPE_PREFETCH_NTA:
                case CL_TYPE_PREFETCH_W:
                case CL_TYPE_READ_NTQ:
                case CL_TYPE_READ_WB:
                case CL_TYPE_READ_WB_DQA:
                case CL_TYPE_READ_MOD_WRITE:
#ifdef __x86_64__
                case CL_TYPE_WRITE_DQA:
                case CL_TYPE_WRITE_DQA_FLUSH:
#endif
                case CL_TYPE_WRITE_WB:
#ifdef __x86_64__
                case CL_TYPE_WRITE_WB_AVX512:
#endif
                case CL_TYPE_WRITE_WB_CLWB:
                case CL_TYPE_WRITE_WB_FLUSH:
                case CL_TYPE_WRITE_NTI:
                case CL_TYPE_WRITE_NTI_CLWB:
#ifdef __x86_64__
                case CL_TYPE_WRITE_NT512:
                case CL_TYPE_WRITE_NTDQ:
#endif
                        type = (enum cl_type)cmd;
                        break;
                default:
                        usage(argv);
                        return EXIT_FAILURE;
                        break;
                }
        }

        /* Check if user has supplied all required arguments */
        if (type == CL_TYPE_INVALID || cpu == UINT_MAX || !mem_bw ||
            optind < argc) {
                usage(argv);
                return EXIT_FAILURE;
        }

        features = cpu_feature_detect();

        switch (type) {
        case CL_TYPE_READ_WB_DQA:
#ifdef __x86_64__
        case CL_TYPE_WRITE_DQA:
        case CL_TYPE_WRITE_DQA_FLUSH:
        case CL_TYPE_WRITE_NTDQ:
#endif
                if (!(features & CPU_FEATURE_SSE4_2)) {
                        printf("No CPU support for SSE4.2 instructions!\n");
                        return EXIT_FAILURE;
                }
                break;
        case CL_TYPE_WRITE_NTI_CLWB:
        case CL_TYPE_WRITE_WB_CLWB:
#ifdef __CLWB__
                if (!(features & CPU_FEATURE_CLWB)) {
                        printf("No CPU support for CLWB instructions!\n");
                        return EXIT_FAILURE;
                }
#else
                printf("No compiler support for CLWB instructions!\n");
                return EXIT_FAILURE;
#endif
                break;
#ifdef __x86_64__
        case CL_TYPE_WRITE_NT512:
        case CL_TYPE_WRITE_WB_AVX512:
#ifdef __AVX512F__
                if (!(features & CPU_FEATURE_AVX512F)) {
                        printf("No CPU support for AVX512 instructions!\n");
                        return EXIT_FAILURE;
                }
#else
                printf("No compiler support for AVX512 instructions!\n");
                return EXIT_FAILURE;
#endif
                break;
#endif
        default:
                break;
        }

        printf("- THREAD logical core id: %u, "
               " memory bandwidth [MB]: %u, starting...\n",
               cpu, mem_bw);

        /* Bind thread to cpu */
        set_thread_affinity(cpu);

        /* Allocate memory */
        memchunk = malloc_and_init_memory(MEMCHUNK_SIZE);
        if (memchunk == NULL) {
                printf("Failed to allocate memory!\n");
                return EXIT_FAILURE;
        }

        /* Calculate memory bandwidth to use */
        mem_bw *= (((1024 * 1024) / CL_SIZE)) / CHUNKS;

        /* Stress memory bandwidth */
        while (stop_loop == 0) {
                struct timeval tv_s, tv_e;
                long usec_diff;
                const long interval = 1000000L / CHUNKS; /* interval in [us] */

                /* Get time before executing operation in loop */
                gettimeofday(&tv_s, NULL);

                /* Execute operation */
                mem_execute(mem_bw, type);

                /* Get time after executing operation */
                gettimeofday(&tv_e, NULL);

                usec_diff = get_usec_diff(&tv_s, &tv_e);

                if (usec_diff < interval) {
                        /* Sleep before executing operation again */
                        nano_sleep(interval, usec_diff);
                }
        }

        /* Terminate thread */
        free(memchunk);
        printf("\nexiting...\n");

        return 0;
}
