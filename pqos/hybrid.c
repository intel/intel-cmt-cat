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

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SELECTED_CORES (1U << 20)
#define MON_RESOURCE_MASK  (1U << 1)

struct field_definition {
        const char *name;
        unsigned reg;
        uint32_t mask;
        unsigned shift;
        uint32_t add;
};

static const struct field_definition mon_l3_fields[] = {
    {"Counter width", 0, 0xffU, 0, 24},
    {"Overflow support", 0, 1U << 8, 8, 0},
    {"I/O RDT CMT support", 0, 1U << 9, 9, 0},
    {"I/O RDT MBM support", 0, 1U << 10, 10, 0},
    {"Conversion factor", 1, UINT32_MAX, 0, 0},
    {"Maximum L3 RMID", 2, UINT32_MAX, 0, 0},
    {"L3 occupancy support", 3, 1U, 0, 0},
    {"L3 total bandwidth support", 3, 1U << 1, 1, 0},
    {"L3 local bandwidth support", 3, 1U << 2, 2, 0}};

static const struct field_definition cat_l3_fields[] = {
    {"CBM length", 0, 0x1fU, 0, 1},
    {"Contention mask", 1, UINT32_MAX, 0, 0},
    {"Non-CPU agent support", 2, 1U << 1, 1, 0},
    {"CDP support", 2, 1U << 2, 2, 0},
    {"Non-contiguous CBM support", 2, 1U << 3, 3, 0},
    {"Maximum CLOS", 3, 0xffffU, 0, 0}};

static const struct field_definition cat_l2_fields[] = {
    {"CBM length", 0, 0x1fU, 0, 1},
    {"Contention mask", 1, UINT32_MAX, 0, 0},
    {"CDP support", 2, 1U << 2, 2, 0},
    {"Non-contiguous CBM support", 2, 1U << 3, 3, 0},
    {"Maximum CLOS", 3, 0xffffU, 0, 0}};

static const struct field_definition mba_fields[] = {
    {"Maximum throttling value", 0, 0xfffU, 0, 1},
    {"Per-thread control", 2, 1U, 0, 0},
    {"Linear response", 2, 1U << 2, 2, 0},
    {"Maximum CLOS", 3, 0xffffU, 0, 0}};

static const struct field_definition cba_fields[] = {
    {"Maximum levels", 0, 0xffU, 0, 1},
    {"Bandwidth scope", 0, 0xfU << 8, 8, 0},
    {"Linear response", 2, 1U << 3, 3, 0},
    {"Maximum CLOS", 3, 0xffffU, 0, 0}};

static const struct field_definition priority_fields[] = {
    {"Per-thread enable", 0, 1U, 0, 0},
    {"Per-package enable", 0, 1U << 1, 1, 0}};

/**
 * @brief Compares logical processor identifiers
 *
 * @param [in] a First logical processor identifier
 * @param [in] b Second logical processor identifier
 *
 * @return Negative, zero or positive comparison result
 */
static int
compare_unsigned(const void *a, const void *b)
{
        const unsigned *left = a;
        const unsigned *right = b;

        return (*left > *right) - (*left < *right);
}

/**
 * @brief Appends a logical processor identifier to an array
 *
 * The array may be reallocated by this function.
 *
 * @param [in,out] cores Logical processor identifier array
 * @param [in,out] count Number of array elements
 * @param [in,out] capacity Number of allocated array elements
 * @param [in] value Logical processor identifier to append
 *
 * @return Operation status
 * @retval 0 Success
 * @retval -1 Invalid value, capacity exceeded or allocation failure
 */
static int
append_core(unsigned **cores,
            unsigned *count,
            unsigned *capacity,
            unsigned value)
{
        if (value > INT_MAX || *count >= MAX_SELECTED_CORES)
                return -1;
        if (*count == *capacity) {
                unsigned new_capacity = *capacity == 0 ? 16U : *capacity * 2U;
                unsigned *resized;

                if (new_capacity > MAX_SELECTED_CORES)
                        new_capacity = MAX_SELECTED_CORES;
                resized =
                    realloc(*cores, (size_t)new_capacity * sizeof(**cores));
                if (resized == NULL)
                        return -1;
                *cores = resized;
                *capacity = new_capacity;
        }
        (*cores)[*count] = value;
        (*count)++;
        return 0;
}

/**
 * @brief Parses one logical processor identifier
 *
 * @param [in] start Start of the numeric string
 * @param [in] end First character after the numeric string
 * @param [out] value Parsed logical processor identifier
 *
 * @return Operation status
 * @retval 0 Success
 * @retval -1 Empty, non-numeric or overflowing value
 */
static int
parse_core_number(const char *start, const char *end, unsigned *value)
{
        unsigned parsed = 0;
        const char *p;

        if (start == end)
                return -1;
        for (p = start; p < end; p++) {
                const unsigned digit = (unsigned)(*p - '0');

                if (!isdigit((unsigned char)*p) ||
                    parsed > (UINT_MAX - digit) / 10U)
                        return -1;
                parsed = parsed * 10U + digit;
        }
        *value = parsed;
        return 0;
}

int
hybrid_parse_core_list(const char *text, unsigned **cores, unsigned *count)
{
        const char *token;
        unsigned *parsed = NULL;
        unsigned parsed_count = 0, parsed_capacity = 0;

        if (cores == NULL || count == NULL)
                return -1;
        *cores = NULL;
        *count = 0;
        if (text == NULL || *text == '\0')
                return -1;
        token = text;
        while (*token != '\0') {
                const char *end = strchr(token, ',');
                const char *dash;
                unsigned first, last, value;

                if (end == NULL)
                        end = token + strlen(token);
                if (end == token)
                        goto error;
                dash = memchr(token, '-', (size_t)(end - token));
                if (dash != NULL &&
                    memchr(dash + 1, '-', (size_t)(end - dash - 1)) != NULL)
                        goto error;

                if (dash == NULL) {
                        if (parse_core_number(token, end, &first) != 0 ||
                            append_core(&parsed, &parsed_count,
                                        &parsed_capacity, first) != 0)
                                goto error;
                } else {
                        if (parse_core_number(token, dash, &first) != 0 ||
                            parse_core_number(dash + 1, end, &last) != 0 ||
                            first > last)
                                goto error;
                        if ((uint64_t)last - first + 1 >
                            MAX_SELECTED_CORES - parsed_count)
                                goto error;
                        for (value = first;; value++) {
                                if (append_core(&parsed, &parsed_count,
                                                &parsed_capacity, value) != 0)
                                        goto error;
                                if (value == last)
                                        break;
                        }
                }
                if (*end == '\0')
                        break;
                token = end + 1;
                if (*token == '\0')
                        goto error;
        }

        qsort(parsed, parsed_count, sizeof(*parsed), compare_unsigned);
        if (parsed_count > 1) {
                unsigned read_index, write_index = 1;

                for (read_index = 1; read_index < parsed_count; read_index++)
                        if (parsed[read_index] != parsed[write_index - 1])
                                parsed[write_index++] = parsed[read_index];
                parsed_count = write_index;
        }
        *cores = parsed;
        *count = parsed_count;
        return 0;

error:
        free(parsed);
        return -1;
}

/**
 * @brief Retrieves the hybrid capability from system configuration
 *
 * @param [in] sys PQoS system configuration
 *
 * @return Hybrid capability on success
 * @retval NULL Hybrid capability is unavailable
 */
static const struct pqos_hybrid_capabilities *
get_hybrid_cap(const struct pqos_sysconfig *sys)
{
        const struct pqos_capability *item = NULL;

        if (sys == NULL || sys->cap == NULL)
                return NULL;
        if (pqos_cap_get_type(sys->cap, PQOS_CAP_TYPE_HYBRID, &item) !=
                PQOS_RETVAL_OK ||
            item == NULL)
                return NULL;
        return item->u.hybrid;
}

/**
 * @brief Prints the hybrid status line of a capability
 *
 * @param [in] cap capability to report, NULL being an unknown status
 */
static void
print_status(const struct pqos_hybrid_capabilities *cap)
{
        printf("Hybrid Processor: %s\n",
               cap == NULL || cap->status == PQOS_HYBRID_STATUS_UNKNOWN
                   ? "Unknown"
                   : (cap->status == PQOS_HYBRID_STATUS_YES ? "Yes" : "No"));
}

void
hybrid_print_status(const struct pqos_sysconfig *sys)
{
        print_status(get_hybrid_cap(sys));
}

/**
 * @brief Selects a register value from CPUID output
 *
 * @param [in] out CPUID register values
 * @param [in] reg Register index from EAX through EDX
 *
 * @return Selected register value
 */
static uint32_t
register_value(const struct pqos_hybrid_cpuid_out *out, unsigned reg)
{
        if (reg == 0)
                return out->eax;
        if (reg == 1)
                return out->ebx;
        if (reg == 2)
                return out->ecx;
        return out->edx;
}

/**
 * @brief Prints decoded fields for one CPUID resource
 *
 * @param [in] out CPUID register values
 * @param [in] fields Field decoding definitions
 * @param [in] count Number of field definitions
 */
static void
print_fields(const struct pqos_hybrid_cpuid_out *out,
             const struct field_definition *fields,
             unsigned count)
{
        unsigned i;

        for (i = 0; i < count; i++) {
                const uint32_t value =
                    ((register_value(out, fields[i].reg) & fields[i].mask) >>
                     fields[i].shift) +
                    fields[i].add;

                printf("    %s: %u\n", fields[i].name, value);
        }
}

/**
 * @brief Prints asymmetric monitoring capabilities
 *
 * @param [in] cap Logical processor hybrid capability
 */
static void
print_monitoring(const struct pqos_hybrid_core_capability *cap)
{
        printf("Asymmetric Monitoring:\n  CPUID Leaf 0x27: %s\n",
               cap->mon_supported ? "Supported" : "Not supported");
        if (!cap->mon_supported)
                return;
        printf("  Maximum RMID: %u\n  L3 Monitoring: %s\n", cap->mon[0].ebx,
               (cap->mon_resources & MON_RESOURCE_MASK) != 0 ? "Supported"
                                                             : "Not supported");
        if ((cap->mon_resources & MON_RESOURCE_MASK) != 0)
                print_fields(&cap->mon[1], mon_l3_fields,
                             sizeof(mon_l3_fields) / sizeof(mon_l3_fields[0]));
}

/**
 * @brief Prints one asymmetric allocation resource
 *
 * @param [in] cap Logical processor hybrid capability
 * @param [in] id CPUID resource identifier
 * @param [in] name Resource display name
 * @param [in] fields Field decoding definitions
 * @param [in] field_count Number of field definitions
 */
static void
print_alloc_resource(const struct pqos_hybrid_core_capability *cap,
                     unsigned id,
                     const char *name,
                     const struct field_definition *fields,
                     unsigned field_count)
{
        const int supported = (cap->alloc_resources & (1U << id)) != 0;

        printf("  %s: %s\n", name, supported ? "Supported" : "Not supported");
        if (supported)
                print_fields(&cap->alloc[id], fields, field_count);
}

/**
 * @brief Prints asymmetric allocation capabilities
 *
 * @param [in] cap Logical processor hybrid capability
 */
static void
print_allocation(const struct pqos_hybrid_core_capability *cap)
{
        printf("Asymmetric Allocation:\n  CPUID Leaf 0x28: %s\n",
               cap->alloc_supported ? "Supported" : "Not supported");
        if (!cap->alloc_supported)
                return;
        print_alloc_resource(cap, 1, "L3 CAT", cat_l3_fields,
                             sizeof(cat_l3_fields) / sizeof(cat_l3_fields[0]));
        print_alloc_resource(cap, 2, "L2 CAT", cat_l2_fields,
                             sizeof(cat_l2_fields) / sizeof(cat_l2_fields[0]));
        print_alloc_resource(cap, 3, "MBA", mba_fields,
                             sizeof(mba_fields) / sizeof(mba_fields[0]));
        print_alloc_resource(cap, 5, "CBA", cba_fields,
                             sizeof(cba_fields) / sizeof(cba_fields[0]));
        print_alloc_resource(cap, 6, "Resource Priority", priority_fields,
                             sizeof(priority_fields) /
                                 sizeof(priority_fields[0]));
}

/**
 * @brief Converts a native core type to a display name
 *
 * @param [in] type Native core type
 *
 * @return Core type display name
 */
static const char *
core_type_name(uint8_t type)
{
        if (type == 0x20)
                return "Intel Atom";
        if (type == 0x40)
                return "Intel Core";
        return "Unknown";
}

/**
 * @brief Converts a hybrid resource to a display name
 *
 * @param [in] resource Hybrid capability resource
 *
 * @return Resource display name
 */
static const char *
resource_name(enum pqos_hybrid_resource resource)
{
        static const char *const names[] = {
            "Monitoring", "L3 Monitoring", "Allocation", "L3 CAT",
            "L2 CAT",     "MBA",           "CBA",        "Resource Priority"};

        if ((unsigned)resource >= sizeof(names) / sizeof(names[0]))
                return "Unknown resource";
        return names[resource];
}

/**
 * @brief Converts a hybrid field to a display name
 *
 * @param [in] field Hybrid capability field
 *
 * @return Field display name
 */
static const char *
field_name(enum pqos_hybrid_field field)
{
        static const char *const names[] = {"Support",
                                            "Regular enumeration support",
                                            "Maximum RMID",
                                            "Counter width",
                                            "Overflow support",
                                            "I/O RDT CMT support",
                                            "I/O RDT MBM support",
                                            "Conversion factor",
                                            "L3 occupancy support",
                                            "L3 total bandwidth support",
                                            "L3 local bandwidth support",
                                            "CBM length",
                                            "Contention mask",
                                            "Non-CPU agent support",
                                            "CDP support",
                                            "Non-contiguous CBM support",
                                            "Maximum CLOS",
                                            "Maximum throttling value",
                                            "Per-thread control",
                                            "Linear response",
                                            "Maximum levels",
                                            "Bandwidth scope",
                                            "Per-thread enable",
                                            "Per-package enable"};

        if ((unsigned)field >= sizeof(names) / sizeof(names[0]))
                return "Unknown field";
        return names[field];
}

/**
 * @brief Prints one logical processor hybrid capability
 *
 * @param [in] cap Logical processor hybrid capability
 */
static void
print_capabilities(const struct pqos_hybrid_core_capability *cap)
{
        unsigned i;

        printf("\nLogical Core %u\nSocket ID: %u\n", cap->lcore, cap->socket);
        if (cap->physical_core_valid)
                printf("Physical Core ID: %u\n", cap->physical_core);
        else
                printf("Physical Core ID: Not available\n");
        if (cap->core_type_valid)
                printf("Core Type: %s (0x%02x), Native Model ID: 0x%06x\n",
                       core_type_name(cap->core_type), cap->core_type,
                       cap->native_model_id);
        else
                printf("Core Type: Not available\n");
        print_monitoring(cap);
        print_allocation(cap);
        printf("Capability Differences:\n");
        if (cap->num_differences == 0) {
                printf("  None\n");
                return;
        }
        for (i = 0; i < cap->num_differences; i++)
                printf("  WARNING: Logical core %u, %s, %s: regular=%u, "
                       "asymmetric=%u\n",
                       cap->lcore, resource_name(cap->differences[i].resource),
                       field_name(cap->differences[i].field),
                       cap->differences[i].regular,
                       cap->differences[i].asymmetric);
}

/**
 * @brief Finds one logical processor hybrid capability
 *
 * @param [in] cap Platform hybrid capabilities
 * @param [in] lcore Logical processor identifier
 *
 * @return Logical processor capability on success
 * @retval NULL Logical processor is not represented
 */
static const struct pqos_hybrid_core_capability *
find_core(const struct pqos_hybrid_capabilities *cap, unsigned lcore)
{
        unsigned i;

        for (i = 0; i < cap->num_cores; i++)
                if (cap->cores[i].lcore == lcore)
                        return &cap->cores[i];
        return NULL;
}

int
hybrid_enum_cores(const struct pqos_hybrid_capabilities *cap,
                  const char *selection)
{
        unsigned *selected = NULL;
        unsigned selected_count = 0, i;
        int ret = 0;

        if (cap == NULL) {
                fprintf(stderr,
                        "Hybrid processor capability is not available\n");
                return -1;
        }
        print_status(cap);

        /* the selection is judged before the processor is: a malformed list is
         * a command line error whatever the platform, and a non-hybrid one used
         * to report success for it, which is the silent acceptance of an
         * invalid selection the requirements forbid
         */
        if (selection != NULL && hybrid_parse_core_list(selection, &selected,
                                                        &selected_count) != 0) {
                fprintf(stderr, "Invalid logical processor list: %s\n",
                        selection);
                return -1;
        }

        if (cap->status != PQOS_HYBRID_STATUS_YES) {
                printf("Asymmetric RDT capability enumeration is not "
                       "available on this processor.\n");
                free(selected);
                return 0;
        }

        if (selection == NULL) {
                for (i = 0; i < cap->num_cores; i++)
                        print_capabilities(&cap->cores[i]);
                return 0;
        }
        for (i = 0; i < selected_count; i++) {
                const struct pqos_hybrid_core_capability *core =
                    find_core(cap, selected[i]);

                if (core == NULL) {
                        fprintf(stderr,
                                "Logical processor %u is not available\n",
                                selected[i]);
                        ret = -1;
                        continue;
                }
                print_capabilities(core);
        }
        free(selected);
        return ret;
}
