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
 */

/**
 * What --rmid-channels does with the channel groups it is given, in a utility
 * built with PQOS_RMID_CUSTOM: the option and its handler exist only in that
 * build. A group that partly overlaps one given before it is refused, and the
 * process ends with the refusal's message. grp_add() frees the description of
 * the group it refuses, and the handler used to free it a second time, which
 * glibc ends the process on.
 *
 * monitor.c is included rather than linked, so that it is compiled here with
 * PQOS_RMID_CUSTOM while every other object of the utility is not, and for the
 * selection and grp_free(), which are private to it. struct mon_group, the
 * type the flag changes, is private to it too. strdup() and free() are wrapped:
 * each copy is recorded from strdup() on, and a second free of one is counted
 * and not made. A copy is recorded until the case ends, and nothing here
 * allocates between the free of a copy and the end of the handler, so an
 * address that a free released and an allocation handed out again is not
 * taken for that copy.
 */

/* before anything is included, as the build that has the option defines it */
#define PQOS_RMID_CUSTOM

#include "test_monitor_rmid_custom.h"

#include "monitor.c"
#include "output.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

/*
 * monitor_setup() starts the groups through these two. A shared library built
 * with PQOS_RMID_CUSTOM exports them, and the one these tests link, built
 * without it, hides them; a static one defines them along with the rest of
 * its api.o. So they are wrapped rather than defined, and no case here starts
 * a group: a call fails the case.
 */
int
__wrap_pqos_mon_start_cores_ext(const unsigned num_cores,
                                const unsigned *cores,
                                const enum pqos_mon_event event,
                                void *context,
                                struct pqos_mon_mem_region *mem_region,
                                struct pqos_mon_data **group,
                                const struct pqos_mon_options *opt)
{
        (void)num_cores;
        (void)cores;
        (void)event;
        (void)context;
        (void)mem_region;
        (void)group;
        (void)opt;
        fail_msg("no case starts a group");
        return PQOS_RETVAL_ERROR;
}

int
__wrap_pqos_mon_start_channels_ext(const unsigned num_channels,
                                   const pqos_channel_t *channels,
                                   const enum pqos_mon_event event,
                                   void *context,
                                   struct pqos_mon_data **group,
                                   const struct pqos_mon_options *opt)
{
        (void)num_channels;
        (void)channels;
        (void)event;
        (void)context;
        (void)group;
        (void)opt;
        fail_msg("no case starts a group");
        return PQOS_RETVAL_ERROR;
}

/** the copies strdup() made in the case, and how often each was freed */
static struct {
        const void *ptr;
        unsigned frees;
} m_copies[16];
static unsigned m_copies_num;

/** frees of a copy that had been freed already: counted, not made */
static unsigned m_second_frees;

char *
__wrap_strdup(const char *s)
{
        char *copy = __real_strdup(s);

        if (copy != NULL) {
                assert_true(m_copies_num < DIM(m_copies));
                m_copies[m_copies_num].ptr = copy;
                m_copies[m_copies_num].frees = 0;
                m_copies_num++;
        }
        return copy;
}

void
__wrap_free(void *ptr)
{
        unsigned i;

        for (i = 0; ptr != NULL && i < m_copies_num; i++) {
                if (m_copies[i].ptr != ptr)
                        continue;
                if (m_copies[i].frees++ > 0) {
                        m_second_frees++;
                        return;
                }
                break;
        }
        __real_free(ptr);
}

/**
 * @brief Frees the selection the handler built, then forgets the copies
 */
static int
reset(void **state)
{
        unsigned i;

        (void)state;
        for (i = 0; i < sel_monitor_num; i++)
                grp_free(&sel_monitor_group[i]);
        free(sel_monitor_group);
        sel_monitor_group = NULL;
        sel_monitor_num = 0;
        sel_monitor_type = 0;
        m_copies_num = 0;
        m_second_frees = 0;
        return 0;
}

/**
 * @brief Groups that share no channel are both added, with their RMIDs
 */
static void
test_rmid_channel_groups_are_added(void **state)
{
        (void)state;

        run_void_function(selfn_monitor_rmid_channels, "1=0x10000;2=0x10001");

        assert_false(output_exit_was_called());
        assert_int_equal(sel_monitor_num, 2);
        assert_int_equal(sel_monitor_group[0].type, MON_GROUP_TYPE_CHANNEL);
        assert_string_equal(sel_monitor_group[0].desc, "0x10000");
        assert_int_equal(sel_monitor_group[0].opt.rmid.type,
                         PQOS_RMID_TYPE_MAP);
        assert_int_equal(sel_monitor_group[0].opt.rmid.rmid, 1);
        assert_string_equal(sel_monitor_group[1].desc, "0x10001");
        assert_int_equal(sel_monitor_group[1].opt.rmid.rmid, 2);
        assert_int_equal(m_second_frees, 0);
}

/**
 * @brief A group that partly overlaps an earlier one ends the process, and its
 *        description, which grp_add() frees with the group, is freed once
 */
static void
test_a_refused_rmid_channel_group_is_freed_once(void **state)
{
        (void)state;

        run_void_function(selfn_monitor_rmid_channels,
                          "1=0x10000;2=0x10001,0x10000");

        assert_true(output_exit_was_called());
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_true(output_has_text(
            "Error: cannot monitor same channels in different groups"));
        /* the group given before stays selected */
        assert_int_equal(sel_monitor_num, 1);
        assert_string_equal(sel_monitor_group[0].desc, "0x10000");
        assert_int_equal(m_second_frees, 0);
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test_teardown(test_rmid_channel_groups_are_added,
                                      reset),
            cmocka_unit_test_teardown(
                test_a_refused_rmid_channel_group_is_freed_once, reset),
        };

        return cmocka_run_group_tests(tests, NULL, NULL);
}
