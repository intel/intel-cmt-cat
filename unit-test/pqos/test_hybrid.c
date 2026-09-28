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

#include "hybrid.h"
#include "output.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
/* clang-format off */
#include <cmocka.h>
/* clang-format on */

static void
test_parse_core_list(void **state)
{
        static const char *const invalid[] = {"",    ",1", "1,", "1,,2", "x",
                                              "2-1", "1-", "-1", "1-2-3"};
        unsigned *cores = NULL;
        unsigned count = 0, i;

        assert_int_equal(
            hybrid_parse_core_list("8,0-3,2,10-12", &cores, &count), 0);
        assert_int_equal(count, 8);
        assert_int_equal(cores[0], 0);
        assert_int_equal(cores[4], 8);
        assert_int_equal(cores[7], 12);
        free(cores);

        assert_int_equal(hybrid_parse_core_list("0-1023", &cores, &count), 0);
        assert_int_equal(count, 1024);
        assert_int_equal(cores[0], 0);
        assert_int_equal(cores[1023], 1023);
        free(cores);

        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
                assert_int_equal(
                    hybrid_parse_core_list(invalid[i], &cores, &count), -1);
                assert_null(cores);
                assert_int_equal(count, 0);
        }
        (void)state;
}

static void
test_enum_cores_reads_the_capability_it_is_given(void **state)
{
        const size_t hybrid_size = sizeof(struct pqos_hybrid_capabilities) +
                                   sizeof(struct pqos_hybrid_core_capability);
        struct pqos_hybrid_capabilities *hybrid = calloc(1, hybrid_size);
        struct pqos_hybrid_core_capability *core;

        assert_non_null(hybrid);
        hybrid->mem_size = hybrid_size;
        hybrid->status = PQOS_HYBRID_STATUS_YES;
        hybrid->num_cores = 1;
        core = &hybrid->cores[0];
        core->mem_size = sizeof(*core);
        core->lcore = 3;
        core->socket = 1;
        core->physical_core_valid = 1;
        core->physical_core = 2;
        core->core_type_valid = 1;
        core->core_type = 0x40;
        core->native_model_id = 4;

        /* the capability the caller discovered, not a system configuration: the
         * command reads CPUID and never initializes an interface, so there is
         * no sysconfig to take it from
         */
        assert_int_equal(hybrid_enum_cores(hybrid, "3"), 0);
        assert_int_equal(hybrid_enum_cores(hybrid, "4"), -1);
        assert_int_equal(hybrid_enum_cores(NULL, "3"), -1);

        free(hybrid);
        (void)state;
}

/** one processor, described down to the fields the report decodes */
struct fake_core {
        struct pqos_hybrid_capabilities *hybrid;
        struct pqos_hybrid_core_capability *core;
};

/**
 * @brief Builds a capability for one processor, hybrid and fully readable
 *
 * The values are chosen to be recognisable in the output rather than realistic:
 * a case asserting on "Maximum RMID: 31" is asserting that the report read
 * mon[0].ebx and printed it, which is what a return value cannot say.
 *
 * @param [out] fake capability to fill in, released with free_core()
 */
static void
build_core(struct fake_core *fake)
{
        const size_t size = sizeof(struct pqos_hybrid_capabilities) +
                            sizeof(struct pqos_hybrid_core_capability);
        struct pqos_hybrid_core_capability *core;

        fake->hybrid = calloc(1, size);
        assert_non_null(fake->hybrid);
        fake->hybrid->mem_size = size;
        fake->hybrid->status = PQOS_HYBRID_STATUS_YES;
        fake->hybrid->num_cores = 1;

        core = &fake->hybrid->cores[0];
        fake->core = core;
        core->mem_size = sizeof(*core);
        core->lcore = 3;
        core->socket = 1;
        core->physical_core_valid = 1;
        core->physical_core = 2;
        core->core_type_valid = 1;
        core->core_type = 0x40; /* Intel Core */
        core->native_model_id = 4;

        /* monitoring: the leaf is there, L3 monitoring with it */
        core->mon_supported = 1;
        core->mon_resources = 1U << 1;
        core->mon[0].ebx = 31; /* Maximum RMID */
        core->mon[1].eax = 0;  /* Counter width, which the report adds 24 to
                                */
        core->mon[1].ebx = 77; /* Conversion factor */
        core->mon[1].ecx = 15; /* Maximum L3 RMID */
        core->mon[1].edx = 1;  /* L3 occupancy support */

        /* allocation: L3 CAT only, so the report has both answers to give */
        core->alloc_supported = 1;
        core->alloc_resources = 1U << 1;
        core->alloc[1].eax = 10; /* CBM length, which the report adds 1 to */
        core->alloc[1].edx = 8;  /* Maximum CLOS */
}

static void
free_core(struct fake_core *fake)
{
        free(fake->hybrid);
        fake->hybrid = NULL;
        fake->core = NULL;
}

static void
test_the_report_names_the_processor_it_read(void **state)
{
        struct fake_core fake;

        build_core(&fake);

        output_start();
        assert_int_equal(hybrid_enum_cores(fake.hybrid, "3"), 0);
        output_stop();

        assert_int_equal(output_has_text("Hybrid Processor: Yes"), 1);
        assert_int_equal(output_has_text("Logical Core 3"), 1);
        assert_int_equal(output_has_text("Socket ID: 1"), 1);
        assert_int_equal(output_has_text("Physical Core ID: 2"), 1);
        assert_int_equal(
            output_has_text(
                "Core Type: Intel Core (0x40), Native Model ID: 0x000004"),
            1);

        free_core(&fake);
        (void)state;
}

static void
test_the_report_decodes_the_resources_the_leaves_carry(void **state)
{
        struct fake_core fake;

        build_core(&fake);

        output_start();
        assert_int_equal(hybrid_enum_cores(fake.hybrid, "3"), 0);
        output_stop();

        /* what each leaf says about itself */
        assert_int_equal(output_has_text("CPUID Leaf 0x27: Supported"), 1);
        assert_int_equal(output_has_text("CPUID Leaf 0x28: Supported"), 1);

        /* the monitoring fields, decoded: the RMID out of mon[0], the counter
         * width out of mon[1] with the 24 the encoding leaves out
         */
        assert_int_equal(output_has_text("Maximum RMID: 31"), 1);
        assert_int_equal(output_has_text("L3 Monitoring: Supported"), 1);
        assert_int_equal(output_has_text("Counter width: 24"), 1);
        assert_int_equal(output_has_text("Conversion factor: 77"), 1);
        assert_int_equal(output_has_text("Maximum L3 RMID: 15"), 1);
        assert_int_equal(output_has_text("L3 occupancy support: 1"), 1);

        /* and the allocation side, where only L3 CAT is there - so the report
         * has to say both things, and the CBM length carries its own +1
         */
        assert_int_equal(output_has_text("L3 CAT: Supported"), 1);
        assert_int_equal(output_has_text("CBM length: 11"), 1);
        assert_int_equal(output_has_text("Maximum CLOS: 8"), 1);
        assert_int_equal(output_has_text("L2 CAT: Not supported"), 1);
        assert_int_equal(output_has_text("MBA: Not supported"), 1);
        assert_int_equal(output_has_text("CBA: Not supported"), 1);
        assert_int_equal(output_has_text("Resource Priority: Not supported"),
                         1);

        /* a resource that is not supported is named and left there: none of its
         * fields is printed as a measured zero
         */
        assert_int_equal(output_has_text("Per-thread enable"), 0);
        assert_int_equal(output_has_text("Maximum throttling value"), 0);

        free_core(&fake);
        (void)state;
}

static void
test_the_report_prints_the_difference_it_was_given(void **state)
{
        struct fake_core fake;

        build_core(&fake);

        output_start();
        assert_int_equal(hybrid_enum_cores(fake.hybrid, "3"), 0);
        output_stop();

        /* nothing differs yet, and the report says so rather than saying
         * nothing
         */
        assert_int_equal(output_has_text("Capability Differences:"), 1);
        assert_int_equal(output_has_text("None"), 1);

        fake.core->num_differences = 1;
        fake.core->differences[0].resource = PQOS_HYBRID_RESOURCE_L3_CAT;
        fake.core->differences[0].field = PQOS_HYBRID_FIELD_CBM_LENGTH;
        fake.core->differences[0].regular = 11;
        fake.core->differences[0].asymmetric = 7;

        output_start();
        assert_int_equal(hybrid_enum_cores(fake.hybrid, "3"), 0);
        output_stop();

        /* the whole point of the comparison: which processor, which resource,
         * which field, and both values
         */
        assert_int_equal(
            output_has_text("WARNING: Logical core 3, L3 CAT, "
                            "CBM length: regular=11, asymmetric=7"),
            1);
        assert_int_equal(output_has_text("  None"), 0);

        free_core(&fake);
        (void)state;
}

static void
test_the_report_says_what_it_could_not_read(void **state)
{
        struct fake_core fake;

        build_core(&fake);
        fake.core->physical_core_valid = 0;
        fake.core->core_type_valid = 0;
        fake.core->mon_supported = 0;
        fake.core->alloc_supported = 0;

        output_start();
        assert_int_equal(hybrid_enum_cores(fake.hybrid, "3"), 0);
        output_stop();

        /* every line is still printed, and an absence is said rather than left
         * to be inferred from a missing line
         */
        assert_int_equal(output_has_text("Physical Core ID: Not available"), 1);
        assert_int_equal(output_has_text("Core Type: Not available"), 1);
        assert_int_equal(output_has_text("CPUID Leaf 0x27: Not supported"), 1);
        assert_int_equal(output_has_text("CPUID Leaf 0x28: Not supported"), 1);

        /* and a leaf that is not supported has no fields to decode, so none of
         * the values in the capability reaches the report
         */
        assert_int_equal(output_has_text("Maximum RMID"), 0);
        assert_int_equal(output_has_text("L3 CAT"), 0);

        free_core(&fake);
        (void)state;
}

static void
test_the_report_says_when_there_is_nothing_to_enumerate(void **state)
{
        const size_t size = sizeof(struct pqos_hybrid_capabilities);
        struct pqos_hybrid_capabilities *hybrid = calloc(1, size);

        assert_non_null(hybrid);
        hybrid->mem_size = size;
        hybrid->status = PQOS_HYBRID_STATUS_NO;

        output_start();
        assert_int_equal(hybrid_enum_cores(hybrid, "0-3"), 0);
        output_stop();

        assert_int_equal(output_has_text("Hybrid Processor: No"), 1);
        assert_int_equal(output_has_text("Asymmetric RDT capability "
                                         "enumeration is not available on this "
                                         "processor."),
                         1);
        assert_int_equal(output_has_text("Logical Core"), 0);

        /* a malformed list is refused on stderr, whatever the platform is */
        output_start();
        assert_int_equal(hybrid_enum_cores(hybrid, "0,,1"), -1);
        output_stop();
        assert_int_equal(
            output_has_text("Invalid logical processor list: 0,,1"), 1);

        /* and so is a processor the platform does not have, where there was
         * something to enumerate
         */
        hybrid->status = PQOS_HYBRID_STATUS_YES;
        output_start();
        assert_int_equal(hybrid_enum_cores(hybrid, "9999"), -1);
        output_stop();
        assert_int_equal(output_has_text("Logical processor 9999 is not "
                                         "available"),
                         1);

        free(hybrid);
        (void)state;
}

static void
test_print_status_reads_the_sysconfig(void **state)
{
        const size_t cap_size =
            sizeof(struct pqos_cap) + sizeof(struct pqos_capability);
        const size_t hybrid_size = sizeof(struct pqos_hybrid_capabilities);
        struct pqos_hybrid_capabilities *hybrid = calloc(1, hybrid_size);
        struct pqos_cap *cap = calloc(1, cap_size);
        struct pqos_sysconfig sys = {0};

        assert_non_null(hybrid);
        assert_non_null(cap);
        hybrid->mem_size = hybrid_size;
        hybrid->status = PQOS_HYBRID_STATUS_YES;
        cap->mem_size = cap_size;
        cap->num_cap = 1;
        cap->capabilities[0].type = PQOS_CAP_TYPE_HYBRID;
        cap->capabilities[0].u.hybrid = hybrid;
        sys.cap = cap;

        /* -d and -D print the same line from the capability the library
         * published, which is the one path that does have a sysconfig
         */
        hybrid_print_status(&sys);
        hybrid_print_status(NULL);

        free(cap);
        free(hybrid);
        (void)state;
}

static void
test_enum_cores_rejects_a_list_on_a_non_hybrid_processor(void **state)
{
        const size_t hybrid_size = sizeof(struct pqos_hybrid_capabilities);
        struct pqos_hybrid_capabilities *hybrid = calloc(1, hybrid_size);

        assert_non_null(hybrid);
        hybrid->mem_size = hybrid_size;
        hybrid->status = PQOS_HYBRID_STATUS_NO;
        hybrid->num_cores = 0;

        /* a malformed list is a command line error whatever the processor is,
         * and this path used to report success for it: the platform was judged
         * first and the list never looked at
         */
        assert_int_equal(hybrid_enum_cores(hybrid, "0,,1"), -1);
        assert_int_equal(hybrid_enum_cores(hybrid, "4-2"), -1);

        /* a well formed list on a processor that has no asymmetric capability
         * is not an error, and says so
         */
        assert_int_equal(hybrid_enum_cores(hybrid, "0-3"), 0);
        assert_int_equal(hybrid_enum_cores(hybrid, NULL), 0);

        free(hybrid);
        (void)state;
}

int
main(void)
{
        const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_parse_core_list),
            cmocka_unit_test(test_enum_cores_reads_the_capability_it_is_given),
            cmocka_unit_test(test_the_report_names_the_processor_it_read),
            cmocka_unit_test(
                test_the_report_decodes_the_resources_the_leaves_carry),
            cmocka_unit_test(
                test_the_report_prints_the_difference_it_was_given),
            cmocka_unit_test(test_the_report_says_what_it_could_not_read),
            cmocka_unit_test(
                test_the_report_says_when_there_is_nothing_to_enumerate),
            cmocka_unit_test(test_print_status_reads_the_sysconfig),
            cmocka_unit_test(
                test_enum_cores_rejects_a_list_on_a_non_hybrid_processor)};

        return cmocka_run_group_tests(tests, NULL, NULL);
}
