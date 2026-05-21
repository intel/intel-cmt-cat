/*
 * BSD LICENSE
 *
 * Copyright(c) 2014-2026 Intel Corporation. All rights reserved.
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
 * @brief Platform QoS utility - main module
 *
 */
#include "main.h"

#include "alloc.h"
#include "cap.h"
#include "common.h"
#include "dump.h"
#include "dump_rmids.h"
#include "monitor.h"
#include "pqos.h"
#include "profiles.h"

#include <ctype.h> /**< isspace() */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h> /**< getopt_long() */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h> /**< open() */
#include <unistd.h>

#define FILE_READ_WRITE (0600)

#define DEFAULT_TABLE_SIZE 128

#define BUF_SIZE 256

/**
 * Default configuration of allocation
 */
static struct pqos_alloc_config sel_alloc_config = {
    .l3_cdp = PQOS_REQUIRE_CDP_ANY,
    .l3_iordt = PQOS_REQUIRE_IORDT_ANY,
    .l2_cdp = PQOS_REQUIRE_CDP_ANY,
    .mba = PQOS_MBA_ANY,
    .smba = PQOS_MBA_ANY,
    .mba40 = PQOS_FEATURE_ANY};

/**
 * Default configuration of monitoring
 */
static struct pqos_mon_config sel_mon_config = {.l3_iordt =
                                                    PQOS_REQUIRE_IORDT_ANY,
                                                .snc = PQOS_REQUIRE_SNC_ANY};

/**
 * Monitoring reset
 */
static int sel_mon_reset = 0;

/**
 * Maintains pointer to selected log file name
 */
static char *sel_log_file = NULL;

/**
 * Maintains pointer to selected config file
 */
static char *sel_config_file = NULL;

/**
 * Maintains pointer to allocation profile from internal DB
 */
static char *sel_allocation_profile = NULL;

/**
 * Maintains verbose mode choice selected in config string
 */
static int sel_verbose_mode = 0;

/**
 * Reset allocation configuration
 */
static int sel_reset_alloc = 0;

/**
 * Enable showing cache allocation settings
 */
static int sel_show_allocation_config = 0;

/**
 * Enable displaying supported RDT capabilities
 */
static int sel_display = 0;

/**
 * Enable displaying supported RDT capabilities in verbose mode
 */
static int sel_display_verbose = 0;

/**
 * Selected library interface
 */
enum pqos_interface sel_interface = PQOS_INTER_AUTO;

/**
 * Selected library interface explicitly
 */
static int sel_interface_selected = 0;

/**
 * Interface compatibility bitmask used by the auto-selection logic.
 *
 * Each command-line option contributes a mask describing which interfaces
 * can satisfy it. The accumulated AND across all parsed options yields the
 * set of interfaces that are still compatible with the request. If the user
 * also supplies --iface / -I explicitly, the resolver verifies that the
 * explicit choice is in the accumulated set.
 */
#define IFACE_MSR  (1U << 0)
#define IFACE_OS   (1U << 1)
#define IFACE_MMIO (1U << 2)
#define IFACE_ANY  (IFACE_MSR | IFACE_OS | IFACE_MMIO)

/**
 * Accumulated interface constraint mask. Starts at IFACE_ANY (no constraint)
 * and is AND-narrowed as options are parsed.
 */
static unsigned iface_constraint_mask = IFACE_ANY;

/**
 * Description of the option that last narrowed @ref iface_constraint_mask
 * (used in error messages when a subsequent option creates a conflict).
 */
static const char *iface_constraint_origin = NULL;

/**
 * Interface explicitly requested by the user via --iface / -I, valid only
 * when @ref user_interface_set is non-zero. PQOS_INTER_AUTO means "auto"
 * and is treated as no explicit override.
 */
static enum pqos_interface user_interface = PQOS_INTER_AUTO;

/**
 * Set when the user supplies an explicit interface (--iface=msr|os|mmio
 * or -I). --iface=auto leaves this 0.
 */
static int user_interface_set = 0;

/**
 * Enable displaying library version
 */
static int sel_print_version = 0;

/**
 * Enable displaying memory regions
 */
static int sel_print_mem_regions = 0;

/**
 * Enable displaying processor topology
 */
static int sel_print_topology = 0;

/**
 * Enable displaying available MMIO registers to dump
 */
static int sel_print_dump_info = 0;

/**
 * Enable MMIO registers dump
 */
static int sel_dump = 0;

/**
 * Enable RMID registers dump
 */
static int sel_dump_rmid_regs = 0;

/**
 * Enable displaying available I/O devices
 */
static int sel_print_io_devs = 0;

/**
 * Enable displaying specific I/O device
 */
static int sel_print_io_dev = 0;

static uint64_t strtouint64_base(const char *s, int default_base);

/**
 * @brief Function to check if a value is already contained in a table
 *
 * @param tab table of values to check
 * @param size table size
 * @param val value to search for
 *
 * @return If the value is already in the table
 * @retval 1 if value if found
 * @retval 0 if value is not found
 */
static int
isdup(const uint64_t *tab, const unsigned size, const uint64_t val)
{
        unsigned i;

        for (i = 0; i < size; i++)
                if (tab[i] == val)
                        return 1;
        return 0;
}

uint64_t
strtouint64(const char *s)
{
        return strtouint64_base(s, 10);
}

uint64_t
strhextouint64(const char *s)
{
        return strtouint64_base(s, 16);
}

static uint64_t
strtouint64_base(const char *s, int default_base)
{
        const char *str = s;
        int base = default_base;
        uint64_t n = 0;
        char *endptr = NULL;

        ASSERT(s != NULL);

        if (strncasecmp(s, "0x", 2) == 0) {
                base = 16;
                s += 2;
        }

        errno = 0;
        n = strtoull(s, &endptr, base);

        /* Check for various possible errors */
        if ((errno == ERANGE && n == ULLONG_MAX) || (errno != 0 && n == 0)) {
                perror("strtoull");
                exit(EXIT_FAILURE);
        }

        if (endptr == s) {
                printf("No digits were found\n");
                exit(EXIT_FAILURE);
        }

        if (!(*s != '\0' && *endptr == '\0')) {
                printf("Error converting '%s' to unsigned number!\n", str);
                exit(EXIT_FAILURE);
        }

        return n;
}

unsigned
strlisttotab(char *s, uint64_t *tab, const unsigned max)
{
        unsigned index = 0;
        char *saveptr = NULL;
        char *tmp = NULL;

        if (s == NULL || tab == NULL || max == 0)
                return index;

        tmp = strdup(s);
        if (tmp == NULL) {
                printf("Failed to allocate memory for argument copy!\n");
                exit(EXIT_FAILURE);
        }

        for (;;) {
                char *p = NULL;
                char *token = NULL;

                token = strtok_r(s, ",", &saveptr);
                if (token == NULL)
                        break;

                s = NULL;

                /* get rid of leading spaces & skip empty tokens */
                while (isspace(*token))
                        token++;
                if (*token == '\0')
                        continue;

                p = strchr(token, '-');
                if (p != NULL) {
                        /**
                         * range of numbers provided
                         * example: 1-5 or 12-9
                         */
                        uint64_t n, start, end;
                        *p = '\0';
                        start = strtouint64(token);
                        end = strtouint64(p + 1);
                        if (start > end) {
                                /**
                                 * no big deal just swap start with end
                                 */
                                n = start;
                                start = end;
                                end = n;
                        }
                        if (end > UINT_FAST64_MAX) {
                                printf("Too large group items.\n");
                                exit(EXIT_FAILURE);
                        }
                        for (n = start; n <= end; n++) {
                                if (index >= max) {
                                        printf("Exceeded parser capacity of "
                                               "%u entries\n",
                                               max);
                                        parse_error(
                                            tmp, "Too many groups selected.\n");
                                }
                                if (!(isdup(tab, index, n))) {
                                        tab[index] = n;
                                        index++;
                                }
                        }
                } else {
                        /**
                         * single number provided here
                         * remove duplicates if necessary
                         */
                        uint64_t val = strtouint64(token);

                        if (!(isdup(tab, index, val))) {
                                if (index >= max) {
                                        printf("Exceeded parser capacity of "
                                               "%u entries\n",
                                               max);
                                        parse_error(
                                            tmp, "Too many groups selected.\n");
                                }
                                tab[index] = val;
                                index++;
                        }
                }
        }

        free(tmp);
        return index;
}

unsigned
strlisttotabrealloc(char *s, uint64_t **tab, unsigned *max)
{
        unsigned index = 0;
        char *saveptr = NULL;

        if (s == NULL || (*tab) == NULL || *max == 0)
                return index;

        for (;;) {
                char *p = NULL;
                char *token = NULL;

                token = strtok_r(s, ",", &saveptr);
                if (token == NULL)
                        break;

                s = NULL;

                /* get rid of leading spaces & skip empty tokens */
                while (isspace(*token))
                        token++;
                if (*token == '\0')
                        continue;

                p = strchr(token, '-');
                if (p != NULL) {
                        /**
                         * range of numbers provided
                         * example: 1-5 or 12-9
                         */
                        uint64_t n, start, end;
                        *p = '\0';
                        start = strtouint64(token);
                        if (*(p + 1) == '\0')
                                parse_error(
                                    token,
                                    "Incomplete cores association format");

                        if (!(*(p + 1) >= '0' && *(p + 1) <= '9'))
                                parse_error(p + 1,
                                            "Invalid cores association format");

                        end = strtouint64(p + 1);
                        if (start > end) {
                                /**
                                 * no big deal just swap start with end
                                 */
                                n = start;
                                start = end;
                                end = n;
                        }
                        if (end > UINT_FAST64_MAX) {
                                printf("Too large group items.\n");
                                exit(EXIT_FAILURE);
                        }
                        for (n = start; n <= end; n++) {
                                if (!(isdup(*tab, index, n))) {
                                        (*tab)[index] = n;
                                        index++;
                                }
                                if (index >= *max) {
                                        (*tab) = realloc_and_init(
                                            *tab, max, sizeof(**tab));
                                        if ((*tab) == NULL) {
                                                printf("Reallocation error!\n");
                                                exit(EXIT_FAILURE);
                                        }
                                }
                        }
                } else {
                        /**
                         * single number provided here
                         * remove duplicates if necessary
                         */
                        uint64_t val = strtouint64(token);

                        if (!(isdup((*tab), index, val))) {
                                (*tab)[index] = val;
                                index++;
                        }
                        if (index >= *max) {
                                (*tab) =
                                    realloc_and_init(*tab, max, sizeof(**tab));
                                if ((*tab) == NULL) {
                                        printf("Reallocation error!\n");
                                        exit(EXIT_FAILURE);
                                }
                        }
                }
        }

        return index;
}

void *
realloc_and_init(void *ptr, unsigned *elem_count, const size_t elem_size)
{
        unsigned next_count;
        const unsigned max_elem_count = UINT_MAX / 2U;
        size_t prev_size, next_size;
        uint8_t *tmp_ptr;

        if (elem_count == NULL)
                return NULL;

        if (elem_size != 0 && *elem_count > SIZE_MAX / elem_size)
                return NULL;

        if (*elem_count == 0)
                next_count = 1;
        else {
                if (*elem_count > max_elem_count)
                        return NULL;

                next_count = *elem_count * 2;
        }

        if (elem_size != 0 && next_count > SIZE_MAX / elem_size)
                return NULL;

        prev_size = elem_size * *elem_count;
        next_size = elem_size * next_count;
        tmp_ptr = realloc(ptr, next_size);

        if (tmp_ptr != NULL) {
                memset(tmp_ptr + prev_size, 0, next_size - prev_size);
                *elem_count = next_count;
        }
        return tmp_ptr;
}

void
selfn_strdup(char **sel, const char *arg)
{
        ASSERT(sel != NULL && arg != NULL);
        if (arg == NULL) {
                printf("String duplicate error!\n");
                exit(EXIT_FAILURE);
        }
        if (*sel != NULL) {
                free(*sel);
                *sel = NULL;
        }
        *sel = strdup(arg);
        ASSERT(*sel != NULL);
        if (*sel == NULL) {
                printf("String duplicate error!\n");
                exit(EXIT_FAILURE);
        }
}

/**
 * @brief Selects log file
 *
 * @param arg string passed to -l command line option
 */
static void
selfn_log_file(const char *arg)
{
        selfn_strdup(&sel_log_file, arg);
}

/**
 * @brief Selects verbose mode on
 *
 * @param arg not used
 */
static void
selfn_verbose_mode(const char *arg)
{
        UNUSED_ARG(arg);
        sel_verbose_mode = 1;
}

/**
 * @brief Selects super verbose mode on
 *
 * @param arg not used
 */
static void
selfn_super_verbose_mode(const char *arg)
{
        UNUSED_ARG(arg);
        sel_verbose_mode = 2;
}

/**
 * @brief Sets allocation reset flag
 *
 * @param [in] arg optional configuration string
 *             if NULL or zero length  then configuration check is skipped
 */
static void
selfn_reset_alloc(const char *arg)
{
        if (arg != NULL && *arg != '\0') {
                unsigned i;
                char *tok = NULL;
                char *saveptr = NULL;
                char *s = NULL;
                struct pqos_alloc_config *cfg = &sel_alloc_config;

                selfn_strdup(&s, arg);

                /* L3 CDP reset options */
                const struct {
                        const char *name;
                        enum pqos_cdp_config cdp;
                } patternsl3[] = {
                    {"l3cdp-on", PQOS_REQUIRE_CDP_ON},
                    {"l3cdp-off", PQOS_REQUIRE_CDP_OFF},
                    {"l3cdp-any", PQOS_REQUIRE_CDP_ANY},
                };

                /* L2 CDP reset options */
                const struct {
                        const char *name;
                        enum pqos_cdp_config cdp;
                } patternsl2[] = {
                    {"l2cdp-on", PQOS_REQUIRE_CDP_ON},
                    {"l2cdp-off", PQOS_REQUIRE_CDP_OFF},
                    {"l2cdp-any", PQOS_REQUIRE_CDP_ANY},
                };

                /* MBA CTRL reset options */
                const struct {
                        const char *name;
                        enum pqos_mba_config mba;
                } patternsmb[] = {
                    {"mbaCtrl-on", PQOS_MBA_CTRL},
                    {"mbaCtrl-off", PQOS_MBA_DEFAULT},
                    {"mbaCtrl-any", PQOS_MBA_ANY},
                };

                /* L3 I/O RDT reset options */
                const struct {
                        const char *name;
                        enum pqos_iordt_config iordt;
                } patterniordt[] = {
                    {"l3iordt-on", PQOS_REQUIRE_IORDT_ON},
                    {"l3iordt-off", PQOS_REQUIRE_IORDT_OFF},
                    {"l3iordt-any", PQOS_REQUIRE_IORDT_ANY},
                };

                /* MBA CTRL 4.0 reset options */
                const struct {
                        const char *name;
                        enum pqos_feature_cfg cfg;
                } pattsmba40[] = {
                    {"mba40-on", PQOS_FEATURE_ON},
                    {"mba40-off", PQOS_FEATURE_OFF},
                    {"mba40-any", PQOS_FEATURE_ANY},
                };

                tok = s;
                while ((tok = strtok_r(tok, ",", &saveptr)) != NULL) {
                        unsigned valid = 0;

                        for (i = 0; i < DIM(patternsl3); i++)
                                if (strcasecmp(tok, patternsl3[i].name) == 0) {
                                        cfg->l3_cdp = patternsl3[i].cdp;
                                        valid = 1;
                                        break;
                                }
                        for (i = 0; i < DIM(patternsl2); i++)
                                if (strcasecmp(tok, patternsl2[i].name) == 0) {
                                        cfg->l2_cdp = patternsl2[i].cdp;
                                        valid = 1;
                                        break;
                                }
                        for (i = 0; i < DIM(patternsmb); i++)
                                if (strcasecmp(tok, patternsmb[i].name) == 0) {
                                        cfg->mba = patternsmb[i].mba;
                                        valid = 1;
                                        break;
                                }
                        for (i = 0; i < DIM(patterniordt); i++)
                                if (strcasecmp(tok, patterniordt[i].name) ==
                                    0) {
                                        cfg->l3_iordt = patterniordt[i].iordt;
                                        valid = 1;
                                        break;
                                }

                        for (i = 0; i < DIM(pattsmba40); i++)
                                if (strcasecmp(tok, pattsmba40[i].name) == 0) {
                                        cfg->mba40 = pattsmba40[i].cfg;
                                        valid = 1;
                                        break;
                                }

                        if (!valid) {
                                printf("Unrecognized '%s' allocation "
                                       "reset option!\n",
                                       tok);
                                free(s);
                                exit(EXIT_FAILURE);
                        }

                        tok = NULL;
                }

                free(s);
        }
        sel_reset_alloc = 1;
}

/**
 * @brief Sets monitoring reset flag
 *
 * @param [in] arg optional configuration string
 *             if NULL or zero length  then configuration check is skipped
 */
static void
selfn_reset_mon(const char *arg)
{
        if (arg != NULL && *arg != '\0') {
                unsigned i;
                char *tok = NULL;
                char *saveptr = NULL;
                char *s = NULL;
                struct pqos_mon_config *cfg = &sel_mon_config;

                selfn_strdup(&s, arg);

                /* L3 I/O RDT reset options */
                const struct {
                        const char *name;
                        enum pqos_iordt_config iordt;
                } patterniordt[] = {
                    {"l3iordt-on", PQOS_REQUIRE_IORDT_ON},
                    {"l3iordt-off", PQOS_REQUIRE_IORDT_OFF},
                    {"l3iordt-any", PQOS_REQUIRE_IORDT_ANY},
                };

                /* SNC reset options */
                const struct {
                        const char *name;
                        enum pqos_snc_config snc;
                } patternsnc[] = {
                    {"snc-local", PQOS_REQUIRE_SNC_LOCAL},
                    {"snc-total", PQOS_REQUIRE_SNC_TOTAL},
                    {"snc-any", PQOS_REQUIRE_SNC_ANY},
                };

                tok = s;
                while ((tok = strtok_r(tok, ",", &saveptr)) != NULL) {
                        unsigned valid = 0;

                        for (i = 0; i < DIM(patterniordt); i++)
                                if (strcasecmp(tok, patterniordt[i].name) ==
                                    0) {
                                        cfg->l3_iordt = patterniordt[i].iordt;
                                        valid = 1;
                                        break;
                                }

                        for (i = 0; i < DIM(patternsnc); i++)
                                if (strcasecmp(tok, patternsnc[i].name) == 0) {
                                        cfg->snc = patternsnc[i].snc;
                                        valid = 1;
                                        break;
                                }

                        if (!valid) {
                                printf("Unrecognized '%s' monitoring "
                                       "reset option!\n",
                                       tok);
                                free(s);
                                exit(EXIT_FAILURE);
                        }

                        tok = NULL;
                }

                free(s);
        }
        sel_mon_reset = 1;
}

/**
 * @brief Selects showing allocation settings
 *
 * @param arg not used
 */
static void
selfn_show_allocation(const char *arg)
{
        UNUSED_ARG(arg);
        sel_show_allocation_config = 1;
}

/**
 * @brief Selects displaying supported capabilities
 *
 * @param arg not used
 */
static void
selfn_display(const char *arg)
{
        UNUSED_ARG(arg);
        sel_display = 1;
}

/**
 * @brief Selects displaying supported capabilities in verbose mode
 *
 * @param arg not used
 */
static void
selfn_display_verbose(const char *arg)
{
        UNUSED_ARG(arg);
        sel_display_verbose = 1;
}

/**
 * @brief Selects allocation profile from internal DB
 *
 * @param arg string passed to -c command line option
 */
static void
selfn_allocation_select(const char *arg)
{
        selfn_strdup(&sel_allocation_profile, arg);
}

/**
 * @brief Case-insensitive substring search (locale-independent).
 *
 * @param haystack string to search in (must be non-NULL)
 * @param needle   substring to look for (must be non-NULL)
 *
 * @return pointer into @a haystack at the start of the first match, or
 *         NULL if @a needle is not found
 */
static const char *
strcasestr_local(const char *haystack, const char *needle)
{
        size_t nlen;

        if (haystack == NULL || needle == NULL)
                return NULL;
        if (*needle == '\0')
                return haystack;

        nlen = strlen(needle);
        for (; *haystack != '\0'; haystack++)
                if (strncasecmp(haystack, needle, nlen) == 0)
                        return haystack;
        return NULL;
}

/**
 * @brief Convert an interface mask to a printable "msr|os|mmio" string.
 *
 * @param [in] mask interface bitmask (combination of IFACE_MSR/OS/MMIO)
 * @param [out] buf destination buffer
 * @param [in] buf_size size of @a buf in bytes
 *
 * @return @a buf (always NUL-terminated)
 */
static const char *
iface_mask_to_str(unsigned mask, char *buf, size_t buf_size)
{
        size_t pos = 0;
        const char *sep = "";

        if (buf == NULL || buf_size == 0)
                return "";

        buf[0] = '\0';
        if (mask == 0) {
                snprintf(buf, buf_size, "<none>");
                return buf;
        }
        if (mask & IFACE_MMIO) {
                pos += snprintf(buf + pos, buf_size - pos, "%smmio", sep);
                sep = "|";
        }
        if (mask & IFACE_MSR) {
                pos += snprintf(buf + pos, buf_size - pos, "%smsr", sep);
                sep = "|";
        }
        if (mask & IFACE_OS) {
                pos += snprintf(buf + pos, buf_size - pos, "%sos", sep);
                sep = "|";
        }
        return buf;
}

/**
 * @brief Convert a single resolved interface enum to its short name.
 */
static const char *
iface_enum_to_str(enum pqos_interface iface)
{
        switch (iface) {
        case PQOS_INTER_MSR:
                return "msr";
        case PQOS_INTER_OS:
        case PQOS_INTER_OS_RESCTRL_MON:
                return "os";
        case PQOS_INTER_MMIO:
                return "mmio";
        case PQOS_INTER_AUTO:
                return "auto";
        }
        return "unknown";
}

/**
 * @brief Translate an interface enum to its single-bit mask.
 *
 * @return IFACE_MSR / IFACE_OS / IFACE_MMIO or 0 for AUTO/unknown
 */
static unsigned
iface_enum_to_bit(enum pqos_interface iface)
{
        switch (iface) {
        case PQOS_INTER_MSR:
                return IFACE_MSR;
        case PQOS_INTER_OS:
        case PQOS_INTER_OS_RESCTRL_MON:
                return IFACE_OS;
        case PQOS_INTER_MMIO:
                return IFACE_MMIO;
        case PQOS_INTER_AUTO:
                return 0;
        }
        return 0;
}

/**
 * @brief Pure helper: AND-narrow @a current with @a add.
 *
 * @param current current mask
 * @param add     new constraint to apply (subset of IFACE_ANY)
 *
 * @return resulting mask. A return value of 0 indicates the two masks are
 *         incompatible; the caller decides how to surface this. Both
 *         @c IFACE_ANY and @c 0 in @a add are treated as a no-op (return
 *         @a current unchanged).
 */
unsigned
iface_narrow(unsigned current, unsigned add)
{
        add &= IFACE_ANY;
        if (add == 0 || add == IFACE_ANY)
                return current & IFACE_ANY;
        return (current & IFACE_ANY) & add;
}

/**
 * @brief Pure helper: pick a single interface for the given constraint mask.
 *
 * Implements the resolution rules used by @ref resolve_interface(),
 * without any global state, error reporting, or process exit. Useful for
 * unit testing.
 *
 * @param mask constraint mask (subset of IFACE_ANY); 0 is invalid
 * @param user_set non-zero when the user supplied --iface / -I
 * @param user explicit interface chosen by the user (only used when
 *             @a user_set is non-zero); PQOS_INTER_AUTO means no override
 * @param [out] out resolved interface (only written on success)
 *
 * @retval 0 on success (resolution found, written to @a out)
 * @retval -1 when @a mask is empty
 * @retval -2 when @a user_set is non-zero and @a user is incompatible with
 *            @a mask
 */
int
iface_resolve(unsigned mask, int user_set, enum pqos_interface user,
              enum pqos_interface *out)
{
        unsigned bit;

        if (out == NULL)
                return -1;

        mask &= IFACE_ANY;
        if (mask == 0)
                return -1;

        if (user_set && user != PQOS_INTER_AUTO) {
                bit = iface_enum_to_bit(user);
                if (bit == 0 || (mask & bit) == 0)
                        return -2;
                *out = user;
                return 0;
        }

        if (mask == IFACE_ANY) {
                /* No option narrowed the mask; keep AUTO so the library
                 * performs its own auto-detection. */
                *out = PQOS_INTER_AUTO;
                return 0;
        }

        /* Preference order: MMIO > MSR > OS. */
        if (mask & IFACE_MMIO)
                *out = PQOS_INTER_MMIO;
        else if (mask & IFACE_MSR)
                *out = PQOS_INTER_MSR;
        else if (mask & IFACE_OS)
                *out = PQOS_INTER_OS;
        else
                return -1; /* unreachable: mask is non-empty subset */

        return 0;
}

/**
 * @brief Narrow the global interface constraint mask by a per-option mask.
 *
 * AND-combines @a mask with @ref iface_constraint_mask. If the result is
 * empty, prints a descriptive error showing which options conflict and
 * exits the process with EXIT_FAILURE.
 *
 * @param [in] mask interface compatibility mask of the option being parsed
 * @param [in] opt_descr human-readable name of the option (e.g. "-p" or
 *                       "--print-mem-regions"); used in error messages
 */
static void
narrow_iface(unsigned mask, const char *opt_descr)
{
        unsigned next;
        char prev_str[32];
        char new_str[32];

        if (opt_descr == NULL)
                opt_descr = "<option>";

        mask &= IFACE_ANY;
        if (mask == 0 || mask == IFACE_ANY)
                /* IFACE_ANY adds no constraint; treat zero as a no-op too */
                return;

        next = iface_narrow(iface_constraint_mask, mask);
        if (next == 0) {
                iface_mask_to_str(iface_constraint_mask, prev_str,
                                  sizeof(prev_str));
                iface_mask_to_str(mask, new_str, sizeof(new_str));
                fprintf(stderr,
                        "Error: option '%s' requires interface '%s' but "
                        "earlier option '%s' requires '%s'. The combination "
                        "is not supported.\n",
                        opt_descr, new_str,
                        iface_constraint_origin != NULL
                            ? iface_constraint_origin
                            : "<earlier option>",
                        prev_str);
                exit(EXIT_FAILURE);
        }

        if (next != iface_constraint_mask) {
                /* Only update origin when the mask actually narrows. */
                iface_constraint_mask = next;
                iface_constraint_origin = opt_descr;
        }
}

/**
 * @brief Examine an -R / --alloc-reset argument string and narrow the
 *        interface constraint when CDP-related tokens are present.
 *
 * CDP (l3cdp-on/off, l2cdp-on/off, l3cdp-any, l2cdp-any) configuration
 * is only available through the MSR or OS interfaces today, so the mask
 * is narrowed to IFACE_MSR | IFACE_OS when any such token is detected.
 *
 * @param [in] arg raw argument string (may be NULL)
 */
static void
narrow_iface_for_reset_alloc(const char *arg)
{
        if (arg == NULL || *arg == '\0')
                return;
        /* Tokens of interest: l3cdp-*, l2cdp-*. The substring "cdp" uniquely
         * identifies them inside the comma-separated list. */
        if (strcasestr_local(arg, "cdp") != NULL)
                narrow_iface(IFACE_MSR | IFACE_OS, "-R/--alloc-reset (cdp)");
}

/**
 * @brief Examine a -m / --mon-core or --mon-uncore argument string and
 *        narrow the interface constraint when AET/uncore-energy events are
 *        requested.
 *
 * Per project requirements, cr_en, act, pow and aet event types are only
 * available through the MSR or MMIO interfaces, so the mask is narrowed
 * accordingly when any of these tokens is detected.
 *
 * @param [in] arg raw argument string (may be NULL)
 * @param [in] opt_descr description used in conflict error messages
 */
static void
narrow_iface_for_mon_events(const char *arg, const char *opt_descr)
{
        static const char *const tokens[] = {"cr_en:", "act:", "pow:", "aet:"};
        unsigned i;

        if (arg == NULL || *arg == '\0')
                return;
        for (i = 0; i < DIM(tokens); i++) {
                /* Match either as the very first token or after a separator
                 * (',' or ';') so that, for example, "all:0;aet:0" matches
                 * but a substring inside a list of cores does not. */
                const char *p = arg;
                size_t tlen = strlen(tokens[i]);

                while ((p = strcasestr_local(p, tokens[i])) != NULL) {
                        if (p == arg || p[-1] == ',' || p[-1] == ';' ||
                            p[-1] == ' ') {
                                narrow_iface(IFACE_MSR | IFACE_MMIO, opt_descr);
                                return;
                        }
                        p += tlen;
                }
        }
}

/**
 * @brief Resolve the final interface to use, given the accumulated
 *        constraint mask and any explicit user override.
 *
 * Rules:
 *   1. If the user supplied --iface / -I, ensure it is compatible with
 *      the accumulated mask; otherwise print an error and exit.
 *   2. If no constraint was added (mask == IFACE_ANY) and the user did
 *      not override, leave the interface as PQOS_INTER_AUTO so the
 *      library performs its own auto-detection (preserves legacy
 *      behaviour for callers that just run e.g. "pqos -s").
 *   3. Otherwise pick from the surviving bits in the preference order
 *      MMIO > MSR > OS.
 *
 * Side effect: updates the global @ref sel_interface.
 */
static void
resolve_interface(void)
{
        enum pqos_interface resolved = PQOS_INTER_AUTO;
        char mask_str[32];
        int ret;
        unsigned mask = iface_constraint_mask;

        /* When no explicit interface is requested and the constraint mask is
         * not IFACE_ANY, narrow the working mask to only interfaces actually
         * available on this machine.  This prevents blindly selecting MMIO
         * when the required ACPI tables are absent, allowing graceful fallback
         * to MSR or OS. */
        if (!user_interface_set && mask != IFACE_ANY) {
                enum pqos_interface avail[4];
                unsigned n = (unsigned)(sizeof(avail) / sizeof(avail[0]));
                unsigned avail_mask = 0;
                unsigned i;

                if (pqos_get_available_interfaces(avail, &n) == PQOS_RETVAL_OK
                    && n > 0) {
                        for (i = 0; i < n; i++)
                                avail_mask |= iface_enum_to_bit(avail[i]);
                        /* Only narrow if the result is non-empty. */
                        if ((mask & avail_mask) != 0)
                                mask &= avail_mask;
                }
        }

        ret = iface_resolve(mask, user_interface_set,
                            user_interface, &resolved);
        if (ret == -2) {
                iface_mask_to_str(iface_constraint_mask, mask_str,
                                  sizeof(mask_str));
                fprintf(stderr,
                        "Error: explicit interface '%s' is not "
                        "compatible with option '%s' which requires "
                        "'%s'.\n",
                        iface_enum_to_str(user_interface),
                        iface_constraint_origin != NULL
                            ? iface_constraint_origin
                            : "<earlier option>",
                        mask_str);
                exit(EXIT_FAILURE);
        }
        if (ret != 0) {
                /* Should be unreachable: the constraint mask is non-empty
                 * by construction (narrow_iface() exits on conflict). */
                fprintf(stderr,
                        "Error: failed to resolve PQoS interface from "
                        "command-line options.\n");
                exit(EXIT_FAILURE);
        }
        sel_interface = resolved;
}

/**
 * @brief Selects library OS interface
 *
 * @param arg not used
 */
static void
selfn_iface_os(const char *arg)
{
        UNUSED_ARG(arg);
        sel_interface = PQOS_INTER_OS;
        sel_interface_selected = 1;
        user_interface = PQOS_INTER_OS;
        user_interface_set = 1;
}

/**
 * @brief Selects library interface
 *
 * @param arg "auto", "msr" or "os" that sets the interface automatically,
 *            MSR interface or OS interface respectively
 */
static void
selfn_iface(const char *arg)
{
        if (arg == NULL) {
                parse_error(arg, "NULL pointer!\n");
                return;
        }

        if (strcasecmp(arg, "auto") == 0) {
                sel_interface = PQOS_INTER_AUTO;
                user_interface = PQOS_INTER_AUTO;
                user_interface_set = 0; /* auto is not an explicit override */
        } else if (strcasecmp(arg, "msr") == 0) {
                sel_interface = PQOS_INTER_MSR;
                user_interface = PQOS_INTER_MSR;
                user_interface_set = 1;
        } else if (strcasecmp(arg, "os") == 0) {
                sel_interface = PQOS_INTER_OS;
                user_interface = PQOS_INTER_OS;
                user_interface_set = 1;
        } else if (strcasecmp(arg, "mmio") == 0) {
                sel_interface = PQOS_INTER_MMIO;
                user_interface = PQOS_INTER_MMIO;
                user_interface_set = 1;
        } else {
                parse_error(arg,
                            "Unknown interface! Available options: auto, "
                            "msr, os, mmio\n");
                return;
        }

        sel_interface_selected = 1;
}

/**
 * @brief Selects printing library version
 *
 * @param arg not used
 */
static void
selfn_print_version(const char *arg)
{
        UNUSED_ARG(arg);
        sel_print_version = 1;
}

/**
 * @brief Selects displaying supported capabilities
 *
 * @param arg not used
 */
static void
selfn_print_mem_regions(const char *arg)
{
        UNUSED_ARG(arg);
        sel_print_mem_regions = 1;
}

/**
 * @brief Selects displaying available topology
 *
 * @param arg not used
 */
static void
selfn_print_topology(const char *arg)
{
        UNUSED_ARG(arg);
        sel_print_topology = 1;
}

/**
 * @brief Displays available mmio address spaces to dump
 *
 * @param arg not used
 */
static void
selfn_print_dump_info(const char *arg)
{
        UNUSED_ARG(arg);
        sel_print_dump_info = 1;
}

/**
 * @brief Dumps selected mmio address space
 *
 * @param arg not used
 */
static void
selfn_dump(const char *arg)
{
        UNUSED_ARG(arg);
        sel_dump = 1;
}

/**
 * @brief Dumps selected rmids' mmio registers
 *
 * @param arg not used
 */
static void
selfn_dump_rmid_regs(const char *arg)
{
        UNUSED_ARG(arg);
        sel_dump_rmid_regs = 1;
}

/**
 * @brief Selects displaying available I/O devices
 *
 * @param arg not used
 */
static void
selfn_print_io_devs(const char *arg)
{
        UNUSED_ARG(arg);
        sel_print_io_devs = 1;
}

/**
 * @brief Selects displaying specific I/O device
 *
 * @param arg PCI Device's Segment and BDF information
 */
static void
selfn_print_io_dev(const char *arg)
{
        char *cp = NULL, *str = NULL;
        char *saveptr = NULL;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&cp, arg);

        for (str = cp;; str = NULL) {
                char *token = NULL;

                token = strtok_r(str, ";", &saveptr);
                if (token == NULL)
                        break;

                parse_io_dev(token);
        }

        sel_print_io_dev = 1;
        free(cp);
}

/**
 * @brief Opens configuration file and parses its contents
 *
 * @param fname Name of the file with configuration parameters
 */
static void
parse_config_file(const char *fname)
{
        if (fname == NULL)
                parse_error("-f", "Invalid configuration file name!\n");

        static const struct {
                const char *option;
                void (*fn)(const char *arg);
        } optab[] = {
            /* clang-format off */

            {"show-alloc:",         selfn_show_allocation },   /**< -s */
            {"display:",            selfn_display },           /**< -d */
            {"display-verbose:",    selfn_display_verbose },   /**< -D */
            {"log-file:",           selfn_log_file },          /**< -l */
            {"verbose-mode:",       selfn_verbose_mode },      /**< -v */
            {"super-verbose-mode:", selfn_super_verbose_mode },/**< -V */
            {"alloc-class-set:",    selfn_allocation_class },  /**< -e */
            {"alloc-assoc-set:",    selfn_allocation_assoc },  /**< -a */
            {"alloc-class-select:", selfn_allocation_select }, /**< -c */
            {"monitor-pids:",       selfn_monitor_pids },      /**< -p */
            {"monitor-cores:",      selfn_monitor_cores },     /**< -m */
#ifdef PQOS_RMID_CUSTOM
            {"monitor-rmid:",          selfn_monitor_rmid_cores },
            {"monitor-rmid-channels:", selfn_monitor_rmid_channels },
#endif
            {"monitor-devs:",       selfn_monitor_devs },
            {"monitor-channels:",   selfn_monitor_channels },
            {"monitor-time:",       selfn_monitor_time },      /**< -t */
            {"monitor-interval:",   selfn_monitor_interval },  /**< -i */
            {"monitor-file:",       selfn_monitor_file },      /**< -o */
            {"monitor-file-type:",  selfn_monitor_file_type }, /**< -u */
            {"monitor-top-like:",   selfn_monitor_top_like },  /**< -T */
            {"reset-cat:",          selfn_reset_alloc },       /**< -R */
            {"iface-os:",           selfn_iface_os },          /**< -I */
            {"iface:",              selfn_iface },
            /* clang-format on */
        };
        FILE *fp = NULL;
        char cb[256];

        fp = safe_fopen(fname, "r");
        if (fp == NULL)
                parse_error(fname, "cannot open configuration file!");

        memset(cb, 0, sizeof(cb));

        while (fgets(cb, sizeof(cb) - 1, fp) != NULL) {
                int i, j, remain;
                char *cp = NULL;

                for (j = 0; j < (int)sizeof(cb) - 1; j++)
                        if (!isspace(cb[j]))
                                break;

                if (j >= (int)(sizeof(cb) - 1))
                        continue; /**< blank line */

                if (strlen(cb + j) == 0)
                        continue; /**< blank line */

                if (cb[j] == '#')
                        continue; /**< comment */

                cp = cb + j;
                remain = (int)strlen(cp);

                /**
                 * remove trailing white spaces
                 */
                for (i = (int)strlen(cp) - 1; i > 0; i--)
                        if (!isspace(cp[i])) {
                                cp[i + 1] = '\0';
                                break;
                        }

                for (i = 0; i < (int)DIM(optab); i++) {
                        int len = (int)strlen(optab[i].option);

                        if (len > remain)
                                continue;

                        if (strncasecmp(cp, optab[i].option, (size_t)len) != 0)
                                continue;

                        while (isspace(cp[len]))
                                len++; /**< skip space characters */

                        optab[i].fn(cp + len);
                        break;
                }

                if (i >= (int)DIM(optab))
                        parse_error(cp,
                                    "Unrecognized configuration file command");
        }

        fclose(fp);
}

static const char *m_cmd_name = "pqos"; /**< command name */
static const char help_printf_short[] =
    "Usage: %s [-h] [--help] [-v] [--verbose] [-V] [--super-verbose]\n"
    "       %s [--version]\n"
    "          [-l FILE] [--log-file=FILE] [-I] [--iface-os]\n"
    "          [--iface=INTERFACE]\n"
    "       %s [-s] [--show]\n"
    "       %s [-d] [--display] [-D] [--display-verbose]\n"
    "       %s [-m EVTCORES] [--mon-core=EVTCORES] |\n"
    "          [-p [EVTPIDS]] [--mon-pid[=EVTPIDS]] |\n"
    "          [--mon-uncore[=EVTUNCORE]]\n"
    "          [--mon-dev=EVTDEVICES] [--mon-channel=EVTCHANNELS]\n"
#ifdef PQOS_RMID_CUSTOM
    "          [--rmid=RMIDCORES]\n"
    "          [--rmid-channels=RMIDCORES]\n"
#endif
    "       %s [--disable-mon-ipc] [--disable-mon-llc_miss]\n"
    "          [-t SECONDS] [--mon-time=SECONDS]\n"
    "          [-i N] [--mon-interval=N]\n"
    "          [-T] [--mon-top]\n"
    "          [-o FILE] [--mon-file=FILE]\n"
    "          [-u TYPE] [--mon-file-type=TYPE]\n"
    "          [-r] [--mon-reset]\n"
    "          [-P] [--percent-llc]\n"
    "       %s [-e CLASSDEF] [--alloc-class=CLASSDEF]\n"
    "          [-a CLASS2ID] [--alloc-assoc=CLASS2ID]\n"
    "       %s [-R] [--alloc-reset]\n"
    "       %s [-H] [--profile-list] | [-c PROFILE] "
    "[--profile-set=PROFILE]\n"
    "       %s [-f FILE] [--config-file=FILE]\n";

static const char help_printf_long[] =
    "Description:\n"
    "  -h, --help                  help page\n"
    "  -v, --verbose               verbose mode\n"
    "  -V, --super-verbose         super-verbose mode\n"
    "  --version                   show PQoS library version\n"
    "  -s, --show                  show current PQoS configuration\n"
    "  -d, --display               display supported capabilities\n"
    "  -D, --display-verbose       display supported capabilities in verbose "
    "mode\n"
    "  -f FILE, --config-file=FILE load commands from selected file\n"
    "  -l FILE, --log-file=FILE    log messages into selected file\n"
    "  -e CLASSDEF, --alloc-class=CLASSDEF\n"
    "          define allocation classes.\n"
    "          CLASSDEF format is 'TYPE:ID=DEFINITION;'.\n"
    "          To specify specific resources "
    "'TYPE[@RESOURCE_ID]:ID=DEFINITION;'.\n"
    "          Examples: 'llc:0=0xffff;llc:1=0x00ff;llc@0-1:2=0xff00',\n"
    "                    'llc:0d=0xfff;llc:0c=0xfff00',\n"
    "                    'l2:2=0x3f;l2@2:1=0xf',\n"
    "                    'l2:2d=0xf;l2:2c=0xc',\n"
    "                    'mba:1=30;mba@1:3=80',\n"
    "                    'mba_max:1=4000;mba_max@1:3=6000'.\n"
    "  -a CLASS2ID, --alloc-assoc=CLASS2ID\n"
    "          associate cores/tasks with an allocation class.\n"
    "          CLASS2ID format is 'TYPE:ID=CORE_LIST/TASK_LIST'.\n"
    "          Example 'cos:0=0,2,4,6-10;llc:1=1',\n"
    "          Example 'llc:0=0,2,4,6-10;llc:1=1'.\n"
    "          Example 'core:0=0,2,4,6-10;core:1=1'.\n"
    "          Example 'pid:0=3543,7643,4556;pid:1=7644'.\n"
    "  -R [CONFIG[,CONFIG]], --alloc-reset[=CONFIG[,CONFIG]]\n"
    "          reset allocation configuration (L2/L3 CAT & MBA)\n"
    "          CONFIG can be: l3cdp-on, l3cdp-off, l3cdp-any,\n"
    "                         l2cdp-on, l2cdp-off, l2cdp-any,\n"
    "                         mbaCtrl-on, mbaCtrl-off, mbaCtrl-any\n"
    "                         l3iordt-on, l3iordt-off, l3iordt-any,\n"
    "                         mba40-on, mba40-off, mba40-any\n"
    "          (default l3cdp-any,l2cdp-any,mbaCtrl-any).\n"
    "  -m EVTCORES, --mon-core=EVTCORES\n"
    "          select cores and events for monitoring.\n"
    "          EVTCORES format is 'EVENT:CORE_LIST'.\n"
    "          EVENT is one of the following:\n"
    "              all - all default classic events\n"
    "                    (AET telemetry must be requested explicitly)\n"
    "              llc - Last Level Cache Occupancy\n"
    "              mbl - Memory Bandwidth Local\n"
    "              mbt - Memory Bandwidth Total\n"
    "              mbr - Memory Bandwidth Remote\n"
    "              llc_ref - LLC references event\n"
    "              cr_en - Core Energy (OS interface only)\n"
    "              act   - Activity Counter (OS interface only)\n"
    "              pow   - Power (delta core energy / interval, OS interface "
    "only)\n"
    "              aet   - Alias for cr_en+act+pow (OS interface only)\n"
    "          Example: \"all:0,2,4-10;llc:1,3;mbr:11-12\".\n"
    "          Cores can be grouped by enclosing them in square brackets,\n"
    "          example: \"llc:[0-3];all:[4,5,6];mbr:[0-3],7,8\".\n"
    "          AET telemetry examples (OS interface only):\n"
    "            'cr_en:0-3'    monitor core energy telemetry on cores 0-3\n"
    "            'act:0-3'      monitor activity telemetry on cores 0-3\n"
    "            'aet:0-3'      monitor core energy, activity and power on\n"
    "                           cores 0-3\n"
    "          Example commands:\n"
    "            pqos --iface=os -m cr_en:0-3\n"
    "            pqos --iface=os -m act:0-3\n"
    "            pqos --iface=os -m aet:0-3\n"
#ifdef PQOS_RMID_CUSTOM
    "  --rmid=RMIDCORES\n"
    "          assign RMID for cores\n"
    "          RMIDCORES format is 'RMID_NUMBER=CORE_LIST'\n"
    "          Example \"10=0,2,4;11=1,3,5 \"\n"
    "  --rmid-channels=RMIDCHANNELS\n"
    "          assign RMID for channels\n"
    "          RMIDCHANNELS format is 'RMID_NUMBER=CHANNEL_LIST'\n"
#endif
    "  --disable-mon-ipc\n"
    "          Disable IPC monitoring\n"
    "  --disable-mon-llc_miss\n"
    "          Disable LLC misses monitoring\n"
    "  -p [EVTPIDS], --mon-pid[=EVTPIDS]\n"
    "          select top 10 most active (CPU utilizing) process ids to "
    "monitor\n"
    "          or select process ids and events to monitor.\n"
    "          EVTPIDS format is 'EVENT:PID_LIST'.\n"
    "          Examples: 'llc:22,25673' or 'all:892,4588-4592'\n"
    "          Process's IDs can be grouped by enclosing them in square "
    "brackets,\n"
    "          Examples: 'llc:[22,25673]' or 'all:892,[4588-4592]'\n"
    "          Note:\n"
    "               Requires Linux and kernel versions 4.10 and newer.\n"
    "               The -I option must be used for PID monitoring.\n"
    "               Processes and cores cannot be monitored together.\n"
    "  --mon-dev=EVTDEVICES\n"
    "          select I/O RDT devices and events for monitoring.\n"
    "          EVTDEVICES format is 'EVENT:DEVICE_LIST'.\n"
    "          Example: \"all:0000:0010:04.0@1;llc:0000:0010:05.0\"."
    "  --mon-channel=EVTCHANNELS\n"
    "          select I/O RDT channels and events for monitoring.\n"
    "          EVTCHANNELS format is 'EVENT:CHANNEL_LIST'.\n"
    "          Channels can be grouped by enclosing them in square brackets.\n"
    "  --mon-uncore[=EVTUNCORE]\n"
    "          select sockets and uncore events for monitoring.\n"
    "          Example: all:0.\n"
    "          EVTUNCORE format is 'EVENT:SOCKET_LIST'.\n"
    "          Socket's IDs can be grouped by enclosing them in square "
    "brackets\n"
    "  -P, --percent-llc\n"
    "         Displays LLC as percentage value (by default LLC is displayed\n"
    "         in kilobytes if this parameter is not used)\n"
    "  -o FILE, --mon-file=FILE    output monitored data in a FILE\n"
    "  -u TYPE, --mon-file-type=TYPE\n"
    "          select output file format type for monitored data.\n"
    "          TYPE is one of: text (default), xml or csv.\n"
    "  -i N, --mon-interval=N      set sampling interval to Nx100ms,\n"
    "                              default 10 = 10 x 100ms = 1s.\n"
    "  -T, --mon-top               top like monitoring output\n"
    "  -t SECONDS, --mon-time=SECONDS\n"
    "          set monitoring time in seconds. Use 'inf' or 'infinite'\n"
    "          for infinite monitoring. CTRL+C stops monitoring.\n"
    "  -r [CONFIG], --mon-reset[=CONFIG]\n"
    "          reset monitoring configuration, claim all RMID's\n"
    "          CONFIG can be: l3iordt-on, l3iordt-off, l3iordt-any\n"
    "                         snc-local, snc-total, snc-any\n"
    "  -H, --profile-list          list supported allocation profiles\n"
    "  -c PROFILE, --profile-set=PROFILE\n"
    "          select a PROFILE of predefined allocation classes.\n"
    "          Use -H to list available profiles.\n"
    "  -I, --iface-os\n"
    "          set the library interface to use the kernel\n"
    "          implementation (equivalent to --iface=os). When neither\n"
    "          -I nor --iface is given, the tool now infers the\n"
    "          interface automatically from the other options on the\n"
    "          command line (preference order: mmio > msr > os).\n"
    "  --iface=INTERFACE\n"
    "          override the auto-detected library interface.\n"
    "          INTERFACE can be set to either 'auto', 'msr', 'os' or\n"
    "          'mmio'. With 'auto' (or when --iface is omitted) the\n"
    "          interface is selected automatically from the requested\n"
    "          options. If the supplied options imply two incompatible\n"
    "          interfaces, the tool fails with a clear error.\n"
    "          The auto-detection rules are:\n"
    "                  1) Options that only work under MMIO (e.g.\n"
    "                     --print-mem-regions, --print-topology, --dump,\n"
    "                     --alloc-domain-id, --alloc-mem-region, ...)\n"
    "                     force the MMIO interface.\n"
    "                  2) -p / --mon-pid (and PID-based --alloc-assoc)\n"
    "                     force the OS interface.\n"
    "                  3) --mon-channel, --mon-dev, --rmid and\n"
    "                     --rmid-channels restrict the choice to\n"
    "                     MSR or MMIO.\n"
    "                  4) --print-io-devs and --print-io-dev restrict\n"
    "                     the choice to MSR or MMIO.\n"
    "                  5) -m/--mon-uncore events cr_en, act, pow and aet\n"
    "                     restrict the choice to MSR or MMIO.\n"
    "                  6) -R / --alloc-reset values cdp-on/off and\n"
    "                     l2cdp-on/off / l3cdp-on/off restrict the\n"
    "                     choice to MSR or OS.\n"
    "                  7) When several interfaces remain valid the\n"
    "                     preference order is mmio > msr > os.\n"
    "                  8) When no option constrains the interface the\n"
    "                     library's own auto-detection is used:\n"
    "                       a) Takes RDT_IFACE environment variable\n"
    "                          into account if it is set\n"
    "                       b) Selects MMIO interface if supported\n"
    "                          (ERDT and MRRM ACPI tables present)\n"
    "                       c) Selects OS interface if the kernel\n"
    "                          interface is supported\n"
    "                       d) Selects MSR interface otherwise\n\n"
    "---------------- MMIO interface help section ----------------\n"
    "-------------------  Detect capabilities --------------------\n"
    "  --print-mem-regions         print memory mapped regions\n"
    "  Example:\n"
    "      pqos --iface=mmio --print-mem-regions\n\n"
    "  --print-topology            print available topology\n"
    "  Example:\n"
    "      pqos --iface=mmio --print-topology\n\n"
    "-------------------  Monitoring options --------------------\n"
    "  No options. Monitor all memory regions\n"
    "  Example:\n"
    "      pqos --iface=mmio -m all:0-5\n\n"
    "  --mon-mem-regions=REGIONS  monitor selected memory regions\n"
    "  REGIONS format is comma-separated list.\n"
    "  Examples:\n"
    "      pqos --iface=mmio --mon-mem-regions=0 -m all:0-5\n"
    "      pqos --iface=mmio --mon-mem-regions=3,2 -m all:0-5\n\n"
    "-------------------  Allocation options --------------------\n"
    "  --alloc-domain-id=DOMAINS domains to apply settings\n"
    "  DOMAINS format is a comma-separated list or range.\n"
    "  --alloc-mem-region=REGIONS memory regions to apply settings\n"
    "  REGIONS format is a range or a single domain.\n"
    "  --alloc-min-bw (optional) apply settings to min bandwidth\n"
    "  --alloc-max-bw (optional) apply settings to max bandwidth\n"
    "  --alloc-opt-bw (optional) apply settings to optimal bandwidth\n"
    "  General MMIO MBA command format:\n"
    "  pqos --iface=mmio --alloc-domain-id=DOMAINS --alloc-mem-region=REGIONS\n"
    "       [--alloc-min-bw] [--alloc-max-bw] [--alloc-opt-bw] -e "
    "\"CLASSDEF\"\n"
    "  CLASSDEF format is 'mba:CLOS_ID=VALUE;'\n"
    "  VALUE is in range 0-0x1FF according to Intel RDT arch specification.\n"
    "  Note: VALUE could vary depending on the platform.\n"
    "  Examples:\n"
    "      1. The CLOS 1 and CLOS 2 in domain 0 are set to 0x50 and 0x70 for "
    "memory region 0.\n"
    "         pqos --iface=mmio --alloc-domain-id=0 --alloc-mem-region=0 -e "
    "\"mba:1=0x50;mba:2=0x70;\"\n"
    "      2. The CLOS 1 and CLOS 2 in domains 0,1,2,3 are set to 0x50 and "
    "0x70 for memory region 3.\n"
    "         pqos --iface=mmio --alloc-domain-id=0,1,2,3 --alloc-mem-region=3 "
    "-e \"mba:1=0x50;mba:2=0x70;\"\n"
    "      3. The CLOS 1 and CLOS 2 in domains 0,1,2,3 are set to 0x50 and "
    "0x70 "
    "for memory region 1.\n"
    "         Only the minimum bandwidth control type is applied.\n"
    "         pqos --iface=mmio --alloc-domain-id=0-3 --alloc-mem-region=1 "
    "--alloc-min-bw -e \"mba:1=0x50;mba:2=0x70;\"\n"
    "      4. The CLOS1 MBA is set to value 80 in domains 4 and 5.\n"
    "         The CLOS2 MBA is set to value 64 in domains 4 and 5.\n"
    "         The CLOS3 MBA is set to value 85 in domains 4 and 5.\n"
    "         This is applicable to all bandwidth control types (min, max, and "
    "opt) in memory region 1.\n"
    "         pqos --iface=mmio --alloc-domain-id=4-5 --alloc-mem-region=1 "
    "--alloc-min-bw --alloc-max-bw\n"
    "         --alloc-opt-bw -e \"mba:1=80;mba:2=64;mba:3=85\"\n"
    "      5. Use multiple domain-ids and memory regions\n"
    "         pqos --iface=mmio --alloc-domain-id=0-4 --alloc-mem-region=0-3 "
    "-e \"mba:1=0x50;mba:2=0x70;\"\n\n"
    "----------------  IORDT Allocation options -----------------\n"
    "  --alloc-domain-id=DOMAINS domains to apply settings\n"
    "  DOMAINS format is a comma-separated list or range.\n"
    "  General MMIO IORDT allocation command format:\n"
    "  pqos --iface=mmio --alloc-domain-id=DOMAINS -e \"CLASSDEF\"\n"
    "  CLASSDEF format is 'llc:CLOS_ID=VALUE;'\n"
    "  Example:\n"
    "      Allocates channel 0x30000 to CLOS1 in domain 16 and allocates cache "
    "ways to CLOS1.\n"
    "      pqos --iface=mmio -R l3iordt-on\n"
    "      pqos --iface=mmio -a \"channel:1=0x30000;\"\n"
    "      pqos --iface=mmio --alloc-domain-id=16 -e \"llc:1=0xdeadbeef;\"\n"
    "      pqos --iface=mmio --alloc-domain-id=16,17 -e "
    "\"llc:15=0xdeadbeef;\"\n\n"
    "----------------  IORDT Monitoring options -----------------\n"
    "  -–mon-channel=CHANNEL a channel to monitor\n"
    "  CHANNEL format is \"mon_type: channel_id\"\n"
    "  mon_type is one of the following:\n"
    "      all - all default events\n"
    "      llc - Last Level Cache Occupancy\n"
    "      iot - I/O Total\n"
    "      iom - I/O Miss\n"
    "  General MMIO IORDT monitoring command format:\n"
    "  pqos --iface=mmio --mon-channel=CHANNEL\n"
    "  Example:\n"
    "      pqos --iface=mmio -r l3iordt-on\n"
    "      pqos --iface=mmio --mon-channel=\"all:0x30000\"\n"
    "      pqos --iface=mmio "
    "--mon-channel=\"llc:0x30000;iot:0x30000;iom:0x30000\"\n"
    "--------------- IORDT dev exploring options ---------------\n"
    "  --print-io-devs   print all IORDT devices\n"
    "  Example:\n"
    "      pqos --iface=mmio --print-io-devs\n"
    "  --print-io-dev=DEV print specific IORDT device\n"
    "  DEV format is DOMAIN:BUS:DEVICE.FUNCTION\n"
    "      DOMAIN is the PCI domain number in hexadecimal\n"
    "      BUS is the PCI bus number in hexadecimal\n"
    "      DEVICE is the PCI device number in hexadecimal\n"
    "      FUNCTION is the PCI function number in hexadecimal\n"
    "  Example:\n"
    "      pqos --iface=mmio --print-io-dev=0000:90:00.0\n\n"
    "------------------- Dump MMIO registers --------------------\n"
    "  --print-dump-info   print all available MMIO spaces\n"
    "  Example:\n"
    "      pqos --iface=mmio --print-dump-info\n"
    "  --dump print dump for a specific MMIO space\n"
    "    --dump-domain-id=DOMAIN domain to dump\n"
    "    DOMAIN format is a RDT domain number.\n"
    "    --space=SPACE MMIO space to dump\n"
    "    SPACE format is a name of the MMIO space to dump.\n"
    "        SPACE := cmrc | mmrc | marc-opt | marc-min | marc-max | cmrd | "
    "ibrd | card\n"
    "    --offset=OFFSET (optional) offset in bytes to start dumping from\n"
    "    --length=LENGTH (optional) length in bytes to dump\n"
    "    --width=WIDTH (optional) width in bits of a single dump entry\n"
    "        WIDTH := 8 | 64\n"
    "    --binary (optional) dump in binary format\n"
    "    --le (optional) dump in little-endian format\n"
    "  Example:\n"
    "      pqos --iface=mmio --dump --dump-domain-id=0 --space=cmrc --offset=0 "
    "--length=10 --width=8 --binary --le\n"
    "  --dump-rmid-regs print RMID registers for selected RMIDs\n"
    "    --dump-rmid-domain-ids=DOMAIN domain to dump\n"
    "    DOMAIN format is a RDT domain number.\n"
    "    --dump-rmid-type=TYPE type of RMID registers to dump\n"
    "    TYPE format is a name of the RMID register type to dump.\n"
    "        TYPE := cmrc | mmrc | marc-opt | marc-min | marc-max | cmrd | "
    "ibrd | card\n"
    "    --dump-rmid-mem-regions=REGIONS (optional) memory regions to dump\n"
    "    REGIONS format is a range or a single domain.\n"
    "    --dump-rmids=RMIDS (optional) RMIDs to dump\n"
    "    RMIDS format is a comma-separated list or range.\n"
    "  Example:\n"
    "      pqos --iface=mmio --dump-rmid-regs --dump-rmids=0 "
    "--dump-rmid-domain-ids=0 --dump-rmid-mem-regions=0 "
    "--dump-rmid-type=cmrc\n\n";

/* clang-format on */

/**
 * @brief Displays help information
 *
 * @param is_long print long help version or a short one
 *
 */
static void
print_help(const int is_long)
{
        printf(help_printf_short, m_cmd_name, m_cmd_name, m_cmd_name,
               m_cmd_name, m_cmd_name, m_cmd_name, m_cmd_name, m_cmd_name,
               m_cmd_name, m_cmd_name);
        if (is_long)
                printf("%s", help_printf_long);
}

/**
 * @brief Displays PQoS tool version
 */
static void
print_tool_version(void)
{
        int major = PQOS_VERSION / 10000;
        int minor = (PQOS_VERSION % 10000) / 100;
        int patch = PQOS_VERSION % 100;

        printf("PQoS Tool version: %d.%d.%d\n", major, minor, patch);
}

/**
 * @brief Displays PQoS library version
 *
 * @param [in] p_cap platform capabilities
 */
static void
print_lib_version(const struct pqos_cap *p_cap)
{
        int major = p_cap->version / 10000;
        int minor = (p_cap->version % 10000) / 100;
        int patch = p_cap->version % 100;

        printf("PQoS Library version: %d.%d.%d\n", major, minor, patch);
}

#ifdef PQOS_RMID_CUSTOM
#define OPTION_RMID          1000
#define OPTION_RMID_CHANNELS 1001
#endif
#define OPTION_DISABLE_MON_IPC       1002
#define OPTION_DISABLE_MON_LLC_MISS  1003
#define OPTION_VERSION               1004
#define OPTION_INTERFACE             1005
#define OPTION_MON_UNCORE            1006
#define OPTION_MON_DEVS              1007
#define OPTION_MON_CHANNELS          1008
#define OPTION_PRINT_MEM_REGIONS     1009
#define OPTION_PRINT_TOPOLOGY        1010
#define OPTION_MON_MEM_REGIONS       1011
#define OPTION_ALLOC_MEM_REGIONS     1012
#define OPTION_ALLOC_OPT_BW          1013
#define OPTION_ALLOC_MIN_BW          1014
#define OPTION_ALLOC_MAX_BW          1015
#define OPTION_ALLOC_DOMAIN_ID       1016
#define OPTION_PRINT_DUMP_INFO       1017
#define OPTION_DUMP                  1018
#define OPTION_DUMP_SOCKET           1019
#define OPTION_DUMP_DOMAIN_ID        1020
#define OPTION_DUMP_SPACE            1021
#define OPTION_DUMP_OFFSET           1022
#define OPTION_DUMP_LENGTH           1023
#define OPTION_DUMP_WIDTH            1024
#define OPTION_DUMP_BINARY           1025
#define OPTION_DUMP_LE               1026
#define OPTION_DUMP_RMID_REGS        1027
#define OPTION_DUMP_RMIDS            1028
#define OPTION_DUMP_RMID_DOMAIN_IDS  1029
#define OPTION_DUMP_RMID_MEM_REGIONS 1030
#define OPTION_DUMP_RMID_TYPE        1031
#define OPTION_DUMP_RMID_BINARY      1032
#define OPTION_DUMP_RMID_UPSCALING   1033
#define OPTION_PRINT_IO_DEVS         1034
#define OPTION_PRINT_IO_DEV          1035

static struct option long_cmd_opts[] = {
    /* clang-format off */
    {"help",                  no_argument,       0, 'h'},
    {"log-file",              required_argument, 0, 'l'},
    {"config-file",           required_argument, 0, 'f'},
    {"show",                  no_argument,       0, 's'},
    {"display",               no_argument,       0, 'd'},
    {"display-verbose",       no_argument,       0, 'D'},
    {"profile-list",          no_argument,       0, 'H'},
    {"profile-set",           required_argument, 0, 'c'},
    {"mon-interval",          required_argument, 0, 'i'},
    {"mon-pid",               required_argument, 0, 'p'},
    {"mon-core",              required_argument, 0, 'm'},
    {"mon-uncore",            optional_argument, 0, OPTION_MON_UNCORE},
    {"mon-dev",               required_argument, 0, OPTION_MON_DEVS},
    {"mon-channel",           required_argument, 0, OPTION_MON_CHANNELS},
    {"mon-time",              required_argument, 0, 't'},
    {"mon-top",               no_argument,       0, 'T'},
    {"mon-file",              required_argument, 0, 'o'},
    {"mon-file-type",         required_argument, 0, 'u'},
    {"mon-reset",             optional_argument, 0, 'r'},
    {"disable-mon-ipc",       no_argument,       0, OPTION_DISABLE_MON_IPC},
    {"disable-mon-llc_miss",  no_argument,       0,
                                              OPTION_DISABLE_MON_LLC_MISS},
    {"alloc-class",           required_argument, 0, 'e'},
    {"alloc-reset",           required_argument, 0, 'R'},
    {"alloc-assoc",           required_argument, 0, 'a'},
    {"verbose",               no_argument,       0, 'v'},
    {"super-verbose",         no_argument,       0, 'V'},
    {"iface-os",              no_argument,       0, 'I'},
    {"iface",                 required_argument, 0, OPTION_INTERFACE},
    {"percent-llc",           no_argument,       0, 'P'},
    {"version",               no_argument,       0, OPTION_VERSION},
#ifdef PQOS_RMID_CUSTOM
    {"rmid",                  required_argument, 0, OPTION_RMID},
    {"rmid-channels",         required_argument, 0, OPTION_RMID_CHANNELS},
#endif
    {"print-mem-regions",     no_argument,       0, OPTION_PRINT_MEM_REGIONS},
    {"print-topology",        no_argument,       0, OPTION_PRINT_TOPOLOGY},
    {"mon-mem-regions",       required_argument, 0, OPTION_MON_MEM_REGIONS},
    {"alloc-mem-regions",     required_argument, 0, OPTION_ALLOC_MEM_REGIONS},
    {"alloc-opt-bw",          no_argument,       0, OPTION_ALLOC_OPT_BW},
    {"alloc-min-bw",          no_argument,       0, OPTION_ALLOC_MIN_BW},
    {"alloc-max-bw",          no_argument,       0, OPTION_ALLOC_MAX_BW},
    {"alloc-domain-id",       required_argument, 0, OPTION_ALLOC_DOMAIN_ID},
    {"print-dump-info",       no_argument,       0, OPTION_PRINT_DUMP_INFO},
    {"dump",                  no_argument,       0, OPTION_DUMP},
    {"socket",                required_argument, 0, OPTION_DUMP_SOCKET},
    {"dump-domain-id",        required_argument, 0, OPTION_DUMP_DOMAIN_ID},
    {"space",                 required_argument, 0, OPTION_DUMP_SPACE},
    {"offset",                required_argument, 0, OPTION_DUMP_OFFSET},
    {"length",                required_argument, 0, OPTION_DUMP_LENGTH},
    {"width",                 required_argument, 0, OPTION_DUMP_WIDTH},
    {"binary",                no_argument,       0, OPTION_DUMP_BINARY},
    {"le",                    no_argument,       0, OPTION_DUMP_LE},
    {"dump-rmid-regs",        no_argument,       0, OPTION_DUMP_RMID_REGS},
    {"dump-rmids",            required_argument, 0, OPTION_DUMP_RMIDS},
    {"dump-rmid-domain-ids",  required_argument, 0,
                                              OPTION_DUMP_RMID_DOMAIN_IDS},
    {"dump-rmid-mem-regions", required_argument, 0,
                                              OPTION_DUMP_RMID_MEM_REGIONS},
    {"dump-rmid-type",        required_argument, 0, OPTION_DUMP_RMID_TYPE},
    {"dump-rmid-binary",      no_argument,       0, OPTION_DUMP_RMID_BINARY},
    {"dump-rmid-upscaling",   no_argument,       0, OPTION_DUMP_RMID_UPSCALING},
    {"print-io-devs",         no_argument,       0, OPTION_PRINT_IO_DEVS},
    {"print-io-dev",          required_argument, 0, OPTION_PRINT_IO_DEV},
    {0, 0, 0, 0} /* end */
    /* clang-format on */
};

int
main(int argc, char **argv)
{
        struct pqos_config cfg;
        const struct pqos_sysconfig *p_sys = NULL;
        const struct pqos_capability *cap_mon = NULL;
        const struct pqos_capability *cap_l3ca = NULL;
        const struct pqos_capability *cap_l2ca = NULL;
        const struct pqos_capability *cap_mba = NULL;
        const struct pqos_capability *cap_smba = NULL;
        unsigned l3cat_id_count, *l3cat_ids = NULL;
        int cmd, ret, exit_val = EXIT_SUCCESS;
        int opt_index = 0, pid_flag = 0;

        m_cmd_name = argv[0];

        memset(&cfg, 0, sizeof(cfg));

        while ((cmd = getopt_long(argc, argv,
                                  ":Hhf:i:m:Tt:l:o:u:e:c:a:p:sdDr:vVIPR:",
                                  long_cmd_opts, &opt_index)) != -1) {
                switch (cmd) {
                case 'h':
                        print_help(1);
                        return EXIT_SUCCESS;
                case 'H':
                        profile_l3ca_list();
                        return EXIT_SUCCESS;
                case OPTION_VERSION:
                        selfn_print_version(NULL);
                        break;
                case 'f':
                        if (sel_config_file != NULL) {
                                printf("Only one config file argument is "
                                       "accepted!\n");
                                return EXIT_FAILURE;
                        }
                        if (optarg == NULL)
                                return EXIT_FAILURE;
                        selfn_strdup(&sel_config_file, optarg);
                        parse_config_file(sel_config_file);
                        break;
                case 'i':
                        if (optarg == NULL)
                                return EXIT_FAILURE;
                        selfn_monitor_interval(optarg);
                        break;
                case 'p':
                        pid_flag = 1;
                        narrow_iface(IFACE_OS, "-p/--mon-pid");
                        if (optarg != NULL && *optarg == '-') {
                                /**
                                 * Next switch option wrongly assumed to be
                                 * argument to '-p'.
                                 * In order to fix it, we are handling this as
                                 * '-p' without parameters (as it should be)
                                 * to start top-pids monitoring mode.
                                 * Have to rewind \a optind as well.
                                 */
                                selfn_monitor_top_pids();
                                optind--;
                                break;
                        }
                        selfn_monitor_pids(optarg);
                        break;
                case 'P':
                        selfn_monitor_set_llc_percent();
                        break;
                case 'm':
                        narrow_iface_for_mon_events(optarg, "-m/--mon-core");
                        selfn_monitor_cores(optarg);
                        break;
                case OPTION_MON_UNCORE:
                        narrow_iface_for_mon_events(optarg, "--mon-uncore");
                        selfn_monitor_uncore(optarg);
                        break;
                case 't':
                        selfn_monitor_time(optarg);
                        break;
                case 'T':
                        selfn_monitor_top_like(NULL);
                        break;
                case 'l':
                        if (optarg == NULL)
                                return EXIT_FAILURE;
                        selfn_log_file(optarg);
                        break;
                case 'o':
                        selfn_monitor_file(optarg);
                        break;
                case 'u':
                        selfn_monitor_file_type(optarg);
                        break;
                case 'e':
                        selfn_allocation_class(optarg);
                        break;
                case 'r':
                        /*
                         * Next switch option wrongly assumed
                         * to be argument to '-r'.
                         */
                        if (optarg != NULL && *optarg == '-') {
                                selfn_reset_mon(NULL);
                                optind--;
                                break;
                        }
                        selfn_reset_mon(optarg);
                        break;
                case 'R':
                        if (optarg != NULL) {
                                if (*optarg == '-') {
                                        /**
                                         * Next switch option wrongly assumed
                                         * to be argument to '-R'.
                                         * Pass NULL as argument to
                                         * '-R' function and rewind \a optind.
                                         */
                                        selfn_reset_alloc(NULL);
                                        optind--;
                                } else {
                                        narrow_iface_for_reset_alloc(optarg);
                                        selfn_reset_alloc(optarg);
                                }
                        } else
                                selfn_reset_alloc(NULL);
                        break;
                case ':':
                        /**
                         * This is handler for missing mandatory argument
                         * (enabled by leading ':' in getopt() argument).
                         * -R and -p are only allowed switch for optional args.
                         * Other switches need to report error.
                         */
                        if (optopt == 'R') {
                                selfn_reset_alloc(NULL);
                        } else if (optopt == 'r') {
                                selfn_reset_mon(NULL);
                        } else if (optopt == 'p') {
                                /**
                                 * Top pids mode - in case of '-I -p' top N
                                 * pids (by CPU usage) will be displayed and
                                 * monitored for cache/mbm/misses
                                 */
                                selfn_monitor_top_pids();
                                pid_flag = 1;
                                narrow_iface(IFACE_OS, "-p/--mon-pid");
                        } else {
                                printf("Option -%c is missing required "
                                       "argument\n",
                                       optopt);
                                return EXIT_FAILURE;
                        }
                        break;
                case 'a':
                        selfn_allocation_assoc(optarg);
                        pid_flag |= alloc_pid_flag;
                        if (alloc_pid_flag)
                                narrow_iface(IFACE_OS,
                                             "-a/--alloc-assoc (pid)");
                        break;
                case 'c':
                        selfn_allocation_select(optarg);
                        break;
                case 's':
                        selfn_show_allocation(NULL);
                        break;
                case 'd':
                        selfn_display(NULL);
                        break;
                case 'D':
                        selfn_display_verbose(NULL);
                        break;
                case 'v':
                        selfn_verbose_mode(NULL);
                        break;
                case 'V':
                        selfn_super_verbose_mode(NULL);
                        break;
                case 'I':
                        if (sel_interface_selected) {
                                printf("Only single interface selection "
                                       "argument is accepted!\n");
                                return EXIT_FAILURE;
                        }
                        selfn_iface_os(NULL);
                        break;
                case OPTION_INTERFACE:
                        if (sel_interface_selected) {
                                printf("Only single interface selection "
                                       "argument is accepted!\n");
                                return EXIT_FAILURE;
                        }
                        if (optarg == NULL)
                                return EXIT_FAILURE;
                        selfn_iface(optarg);
                        break;
                case OPTION_DISABLE_MON_IPC:
                        selfn_monitor_disable_ipc(NULL);
                        break;
                case OPTION_DISABLE_MON_LLC_MISS:
                        selfn_monitor_disable_llc_miss(NULL);
                        break;
                case OPTION_MON_DEVS:
                        narrow_iface(IFACE_MSR | IFACE_MMIO, "--mon-dev");
                        selfn_monitor_devs(optarg);
                        break;
                case OPTION_MON_CHANNELS:
                        narrow_iface(IFACE_MSR | IFACE_MMIO, "--mon-channel");
                        selfn_monitor_channels(optarg);
                        break;
                case OPTION_MON_MEM_REGIONS:
                        narrow_iface(IFACE_MMIO, "--mon-mem-regions");
                        selfn_mon_mem_regions(optarg);
                        break;
#ifdef PQOS_RMID_CUSTOM
                case OPTION_RMID:
                        narrow_iface(IFACE_MSR | IFACE_MMIO, "--rmid");
                        selfn_monitor_rmid_cores(optarg);
                        break;
                case OPTION_RMID_CHANNELS:
                        narrow_iface(IFACE_MSR | IFACE_MMIO,
                                     "--rmid-channels");
                        selfn_monitor_rmid_channels(optarg);
                        break;
#endif
                case OPTION_PRINT_MEM_REGIONS:
                        narrow_iface(IFACE_MMIO, "--print-mem-regions");
                        selfn_print_mem_regions(NULL);
                        break;
                case OPTION_PRINT_TOPOLOGY:
                        narrow_iface(IFACE_MMIO, "--print-topology");
                        selfn_print_topology(NULL);
                        break;
                case OPTION_ALLOC_MEM_REGIONS:
                        narrow_iface(IFACE_MMIO, "--alloc-mem-regions");
                        selfn_alloc_mem_regions(optarg);
                        break;
                case OPTION_ALLOC_OPT_BW:
                        narrow_iface(IFACE_MMIO, "--alloc-opt-bw");
                        selfn_alloc_opt_bw(NULL);
                        break;
                case OPTION_ALLOC_MIN_BW:
                        narrow_iface(IFACE_MMIO, "--alloc-min-bw");
                        selfn_alloc_min_bw(NULL);
                        break;
                case OPTION_ALLOC_MAX_BW:
                        narrow_iface(IFACE_MMIO, "--alloc-max-bw");
                        selfn_alloc_max_bw(NULL);
                        break;
                case OPTION_ALLOC_DOMAIN_ID:
                        narrow_iface(IFACE_MMIO, "--alloc-domain-id");
                        selfn_alloc_domain_id(optarg);
                        break;
                case OPTION_PRINT_DUMP_INFO:
                        narrow_iface(IFACE_MMIO, "--print-dump-info");
                        selfn_print_dump_info(NULL);
                        break;
                case OPTION_DUMP:
                        narrow_iface(IFACE_MMIO, "--dump");
                        selfn_dump(NULL);
                        break;
                case OPTION_DUMP_SOCKET:
                        narrow_iface(IFACE_MMIO, "--socket");
                        selfn_dump_socket(optarg);
                        break;
                case OPTION_DUMP_DOMAIN_ID:
                        narrow_iface(IFACE_MMIO, "--dump-domain-id");
                        selfn_dump_domain_id(optarg);
                        break;
                case OPTION_DUMP_SPACE:
                        narrow_iface(IFACE_MMIO, "--space");
                        selfn_dump_space(optarg);
                        break;
                case OPTION_DUMP_WIDTH:
                        narrow_iface(IFACE_MMIO, "--width");
                        selfn_dump_width(optarg);
                        break;
                case OPTION_DUMP_BINARY:
                        narrow_iface(IFACE_MMIO, "--binary");
                        selfn_dump_binary(NULL);
                        break;
                case OPTION_DUMP_LE:
                        narrow_iface(IFACE_MMIO, "--le");
                        selfn_dump_le(NULL);
                        break;
                case OPTION_DUMP_OFFSET:
                        narrow_iface(IFACE_MMIO, "--offset");
                        selfn_dump_offset(optarg);
                        break;
                case OPTION_DUMP_LENGTH:
                        narrow_iface(IFACE_MMIO, "--length");
                        selfn_dump_length(optarg);
                        break;
                case OPTION_DUMP_RMID_REGS:
                        narrow_iface(IFACE_MMIO, "--dump-rmid-regs");
                        selfn_dump_rmid_regs(NULL);
                        break;
                case OPTION_DUMP_RMIDS:
                        narrow_iface(IFACE_MMIO, "--dump-rmids");
                        selfn_dump_rmids(optarg);
                        break;
                case OPTION_DUMP_RMID_DOMAIN_IDS:
                        narrow_iface(IFACE_MMIO, "--dump-rmid-domain-ids");
                        selfn_dump_rmid_domain_ids(optarg);
                        break;
                case OPTION_DUMP_RMID_MEM_REGIONS:
                        narrow_iface(IFACE_MMIO, "--dump-rmid-mem-regions");
                        selfn_dump_rmid_mem_regions(optarg);
                        break;
                case OPTION_DUMP_RMID_TYPE:
                        narrow_iface(IFACE_MMIO, "--dump-rmid-type");
                        selfn_dump_rmid_type(optarg);
                        break;
                case OPTION_DUMP_RMID_BINARY:
                        narrow_iface(IFACE_MMIO, "--dump-rmid-binary");
                        selfn_dump_rmid_binary(NULL);
                        break;
                case OPTION_DUMP_RMID_UPSCALING:
                        narrow_iface(IFACE_MMIO, "--dump-rmid-upscaling");
                        selfn_dump_rmid_upscaling(NULL);
                        break;
                case OPTION_PRINT_IO_DEVS:
                        narrow_iface(IFACE_MSR | IFACE_MMIO,
                                     "--print-io-devs");
                        selfn_print_io_devs(NULL);
                        break;
                case OPTION_PRINT_IO_DEV:
                        narrow_iface(IFACE_MSR | IFACE_MMIO, "--print-io-dev");
                        selfn_print_io_dev(optarg);
                        break;
                default:
                        printf("Unsupported option: -%c. "
                               "See option -h for help.\n",
                               optopt);
                        return EXIT_FAILURE;
                        break;
                case '?':
                        print_help(0);
                        return EXIT_SUCCESS;
                        break;
                }
        }

        /*
         * Resolve the final interface from the per-option constraints
         * accumulated above. This replaces the older ad-hoc check that
         * verified -p/--mon-pid implied OS only: any interface conflict
         * (PID, MMIO-only options, etc.) is now reported by
         * resolve_interface() with a unified error message.
         */
        resolve_interface();
        (void)pid_flag;
        cfg.verbose = sel_verbose_mode;
        cfg.interface = sel_interface;
        /**
         * Set up file descriptor for message log
         */
        if (sel_log_file == NULL) {
                cfg.fd_log = STDOUT_FILENO;
        } else {
                cfg.fd_log = safe_open(sel_log_file, O_WRONLY | O_CREAT,
                                       FILE_READ_WRITE);
                if (cfg.fd_log == -1) {
                        printf("Error opening %s log file!\n", sel_log_file);
                        exit_val = EXIT_FAILURE;
                        goto error_exit_1;
                }
        }

        if (sel_print_version)
                print_tool_version();

        ret = pqos_init(&cfg);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error initializing PQoS library!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_1;
        }

        ret = pqos_inter_get(&sel_interface);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error retrieving PQoS interface!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        ret = pqos_sysconfig_get(&p_sys);
        if (ret != PQOS_RETVAL_OK) {
                printf("Error retrieving PQoS capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        l3cat_ids = pqos_cpu_get_l3cat_ids(p_sys->cpu, &l3cat_id_count);
        if (l3cat_ids == NULL) {
                printf("Error retrieving CPU socket information!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        ret = pqos_cap_get_type(p_sys->cap, PQOS_CAP_TYPE_MON, &cap_mon);
        if (ret == PQOS_RETVAL_PARAM) {
                printf("Error retrieving monitoring capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        ret = pqos_cap_get_type(p_sys->cap, PQOS_CAP_TYPE_L3CA, &cap_l3ca);
        if (ret == PQOS_RETVAL_PARAM) {
                printf("Error retrieving L3 allocation capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        ret = pqos_cap_get_type(p_sys->cap, PQOS_CAP_TYPE_L2CA, &cap_l2ca);
        if (ret == PQOS_RETVAL_PARAM) {
                printf("Error retrieving L2 allocation capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        ret = pqos_cap_get_type(p_sys->cap, PQOS_CAP_TYPE_MBA, &cap_mba);
        if (ret == PQOS_RETVAL_PARAM) {
                printf("Error retrieving MB allocation capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        ret = pqos_cap_get_type(p_sys->cap, PQOS_CAP_TYPE_SMBA, &cap_smba);
        if (ret == PQOS_RETVAL_PARAM) {
                printf("Error retrieving SMBA allocation capabilities!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        if (sel_print_version) {
                print_lib_version(p_sys->cap);
                exit_val = EXIT_SUCCESS;
                goto error_exit_2;
        }

        if (sel_mon_reset && cap_mon != NULL) {
                ret = pqos_mon_reset_config(&sel_mon_config);
                if (sel_interface != PQOS_INTER_MSR &&
                    ret == PQOS_RETVAL_RESOURCE) {
                        exit_val = EXIT_FAILURE;
                        printf("Monitoring cannot be reset on systems "
                               "without resctrl monitoring capability. "
                               "Required kernel version 4.14 or newer.\n");
                } else if (ret != PQOS_RETVAL_OK) {
                        exit_val = EXIT_FAILURE;
                        printf("CMT/MBM reset failed!\n");
                } else {
                        printf("CMT/MBM reset successful\n");
                }
        }

        if (sel_reset_alloc) {
                /**
                 * Reset allocation configuration to after-reset state and exit
                 */
                ret = pqos_alloc_reset_config(&sel_alloc_config);
                if (ret != PQOS_RETVAL_OK) {
                        exit_val = EXIT_FAILURE;
                        printf("Allocation reset failed!\n");
                } else
                        printf("Allocation reset successful\n");
        }

        /**
         * Show info about allocation config and exit
         */
        if (sel_show_allocation_config) {
                if (sel_interface == PQOS_INTER_MMIO)
                        print_domain_alloc_config(cap_mon, cap_l3ca, cap_l2ca,
                                                  cap_mba, p_sys);
                else
                        alloc_print_config(cap_mon, cap_l3ca, cap_l2ca, cap_mba,
                                           cap_smba, p_sys, sel_verbose_mode);
                goto allocation_exit;
        }

        if (sel_display || sel_display_verbose) {
                /**
                 * Display info about supported capabilities
                 */
                cap_print_features(p_sys, sel_display_verbose);
                goto allocation_exit;
        }

        if (sel_print_mem_regions) {
                cap_print_mem_regions(p_sys);
                goto allocation_exit;
        }

        if (sel_print_topology) {
                cap_print_topology(p_sys);
                goto allocation_exit;
        }

        if (sel_print_dump_info) {
                pqos_print_dump_info(p_sys);
                goto allocation_exit;
        }

        if (sel_dump) {
                dump_mmio_regs(p_sys);
                goto allocation_exit;
        }

        if (sel_dump_rmid_regs) {
                dump_rmid_regs(p_sys);
                goto allocation_exit;
        }

        if (sel_print_io_devs) {
                cap_print_io_devs(p_sys);
                goto allocation_exit;
        }

        if (sel_print_io_dev) {
                cap_print_io_dev(p_sys);
                goto allocation_exit;
        }

        if (sel_allocation_profile != NULL) {
                if (profile_l3ca_apply(sel_allocation_profile, cap_l3ca) != 0) {
                        exit_val = EXIT_FAILURE;
                        goto error_exit_2;
                }
        }

        ret = alloc_apply(cap_l3ca, cap_l2ca, cap_mba, cap_smba, p_sys->cpu,
                          p_sys->dev);
        switch (ret) {
        case 0: /* nothing to apply */
                break;
        case 1: /* new allocation config applied and all is good */
                goto allocation_exit;
                break;
        case -1: /* something went wrong */
        default:
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
                break;
        }

        /**
         * If -R was present ignore all monitoring related options
         */
        if (sel_reset_alloc)
                goto allocation_exit;

        /**
         * Just monitoring option left on the table now
         */
        if (cap_mon == NULL) {
                printf("Monitoring capability not detected!\n");
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }

        if (monitor_setup(p_sys->cpu, cap_mon, p_sys->dev) != 0) {
                exit_val = EXIT_FAILURE;
                goto error_exit_2;
        }
        monitor_loop();
        monitor_stop();

allocation_exit:
error_exit_2:
        ret = pqos_fini();
        ASSERT(ret == PQOS_RETVAL_OK);
        if (ret != PQOS_RETVAL_OK)
                printf("Error shutting down PQoS library!\n");

error_exit_1:
        monitor_cleanup();

        /**
         * Close file descriptor for message log
         */
        if (sel_log_file != NULL && cfg.fd_log != -1)
                close(cfg.fd_log);

        /**
         * Free allocated memory
         */
        if (sel_allocation_profile != NULL)
                free(sel_allocation_profile);
        if (sel_log_file != NULL)
                free(sel_log_file);
        if (sel_config_file != NULL)
                free(sel_config_file);
        if (l3cat_ids != NULL)
                free(l3cat_ids);
        return exit_val;
}
