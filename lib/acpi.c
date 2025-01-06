/*
 * BSD LICENSE
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 * All rights reserved.
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

#include "acpi.h"

#include "common.h"
#include "log.h"
#include "types.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define DEV_MEM            "/dev/mem"
#define EFI_SYSTAB         "/sys/firmware/efi/systab"
#define ACPI_TABLE_FS_PATH "/sys/firmware/acpi/tables"

#define BIOS_RO_MEM_ADDR 0x000E0000LLU
#define BIOS_RO_MEM_SIZE 0x00020000LLU
#define PAGE_SIZE        4096

enum acpi_tbl_mtype {
        ACPI_TBL_MMAP,
        ACPI_TBL_ALLOC,
};

struct acpi_table_internal {
        struct acpi_table table;
        acpi_address address;
        acpi_size size;
        enum acpi_tbl_mtype mtype;
};

int
acpi_init(void)
{
        return PQOS_RETVAL_OK;
}

int
acpi_fini(void)
{
        return PQOS_RETVAL_OK;
}

/**
 * @brief Verify checksum of ACPI table
 *
 * @param[in] mem ACPI Table/buffer
 * @param[in] size ACPI Table/buffer size
 *
 * @return 1 if checksum is valid
 *         0 otherwise
 */
static unsigned
acpi_verify_checksum(const uint8_t *mem, size_t size)
{
        if (!mem || !size)
                return 0;

        uint8_t sum = 0;

        while (size--)
                sum += *mem++;

        return (int)(sum == 0);
}

/**
 * @brief Verify checksum of RSDP ACPI table
 *
 * @param[in] mem RSDP ACPI Table/buffer
 *
 * @return 1 if checksum is valid
 *         0 otherwise
 */
static unsigned
acpi_rsdp_verify_checksum(const uint8_t *mem)
{
        if (!mem)
                return 0;

        /* check standard checksum */
        if (!acpi_verify_checksum(mem, ACPI_TABLE_RSDP_SIZE))
                return 0;

        /* check extended checksum */
        size_t size = sizeof(struct acpi_table_rsdp);
        const struct acpi_table_rsdp *rsdp =
            (const struct acpi_table_rsdp *)mem;

        if (rsdp->revision >= 2 && !acpi_verify_checksum(mem, size))
                return 0;

        return 1;
}

/**
 * @brief Map physical memory
 *
 * @param[in] address Physical memory address
 * @param[in] size Memory size
 *
 * @return Mapped memory
 */
static uint8_t *
acpi_memory_map(const acpi_address address, const acpi_size size)
{
        return pqos_mmap_read(address, size);
}

/**
 * @brief Unmap physical memory
 *
 * @param[in] mem logical memory address
 * @param[in] size Memory size
 */
static void
acpi_memory_unmap(void *mem, const acpi_size size)
{
        pqos_munmap(mem, size);
}

/**
 * @brief Obtain RSDP address
 *
 * @return Obtained RSDP address
 */
static acpi_address
acpi_rsdp_address_efi(void)
{
        unsigned long long addr = 0;
        unsigned long long acpi20_addr = 0;
        unsigned long long acpi_addr = 0;
        FILE *fd;
        char buffer[80];

        fd = pqos_fopen(EFI_SYSTAB, "r");
        if (fd) {
                while (fgets(buffer, sizeof(buffer), fd)) {
                        if (sscanf(buffer, "ACPI20=0x%llx", &acpi20_addr) ==
                            1) {
                                addr = acpi20_addr;
                                break;
                        }
                        if (sscanf(buffer, "ACPI=0x%llx", &acpi_addr) == 1)
                                addr = acpi_addr;
                }
                fclose(fd);
        }

        if (!addr && addr > (ULLONG_MAX - sizeof(struct acpi_table_rsdp)))
                return 0;

        return addr;
}

/**
 * @brief Scan for RSDP address in read-only BIOS area
 *
 * @return Obtained RSDP Address
 */
static acpi_address
acpi_rsdp_address_scan(void)
{
        acpi_address addr = 0;
        uint8_t *data = NULL;
        uint32_t offset;

        data = acpi_memory_map(BIOS_RO_MEM_ADDR, BIOS_RO_MEM_SIZE);
        if (data == NULL)
                return 0;

        for (offset = 0; offset < BIOS_RO_MEM_SIZE; offset += 16) {
                /* check signature */
                if (memcmp(data + offset, "RSD PTR ", 8) != 0)
                        continue;

                /* verify checksum */
                if (!acpi_rsdp_verify_checksum(data + offset))
                        continue;

                addr = (acpi_address)(BIOS_RO_MEM_ADDR + offset);
                break;
        }

        acpi_memory_unmap(data, BIOS_RO_MEM_SIZE);

        return addr;
}

struct acpi_table *
acpi_get_addr(acpi_address addr)
{
        struct acpi_table_internal *data;
        acpi_size size = sizeof(struct acpi_table_header);
        struct acpi_table_header *table;

        data = (struct acpi_table_internal *)malloc(sizeof(*data));
        if (data == NULL) {
                LOG_ERROR("Memory allocation failed!\n");
                return NULL;
        }

        /* mmap ACPI table header section to get true table size */
        table = (struct acpi_table_header *)acpi_memory_map(
            addr, sizeof(struct acpi_table_header));
        if (table == NULL) {
                free(data);
                return NULL;
        }

        /* Get true table size and unmmap */
        size = table->length;
        acpi_memory_unmap(table, sizeof(struct acpi_table_header));

        /* mmap ACPI table again, but full size this time */
        uint8_t *mem = acpi_memory_map(addr, size);

        if (mem == NULL) {
                free(data);
                return NULL;
        }

        /* Verify ACPI table checksum */
        if (!acpi_verify_checksum(mem, size)) {
                LOG_ERROR("Table is invalid! Checksum failed!\n");
                acpi_memory_unmap(mem, size);
                free(data);
                return NULL;
        }

        data->table.header = (struct acpi_table_header *)mem;
        data->address = addr;
        data->size = size;
        data->mtype = ACPI_TBL_MMAP;

        return (struct acpi_table *)data;
}

/**
 * @brief Reads ACPI tables from sysfs
 *
 * @param [in] path file contains ACPI table in sysfs
 *
 * @return Pointer to ACPI table in memory
 * @retval NULL if file wasn't read
 */
static struct acpi_table *
acpi_read_fs(const char *path)
{
        struct acpi_table_internal *tbl_internal = NULL;
        void *tbl_header;
        char *tbl_entries_addr;
        int tbl_entries_len;
        unsigned int hdr_len;
        int fd;

        tbl_internal = malloc(sizeof(*tbl_internal));
        if (tbl_internal == NULL)
                goto acpi_read_fs_out;

        tbl_internal->mtype = ACPI_TBL_ALLOC;

        tbl_internal->table.header = malloc(sizeof(struct acpi_table_header));
        if (tbl_internal->table.header == NULL)
                goto acpi_read_fs_clean_table;

        fd = open(path, O_RDONLY);
        if (fd < 0)
                goto acpi_read_fs_clean_table;

        /* Read table header to obtain table length */
        hdr_len = sizeof(*tbl_internal->table.header);
        if (pqos_read(fd, tbl_internal->table.header, hdr_len) != hdr_len)
                goto acpi_read_fs_close_file;

        /* Validate table length */
        if ((tbl_internal->table.header->length <= hdr_len) ||
            (tbl_internal->table.header->length >= UINT32_MAX))
                goto acpi_read_fs_close_file;

        /* Increase memory size allocated for ACPI table to its length */
        tbl_header = tbl_internal->table.generic;
        tbl_internal->table.generic = realloc(
            tbl_internal->table.generic, tbl_internal->table.header->length);
        if (tbl_internal->table.generic == NULL) {
                tbl_internal->table.generic = tbl_header;
                goto acpi_read_fs_close_file;
        }

        /* Read the rest of APCI table into memory */
        tbl_entries_len = tbl_internal->table.header->length - hdr_len;
        tbl_entries_addr = (char *)tbl_internal->table.generic + hdr_len;
        if (pqos_read(fd, tbl_entries_addr, tbl_entries_len) != tbl_entries_len)
                goto acpi_read_fs_close_file;

        /* Verify ACPI table checksum */
        if (!acpi_verify_checksum(tbl_internal->table.generic,
                                  tbl_internal->table.header->length)) {
                LOG_ERROR("Invalid ACPI table checksum\n");
                goto acpi_read_fs_close_file;
        }

        close(fd);

        return (struct acpi_table *)tbl_internal;

acpi_read_fs_close_file:
        close(fd);

acpi_read_fs_clean_table:
        acpi_free((struct acpi_table *)tbl_internal);

acpi_read_fs_out:
        return NULL;
}

/**
 * @brief Gets ACPI Table with matching signature from memory
 *
 * @param sig Signature of ACPI Table to get
 *
 * @return Pointer to struct with ACPI table
 * @retval NULL on error
 */
static struct acpi_table *
acpi_get_mmap(const char *sig)
{
        struct acpi_table *tbl = NULL;
        struct acpi_table *rsdp = NULL;

        rsdp = acpi_get_rsdp();
        if (rsdp == NULL) {
                LOG_ERROR("Failed to obtain RSDP table!\n");
                return NULL;
        }

        acpi_print(rsdp);

        /* XSDT - eXtended System Descriptor Table detection */
        if (rsdp->rsdp->revision >= 2 && rsdp->rsdp->xsdt_address) {
                struct acpi_table *xsdt;
                unsigned i;
                unsigned count;

                xsdt = acpi_get_addr(rsdp->rsdp->xsdt_address);
                if (xsdt == NULL) {
                        LOG_ERROR("Failed to obtain XSDT table!\n");
                        acpi_free(rsdp);
                        return NULL;
                }

                acpi_print(xsdt);

                count =
                    (xsdt->xsdt->header.length - sizeof(xsdt->xsdt->header)) /
                    sizeof(xsdt->xsdt->entry[0]);
                for (i = 0; i < count; i++) {
                        acpi_address addr = xsdt->xsdt->entry[i];
                        uint8_t *mem = acpi_memory_map(addr, 4);

                        if (mem == NULL)
                                break;

                        if (memcmp(sig, mem, 4) == 0) {
                                acpi_memory_unmap(mem, 4);
                                tbl = acpi_get_addr(addr);
                                break;
                        }

                        acpi_memory_unmap(mem, 4);
                }

                acpi_free(xsdt);

        } else if (rsdp->rsdp->rsdt_address) {
                /* RSDT - Root System Description Table */
                struct acpi_table *rsdt;
                unsigned i;
                unsigned count;

                rsdt = acpi_get_addr(rsdp->rsdp->rsdt_address);
                if (rsdt == NULL) {
                        LOG_ERROR("Failed to obtain RSDT table!\n");
                        acpi_free(rsdp);
                        return NULL;
                }

                acpi_print(rsdt);

                count =
                    (rsdt->rsdt->header.length - sizeof(rsdt->rsdt->header)) /
                    sizeof(rsdt->rsdt->entry[0]);
                for (i = 0; i < count; i++) {
                        acpi_address addr = rsdt->rsdt->entry[i];
                        uint8_t *mem = acpi_memory_map(addr, 4);

                        if (mem == NULL)
                                break;

                        if (memcmp(sig, mem, 4) == 0) {
                                acpi_memory_unmap(mem, 4);
                                tbl = acpi_get_addr(addr);
                                break;
                        }

                        acpi_memory_unmap(mem, 4);
                }

                acpi_free(rsdt);

        } else
                LOG_ERROR("No RSDT or XSDT table!\n");

        acpi_free(rsdp);

        return tbl;
}

struct acpi_table *
acpi_get_sig(const char *sig)
{
        struct acpi_table *tbl = NULL;
        char sysfs_sig_fp[256];

        snprintf(sysfs_sig_fp, sizeof(sysfs_sig_fp), ACPI_TABLE_FS_PATH "/%s",
                 sig);

        if (pqos_file_exists(sysfs_sig_fp)) {
                LOG_DEBUG("Trying to obtain %s acpi table from file: %s\n", sig,
                          sysfs_sig_fp);
                tbl = acpi_read_fs(sysfs_sig_fp);
                if (tbl != NULL)
                        return tbl;
        }

        LOG_DEBUG("Trying to obtain %s acpi table from ACPI memory\n", sig);
        return acpi_get_mmap(sig);
}

struct acpi_table *
acpi_get_rsdp(void)
{
        acpi_address address = 0;
        acpi_size size;
        struct acpi_table_internal *data;
        uint8_t *mem;

        size = sizeof(struct acpi_table_rsdp);
        address = acpi_rsdp_address_efi();
        if (address == 0)
                address = acpi_rsdp_address_scan();

        if (address == 0) {
                LOG_ERROR("RSDP table not found!\n");
                return NULL;
        }

        LOG_DEBUG("RSDP@%llx\n", (unsigned long long)address);

        data = (struct acpi_table_internal *)malloc(sizeof(*data));
        if (data == NULL) {
                LOG_ERROR("Memory allocation failed!\n");
                return NULL;
        }

        mem = acpi_memory_map(address, size);
        if (mem == NULL) {
                LOG_ERROR("Memory mapping failed!\n");
                free(data);
                return NULL;
        }

        if (!acpi_rsdp_verify_checksum(mem)) {
                LOG_ERROR("RSDP Checksum failed!\n");
                acpi_memory_unmap(mem, size);
                free(data);
                return NULL;
        }

        data->table.generic = mem;
        data->address = address;
        data->size = size;

        return (struct acpi_table *)data;
}

struct acpi_table *
acpi_get_xsdt(void)
{
        struct acpi_table *rsdp;
        struct acpi_table *xsdt;

        rsdp = acpi_get_rsdp();
        if (rsdp == NULL) {
                LOG_ERROR("Failed to obtain XSDT table!\n");
                return NULL;
        }

        if (rsdp->rsdp->revision < 2 || !rsdp->rsdp->xsdt_address) {
                LOG_ERROR("XSDT table not available!\n");
                acpi_free(rsdp);
                return NULL;
        }

        xsdt = acpi_get_addr(rsdp->rsdp->xsdt_address);
        acpi_free(rsdp);

        return xsdt;
}

void
acpi_free(struct acpi_table *table)
{
        struct acpi_table_internal *data = (struct acpi_table_internal *)table;

        if (data != NULL) {
                if (data->mtype == ACPI_TBL_ALLOC)
                        free(data->table.generic);
                else
                        acpi_memory_unmap(data->table.generic, data->size);

                free(data);
        }
}

int
acpi_get_irdt_chms(struct acpi_table_irdt_device *dev,
                   struct acpi_table_irdt_chms **chms[],
                   size_t *num)
{
        if (!dev || !chms || !num)
                return PQOS_RETVAL_PARAM;

        if (dev->type != ACPI_TABLE_IRDT_TYPE_DSS)
                return PQOS_RETVAL_PARAM;

        unsigned dss_size =
            (sizeof(dev->dss) + sizeof(dev->type) + sizeof(dev->length));

        if (dev->length < dss_size) {
                LOG_ERROR("Invalid DEV DSS length!\n");
                return PQOS_RETVAL_RESOURCE;
        }

        size_t chms_len = dev->length - dss_size;

        *num = chms_len / sizeof(struct acpi_table_irdt_chms);
        *chms = calloc(*num, sizeof(struct acpi_table_irdt_chms *));
        if (!*chms) {
                LOG_ERROR("Memory allocation failed!\n");
                *num = 0;
                return PQOS_RETVAL_RESOURCE;
        }
        size_t i;

        for (i = 0; i < *num; i++)
                (*chms)[i] = &dev->dss.chms_rcs_enumeration[i];

        return PQOS_RETVAL_OK;
}

struct acpi_table_irdt_device **
acpi_get_irdt_dev(struct acpi_table_irdt_rmud *rmud, size_t *num)
{
        struct acpi_table_irdt_device **devs = NULL;

        if (!rmud || !num)
                return NULL;

        if (rmud->length < sizeof(*rmud)) {
                LOG_ERROR("Invalid RMUD len!\n");
                return NULL;
        }

        size_t dev_len = rmud->length - sizeof(*rmud);
        struct acpi_table_irdt_device *dev = rmud->device;

        *num = 0;

        while (dev_len > 0) {
                (*num)++;

                struct acpi_table_irdt_device **devs_tmp =
                    realloc(devs, (*num) * sizeof(dev));

                if (devs_tmp == NULL) {
                        LOG_ERROR("Memory allocation failed!\n");
                        free(devs);
                        *num = 0;
                        return NULL;
                }
                devs = devs_tmp;
                devs[(*num) - 1] = dev;

                if (dev_len == dev->length)
                        break;
                else if (dev_len < dev->length) {
                        LOG_ERROR("Invalid DEV len!\n");
                        free(devs);
                        *num = 0;
                        return NULL;
                }

                dev_len -= dev->length;

                dev = (struct acpi_table_irdt_device *)((uint8_t *)dev +
                                                        dev->length);
        }

        return devs;
}

struct acpi_table_irdt_rmud **
acpi_get_irdt_rmud(struct acpi_table_irdt *irdt, size_t *num)
{
        struct acpi_table_irdt_rmud **rmuds = NULL;

        if (!irdt || !num)
                return NULL;

        if (irdt->header.length < sizeof(*irdt)) {
                LOG_ERROR("Invalid IRDT len!\n");
                return NULL;
        }

        size_t rmuds_len = irdt->header.length - sizeof(*irdt);
        struct acpi_table_irdt_rmud *rmud = irdt->rmud;

        *num = 0;

        while (rmuds_len > 0) {
                (*num)++;

                struct acpi_table_irdt_rmud **rmuds_tmp =
                    realloc(rmuds, (*num) * sizeof(rmud));

                if (!rmuds_tmp) {
                        LOG_ERROR("Memory allocation failed!\n");
                        free(rmuds);
                        *num = 0;
                        return NULL;
                }
                rmuds = rmuds_tmp;
                rmuds[(*num) - 1] = rmud;

                if (rmuds_len == rmud->length)
                        break;
                else if (rmuds_len < rmud->length) {
                        LOG_ERROR("Invalid RMUD len!\n");
                        free(rmuds);
                        *num = 0;
                        return NULL;
                }

                rmuds_len -= rmud->length;

                rmud = (struct acpi_table_irdt_rmud *)((uint8_t *)rmud +
                                                       rmud->length);
        }

        return rmuds;
}

static void
acpi_print_irdt_chms(struct acpi_table_irdt_chms *chms)
{
        if (!chms)
                return;

        LOG_DEBUG("   RCS Enum ID:         %u\n", chms->rcs_enum_id);
        size_t i;

        for (i = 0; i < DIM(chms->vc_map); i++) {
                uint8_t vc_map = chms->vc_map[i];

                /* Check if this is a valid entry */
                if (!(vc_map & ACPI_TABLE_IRDT_CHMS_CHAN_VALID))
                        continue;

                uint8_t chan = vc_map & ~(ACPI_TABLE_IRDT_CHMS_CHAN_MASK);
                uint8_t shared = vc_map & ACPI_TABLE_IRDT_CHMS_CHAN_SHARED;

                LOG_DEBUG("    VC%zu - Channel:       %u %s\n", i, chan,
                          shared ? "SHARED" : "");
        }

        return;
}

static const char *
acpi_get_irdt_device_type(uint16_t type)
{
        if (type == ACPI_TABLE_IRDT_TYPE_DSS)
                return "DSS";
        else if (type == ACPI_TABLE_IRDT_TYPE_RCS)
                return "RCS";
        else
                return "Unknown Type!";
}

static void
acpi_print_irdt_device(struct acpi_table_irdt_device *dev)
{
        if (!dev)
                return;

        LOG_DEBUG(" %s\n", acpi_get_irdt_device_type(dev->type));
        LOG_DEBUG("  Type:                 %#X/%s\n", dev->type,
                  acpi_get_irdt_device_type(dev->type));
        LOG_DEBUG("  Length:               %u\n", dev->length);

        if (dev->type == ACPI_TABLE_IRDT_TYPE_DSS) {
                LOG_DEBUG("  Device Type:          %#X\n",
                          dev->dss.device_type);
                LOG_DEBUG("  Enumeration ID:       %u\n",
                          dev->dss.enumeration_id);

                unsigned dss_size = (sizeof(dev->dss) + sizeof(dev->type) +
                                     sizeof(dev->length));

                if (dev->length < dss_size) {
                        LOG_ERROR("Invalid DEV DSS length!\n");
                        return;
                }

                size_t chms_num = 0;
                struct acpi_table_irdt_chms **chms = NULL;

                acpi_get_irdt_chms(dev, &chms, &chms_num);
                LOG_DEBUG("  %llu CHMS(s):\n", chms_num);

                size_t i;

                for (i = 0; i < chms_num; i++)
                        acpi_print_irdt_chms(chms[i]);

                if (chms)
                        free(chms);
        } else if (dev->type == ACPI_TABLE_IRDT_TYPE_RCS) {
                LOG_DEBUG("  Link Interface Type:  %#X\n",
                          dev->rcs.link_interface_type);
                LOG_DEBUG("  Enumeration ID:       %u\n",
                          dev->rcs.rcs_enumeration_id);
                LOG_DEBUG("  Channel Count:        %u\n",
                          dev->rcs.channel_count);
                LOG_DEBUG("  Flags:                %#X\n", dev->rcs.flags);
                LOG_DEBUG("  RMID Block Offset:    %#X\n",
                          dev->rcs.rmid_block_offset);
                LOG_DEBUG("  CLOS Block Offset:    %#X\n",
                          dev->rcs.clos_block_offset);
                LOG_DEBUG("  Block MMIO:           %#018llX\n",
                          (unsigned long long)dev->rcs.rcs_block_mmio_location);
        }

        return;
}

static const char *
acpi_get_irdt_rmud_type(uint16_t type)
{
        if (type == ACPI_TABLE_IRDT_TYPE_RMUD)
                return "RMUD";
        else
                return "Unknown Type!";
}

static void
acpi_print_irdt_rmud(struct acpi_table_irdt_rmud *rmud)
{
        if (!rmud)
                return;

        LOG_DEBUG(" Type:              %#X/%s\n", rmud->type,
                  acpi_get_irdt_rmud_type(rmud->type));

        LOG_DEBUG(" Length:            %u\n", rmud->length);
        LOG_DEBUG(" PCI Segment:       %#X\n", rmud->segment);

        size_t devs_num = 0;
        struct acpi_table_irdt_device **devs = NULL;
        size_t i;

        devs = acpi_get_irdt_dev(rmud, &devs_num);
        if (devs != NULL) {
                for (i = 0; i < devs_num; i++)
                        acpi_print_irdt_device(devs[i]);
                free(devs);
        }
}

static const char *
acpi_get_irdt_proto_flags(uint16_t flags, uint16_t mask)
{
        if (flags & mask & ACPI_TABLE_IRDT_PROTO_FLAGS_MON)
                return " MON";
        if (flags & mask & ACPI_TABLE_IRDT_PROTO_FLAGS_CTL)
                return " CTL";
        if (flags & mask & ACPI_TABLE_IRDT_PROTO_FLAGS_BW_CTL)
                return " BW_CTL";

        return "";
}

void
acpi_print(struct acpi_table *table)
{
        /* RSDP is different than all the other tables */
        if (memcmp(table->signature, "RSD PTR ", 8) == 0) {
                LOG_DEBUG("Signature:         %.8s\n", table->rsdp->signature);
                LOG_DEBUG("Checksum:          %u\n", table->rsdp->checksum);
                LOG_DEBUG("OEM ID:            %.6s\n", table->rsdp->oem_id);
                LOG_DEBUG("Revision:          %u\n", table->rsdp->revision);
                LOG_DEBUG("RSDT Address:      %#08llx\n",
                          (unsigned long long)table->rsdp->rsdt_address);
                if (table->rsdp->revision >= 2) {
                        LOG_DEBUG("Length:            %u\n",
                                  table->rsdp->length);
                        LOG_DEBUG(
                            "XSDT Address:      %#016llx\n",
                            (unsigned long long)table->rsdp->xsdt_address);
                        LOG_DEBUG("Extended Checksum: %u\n",
                                  table->rsdp->extended_checksum);
                }

                LOG_DEBUG("\n");

                return;
        }

        /* Print header common for ACPI tables (other than RSDP) */
        LOG_DEBUG("Signature:         %.4s\n", table->header->signature);
        LOG_DEBUG("Length:            %u\n", table->header->length);
        LOG_DEBUG("Revision:          %u\n", table->header->revision);
        LOG_DEBUG("Checksum:          %u\n", table->header->checksum);
        LOG_DEBUG("OEM ID:            %.6s\n", table->header->oem_id);
        LOG_DEBUG("OEM Table ID:      %.8s\n", table->header->oem_table_id);
        LOG_DEBUG("OEM Revision:      %u\n", table->header->oem_revision);
        LOG_DEBUG("Creator ID:        %u\n", table->header->creator_id);
        LOG_DEBUG("Creator Revision:  %u\n", table->header->creator_revision);

        if (memcmp("XSDT", table->header->signature, 4) == 0) {
                uint32_t i;
                uint32_t count =
                    (table->header->length - sizeof(struct acpi_table_xsdt)) /
                    sizeof(*table->xsdt->entry);

                for (i = 0; i < count; ++i)
                        LOG_DEBUG("Entry:             %#016llx\n",
                                  (unsigned long long)table->xsdt->entry[i]);
        }

        if (memcmp("RSDT", table->header->signature, 4) == 0) {
                uint32_t i;
                uint32_t count =
                    (table->header->length - sizeof(struct acpi_table_rsdt)) /
                    sizeof(*table->rsdt->entry);

                for (i = 0; i < count; ++i)
                        LOG_DEBUG("Entry:             %#016llx\n",
                                  (unsigned long long)table->rsdt->entry[i]);
        }

        if (memcmp("IRDT", table->header->signature, 4) == 0) {
                uint16_t flags = table->irdt->io_protocol_flags;

                LOG_DEBUG("IO Proto Flags:    %#X:%s%s%s\n", flags,
                          acpi_get_irdt_proto_flags(
                              flags, ACPI_TABLE_IRDT_PROTO_FLAGS_MON),
                          acpi_get_irdt_proto_flags(
                              flags, ACPI_TABLE_IRDT_PROTO_FLAGS_CTL),
                          acpi_get_irdt_proto_flags(
                              flags, ACPI_TABLE_IRDT_PROTO_FLAGS_BW_CTL));

                flags = table->irdt->cache_protocol_flags;

                LOG_DEBUG("Cache Proto Flags: %#X:%s%s%s\n", flags,
                          acpi_get_irdt_proto_flags(
                              flags, ACPI_TABLE_IRDT_PROTO_FLAGS_MON),
                          acpi_get_irdt_proto_flags(
                              flags, ACPI_TABLE_IRDT_PROTO_FLAGS_CTL),
                          acpi_get_irdt_proto_flags(
                              flags, ACPI_TABLE_IRDT_PROTO_FLAGS_BW_CTL));

                size_t rmuds_num = 0;
                struct acpi_table_irdt_rmud **rmuds = NULL;
                size_t i;

                rmuds = acpi_get_irdt_rmud(table->irdt, &rmuds_num);
                if (rmuds) {
                        for (i = 0; i < rmuds_num; i++) {
                                LOG_DEBUG("RMUD #%lu:\n", i);
                                acpi_print_irdt_rmud(rmuds[i]);
                        }
                        free(rmuds);
                }
        }

        LOG_DEBUG("\n");
}
