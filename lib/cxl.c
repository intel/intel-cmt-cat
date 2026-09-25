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
 *
 */

/**
 * @brief The CXL memory devices the operating system has mapped
 *
 * What the kernel publishes, and the three steps from an address range to the
 * device behind it:
 *
 *   /sys/bus/cxl/devices/region0/resource   the address the region maps
 *   /sys/bus/cxl/devices/region0/size       and how much of it
 *   /sys/bus/cxl/devices/region0/target0    "decoder4.0", the endpoint decoder
 *   .../port1/endpoint4/decoder4.0          which lives under its endpoint
 *   .../endpoint4/uport -> .../0000:11:00.0/mem0
 *                                           and that endpoint names the device
 *                                           and the PCI function it sits on
 *
 * An interleaved region has a target per way, so a region can name several
 * devices, and each is reported with the region's address range: the range is
 * the region's, and which part of it a given way holds is not something the
 * kernel publishes per device.
 *
 * Every step after the address range is allowed to fail on its own. A kernel
 * that lays these links out differently, or a region whose endpoint has gone
 * away underneath the read, still leaves an address range that a memory range
 * can be compared against - so the device is reported with the names it could
 * not learn left empty, and the report says they are unknown rather than
 * dropping a mapped range it knows about.
 */

#include "cxl.h"

#include "common.h"
#include "log.h"

#include <dirent.h> /**< scandir() */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef __linux__

/** where the kernel publishes the bus, and the devices on it. Overridable at
 *  compile time the way lib/lock.h does it for the lock file, so that a test
 * can point this at a tree of its own: the states this reader has to get right
 * - a bus with no devices, a device whose links lead somewhere unexpected - are
 *  states no available platform can be put into
 */
#ifndef CXL_BUS_PATH
#define CXL_BUS_PATH "/sys/bus/cxl"
#endif
#define CXL_DEVICES_PATH CXL_BUS_PATH "/devices"

/** the prefix of a CXL region device's name */
#define CXL_REGION_PREFIX "region"

/**
 * @brief Whether a directory entry is a CXL region device
 *
 * @param [in] dir the entry scandir() offers
 *
 * @return whether to include the entry
 * @retval 1 include
 * @retval 0 skip
 */
static int
filter_region(const struct dirent *dir)
{
        const size_t len = strlen(CXL_REGION_PREFIX);

        if (strncmp(dir->d_name, CXL_REGION_PREFIX, len) != 0)
                return 0;

        /* "region0", not "region" on its own and not "regionfoo": what follows
         * the prefix is the kernel's number for it
         */
        if (dir->d_name[len] == '\0')
                return 0;

        return strspn(dir->d_name + len, "0123456789") ==
               strlen(dir->d_name + len);
}

/**
 * @brief Reads one line of a sysfs attribute into \a buf
 *
 * @param [in] path the attribute to read
 * @param [out] buf where to put it, NUL terminated and with no trailing newline
 * @param [in] size how much room \a buf has
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK read
 * @retval PQOS_RETVAL_ERROR the attribute could not be read
 */
static int
read_attribute(const char *path, char *buf, size_t size)
{
        FILE *fd;
        char *end;

        fd = pqos_fopen(path, "r");
        if (fd == NULL)
                return PQOS_RETVAL_ERROR;

        if (pqos_fgets(buf, (int)size, fd) == NULL) {
                pqos_fclose(fd);

                return PQOS_RETVAL_ERROR;
        }
        pqos_fclose(fd);

        end = strchr(buf, '\n');
        if (end != NULL)
                *end = '\0';

        if (buf[0] == '\0')
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

/**
 * @brief Names the memory device and PCI function behind one endpoint decoder
 *
 * @param [in] decoder the decoder named by a region's target, e.g. "decoder4.0"
 * @param [out] device the device to fill in
 */
static void
name_device(const char *decoder, struct pqos_cxl_device *device)
{
        char path[PATH_MAX];
        char resolved[PATH_MAX];
        /* the endpoint's path plus the attribute appended to it, which is why
         * this one is not bounded by PATH_MAX alone
         */
        char uport[PATH_MAX + sizeof("/uport")];
        char *endpoint;
        char *name;
        char *parent;

        /* the decoder resolves to a path under its endpoint, so the endpoint is
         * the directory holding it
         */
        snprintf(path, sizeof(path), CXL_DEVICES_PATH "/%s", decoder);
        if (realpath(path, resolved) == NULL) {
                LOG_DEBUG("CXL: %s could not be resolved\n", path);

                return;
        }

        endpoint = strrchr(resolved, '/');
        if (endpoint == NULL)
                return;
        *endpoint = '\0';

        /* and the endpoint's uport is the device it speaks for: the last
         * component names the memory device, the one before it the PCI function
         */
        snprintf(uport, sizeof(uport), "%s/uport", resolved);
        if (realpath(uport, resolved) == NULL) {
                LOG_DEBUG("CXL: %s could not be resolved\n", uport);

                return;
        }

        name = strrchr(resolved, '/');
        if (name == NULL)
                return;
        *name = '\0';
        name++;

        if (strlen(name) < sizeof(device->mem_name))
                strncpy(device->mem_name, name, sizeof(device->mem_name) - 1);

        parent = strrchr(resolved, '/');
        if (parent == NULL)
                return;
        parent++;

        if (strlen(parent) < sizeof(device->pci_address))
                strncpy(device->pci_address, parent,
                        sizeof(device->pci_address) - 1);
}

/**
 * @brief Counts the targets one CXL region has
 *
 * @param [in] region the region's device name, e.g. "region0"
 *
 * @return how many target attributes the region publishes
 */
static unsigned
count_targets(const char *region)
{
        char path[PATH_MAX];
        unsigned targets = 0;

        for (;;) {
                snprintf(path, sizeof(path), CXL_DEVICES_PATH "/%s/target%u",
                         region, targets);
                if (!pqos_file_exists(path))
                        break;

                targets++;
        }

        return targets;
}

/**
 * @brief Reads one target of a region into \a device
 *
 * @param [in] region the region's device name
 * @param [in] target which target of it
 * @param [in] base the address the region maps
 * @param [in] size how much of it, zero where the pair could not be read
 * @param [in] address_valid whether \a base and \a size were read
 * @param [out] device the device to fill in
 */
static void
read_target(const char *region,
            unsigned target,
            uint64_t base,
            uint64_t size,
            int address_valid,
            struct pqos_cxl_device *device)
{
        char path[PATH_MAX];
        char decoder[PQOS_CXL_NAME_LEN];

        memset(device, 0, sizeof(*device));
        if (strlen(region) < sizeof(device->region_name))
                strncpy(device->region_name, region,
                        sizeof(device->region_name) - 1);
        device->address_valid = address_valid;
        device->base_address = base;
        device->size = size;

        snprintf(path, sizeof(path), CXL_DEVICES_PATH "/%s/target%u", region,
                 target);
        if (read_attribute(path, decoder, sizeof(decoder)) != PQOS_RETVAL_OK) {
                LOG_DEBUG("CXL: %s could not be read\n", path);

                return;
        }

        name_device(decoder, device);
}

int
cxl_devices_read(int *available,
                 unsigned *num_devices,
                 struct pqos_cxl_device **devices)
{
        struct dirent **namelist = NULL;
        struct pqos_cxl_device *out = NULL;
        unsigned count = 0;
        unsigned filled = 0;
        int regions;
        int i;

        if (available == NULL || num_devices == NULL || devices == NULL)
                return PQOS_RETVAL_PARAM;

        *available = 0;
        *num_devices = 0;
        *devices = NULL;

        /* no bus is not no devices: the report says which of the two it is, so
         * the answer here is the question's, not an empty list
         */
        if (!pqos_dir_exists(CXL_DEVICES_PATH)) {
                LOG_INFO("CXL: %s is not present, so which devices are behind "
                         "a memory range is unknown\n",
                         CXL_DEVICES_PATH);

                return PQOS_RETVAL_OK;
        }
        *available = 1;

        regions =
            scandir(CXL_DEVICES_PATH, &namelist, filter_region, alphasort);
        if (regions < 0) {
                /* the bus is there and its devices could not be listed, which
                 * is not the same as it having none
                 */
                LOG_WARN("CXL: %s could not be listed\n", CXL_DEVICES_PATH);
                *available = 0;

                return PQOS_RETVAL_OK;
        }

        /* a region has a target per interleave way, and each target is a device
         * of its own in the report, so the array is sized by targets and not by
         * regions
         */
        for (i = 0; i < regions; i++)
                count += count_targets(namelist[i]->d_name);

        if (count == 0) {
                LOG_INFO("CXL: the bus carries no mapped region\n");
                goto free_list;
        }

        out = calloc(count, sizeof(*out));
        if (out == NULL) {
                for (i = 0; i < regions; i++)
                        free(namelist[i]);
                free(namelist);

                return PQOS_RETVAL_RESOURCE;
        }

        for (i = 0; i < regions; i++) {
                const char *region = namelist[i]->d_name;
                char path[PATH_MAX];
                uint64_t base = 0;
                uint64_t size = 0;
                int address_valid = 0;
                unsigned targets;
                unsigned t;

                /* the address range first: it is the whole reason this region
                 * is interesting to a report about memory ranges, and the one
                 * thing the devices below cannot be found without
                 */
                snprintf(path, sizeof(path), CXL_DEVICES_PATH "/%s/resource",
                         region);
                if (pqos_fread_uint64(path, 16, &base) == PQOS_RETVAL_OK) {
                        snprintf(path, sizeof(path),
                                 CXL_DEVICES_PATH "/%s/size", region);
                        if (pqos_fread_uint64(path, 16, &size) ==
                            PQOS_RETVAL_OK)
                                address_valid = 1;
                }
                if (!address_valid)
                        LOG_WARN("CXL: %s does not report an address range\n",
                                 region);

                targets = count_targets(region);
                for (t = 0; t < targets && filled < count; t++)
                        read_target(region, t, base, size, address_valid,
                                    &out[filled++]);
        }

        *num_devices = filled;
        *devices = out;

free_list:
        for (i = 0; i < regions; i++)
                free(namelist[i]);
        free(namelist);

        return PQOS_RETVAL_OK;
}

#else /* !__linux__ */

int
cxl_devices_read(int *available,
                 unsigned *num_devices,
                 struct pqos_cxl_device **devices)
{
        if (available == NULL || num_devices == NULL || devices == NULL)
                return PQOS_RETVAL_PARAM;

        /* the operating system publishes no CXL bus to ask, which the report
         * states as such rather than as an empty list of devices
         */
        *available = 0;
        *num_devices = 0;
        *devices = NULL;

        return PQOS_RETVAL_OK;
}

#endif /* __linux__ */

void
cxl_devices_free(struct pqos_cxl_device *devices)
{
        free(devices);
}
