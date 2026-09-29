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
 * @brief The utility's own small helpers: numbers, lists and growing arrays
 *
 * These were in main.c, which is where the command line is parsed, and they are
 * used by every other file that parses part of it - alloc.c, dump.c,
 * dump_rmids.c and monitor.c. Keeping them here makes that shared use plain and
 * keeps main.c to the command line itself.
 *
 * The move carried three fixes with it, each older than the move and each
 * with a case of its own: a range ending at UINT64_MAX never ended, because
 * "n <= end" is true again once n has wrapped; the first range stored by
 * strlisttotabrealloc() grew the table to its own length and left the next
 * token writing past the end; and strlisttotab() asked about capacity before
 * the duplicate check in one branch and after it in the other, refusing a
 * range a full table already held while accepting the same values singly.
 *
 * What is deliberately not touched is the error discipline: what these do on
 * bad input is print and exit the process, which is tracked separately,
 * because a change there is a change to every caller.
 */

#include "utils.h"

#include "common.h"
#include "main.h"
#include "types.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
                        /* the endpoint ends the loop rather than a test that
                         * has already passed it: the range is inclusive and
                         * these are 64-bit values, so "n <= end" with an end of
                         * UINT64_MAX is true again after n has wrapped to zero,
                         * and a range like UINT64_MAX-UINT64_MAX filled the
                         * table with wrapped values instead of storing one
                         */
                        for (n = start;; n++) {
                                /* capacity is asked about a value that is going
                                 * to be stored, and a duplicate is not stored:
                                 * asking first refuses a range that adds
                                 * nothing to a full table, where the same value
                                 * written on its own is accepted
                                 */
                                if (!(isdup(tab, index, n))) {
                                        if (index >= max) {
                                                printf("Exceeded parser "
                                                       "capacity of %u "
                                                       "entries\n",
                                                       max);
                                                parse_error(tmp,
                                                            "Too many groups "
                                                            "selected.\n");
                                        }
                                        tab[index] = n;
                                        index++;
                                }

                                if (n == end)
                                        break;
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
                        if (index == 0) {
                                /*
                                 * Fast path for the first range.
                                 * There are no prior values, so duplicate
                                 * checks can be skipped.
                                 */
                                uint64_t range_len;

                                if (start == 0 && end == UINT64_MAX) {
                                        printf("Too large group items.\n");
                                        exit(EXIT_FAILURE);
                                }

                                range_len = end - start + 1;

                                /* one more than the range, because what every
                                 * path here relies on is a free element at
                                 * \a index when the next token arrives: this
                                 * one fills the table and returns to the top of
                                 * the loop without the top-up the others do, so
                                 * a capacity of exactly range_len would have
                                 * the next token write past the end
                                 */
                                if (range_len >= UINT_MAX) {
                                        printf("Too large group items.\n");
                                        exit(EXIT_FAILURE);
                                }

                                while (*max <= (unsigned)range_len) {
                                        (*tab) = realloc_and_init(
                                            *tab, max, sizeof(**tab));
                                        if ((*tab) == NULL) {
                                                printf("Reallocation error!\n");
                                                exit(EXIT_FAILURE);
                                        }
                                }

                                /* the endpoint ends the loop, as in the two
                                 * ranges below and above: inclusive and 64 bit,
                                 * so a test of "n <= end" is true again once n
                                 * has wrapped past UINT64_MAX
                                 */
                                for (n = start;; n++) {
                                        (*tab)[index] = n;
                                        index++;

                                        if (n == end)
                                                break;
                                }
                                continue;
                        }
                        for (n = start;; n++) {
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

                                if (n == end)
                                        break;
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

unsigned
selfn_domain_id_list(const char *arg, uint64_t *tab, const unsigned max)
{
        char *str = NULL;
        unsigned i, n;

        if (arg == NULL)
                parse_error(arg, "NULL pointer!");

        if (*arg == '\0')
                parse_error(arg, "Empty string!");

        selfn_strdup(&str, arg);

        n = strlisttotab(str, tab, max);
        if (n == 0) {
                printf("No Domain ID specified: %s\n", str);
                exit(EXIT_FAILURE);
        }

        /* an identifier this platform cannot have */
        for (i = 0; i < n; i++) {
                if (tab[i] >= MAX_DOMAINS) {
                        printf("Domain ID out of range: %s\n", str);
                        exit(EXIT_FAILURE);
                }
        }

        /* An identifier named twice needs nothing here: strlisttotab() calls
         * isdup() before it stores anything, in both its branches, so the table
         * it returns cannot hold the same value twice and the loop the three
         * callers each carried could never run. It also could not have said
         * anything if it had: the message came after a parse_error(), which
         * does not return.
         *
         * So a repeat in the command line is accepted and collapses to one
         * selection. Whether it should instead be refused is a question about
         * the parser, which every list-taking option shares.
         */

        free(str);

        return n;
}
