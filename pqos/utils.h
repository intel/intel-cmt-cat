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
 * Declared here rather than in main.h because every file that parses part of
 * the command line uses them, and main.h is about the command line itself. The
 * definitions are in utils.c, with the three range and capacity fixes the move
 * brought with it; utils.c says what they are.
 */

#ifndef __PQOS_APP_UTILS_H__
#define __PQOS_APP_UTILS_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converts string of characters into numeric value
 *
 * A "0x" prefix selects base sixteen, and a string that is not a number at all
 * ends the process.
 *
 * @param [in] s string to be converted
 *
 * @return Numeric value of the string representing the number
 */
uint64_t strtouint64(const char *s);

/**
 * @brief Converts a string of characters into numeric value, base sixteen
 *
 * @param [in] s string to be converted
 *
 * @return Numeric value of the string representing the number
 */
uint64_t strhextouint64(const char *s);

/**
 * @brief Converts a comma separated list into a table of numbers
 *
 * Accepts single numbers and inclusive ranges, in either order - "5-1" is the
 * same list as "1-5" - and drops duplicates. A list longer than \a max entries,
 * or an element that is not a number, ends the process.
 *
 * @param [in] s the list, which this modifies while parsing
 * @param [out] tab where the numbers go
 * @param [in] max how many \a tab holds
 *
 * @return Number of elements placed into \a tab
 */
unsigned strlisttotab(char *s, uint64_t *tab, const unsigned max);

/**
 * @brief Converts a comma separated list into a table that grows to fit it
 *
 * As strlisttotab(), except that the table is grown rather than the list being
 * refused for being too long.
 *
 * @param [in] s the list, which this modifies while parsing
 * @param [in,out] tab where the numbers go, reallocated as needed
 * @param [in,out] max how many \a tab holds, updated when it grows
 *
 * @return Number of elements placed into \a tab
 */
unsigned strlisttotabrealloc(char *s, uint64_t **tab, unsigned *max);

/**
 * @brief Doubles an array, zeroing what it gains
 *
 * @param [in] ptr the array, or NULL for the first allocation
 * @param [in,out] elem_count how many elements it holds, updated on success
 * @param [in] elem_size the size of one element
 *
 * @return The array, or NULL where it could not be grown - in which case the
 *         caller's array is unchanged and still has to be freed
 */
void *realloc_and_init(void *ptr, unsigned *elem_count, const size_t elem_size);

/**
 * @brief Reads a domain identifier list given on the command line
 *
 * Three options take one - --dump-domain-id, --dump-rmid-domain-ids and
 * --alloc-domain-id - and each had its own copy of this, identical apart from
 * the array it filled and a word in one message. What the list has to be is the
 * same for all three: present, and numbers this platform could have. Ends the
 * process with a message where it is neither, which is what the three copies
 * did.
 *
 * A repeat is not refused. strlisttotab() calls isdup() before it stores
 * anything, so an identifier named twice is stored once and the count returned
 * is the count of distinct identifiers - "0,0,1" selects two. The three copies
 * each carried a duplicate check after that parse, which could therefore never
 * fire, and this does not carry it on; whether a repeat should instead be an
 * error is a question about the parser that every list-taking option shares.
 *
 * @param [in] arg the list, as the option received it
 * @param [out] tab where the identifiers go
 * @param [in] max how many \a tab holds
 *
 * @return Number of identifiers placed into \a tab, which is never zero
 */
unsigned selfn_domain_id_list(const char *arg, uint64_t *tab, unsigned max);

#ifdef __cplusplus
}
#endif

#endif /* __PQOS_APP_UTILS_H__ */
