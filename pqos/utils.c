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
 * The error discipline is split. The helpers the option handlers call
 * before pqos_init() - strtouint64(), strlisttotab(), strlisttotabrealloc() -
 * print what is wrong and end the process, which is all an option handler
 * would do with a refusal. Each of them is a _checked form followed by that
 * exit: strtouint64_checked(), strlisttotab_checked() and
 * strlisttotabrealloc_checked() print the same and return -1, and so do the
 * growth helpers under them, table_grow() and value_set_grow(). Those forms
 * serve the code that has to free what it holds, or reach pqos_fini(),
 * before the process ends.
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
static int
strtouint64_base_checked(const char *s, int default_base, uint64_t *value);

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

/**
 * @brief The values strlisttotabrealloc() has stored, to ask about a repeat
 *
 * isdup() scans the table, which is nothing for a list of a few cores and
 * quadratic for a PID list, where one range stands for up to MAX_PID_LIST_SIZE
 * values: every value of a range after another entry was compared with every
 * value stored before it. This set answers the same question with one probe
 * sequence, open-addressed and never more than half full, while the table
 * keeps the order the list gave. Every 64-bit value is a key, 0 and
 * UINT64_MAX among them, so whether a slot is taken is kept apart from what it
 * holds.
 */
struct value_set {
        uint64_t *value;
        unsigned char *taken;
        size_t size;  /**< how many slots, a power of two or 0 */
        size_t count; /**< how many are taken */
};

/**
 * @brief The slot a value's probe sequence starts at
 *
 * The SplitMix64 finaliser, so that neighbouring values, which is what a range
 * is, start far apart rather than in one run of taken slots.
 */
static size_t
value_slot(uint64_t value, const size_t size)
{
        value ^= value >> 30;
        value *= UINT64_C(0xbf58476d1ce4e5b9);
        value ^= value >> 27;
        value *= UINT64_C(0x94d049bb133111eb);
        value ^= value >> 31;

        return (size_t)value & (size - 1);
}

/**
 * @brief The slot that holds a value, or the free one where it would go
 */
static size_t
value_find(const struct value_set *set, const uint64_t value)
{
        size_t i = value_slot(value, set->size);

        while (set->taken[i] && set->value[i] != value)
                i = (i + 1) & (set->size - 1);

        return i;
}

/**
 * @brief Doubles the set, or makes its first slots
 *
 * @return 0 on success; -1 out of memory, having said so, with the set as it
 *         was
 */
static int
value_set_grow(struct value_set *set)
{
        struct value_set grown = {NULL, NULL, 0, set->count};
        size_t i;

        grown.size = set->size == 0 ? 64 : set->size * 2;
        if (grown.size < set->size || grown.size > SIZE_MAX / sizeof(uint64_t))
                grown.size = 0;
        if (grown.size != 0) {
                grown.value = calloc(grown.size, sizeof(*grown.value));
                grown.taken = calloc(grown.size, sizeof(*grown.taken));
        }
        if (grown.value == NULL || grown.taken == NULL) {
                printf("Reallocation error!\n");
                free(grown.value);
                free(grown.taken);
                return -1;
        }

        for (i = 0; i < set->size; i++)
                if (set->taken[i]) {
                        const size_t j = value_find(&grown, set->value[i]);

                        grown.taken[j] = 1;
                        grown.value[j] = set->value[i];
                }

        free(set->value);
        free(set->taken);
        *set = grown;
        return 0;
}

/**
 * @brief Adds a value to the set, unless it is there already
 *
 * @return 1 where it was added, 0 where it is a repeat, -1 where the set could
 *         not grow to take it, having said so
 */
static int
value_set_add(struct value_set *set, const uint64_t value)
{
        size_t i = 0;

        if (set->size != 0) {
                i = value_find(set, value);
                if (set->taken[i])
                        return 0;
        }

        /* at most half full, so a probe sequence ends soon at a free slot */
        if ((set->count + 1) * 2 > set->size) {
                if (value_set_grow(set) != 0)
                        return -1;
                i = value_find(set, value);
        }

        set->taken[i] = 1;
        set->value[i] = value;
        set->count++;
        return 1;
}

/**
 * @brief What strlisttotabrealloc() has stored of a list: the range that was
 *        its first token, where it began with one, and a set of the rest
 *
 * The first range is kept as its two ends rather than value by value. Nothing
 * before it can repeat it, and a list of one range - "1-90000" - never asks
 * about a repeat at all, so it costs no more than the table, as it did before
 * the set: the set is made only for the values after it.
 */
struct list_seen {
        int first;             /**< whether the list began with a range */
        uint64_t first_start;  /**< its lower end */
        uint64_t first_end;    /**< its upper end */
        struct value_set rest; /**< every other value stored */
};

/**
 * @brief Whether a value is new to the list, recording it where it is
 *
 * @return 1 where it is new, 0 where it is a repeat, -1 where it could not be
 *         recorded, having said so
 */
static int
list_seen_add(struct list_seen *seen, const uint64_t value)
{
        if (seen->first && seen->first_start <= value &&
            value <= seen->first_end)
                return 0;

        return value_set_add(&seen->rest, value);
}

uint64_t
strtouint64(const char *s)
{
        return strtouint64_base(s, 10);
}

int
strtouint64_checked(const char *s, uint64_t *value)
{
        return strtouint64_base_checked(s, 10, value);
}

uint64_t
strhextouint64(const char *s)
{
        return strtouint64_base(s, 16);
}

/**
 * @brief The reading strtouint64() and strhextouint64() share, which says what
 *        is wrong and returns -1 rather than ending the process
 */
static int
strtouint64_base_checked(const char *s, int default_base, uint64_t *value)
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
                return -1;
        }

        if (endptr == s) {
                printf("No digits were found\n");
                return -1;
        }

        if (!(*s != '\0' && *endptr == '\0')) {
                printf("Error converting '%s' to unsigned number!\n", str);
                return -1;
        }

        *value = n;
        return 0;
}

static uint64_t
strtouint64_base(const char *s, int default_base)
{
        uint64_t n = 0;

        if (strtouint64_base_checked(s, default_base, &n) != 0)
                exit(EXIT_FAILURE);

        return n;
}

/**
 * @brief Strips the whitespace around one element of a list, in place
 *
 * @param [in] str the element
 *
 * @return the element without it
 */
static char *
list_trim(char *str)
{
        char *end;

        while (isspace((unsigned char)*str))
                str++;

        end = str + strlen(str);
        while (end > str && isspace((unsigned char)end[-1]))
                end--;
        *end = '\0';

        return str;
}

/**
 * @brief Reads one number of a list element
 *
 * pqos_parse_uint64(), which refuses a sign, and where the caller asks for it
 * a leading '+' before a decimal number besides: the options strlisttotab()
 * reads have always accepted one, since strtoull() does, and --mon-dev, which
 * parsed with pqos_parse_uint64(), never has. Before a decimal number only,
 * because that is the form those options took: their parser looked for "0x"
 * first and read anything else as decimal, so "+0x5" was a "+0" with a stray
 * "x5" after it. "-" stays refused because it separates the ends of a range.
 *
 * @param [in] text the number
 * @param [out] value what it reads as
 * @param [in] plus whether a leading '+' is accepted
 *
 * @return 0 on success, -1 where it is not a number
 */
static int
list_number(const char *text, uint64_t *value, const int plus)
{
        while (isspace((unsigned char)*text))
                text++;
        if (plus && text[0] == '+' && isdigit((unsigned char)text[1]) &&
            !(text[1] == '0' && (text[2] == 'x' || text[2] == 'X')))
                text++;

        return pqos_parse_uint64(text, value);
}

int
list_parse(char *s,
           uint64_t *tab,
           const unsigned max,
           const uint64_t limit,
           const unsigned flags,
           struct list_fault *fault)
{
        struct list_fault unused;
        const int plus = (flags & LIST_PLUS) != 0;
        char *saveptr = NULL;
        char *token;
        unsigned count = 0;

        if (fault == NULL)
                fault = &unused;
        fault->token = NULL;
        fault->value = 0;

        if (s == NULL || tab == NULL || max == 0 || max > INT_MAX)
                return 0;

        for (token = strtok_r(s, ",", &saveptr); token != NULL;
             token = strtok_r(NULL, ",", &saveptr)) {
                uint64_t start;
                uint64_t end;
                uint64_t n;
                char *dash;

                token = list_trim(token);
                if (*token == '\0')
                        continue;

                /* a range is two numbers and one dash between them, which the
                 * number reader enforces: it refuses an empty side, a sign,
                 * and a second dash left in the end of the range
                 */
                fault->token = token;
                dash = strchr(token, '-');
                if (dash != NULL) {
                        *dash = '\0';
                        if (list_number(token, &start, plus) != 0 ||
                            list_number(dash + 1, &end, plus) != 0) {
                                *dash = '-';
                                return LIST_SYNTAX;
                        }
                        *dash = '-';
                } else if (list_number(token, &start, plus) != 0)
                        return LIST_SYNTAX;
                else
                        end = start;

                /* the limit before anything is stored: a value that does not
                 * exist is the fault, and reporting the table full or a repeat
                 * instead would name the wrong one
                 */
                if (start > limit || end > limit) {
                        fault->value = start > limit ? start : end;
                        return LIST_RANGE;
                }

                if (start > end && !(flags & LIST_KEEP_ORDER)) {
                        n = start;
                        start = end;
                        end = n;
                }

                /* the endpoint ends the loop rather than a test that has
                 * already passed it: the range is inclusive and these are
                 * 64-bit values, so "n <= end" with an end of UINT64_MAX is
                 * true again after n has wrapped to zero, and "n >= end" with
                 * an end of 0 after n has wrapped to UINT64_MAX. And capacity
                 * is asked about a value that is going to be stored, so a
                 * repeat needs no room in the table
                 */
                for (n = start;; n = start <= end ? n + 1 : n - 1) {
                        if (isdup(tab, count, n)) {
                                if (flags & LIST_UNIQUE) {
                                        fault->value = n;
                                        return LIST_DUPLICATE;
                                }
                        } else {
                                if (count >= max)
                                        return LIST_FULL;
                                tab[count++] = n;
                        }

                        if (n == end)
                                break;
                }
        }

        fault->token = NULL;
        return (int)count;
}

int
strlisttotab_checked(char *s, uint64_t *tab, const unsigned max)
{
        struct list_fault fault;
        char *tmp = NULL;
        int n;

        if (s == NULL || tab == NULL || max == 0)
                return 0;

        /* the list as the user gave it, for the message: list_parse()
         * tokenises its argument in place
         */
        tmp = strdup(s);
        if (tmp == NULL) {
                printf("Failed to allocate memory for argument copy!\n");
                return -1;
        }

        n = list_parse(s, tab, max, UINT64_MAX, LIST_PLUS, &fault);
        if (n == LIST_FULL) {
                printf("Exceeded parser capacity of %u entries\n", max);
                parse_error_print(tmp, "Too many groups selected.\n");
        } else if (n < 0)
                printf("Error converting '%s' to unsigned number!\n",
                       fault.token != NULL ? fault.token : tmp);

        free(tmp);
        return n < 0 ? -1 : n;
}

unsigned
strlisttotab(char *s, uint64_t *tab, const unsigned max)
{
        const int n = strlisttotab_checked(s, tab, max);

        if (n < 0)
                exit(EXIT_FAILURE);

        return (unsigned)n;
}

/**
 * @brief Doubles the table strlisttotabrealloc_checked() fills
 *
 * The count of what the table holds is returned as an int, so the table is
 * never grown past INT_MAX elements: realloc_and_init() would double one of
 * up to UINT_MAX / 2.
 *
 * @return 0 on success; -1 where it could not grow, having said so, with the
 *         table and its size as they were, for the caller to free
 */
static int
table_grow(uint64_t **tab, unsigned *max)
{
        uint64_t *grown;

        if (*max > (unsigned)INT_MAX / 2) {
                printf("Too large group items.\n");
                return -1;
        }

        grown = realloc_and_init(*tab, max, sizeof(**tab));
        if (grown == NULL) {
                printf("Reallocation error!\n");
                return -1;
        }
        *tab = grown;
        return 0;
}

int
strlisttotabrealloc_checked(char *s, uint64_t **tab, unsigned *max)
{
        struct list_seen seen = {0, 0, 0, {NULL, NULL, 0, 0}};
        unsigned index = 0;
        char *saveptr = NULL;

        if (s == NULL || (*tab) == NULL || *max == 0)
                return 0;

        /* the count is returned as an int, and the table only ever holds
         * fewer elements than its size: a table given larger than INT_MAX
         * could take a count the return turns negative, so it is refused as
         * table_grow() refuses to grow one that far
         */
        if (*max > (unsigned)INT_MAX) {
                printf("Too large group items.\n");
                return -1;
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
                        if (strtouint64_checked(token, &start) != 0)
                                goto failed;
                        if (*(p + 1) == '\0') {
                                parse_error_print(
                                    token,
                                    "Incomplete cores association format");
                                goto failed;
                        }

                        if (!(*(p + 1) >= '0' && *(p + 1) <= '9')) {
                                parse_error_print(
                                    p + 1, "Invalid cores association format");
                                goto failed;
                        }

                        if (strtouint64_checked(p + 1, &end) != 0)
                                goto failed;
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
                                goto failed;
                        }
                        if (index == 0) {
                                /*
                                 * Fast path for the first range.
                                 * There are no prior values, so none of its
                                 * own is a repeat. The tokens after it ask
                                 * about its two ends, not about a set.
                                 */
                                uint64_t range_len;

                                if (start == 0 && end == UINT64_MAX) {
                                        printf("Too large group items.\n");
                                        goto failed;
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
                                        goto failed;
                                }

                                while (*max <= (unsigned)range_len) {
                                        if (table_grow(tab, max) != 0)
                                                goto failed;
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
                                seen.first = 1;
                                seen.first_start = start;
                                seen.first_end = end;
                                continue;
                        }
                        for (n = start;; n++) {
                                const int added = list_seen_add(&seen, n);

                                if (added < 0)
                                        goto failed;
                                if (added) {
                                        (*tab)[index] = n;
                                        index++;
                                }
                                if (index >= *max && table_grow(tab, max) != 0)
                                        goto failed;

                                if (n == end)
                                        break;
                        }
                } else {
                        /**
                         * single number provided here
                         * remove duplicates if necessary
                         */
                        uint64_t val = 0;
                        int added;

                        if (strtouint64_checked(token, &val) != 0)
                                goto failed;

                        added = list_seen_add(&seen, val);
                        if (added < 0)
                                goto failed;
                        if (added) {
                                (*tab)[index] = val;
                                index++;
                        }
                        if (index >= *max && table_grow(tab, max) != 0)
                                goto failed;
                }
        }

        free(seen.rest.value);
        free(seen.rest.taken);
        /* below the table's size, which table_grow() keeps within an int */
        return (int)index;

failed:
        free(seen.rest.value);
        free(seen.rest.taken);
        return -1;
}

unsigned
strlisttotabrealloc(char *s, uint64_t **tab, unsigned *max)
{
        const int n = strlisttotabrealloc_checked(s, tab, max);

        if (n < 0)
                exit(EXIT_FAILURE);

        return (unsigned)n;
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
print_out_of_range(const char *what, const uint64_t value, const char *list)
{
        /* a list of one value is printed as typed, which is how the line has
         * always read - "0x8" stays "0x8". A longer one gets the value at
         * fault and the list as typed, so the user can find the value in it
         */
        if (strchr(list, ',') != NULL || strchr(list, '-') != NULL)
                printf("%s out of range: %" PRIu64 " in %s\n", what, value,
                       list);
        else
                printf("%s out of range: %s\n", what, list);
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

        /* arg and not str in the messages: strlisttotab() leaves str holding
         * only the first element of the list
         */
        n = strlisttotab(str, tab, max);
        if (n == 0) {
                printf("No Domain ID specified: %s\n", arg);
                exit(EXIT_FAILURE);
        }

        /* an identifier this platform cannot have */
        for (i = 0; i < n; i++) {
                if (tab[i] >= MAX_DOMAINS) {
                        print_out_of_range("Domain ID", tab[i], arg);
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
