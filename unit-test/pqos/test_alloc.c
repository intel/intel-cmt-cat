/*
 * BSD LICENSE
 *
 * Copyright(c) 2022-2023 Intel Corporation. All rights reserved.
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
#include "caps_gen.h"
#include "output.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
/* clang-format off */
#include <cmocka.h>
#include "alloc.c"
/* clang-format on */

static void
test_selfn_allocation_assoc_negative(void **state)
{
        run_void_function(selfn_allocation_assoc, NULL);
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"<null>\" command line argument. NULL pointer!\n");

        run_void_function(selfn_allocation_assoc, "");
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"\" command line argument. Empty string!\n");

        run_void_function(selfn_allocation_assoc, "badalloctype:1=0,3-5;");
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"badalloctype:1=0,3-5\" command line argument. "
            "Unrecognized allocation type\n");

        run_void_function(selfn_allocation_assoc, "core:0,3-5;");
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"0,3-5\" command line argument. Invalid allocation "
            "class of service association format\n");

        run_void_function(selfn_allocation_assoc, "pid:0,3-5;");
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"0,3-5\" command line argument. Invalid allocation "
            "class of service association format\n");

        (void)state; /* unused */
}

static void
test_selfn_allocation_assoc_llc(void **state)
{
        run_void_function(selfn_allocation_assoc, "llc:1=0,3-5;llc:2=1,2;");
        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(sel_assoc_pid_num, 0);
        assert_int_equal(sel_assoc_core_num, 6);
        assert_int_equal(alloc_pid_flag, 0);
        assert_int_equal(sel_assoc_tab[0].core, 0);
        assert_int_equal(sel_assoc_tab[0].class_id, 1);
        assert_int_equal(sel_assoc_tab[1].core, 3);
        assert_int_equal(sel_assoc_tab[1].class_id, 1);
        assert_int_equal(sel_assoc_tab[2].core, 4);
        assert_int_equal(sel_assoc_tab[2].class_id, 1);
        assert_int_equal(sel_assoc_tab[3].core, 5);
        assert_int_equal(sel_assoc_tab[3].class_id, 1);
        assert_int_equal(sel_assoc_tab[4].core, 1);
        assert_int_equal(sel_assoc_tab[4].class_id, 2);
        assert_int_equal(sel_assoc_tab[5].core, 2);
        assert_int_equal(sel_assoc_tab[4].class_id, 2);
        (void)state; /* unused */
}

static void
test_selfn_allocation_assoc_pid(void **state)
{
        run_void_function(selfn_allocation_assoc, "pid:1=3019;pid:2=3-5,30;");
        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(sel_assoc_pid_num, 5);
        assert_int_equal(sel_assoc_core_num, 0);
        assert_int_equal(alloc_pid_flag, 1);
        assert_int_equal(sel_assoc_pid_tab[0].task_id, 3019);
        assert_int_equal(sel_assoc_pid_tab[0].class_id, 1);
        assert_int_equal(sel_assoc_pid_tab[1].task_id, 3);
        assert_int_equal(sel_assoc_pid_tab[1].class_id, 2);
        assert_int_equal(sel_assoc_pid_tab[2].task_id, 4);
        assert_int_equal(sel_assoc_pid_tab[2].class_id, 2);
        assert_int_equal(sel_assoc_pid_tab[3].task_id, 5);
        assert_int_equal(sel_assoc_pid_tab[3].class_id, 2);
        assert_int_equal(sel_assoc_pid_tab[4].task_id, 30);
        assert_int_equal(sel_assoc_pid_tab[4].class_id, 2);
        (void)state; /* unused */
}

static void
test_selfn_allocation_assoc_core(void **state)
{
        run_void_function(selfn_allocation_assoc, "core:1=0-2,5;core:2=3,4;");
        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(sel_assoc_pid_num, 0);
        assert_int_equal(sel_assoc_core_num, 6);
        assert_int_equal(alloc_pid_flag, 0);
        assert_int_equal(sel_assoc_tab[0].core, 0);
        assert_int_equal(sel_assoc_tab[0].class_id, 1);
        assert_int_equal(sel_assoc_tab[1].core, 1);
        assert_int_equal(sel_assoc_tab[1].class_id, 1);
        assert_int_equal(sel_assoc_tab[2].core, 2);
        assert_int_equal(sel_assoc_tab[2].class_id, 1);
        assert_int_equal(sel_assoc_tab[3].core, 5);
        assert_int_equal(sel_assoc_tab[3].class_id, 1);
        assert_int_equal(sel_assoc_tab[4].core, 3);
        assert_int_equal(sel_assoc_tab[4].class_id, 2);
        assert_int_equal(sel_assoc_tab[4].core, 3);
        assert_int_equal(sel_assoc_tab[4].class_id, 2);
        (void)state; /* unused */
}

static void
test_selfn_allocation_assoc_cos(void **state)
{
        run_void_function(selfn_allocation_assoc, "cos:2=1-3;cos:7=3,4;");
        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(sel_assoc_pid_num, 0);
        assert_int_equal(sel_assoc_core_num, 4);
        assert_int_equal(alloc_pid_flag, 0);
        assert_int_equal(sel_assoc_tab[0].core, 1);
        assert_int_equal(sel_assoc_tab[0].class_id, 2);
        assert_int_equal(sel_assoc_tab[1].core, 2);
        assert_int_equal(sel_assoc_tab[1].class_id, 2);
        assert_int_equal(sel_assoc_tab[2].core, 3);
        assert_int_equal(sel_assoc_tab[2].class_id, 7);
        assert_int_equal(sel_assoc_tab[3].core, 4);
        assert_int_equal(sel_assoc_tab[3].class_id, 7);
        (void)state; /* unused */
}

static void
test_selfn_allocation_class_negative(void **state)
{
        run_void_function(selfn_allocation_class, NULL);
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"<null>\" command line argument. NULL pointer!\n");

        run_void_function(selfn_allocation_class, "");
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(
            output_get(),
            "Error parsing \"\" command line argument. Empty string!\n");

        const char *param = "llc:1=0x00ff;llc:2=0x3f00;llc:3=0xf000;"
                            "llc:4=0x00ff;llc:5=0x00ff;llc:6=0x3f00;"
                            "llc:7=0xf000;llc:8=0x00ff;llc:9=0x00ff;"
                            "llc:10=0x3f00;llc:11=0xf000;llc:12=0x00ff;"
                            "llc:13=0x3f00;llc:14=0xf000;llc:14=0x3f00;"
                            "llc:15=0xf000; llc:16=0xf000;mba:1=50;mba:2=70;"
                            "mba:3=50;mba:4=70;mba:5=50;mba:6=50;mba:7=70;"
                            "mba:8=50;mba:9=70;mba:10=50;mba:11=50;mba:12=70;"
                            "mba:13=50;mba:14=70;mba:15=50;mba:15=50;";
        char expected_output[512];

        sprintf(expected_output,
                "Error parsing \"%s\" command line argument. Too many "
                "allocation options!\n",
                param);
        run_void_function(selfn_allocation_class, param);
        assert_int_equal(output_exit_was_called(), 1);
        assert_int_equal(output_get_exit_status(), EXIT_FAILURE);
        assert_string_equal(output_get(), expected_output);
        (void)state; /* unused */
}

static void
test_selfn_allocation_class(void **state)
{
        run_void_function(selfn_allocation_class,
                          "llc:1=0x000f;llc@0,1:2=0x0ff0;llc@2-3:3=0x3c;"
                          "mba:1=80;mba@0,1:2=64;mba@2-3:3=85;");
        assert_int_equal(output_exit_was_called(), 0);
        assert_int_equal(sel_alloc_opt_num, 6);
        assert_string_equal(alloc_opts[0], "llc:1=0x000f");
        assert_string_equal(alloc_opts[1], "llc@0,1:2=0x0ff0");
        assert_string_equal(alloc_opts[2], "llc@2-3:3=0x3c");
        assert_string_equal(alloc_opts[3], "mba:1=80");
        assert_string_equal(alloc_opts[4], "mba@0,1:2=64");
        assert_string_equal(alloc_opts[5], "mba@2-3:3=85");
        (void)state; /* unused */
}

static void
test_alloc_print_config_negative(void **state)
{
        run_void_function(alloc_print_config, NULL, NULL, NULL, NULL, NULL,
                          NULL, 0);
        assert_string_equal(output_get(),
                            "Error retrieving information for Sockets\n");

        (void)state; /* unused */
}

static void
test_alloc_print_config_msr(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned i;
        unsigned num_l3ca = data->cap_l3ca->u.l3ca->num_classes;
        unsigned num_l2ca = data->cap_l2ca->u.l2ca->num_classes;
        unsigned num_mba = data->cap_mba->u.mba->num_classes;
        struct pqos_l3ca *l3ca;
        uint64_t ways_mask = 0xf;
        struct pqos_l2ca *l2ca;
        struct pqos_mba *mba;
        unsigned mb_max = 10;

        /* generate data */
        l3ca = calloc(num_l3ca, sizeof(struct pqos_l3ca));
        assert_non_null(l3ca);
        for (i = 0; i < num_l3ca; i++) {
                l3ca[i].class_id = i;
                l3ca[i].cdp = 0;
                l3ca[i].u.ways_mask = ways_mask;
                ways_mask = ways_mask | ways_mask << 1;
        }
        l2ca = calloc(num_l2ca, sizeof(struct pqos_l2ca));
        assert_non_null(l2ca);
        ways_mask = 0x1;
        for (i = 0; i < num_l2ca; i++) {
                l2ca[i].class_id = i;
                l2ca[i].cdp = 0;
                l2ca[i].u.ways_mask = ways_mask;
                ways_mask = ways_mask | ways_mask << 1;
        }
        mba = calloc(num_mba, sizeof(struct pqos_mba));
        assert_non_null(mba);
        for (i = 0; i < num_mba; i++) {
                mba[i].class_id = i;
                mba[i].mb_max = mb_max;
                mba[i].ctrl = 0;
                mb_max += 10;
                if (mb_max > 100)
                        mb_max = 10;
        }

        /* mock pqos_l3ca_get*/
        for (i = 0; i < data->num_socket; i++) {
                expect_value(__wrap_pqos_l3ca_get, l3cat_id, i);
                expect_value(__wrap_pqos_l3ca_get, max_num_ca, num_l3ca);
                will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_l3ca_get, num_l3ca);
                will_return(__wrap_pqos_l3ca_get, l3ca);
        }

        /* mock pqos_l2ca_get*/
        for (i = 0; i < data->cpu_info->num_cores / 2; i++) {
                expect_value(__wrap_pqos_l2ca_get, l2id, i);
                expect_value(__wrap_pqos_l2ca_get, max_num_ca,
                             PQOS_MAX_L2CA_COS);
                will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_l2ca_get, num_l2ca);
                will_return(__wrap_pqos_l2ca_get, l2ca);
        }

        /* mock pqos_mba_get */
        for (i = 0; i < data->num_socket; i++) {
                expect_value(__wrap_pqos_mba_get, mba_id, i);
                expect_value(__wrap_pqos_mba_get, max_num_cos, num_mba);
                will_return(__wrap_pqos_mba_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_mba_get, num_mba);
                will_return(__wrap_pqos_mba_get, mba);
        }

        /* mock pqos_inter_get */
        for (i = 0; i < data->cpu_info->num_cores; i++) {
                will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_inter_get, PQOS_INTER_MSR);
        }

        /* mock pqos_alloc_assoc_get */
        for (i = 0; i < data->cpu_info->num_cores; i++) {
                expect_value(__wrap_pqos_alloc_assoc_get, lcore, i);
                will_return(__wrap_pqos_alloc_assoc_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_alloc_assoc_get, i % 16);
        }

        /* mock pqos_mon_assoc_get */
        for (i = 0; i < data->cpu_info->num_cores; i++) {
                expect_value(__wrap_pqos_mon_assoc_get, lcore, i);
                will_return(__wrap_pqos_mon_assoc_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_mon_assoc_get,
                            data->cpu_info->num_cores - i);
        }

        run_void_function(alloc_print_config, data->cap_mon, data->cap_l3ca,
                          data->cap_l2ca, data->cap_mba, data->cap_smba,
                          data->cpu_info, 1);

        /* check output */
        assert_true(
            output_has_text("L3CA/MBA COS definitions for Socket 0:\n"
                            "    L3CA COS0 => MASK 0xf\n"
                            "    L3CA COS1 => MASK 0x1f\n"
                            "    L3CA COS2 => MASK 0x3f\n"
                            "    L3CA COS3 => MASK 0x7f\n"
                            "    L3CA COS4 => MASK 0xff\n"
                            "    L3CA COS5 => MASK 0x1ff\n"
                            "    MBA COS0 => 10%% available\n"
                            "    MBA COS1 => 20%% available\n"
                            "    MBA COS2 => 30%% available\n"
                            "    MBA COS3 => 40%% available\n"
                            "L3CA/MBA COS definitions for Socket 1:\n"
                            "    L3CA COS0 => MASK 0xf\n"
                            "    L3CA COS1 => MASK 0x1f\n"
                            "    L3CA COS2 => MASK 0x3f\n"
                            "    L3CA COS3 => MASK 0x7f\n"
                            "    L3CA COS4 => MASK 0xff\n"
                            "    L3CA COS5 => MASK 0x1ff\n"
                            "    MBA COS0 => 10%% available\n"
                            "    MBA COS1 => 20%% available\n"
                            "    MBA COS2 => 30%% available\n"
                            "    MBA COS3 => 40%% available\n"
                            "L2CA COS definitions for L2ID 0:\n"
                            "    L2CA COS0 => MASK 0x1\n"
                            "    L2CA COS1 => MASK 0x3\n"
                            "    L2CA COS2 => MASK 0x7\n"
                            "    L2CA COS3 => MASK 0xf\n"
                            "L2CA COS definitions for L2ID 1:\n"
                            "    L2CA COS0 => MASK 0x1\n"
                            "    L2CA COS1 => MASK 0x3\n"
                            "    L2CA COS2 => MASK 0x7\n"
                            "    L2CA COS3 => MASK 0xf\n"
                            "Core information for socket 0:\n"
                            "    Core 0, L2ID 0, L3ID 0 => COS0, RMID4\n"
                            "    Core 1, L2ID 0, L3ID 0 => COS1, RMID3\n"
                            "Core information for socket 1:\n"
                            "    Core 2, L2ID 1, L3ID 1 => COS2, RMID2\n"
                            "    Core 3, L2ID 1, L3ID 1 => COS3, RMID1\n"));

        free(l3ca);
        free(l2ca);
        free(mba);
}

static void
test_alloc_print_config_os(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        unsigned i;
        unsigned num_l3ca = data->cap_l3ca->u.l3ca->num_classes;
        unsigned num_l2ca = data->cap_l2ca->u.l2ca->num_classes;
        unsigned num_mba = data->cap_mba->u.mba->num_classes;
        struct pqos_l3ca *l3ca;
        struct pqos_l2ca *l2ca;
        struct pqos_mba *mba;
        uint64_t ways_mask = 0xf;
        unsigned mb_max = 10;

        /* generate data */
        l3ca = calloc(num_l3ca, sizeof(struct pqos_l3ca));
        assert_non_null(l3ca);
        for (i = 0; i < num_l3ca; i++) {
                l3ca[i].class_id = i;
                l3ca[i].cdp = 0;
                l3ca[i].u.ways_mask = ways_mask;
                ways_mask = ways_mask | ways_mask << 1;
        }
        l2ca = calloc(num_l2ca, sizeof(struct pqos_l2ca));
        assert_non_null(l2ca);
        ways_mask = 0x1;
        for (i = 0; i < num_l2ca; i++) {
                l2ca[i].class_id = i;
                l2ca[i].cdp = 0;
                l2ca[i].u.ways_mask = ways_mask;
                ways_mask = ways_mask | ways_mask << 1;
        }
        mba = calloc(num_mba, sizeof(struct pqos_mba));
        assert_non_null(mba);
        for (i = 0; i < num_mba; i++) {
                mba[i].class_id = i;
                mba[i].mb_max = mb_max;
                mba[i].ctrl = 0;
                mb_max += 10;
                if (mb_max > 100)
                        mb_max = 10;
        }

        /* mock pqos_l3ca_get*/
        for (i = 0; i < data->num_socket; i++) {
                expect_value(__wrap_pqos_l3ca_get, l3cat_id, i);
                expect_value(__wrap_pqos_l3ca_get, max_num_ca, num_l3ca);
                will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_l3ca_get, num_l3ca);
                will_return(__wrap_pqos_l3ca_get, l3ca);
        }

        /* mock pqos_l2ca_get*/
        for (i = 0; i < data->cpu_info->num_cores / 2; i++) {
                expect_value(__wrap_pqos_l2ca_get, l2id, i);
                expect_value(__wrap_pqos_l2ca_get, max_num_ca,
                             PQOS_MAX_L2CA_COS);
                will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_l2ca_get, num_l2ca);
                will_return(__wrap_pqos_l2ca_get, l2ca);
        }

        /* mock pqos_mba_get */
        for (i = 0; i < data->num_socket; i++) {
                expect_value(__wrap_pqos_mba_get, mba_id, i);
                expect_value(__wrap_pqos_mba_get, max_num_cos, num_mba);
                will_return(__wrap_pqos_mba_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_mba_get, num_mba);
                will_return(__wrap_pqos_mba_get, mba);
        }

        /* mock pqos_inter_get */
        for (i = 0; i < data->cpu_info->num_cores; i++) {
                will_return(__wrap_pqos_inter_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_inter_get, PQOS_INTER_OS);
        }

        /* mock pqos_alloc_assoc_get */
        for (i = 0; i < data->cpu_info->num_cores; i++) {
                expect_value(__wrap_pqos_alloc_assoc_get, lcore, i);
                will_return(__wrap_pqos_alloc_assoc_get, PQOS_RETVAL_OK);
                will_return(__wrap_pqos_alloc_assoc_get, i % 16);
        }

        run_void_function(alloc_print_config, data->cap_mon, data->cap_l3ca,
                          data->cap_l2ca, data->cap_mba, data->cap_smba,
                          data->cpu_info, 1);

        /* check output */
        assert_true(output_has_text("L3CA/MBA COS definitions for Socket 0:\n"
                                    "    L3CA COS0 => MASK 0xf\n"
                                    "    L3CA COS1 => MASK 0x1f\n"
                                    "    L3CA COS2 => MASK 0x3f\n"
                                    "    L3CA COS3 => MASK 0x7f\n"
                                    "    L3CA COS4 => MASK 0xff\n"
                                    "    L3CA COS5 => MASK 0x1ff\n"
                                    "    MBA COS0 => 10%% available\n"
                                    "    MBA COS1 => 20%% available\n"
                                    "    MBA COS2 => 30%% available\n"
                                    "    MBA COS3 => 40%% available\n"
                                    "L3CA/MBA COS definitions for Socket 1:\n"
                                    "    L3CA COS0 => MASK 0xf\n"
                                    "    L3CA COS1 => MASK 0x1f\n"
                                    "    L3CA COS2 => MASK 0x3f\n"
                                    "    L3CA COS3 => MASK 0x7f\n"
                                    "    L3CA COS4 => MASK 0xff\n"
                                    "    L3CA COS5 => MASK 0x1ff\n"
                                    "    MBA COS0 => 10%% available\n"
                                    "    MBA COS1 => 20%% available\n"
                                    "    MBA COS2 => 30%% available\n"
                                    "    MBA COS3 => 40%% available\n"
                                    "L2CA COS definitions for L2ID 0:\n"
                                    "    L2CA COS0 => MASK 0x1\n"
                                    "    L2CA COS1 => MASK 0x3\n"
                                    "    L2CA COS2 => MASK 0x7\n"
                                    "    L2CA COS3 => MASK 0xf\n"
                                    "L2CA COS definitions for L2ID 1:\n"
                                    "    L2CA COS0 => MASK 0x1\n"
                                    "    L2CA COS1 => MASK 0x3\n"
                                    "    L2CA COS2 => MASK 0x7\n"
                                    "    L2CA COS3 => MASK 0xf\n"
                                    "Core information for socket 0:\n"
                                    "    Core 0, L2ID 0, L3ID 0 => COS0\n"
                                    "    Core 1, L2ID 0, L3ID 0 => COS1\n"
                                    "Core information for socket 1:\n"
                                    "    Core 2, L2ID 1, L3ID 1 => COS2\n"
                                    "    Core 3, L2ID 1, L3ID 1 => COS3\n"));
        free(l3ca);
        free(l2ca);
        free(mba);
}

static void
test_alloc_apply_cap_not_detected(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;

        /* set static data */
        sel_alloc_opt_num = 1;

        run_function(alloc_apply, ret, NULL, NULL, NULL, NULL, data->cpu_info);
        assert_int_equal(ret, -1);
        assert_true(output_has_text("Allocation capability not detected!"));

        sel_alloc_opt_num = 0;
}

static void
test_alloc_apply_mba(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;
        struct pqos_mba requested_mbas[3];
        struct pqos_mba actual_mbas[3];

        /* set static data */
        sel_alloc_opt_num = 3;
        alloc_opts[0] = strdup("mba:2=10");
        alloc_opts[1] = strdup("mba@1,2:2=64");
        alloc_opts[2] = strdup("mba@2-3:3=85");

        /* generate test data */
        requested_mbas[0].class_id = 2;
        requested_mbas[0].ctrl = 0;
        requested_mbas[0].mb_max = 10;
        requested_mbas[1].class_id = 2;
        requested_mbas[1].ctrl = 0;
        requested_mbas[1].mb_max = 64;
        requested_mbas[2].class_id = 3;
        requested_mbas[2].ctrl = 0;
        requested_mbas[2].mb_max = 85;
        actual_mbas[0].class_id = 2;
        actual_mbas[0].ctrl = 0;
        actual_mbas[0].mb_max = 10;
        actual_mbas[1].class_id = 2;
        actual_mbas[1].ctrl = 0;
        actual_mbas[1].mb_max = 60;
        actual_mbas[2].class_id = 3;
        actual_mbas[2].ctrl = 0;
        actual_mbas[2].mb_max = 90;

        /* mock pqos_mba_set */
        expect_value(__wrap_pqos_mba_set, mba_id, 0);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[0],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[0]);

        expect_value(__wrap_pqos_mba_set, mba_id, 1);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[0],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[0]);

        expect_value(__wrap_pqos_mba_set, mba_id, 1);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[1],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[1]);

        expect_value(__wrap_pqos_mba_set, mba_id, 2);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[1],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[1]);

        expect_value(__wrap_pqos_mba_set, mba_id, 2);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[2],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[2]);

        expect_value(__wrap_pqos_mba_set, mba_id, 3);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[2],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[2]);

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, 1);
        assert_true(output_has_text(
            "SOCKET 0 MBA COS2 => 10%% requested, 10%% applied"));
        assert_true(output_has_text(
            "SOCKET 1 MBA COS2 => 10%% requested, 10%% applied"));
        assert_true(output_has_text(
            "SOCKET 1 MBA COS2 => 64%% requested, 60%% applied"));
        assert_true(output_has_text(
            "SOCKET 2 MBA COS2 => 64%% requested, 60%% applied"));
        assert_true(output_has_text(
            "SOCKET 2 MBA COS3 => 85%% requested, 90%% applied"));
        assert_true(output_has_text(
            "SOCKET 3 MBA COS3 => 85%% requested, 90%% applied"));
        assert_true(output_has_text("Allocation configuration altered"));

        /* clenup before test */
        sel_alloc_opt_num = 0;
}

static void
test_alloc_apply_mba_max(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;
        struct pqos_mba requested_mbas[3];
        struct pqos_mba actual_mbas[3];

        /* set static data */
        sel_alloc_opt_num = 3;
        alloc_opts[0] = strdup("mba_max:2=10");
        alloc_opts[1] = strdup("mba_max@1,2:2=64");
        alloc_opts[2] = strdup("mba_max@2-3:3=85");

        /* generate test data */
        requested_mbas[0].class_id = 2;
        requested_mbas[0].ctrl = 1;
        requested_mbas[0].mb_max = 10;
        requested_mbas[1].class_id = 2;
        requested_mbas[1].ctrl = 1;
        requested_mbas[1].mb_max = 64;
        requested_mbas[2].class_id = 3;
        requested_mbas[2].ctrl = 1;
        requested_mbas[2].mb_max = 85;
        actual_mbas[0].class_id = 2;
        actual_mbas[0].ctrl = 1;
        actual_mbas[0].mb_max = 10;
        actual_mbas[1].class_id = 2;
        actual_mbas[1].ctrl = 1;
        actual_mbas[1].mb_max = 60;
        actual_mbas[2].class_id = 3;
        actual_mbas[2].ctrl = 1;
        actual_mbas[2].mb_max = 90;

        /* mock pqos_mba_set */
        expect_value(__wrap_pqos_mba_set, mba_id, 0);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[0],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[0]);

        expect_value(__wrap_pqos_mba_set, mba_id, 1);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[0],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[0]);

        expect_value(__wrap_pqos_mba_set, mba_id, 1);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[1],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[0]);

        expect_value(__wrap_pqos_mba_set, mba_id, 2);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[1],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[0]);

        expect_value(__wrap_pqos_mba_set, mba_id, 2);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[2],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[0]);

        expect_value(__wrap_pqos_mba_set, mba_id, 3);
        expect_value(__wrap_pqos_mba_set, num_cos, 1);
        expect_memory(__wrap_pqos_mba_set, requested, &requested_mbas[2],
                      sizeof(struct pqos_mba));
        will_return(__wrap_pqos_mba_set, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_mba_set, &actual_mbas[0]);

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, 1);
        assert_true(output_has_text("SOCKET 0 MBA COS2 => 10 MBps"));
        assert_true(output_has_text("SOCKET 1 MBA COS2 => 10 MBps"));
        assert_true(output_has_text("SOCKET 1 MBA COS2 => 64 MBps"));
        assert_true(output_has_text("SOCKET 2 MBA COS2 => 64 MBps"));
        assert_true(output_has_text("SOCKET 2 MBA COS2 => 85 MBps"));
        assert_true(output_has_text("SOCKET 3 MBA COS2 => 85 MBps"));
        assert_true(output_has_text("Allocation configuration altered"));

        sel_alloc_opt_num = 0;
}

static void
test_alloc_apply_l3ca(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;
        unsigned i = 0;
        unsigned num_ca = 4;
        struct pqos_l3ca ca[4];
        struct pqos_l3ca expected_ca[3];

        /* set static data */
        sel_alloc_opt_num = 3;
        alloc_opts[0] = strdup("llc:1=0x000f");
        alloc_opts[1] = strdup("llc@0,1:2=0x0ff0");
        alloc_opts[2] = strdup("llc@2-3:3=0x3c");

        /* generate test data */
        for (i = 0; i < num_ca; i++) {
                ca[i].class_id = i;
                ca[i].cdp = 0;
                ca[i].u.ways_mask = 0xFFFF;
        }

        expected_ca[0].class_id = 1;
        expected_ca[0].cdp = 0;
        expected_ca[0].u.ways_mask = 0xf;

        expected_ca[1].class_id = 2;
        expected_ca[1].cdp = 0;
        expected_ca[1].u.ways_mask = 0xff0;

        expected_ca[2].class_id = 3;
        expected_ca[2].cdp = 0;
        expected_ca[2].u.ways_mask = 0x3c;

        /* mock pqos_l3ca_get */
        expect_value(__wrap_pqos_l3ca_get, l3cat_id, 0);
        expect_value(__wrap_pqos_l3ca_get, max_num_ca, 16);
        will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get, num_ca);
        will_return(__wrap_pqos_l3ca_get, ca);

        expect_value(__wrap_pqos_l3ca_get, l3cat_id, 1);
        expect_value(__wrap_pqos_l3ca_get, max_num_ca, 16);
        will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get, num_ca);
        will_return(__wrap_pqos_l3ca_get, ca);

        expect_value(__wrap_pqos_l3ca_get, l3cat_id, 0);
        expect_value(__wrap_pqos_l3ca_get, max_num_ca, 16);
        will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get, num_ca);
        will_return(__wrap_pqos_l3ca_get, ca);

        expect_value(__wrap_pqos_l3ca_get, l3cat_id, 1);
        expect_value(__wrap_pqos_l3ca_get, max_num_ca, 16);
        will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get, num_ca);
        will_return(__wrap_pqos_l3ca_get, ca);

        expect_value(__wrap_pqos_l3ca_get, l3cat_id, 2);
        expect_value(__wrap_pqos_l3ca_get, max_num_ca, 16);
        will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get, num_ca);
        will_return(__wrap_pqos_l3ca_get, ca);

        expect_value(__wrap_pqos_l3ca_get, l3cat_id, 3);
        expect_value(__wrap_pqos_l3ca_get, max_num_ca, 16);
        will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get, num_ca);
        will_return(__wrap_pqos_l3ca_get, ca);

        // /* mock pqos_l3ca_set */
        expect_value(__wrap_pqos_l3ca_set, l3cat_id, 0);
        expect_value(__wrap_pqos_l3ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l3ca_set, ca, &expected_ca[0],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l3ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l3ca_set, l3cat_id, 1);
        expect_value(__wrap_pqos_l3ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l3ca_set, ca, &expected_ca[0],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l3ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l3ca_set, l3cat_id, 0);
        expect_value(__wrap_pqos_l3ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l3ca_set, ca, &expected_ca[1],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l3ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l3ca_set, l3cat_id, 1);
        expect_value(__wrap_pqos_l3ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l3ca_set, ca, &expected_ca[1],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l3ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l3ca_set, l3cat_id, 2);
        expect_value(__wrap_pqos_l3ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l3ca_set, ca, &expected_ca[2],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l3ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l3ca_set, l3cat_id, 3);
        expect_value(__wrap_pqos_l3ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l3ca_set, ca, &expected_ca[2],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l3ca_set, PQOS_RETVAL_OK);

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, 1);
        assert_true(output_has_text("SOCKET 0 L3CA COS1 => MASK 0xf"));
        assert_true(output_has_text("SOCKET 1 L3CA COS1 => MASK 0xf"));
        assert_true(output_has_text("SOCKET 0 L3CA COS2 => MASK 0xff0"));
        assert_true(output_has_text("SOCKET 1 L3CA COS2 => MASK 0xff0"));
        assert_true(output_has_text("SOCKET 2 L3CA COS3 => MASK 0x3c"));
        assert_true(output_has_text("SOCKET 3 L3CA COS3 => MASK 0x3c"));
        assert_true(output_has_text("Allocation configuration altered"));

        sel_alloc_opt_num = 0;
}

static void
test_alloc_apply_l3ca_cdp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;
        struct pqos_l3ca expected_ca[2];
        unsigned num_ca = 2;
        struct pqos_l3ca ca[2];
        unsigned i = 0;

        /* set static data */
        sel_alloc_opt_num = 2;
        alloc_opts[0] = strdup("llc:1d=0xff");
        alloc_opts[1] = strdup("llc:1c=0xf");

        /* generate test data */
        for (i = 0; i < num_ca; i++) {
                ca[i].class_id = i;
                ca[i].cdp = 1;
                ca[i].u.s.code_mask = 0xFFFF;
                ca[i].u.s.data_mask = 0xFFFF;
        }

        expected_ca[0].class_id = 1;
        expected_ca[0].cdp = 1;
        expected_ca[0].u.s.code_mask = 0xffff;
        expected_ca[0].u.s.data_mask = 0xff;

        expected_ca[1].class_id = 1;
        expected_ca[1].cdp = 1;
        expected_ca[1].u.s.code_mask = 0xf;
        expected_ca[1].u.s.data_mask = 0xffff;

        /* mock pqos_l3ca_get */
        expect_value(__wrap_pqos_l3ca_get, l3cat_id, 0);
        expect_value(__wrap_pqos_l3ca_get, max_num_ca, 16);
        will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get, num_ca);
        will_return(__wrap_pqos_l3ca_get, ca);

        expect_value(__wrap_pqos_l3ca_get, l3cat_id, 1);
        expect_value(__wrap_pqos_l3ca_get, max_num_ca, 16);
        will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get, num_ca);
        will_return(__wrap_pqos_l3ca_get, ca);

        expect_value(__wrap_pqos_l3ca_get, l3cat_id, 0);
        expect_value(__wrap_pqos_l3ca_get, max_num_ca, 16);
        will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get, num_ca);
        will_return(__wrap_pqos_l3ca_get, ca);

        expect_value(__wrap_pqos_l3ca_get, l3cat_id, 1);
        expect_value(__wrap_pqos_l3ca_get, max_num_ca, 16);
        will_return(__wrap_pqos_l3ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l3ca_get, num_ca);
        will_return(__wrap_pqos_l3ca_get, ca);

        /* mock pqos_l3ca_set */
        expect_value(__wrap_pqos_l3ca_set, l3cat_id, 0);
        expect_value(__wrap_pqos_l3ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l3ca_set, ca, &expected_ca[0],
                      sizeof(struct pqos_l3ca));
        will_return(__wrap_pqos_l3ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l3ca_set, l3cat_id, 1);
        expect_value(__wrap_pqos_l3ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l3ca_set, ca, &expected_ca[0],
                      sizeof(struct pqos_l3ca));
        will_return(__wrap_pqos_l3ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l3ca_set, l3cat_id, 0);
        expect_value(__wrap_pqos_l3ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l3ca_set, ca, &expected_ca[1],
                      sizeof(struct pqos_l3ca));
        will_return(__wrap_pqos_l3ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l3ca_set, l3cat_id, 1);
        expect_value(__wrap_pqos_l3ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l3ca_set, ca, &expected_ca[1],
                      sizeof(struct pqos_l3ca));
        will_return(__wrap_pqos_l3ca_set, PQOS_RETVAL_OK);

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, 1);
        assert_true(output_has_text("SOCKET 0 L3CA COS1 => DATA 0xff,CODE "
                                    "0xffff"));
        assert_true(output_has_text("SOCKET 1 L3CA COS1 => DATA 0xff,CODE "
                                    "0xffff"));
        assert_true(output_has_text("SOCKET 0 L3CA COS1 => DATA 0xffff,CODE "
                                    "0xf"));
        assert_true(output_has_text("SOCKET 1 L3CA COS1 => DATA 0xffff,CODE "
                                    "0xf"));
        assert_true(output_has_text("Allocation configuration altered"));
        sel_alloc_opt_num = 2;
}

static void
test_alloc_apply_l2(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;
        unsigned i = 0;
        unsigned num_l2ca = 4;
        struct pqos_l2ca l2ca_tab[6];
        struct pqos_l2ca expected_l2ca_tab[6];

        /* set static data */
        sel_alloc_opt_num = 3;
        alloc_opts[0] = strdup("l2:0=0x1");
        alloc_opts[1] = strdup("l2@2,3:1=0xf");
        alloc_opts[2] = strdup("l2@4-5:2=0xff");

        for (i = 0; i < num_l2ca; i++) {
                l2ca_tab[i].class_id = i;
                l2ca_tab[i].cdp = 0;
                l2ca_tab[i].u.ways_mask = 0xfff;
        }
        expected_l2ca_tab[0].u.ways_mask = 0x1;
        expected_l2ca_tab[0].class_id = 0;
        expected_l2ca_tab[0].cdp = 0;

        expected_l2ca_tab[1].u.ways_mask = 0xf;
        expected_l2ca_tab[1].class_id = 1;
        expected_l2ca_tab[1].cdp = 0;

        expected_l2ca_tab[2].u.ways_mask = 0xff;
        expected_l2ca_tab[2].class_id = 2;
        expected_l2ca_tab[2].cdp = 0;

        /* mock pqos_l2ca_get*/
        expect_value(__wrap_pqos_l2ca_get, l2id, 0);
        expect_value(__wrap_pqos_l2ca_get, max_num_ca, PQOS_MAX_L2CA_COS);
        will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l2ca_get, num_l2ca);
        will_return(__wrap_pqos_l2ca_get, l2ca_tab);

        expect_value(__wrap_pqos_l2ca_get, l2id, 1);
        expect_value(__wrap_pqos_l2ca_get, max_num_ca, PQOS_MAX_L2CA_COS);
        will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l2ca_get, num_l2ca);
        will_return(__wrap_pqos_l2ca_get, l2ca_tab);

        expect_value(__wrap_pqos_l2ca_get, l2id, 2);
        expect_value(__wrap_pqos_l2ca_get, max_num_ca, PQOS_MAX_L2CA_COS);
        will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l2ca_get, num_l2ca);
        will_return(__wrap_pqos_l2ca_get, l2ca_tab);

        expect_value(__wrap_pqos_l2ca_get, l2id, 3);
        expect_value(__wrap_pqos_l2ca_get, max_num_ca, PQOS_MAX_L2CA_COS);
        will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l2ca_get, num_l2ca);
        will_return(__wrap_pqos_l2ca_get, l2ca_tab);

        expect_value(__wrap_pqos_l2ca_get, l2id, 4);
        expect_value(__wrap_pqos_l2ca_get, max_num_ca, PQOS_MAX_L2CA_COS);
        will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l2ca_get, num_l2ca);
        will_return(__wrap_pqos_l2ca_get, l2ca_tab);

        expect_value(__wrap_pqos_l2ca_get, l2id, 5);
        expect_value(__wrap_pqos_l2ca_get, max_num_ca, PQOS_MAX_L2CA_COS);
        will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l2ca_get, num_l2ca);
        will_return(__wrap_pqos_l2ca_get, l2ca_tab);

        /* mock pqos_l2ca_set*/
        expect_value(__wrap_pqos_l2ca_set, l2id, 0);
        expect_value(__wrap_pqos_l2ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l2ca_set, ca, &expected_l2ca_tab[0],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l2ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l2ca_set, l2id, 1);
        expect_value(__wrap_pqos_l2ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l2ca_set, ca, &expected_l2ca_tab[0],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l2ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l2ca_set, l2id, 2);
        expect_value(__wrap_pqos_l2ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l2ca_set, ca, &expected_l2ca_tab[1],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l2ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l2ca_set, l2id, 3);
        expect_value(__wrap_pqos_l2ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l2ca_set, ca, &expected_l2ca_tab[1],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l2ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l2ca_set, l2id, 4);
        expect_value(__wrap_pqos_l2ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l2ca_set, ca, &expected_l2ca_tab[2],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l2ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l2ca_set, l2id, 5);
        expect_value(__wrap_pqos_l2ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l2ca_set, ca, &expected_l2ca_tab[2],
                      sizeof(struct pqos_l3ca) - 8);
        will_return(__wrap_pqos_l2ca_set, PQOS_RETVAL_OK);

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, 1);
        assert_true(output_has_text("L2ID 0 L2CA COS0 => MASK 0x1"));
        assert_true(output_has_text("L2ID 1 L2CA COS0 => MASK 0x1"));
        assert_true(output_has_text("L2ID 2 L2CA COS1 => MASK 0xf"));
        assert_true(output_has_text("L2ID 3 L2CA COS1 => MASK 0xf"));
        assert_true(output_has_text("L2ID 4 L2CA COS2 => MASK 0xff"));
        assert_true(output_has_text("L2ID 5 L2CA COS2 => MASK 0xff"));
        assert_true(output_has_text("Allocation configuration altered"));

        sel_alloc_opt_num = 0;
}

static void
test_alloc_apply_l2_dcp(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;
        unsigned num_ca = 2;
        struct pqos_l2ca ca[2];
        struct pqos_l2ca expected_ca[2];

        /* set static data */
        sel_alloc_opt_num = 2;
        alloc_opts[0] = strdup("l2:1d=0xff");
        alloc_opts[1] = strdup("l2:1c=0xf");

        /* generate test data */
        ca[0].class_id = 0;
        ca[0].cdp = 1;
        ca[0].u.s.code_mask = 0xFFFF;
        ca[0].u.s.data_mask = 0xFFFF;
        ca[1].class_id = 1;
        ca[1].cdp = 1;
        ca[1].u.s.code_mask = 0xFFFF;
        ca[1].u.s.data_mask = 0xFFFF;

        expected_ca[0].class_id = 1;
        expected_ca[0].cdp = 1;
        expected_ca[0].u.s.code_mask = 0xffff;
        expected_ca[0].u.s.data_mask = 0xff;

        expected_ca[1].class_id = 1;
        expected_ca[1].cdp = 1;
        expected_ca[1].u.s.code_mask = 0xf;
        expected_ca[1].u.s.data_mask = 0xffff;

        /* mock pqos_l2ca_get*/
        expect_value(__wrap_pqos_l2ca_get, l2id, 0);
        expect_value(__wrap_pqos_l2ca_get, max_num_ca, PQOS_MAX_L2CA_COS);
        will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l2ca_get, num_ca);
        will_return(__wrap_pqos_l2ca_get, ca);

        expect_value(__wrap_pqos_l2ca_get, l2id, 1);
        expect_value(__wrap_pqos_l2ca_get, max_num_ca, PQOS_MAX_L2CA_COS);
        will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l2ca_get, num_ca);
        will_return(__wrap_pqos_l2ca_get, ca);

        expect_value(__wrap_pqos_l2ca_get, l2id, 0);
        expect_value(__wrap_pqos_l2ca_get, max_num_ca, PQOS_MAX_L2CA_COS);
        will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l2ca_get, num_ca);
        will_return(__wrap_pqos_l2ca_get, ca);

        expect_value(__wrap_pqos_l2ca_get, l2id, 1);
        expect_value(__wrap_pqos_l2ca_get, max_num_ca, PQOS_MAX_L2CA_COS);
        will_return(__wrap_pqos_l2ca_get, PQOS_RETVAL_OK);
        will_return(__wrap_pqos_l2ca_get, num_ca);
        will_return(__wrap_pqos_l2ca_get, ca);

        /* mock pqos_l2ca_set */
        expect_value(__wrap_pqos_l2ca_set, l2id, 0);
        expect_value(__wrap_pqos_l2ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l2ca_set, ca, &expected_ca[0],
                      sizeof(struct pqos_l3ca));
        will_return(__wrap_pqos_l2ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l2ca_set, l2id, 1);
        expect_value(__wrap_pqos_l2ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l2ca_set, ca, &expected_ca[0],
                      sizeof(struct pqos_l3ca));
        will_return(__wrap_pqos_l2ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l2ca_set, l2id, 0);
        expect_value(__wrap_pqos_l2ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l2ca_set, ca, &expected_ca[1],
                      sizeof(struct pqos_l3ca));
        will_return(__wrap_pqos_l2ca_set, PQOS_RETVAL_OK);

        expect_value(__wrap_pqos_l2ca_set, l2id, 1);
        expect_value(__wrap_pqos_l2ca_set, num_cos, 1);
        expect_memory(__wrap_pqos_l2ca_set, ca, &expected_ca[1],
                      sizeof(struct pqos_l3ca));
        will_return(__wrap_pqos_l2ca_set, PQOS_RETVAL_OK);

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, 1);
        assert_true(output_has_text("L2ID 0 L2CA COS1 => DATA 0xff,CODE "
                                    "0xffff"));
        assert_true(output_has_text("L2ID 1 L2CA COS1 => DATA 0xff,CODE "
                                    "0xffff"));
        assert_true(output_has_text("L2ID 0 L2CA COS1 => DATA 0xffff,CODE "
                                    "0xf"));
        assert_true(output_has_text("L2ID 1 L2CA COS1 => DATA 0xffff,CODE "
                                    "0xf"));
        assert_true(output_has_text("Allocation configuration altered"));
        sel_alloc_opt_num = 2;
}

static void
test_alloc_apply_unrecognized_alloc_type(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;

        /* set static data */
        sel_alloc_opt_num = 1;
        alloc_opts[0] = strdup("uat:0=0x1");

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, -1);
        assert_true(output_has_text("Unrecognized allocation type: uat"));

        sel_alloc_opt_num = 0;
}

static void
test_alloc_apply_set_core_to_class_id(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;

        /* set static data */
        sel_assoc_core_num = 2;
        sel_assoc_tab[0].class_id = 1;
        sel_assoc_tab[0].core = 2;
        sel_assoc_tab[1].class_id = 2;
        sel_assoc_tab[1].core = 4;

        /* mock pqos_alloc_assoc_set */
        expect_value(__wrap_pqos_alloc_assoc_set, lcore, 2);
        expect_value(__wrap_pqos_alloc_assoc_set, class_id, 1);
        will_return(__wrap_pqos_alloc_assoc_set, PQOS_RETVAL_OK);
        expect_value(__wrap_pqos_alloc_assoc_set, lcore, 4);
        expect_value(__wrap_pqos_alloc_assoc_set, class_id, 2);
        will_return(__wrap_pqos_alloc_assoc_set, PQOS_RETVAL_OK);

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, 1);
        assert_true(output_has_text("Allocation configuration altered"));
        sel_assoc_core_num = 0;
}

static void
test_alloc_apply_set_core_to_class_id_neg(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;

        /* set static data */
        sel_assoc_core_num = 1;
        sel_assoc_tab[0].class_id = 1;
        sel_assoc_tab[0].core = 2;

        /* mock pqos_alloc_assoc_set */
        expect_value(__wrap_pqos_alloc_assoc_set, lcore, 2);
        expect_value(__wrap_pqos_alloc_assoc_set, class_id, 1);
        will_return(__wrap_pqos_alloc_assoc_set, PQOS_RETVAL_PARAM);
        expect_value(__wrap_pqos_alloc_assoc_set, lcore, 2);
        expect_value(__wrap_pqos_alloc_assoc_set, class_id, 1);
        will_return(__wrap_pqos_alloc_assoc_set, PQOS_RETVAL_ERROR);

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, -1);
        assert_true(output_has_text("Core number or class id is out of "
                                    "bounds!"));

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, -1);
        assert_true(output_has_text("Setting allocation class of service "
                                    "association failed!\n"));

        sel_assoc_core_num = 0;
}

static void
test_alloc_apply_set_pid_to_class_id(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;

        /* set static data */
        sel_assoc_pid_num = 2;
        sel_assoc_pid_tab[0].class_id = 1;
        sel_assoc_pid_tab[0].task_id = 235;
        sel_assoc_pid_tab[1].class_id = 3;
        sel_assoc_pid_tab[1].task_id = 66;

        /* mock pqos_alloc_assoc_set_pid */
        expect_value(__wrap_pqos_alloc_assoc_set_pid, task, 235);
        expect_value(__wrap_pqos_alloc_assoc_set_pid, class_id, 1);
        will_return(__wrap_pqos_alloc_assoc_set_pid, PQOS_RETVAL_OK);
        expect_value(__wrap_pqos_alloc_assoc_set_pid, task, 66);
        expect_value(__wrap_pqos_alloc_assoc_set_pid, class_id, 3);
        will_return(__wrap_pqos_alloc_assoc_set_pid, PQOS_RETVAL_OK);

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, 1);
        assert_true(output_has_text("Allocation configuration altered"));
        sel_assoc_pid_num = 0;
}

static void
test_alloc_apply_set_pid_to_class_id_neg(void **state)
{
        struct test_data *data = (struct test_data *)*state;
        int ret = 0;

        /* set static data */
        sel_assoc_pid_num = 1;
        sel_assoc_pid_tab[0].class_id = 1;
        sel_assoc_pid_tab[0].task_id = 30;

        /* mock pqos_alloc_assoc_set_pid */
        expect_value(__wrap_pqos_alloc_assoc_set_pid, task, 30);
        expect_value(__wrap_pqos_alloc_assoc_set_pid, class_id, 1);
        will_return(__wrap_pqos_alloc_assoc_set_pid, PQOS_RETVAL_PARAM);
        expect_value(__wrap_pqos_alloc_assoc_set_pid, task, 30);
        expect_value(__wrap_pqos_alloc_assoc_set_pid, class_id, 1);
        will_return(__wrap_pqos_alloc_assoc_set_pid, PQOS_RETVAL_ERROR);

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, -1);
        assert_true(output_has_text("Task ID number or class id is out of "
                                    "bounds!"));

        run_function(alloc_apply, ret, data->cap_l3ca, data->cap_l2ca,
                     data->cap_mba, data->cap_smba, data->cpu_info);

        assert_int_equal(ret, -1);
        assert_true(output_has_text("Setting allocation class of service "
                                    "association failed!\n"));
        sel_assoc_pid_num = 0;
}

static int
cleanup_assoc_core_and_pid_tabs(void **state)
{
        int i;

        for (i = 0; i < sel_assoc_core_num; i++) {
                sel_assoc_tab[i].core = 0;
                sel_assoc_tab[i].class_id = 0;
        }
        sel_assoc_core_num = 0;
        for (i = 0; i < sel_assoc_pid_num; i++) {
                sel_assoc_pid_tab[i].task_id = 0;
                sel_assoc_pid_tab[i].class_id = 0;
        }
        sel_assoc_pid_num = 0;
        alloc_pid_flag = 0;
        (void)state; /* unused */
        return 0;
}

static int
cleanup_alloc_opts(void **state)
{
        unsigned i;

        for (i = 0; i < sel_alloc_opt_num; i++) {
                free(alloc_opts[i]);
                alloc_opts[i] = NULL;
        }
        sel_alloc_opt_num = 0;
        (void)state; /* unused */
        return 0;
}

static int
init_all_caps(void **state)
{
        return init_caps(state, 1 << GENERATE_CAP_MON | 1 << GENERATE_CAP_L3CA |
                                    1 << GENERATE_CAP_L2CA |
                                    1 << GENERATE_CAP_MBA);
}

static int
init_cpu_info(void **state)
{
        return init_caps(state, 0);
}

static int
init_sel_assoc_tab(void **state)
{
        sel_assoc_tab = calloc(sel_assoc_tab_size, sizeof(*sel_assoc_tab));
        if (sel_assoc_tab == NULL)
                return -1;
        (void)state; /* unused */
        return 0;
}

static int
fini_sel_assoc_tab(void **state)
{
        if (sel_assoc_tab != NULL) {
                free(sel_assoc_tab);
                sel_assoc_tab = NULL;
        }
        (void)state; /* unused */
        return 0;
}

int
main(void)
{
        int result = 0;

        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_selfn_allocation_assoc_negative),
            cmocka_unit_test_teardown(test_selfn_allocation_assoc_llc,
                                      cleanup_assoc_core_and_pid_tabs),
            cmocka_unit_test_teardown(test_selfn_allocation_assoc_pid,
                                      cleanup_assoc_core_and_pid_tabs),
            cmocka_unit_test_teardown(test_selfn_allocation_assoc_core,
                                      cleanup_assoc_core_and_pid_tabs),
            cmocka_unit_test_teardown(test_selfn_allocation_assoc_cos,
                                      cleanup_assoc_core_and_pid_tabs),
            cmocka_unit_test_teardown(test_selfn_allocation_class_negative,
                                      cleanup_alloc_opts),
            cmocka_unit_test_teardown(test_selfn_allocation_class,
                                      cleanup_alloc_opts),
            cmocka_unit_test(test_alloc_print_config_negative)};

        const struct CMUnitTest tests_need_all_caps[] = {
            cmocka_unit_test(test_alloc_print_config_msr),
            cmocka_unit_test(test_alloc_print_config_os),
            cmocka_unit_test(test_alloc_apply_mba),
            cmocka_unit_test(test_alloc_apply_mba_max),
            cmocka_unit_test(test_alloc_apply_l3ca),
            cmocka_unit_test(test_alloc_apply_l3ca_cdp),
            cmocka_unit_test(test_alloc_apply_l2),
            cmocka_unit_test(test_alloc_apply_l2_dcp),
            cmocka_unit_test(test_alloc_apply_unrecognized_alloc_type),
            cmocka_unit_test_setup_teardown(
                test_alloc_apply_set_core_to_class_id, init_sel_assoc_tab,
                fini_sel_assoc_tab),
            cmocka_unit_test_setup_teardown(
                test_alloc_apply_set_core_to_class_id_neg, init_sel_assoc_tab,
                fini_sel_assoc_tab),
            cmocka_unit_test(test_alloc_apply_set_pid_to_class_id),
            cmocka_unit_test(test_alloc_apply_set_pid_to_class_id_neg)};

        const struct CMUnitTest tests_need_cpu_info[] = {
            cmocka_unit_test(test_alloc_apply_cap_not_detected)};

        result += cmocka_run_group_tests(tests, NULL, NULL);
        result += cmocka_run_group_tests(tests_need_all_caps, init_all_caps,
                                         fini_caps);
        result += cmocka_run_group_tests(tests_need_cpu_info, init_cpu_info,
                                         fini_caps);

        return result;
}
