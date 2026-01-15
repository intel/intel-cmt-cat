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

#include "pci.h"

#include "cap.h"
#include "common.h"
#include "log.h"
#include "pqos.h"
#include "utils.h"

#include <asm/byteorder.h>
#include <fcntl.h>
#include <linux/pci.h>
#include <stdlib.h>
#include <string.h>
#include <sys/io.h>
#include <sys/mman.h>

#define PCI_BASE           0x80000000
#define PCI_CONFIG_ADDRESS 0xCFC
#define PCI_CONFIG_DATA    0xCF8
#define PCI_DEVICES_DIR    "/sys/bus/pci/devices"
#define PCI_IDS_FILE       "/usr/share/misc/pci.ids"

#define PCI_CONFIG_CAPABILITIES_POINTER 0x34

/** Define file names' strings */
#define PCI_SYSFS_FILE_STR_VENDOR   "vendor"
#define PCI_SYSFS_FILE_STR_DEVICE   "device"
#define PCI_SYSFS_FILE_STR_CLASS    "class"
#define PCI_SYSFS_FILE_STR_REVISION "revision"

#define BUF_SIZE_512 512

/** module is initialized */
static int m_initialized = 0;

int
pci_init(void)
{
        if (m_initialized)
                return PQOS_RETVAL_OK;

        if (iopl(3) < 0) {
                LOG_ERROR("Insufficient permission to access I/O ports\n");
                return PQOS_RETVAL_ERROR;
        }
        m_initialized = 1;

        return PQOS_RETVAL_OK;
}

int
pci_fini(void)
{
        if (m_initialized)
                return PQOS_RETVAL_OK;

        iopl(3);
        m_initialized = 0;

        return PQOS_RETVAL_OK;
}

static void
pci_parse_pci_ids(struct pqos_pci_info *info,
                  unsigned int class_code,
                  unsigned int vendor_id,
                  unsigned int device_id)
{
        FILE *fd = NULL;
        char line[BUF_SIZE_512];
        int vendor_found = 0;
        unsigned int subclass = (class_code >> 8) & 0xFF;

        if (!pqos_file_exists(PCI_IDS_FILE)) {
                LOG_ERROR("Unable to find %s. PCI device name will not be "
                          "displayed. Install pciutils.\n",
                          PCI_IDS_FILE);
                return;
        }

        fd = pqos_fopen(PCI_IDS_FILE, "r");
        if (fd == NULL) {
                LOG_ERROR("Unable to open %s. PCI device name will not be "
                          "displayed. Install/update pciutils/%s.\n",
                          PCI_IDS_FILE, PCI_IDS_FILE);
                return;
        }

        while (fgets(line, sizeof(line), fd)) {
                if (line[0] == '#')
                        continue;

                if (line[0] != '\t' && line[0] != 'C') {
                        unsigned int vid = 0;
                        char vname[BUF_SIZE_256] = {0};

                        if (sscanf(line, "%x %[^\n]", &vid, vname) == 2) {
                                if (vid == vendor_id) {
                                        snprintf(info->vendor_name,
                                                 sizeof(info->vendor_name),
                                                 "%s", vname);
                                        vendor_found = 1;
                                } else
                                        vendor_found = 0;
                        }
                } else if (vendor_found && line[1] != '\t') {
                        unsigned int did = 0;
                        char dname[BUF_SIZE_256] = {0};

                        if (sscanf(line, "\t%x %[^\n]", &did, dname) == 2)
                                if (did == device_id)
                                        snprintf(info->device_name,
                                                 sizeof(info->device_name),
                                                 "%s", dname);
                }

                if (line[0] == '\t' && line[1] != '\t') {
                        unsigned int scid = 0;
                        char scname[BUF_SIZE_256] = {0};

                        if (sscanf(line, "\t%x %[^\n]", &scid, scname) == 2)
                                if (scid == subclass)
                                        snprintf(info->subclass_name,
                                                 sizeof(info->subclass_name),
                                                 "%s", scname);
                }
        }

        fclose(fd);
}

static int
pci_read_sysfs(unsigned int *value, const char *sysfs_file, struct pci_dev *dev)
{
        FILE *fd = NULL;
        char path[BUF_SIZE_256];

        ASSERT(dev != NULL);

        if (!pqos_dir_exists(PCI_DEVICES_DIR))
                return PQOS_RETVAL_RESOURCE;

        snprintf(path, sizeof(path), PCI_DEVICES_DIR "/%04x:%02x:%02x.%x/%s",
                 dev->domain, dev->bus, dev->dev, dev->func, sysfs_file);

        fd = pqos_fopen(path, "r");
        if (fd == NULL) {
                LOG_ERROR("PCI %04x:%02x:%02x.%x failed to open %s file\n",
                          (unsigned)dev->domain, (unsigned)dev->bus,
                          (unsigned)dev->dev, (unsigned)dev->func, sysfs_file);
                return PQOS_RETVAL_ERROR;
        }

        if (fscanf(fd, "%x", value) != 1) {
                LOG_ERROR("PCI %04x:%02x:%02x.%x failed to read %s file\n",
                          (unsigned)dev->domain, (unsigned)dev->bus,
                          (unsigned)dev->dev, (unsigned)dev->func, sysfs_file);
                fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
        fclose(fd);

        if (*value >= INT32_MAX)
                return PQOS_RETVAL_ERROR;

        return PQOS_RETVAL_OK;
}

static int
pci_read_driver(struct pqos_pci_info *info, struct pci_dev *dev)
{
        char path[BUF_SIZE_256] = {0};
        char link[BUF_SIZE_256] = {0};
        ssize_t len = 0;

        if (!pqos_dir_exists(PCI_DEVICES_DIR))
                return PQOS_RETVAL_RESOURCE;

        snprintf(path, sizeof(path),
                 PCI_DEVICES_DIR "/%04x:%02x:%02x.%x/driver", dev->domain,
                 dev->bus, dev->dev, dev->func);

        len = readlink(path, link, sizeof(link) - 1);
        if (len > 0) {
                link[len] = '\0';
                strncpy(info->kernel_driver, basename(link),
                        sizeof(info->kernel_driver) - 1);
                info->kernel_driver[sizeof(info->kernel_driver) - 1] = '\0';
        }

        return PQOS_RETVAL_OK;
}

static int
pci_read_config(struct pqos_pci_info *info, struct pci_dev *dev)
{
        int fd;
        unsigned int cap_ptr = 0;
        unsigned char cap_id = 0;
        unsigned char dev_type = 0;
        unsigned char config[BUF_SIZE_256] = {0};
        char path[BUF_SIZE_256] = {0};

        if (!pqos_dir_exists(PCI_DEVICES_DIR))
                return PQOS_RETVAL_RESOURCE;

        snprintf(path, sizeof(path),
                 PCI_DEVICES_DIR "/%04x:%02x:%02x.%x/config", dev->domain,
                 dev->bus, dev->dev, dev->func);

        fd = open(path, O_RDONLY);
        if (fd < 0) {
                LOG_ERROR("PCI %04x:%02x:%02x.%x failed to open config file\n",
                          (unsigned)dev->domain, (unsigned)dev->bus,
                          (unsigned)dev->dev, (unsigned)dev->func);
                return PQOS_RETVAL_ERROR;
        }

        if (pread(fd, config, sizeof(config), 0) < 0) {
                close(fd);
                return PQOS_RETVAL_ERROR;
        }

        close(fd);

        cap_ptr = config[PCI_CONFIG_CAPABILITIES_POINTER];
        if (cap_ptr >= BUF_SIZE_256) {
                LOG_ERROR("PCI %04x:%02x:%02x.%x has wrong config value "
                          "in capabilities pointer (0x%x). Value is 0x%x\n",
                          (unsigned)dev->domain, (unsigned)dev->bus,
                          (unsigned)dev->dev, (unsigned)dev->func,
                          PCI_CONFIG_CAPABILITIES_POINTER, cap_ptr);
                return PQOS_RETVAL_ERROR;
        }

        while (cap_ptr) {
                cap_id = config[cap_ptr];
                /* PCI Express Capability */
                if (cap_id == 0x10) {
                        info->is_pcie = 1;
                        dev_type = (config[cap_ptr + 2] >> 4) & 0xF;
                        switch (dev_type) {
                        case 0x0:
                                snprintf(info->pcie_type,
                                         sizeof(info->pcie_type), "Endpoint");
                                break;
                        case 0x4:
                                snprintf(info->pcie_type,
                                         sizeof(info->pcie_type),
                                         "Root Complex Endpoint");
                                break;
                        default:
                                snprintf(info->pcie_type,
                                         sizeof(info->pcie_type),
                                         "PCIe Device");
                                break;
                        }
                        break;
                }
                cap_ptr = config[cap_ptr + 1];
        }

        return PQOS_RETVAL_OK;
}

/**
 * @brief Obtain socket Id for PCI device
 *
 * @param [in] dev PCI device allocated by \a pci_dev_get
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
static int
pci_read_numa(struct pci_dev *dev)
{
        char path[BUF_SIZE_256];
        FILE *fd = NULL;
        int numa;

        ASSERT(dev != NULL);

        if (!pqos_dir_exists(PCI_DEVICES_DIR))
                return PQOS_RETVAL_RESOURCE;

        snprintf(path, sizeof(path),
                 PCI_DEVICES_DIR "/%04x:%02x:%02x.%x/numa_node", dev->domain,
                 dev->bus, dev->dev, dev->func);

        fd = pqos_fopen(path, "r");
        if (fd == NULL) {
                LOG_ERROR(
                    "PCI %04x:%02x:%02x.%x failed to open numa_node file\n",
                    (unsigned)dev->domain, (unsigned)dev->bus,
                    (unsigned)dev->dev, (unsigned)dev->func);
                return PQOS_RETVAL_ERROR;
        }

        if (fscanf(fd, "%d", &numa) != 1) {
                LOG_ERROR(
                    "PCI %04x:%02x:%02x.%x failed to read numa_node file\n",
                    (unsigned)dev->domain, (unsigned)dev->bus,
                    (unsigned)dev->dev, (unsigned)dev->func);
                fclose(fd);
                return PQOS_RETVAL_ERROR;
        }
        fclose(fd);

        if (numa < 0 || numa >= INT32_MAX)
                return PQOS_RETVAL_ERROR;

        dev->numa = numa;

        return PQOS_RETVAL_OK;
}

struct pci_dev *
pci_dev_get(uint16_t domain, uint16_t bdf)
{
        struct pci_dev *dev;
        uint8_t type;
        unsigned i;
        unsigned bar_num;
        int retval;

        dev = (struct pci_dev *)malloc(sizeof(*dev));
        if (dev == NULL)
                return NULL;

        memset(dev, 0, sizeof(*dev));

        dev->domain = domain;
        dev->bdf = bdf;
        dev->bus = bdf >> 8;
        dev->dev = (bdf >> 3) & 0x1F;
        dev->func = bdf & 0x7;
        dev->numa = PCI_NUMA_INVALID;

        /* Check header type and number of BAR addresses */
        type = pci_read_byte(dev, PCI_HEADER_TYPE) & 0x7f;
        LOG_DEBUG("PCI %04x:%02x:%02x.%x type %x\n", (unsigned)dev->domain,
                  (unsigned)dev->bus, (unsigned)dev->dev, (unsigned)dev->func,
                  type);
        switch (type) {
        case PCI_HEADER_TYPE_NORMAL:
                bar_num = 6;
                break;
        case PCI_HEADER_TYPE_BRIDGE:
                bar_num = 2;
                break;
        case PCI_HEADER_TYPE_CARDBUS:
                bar_num = 1;
                break;
        default:
                pci_dev_release(dev);
                return NULL;
        }

        /* Read bar address  */
        for (i = 0; i < bar_num; i++) {
                uint32_t bar;

                bar = pci_read_long(dev, PCI_BASE_ADDRESS_0 + i * 4);
                if (bar == 0 || bar == UINT32_MAX)
                        continue;

                if ((bar & PCI_BASE_ADDRESS_SPACE) ==
                    PCI_BASE_ADDRESS_SPACE_IO) {
                        dev->bar[dev->bar_num++] = bar;
                        LOG_DEBUG(
                            "PCI %04x:%02x:%02x.%x detected I/O BAR address "
                            "0x%08llx\n",
                            (unsigned)dev->domain, (unsigned)dev->bus,
                            (unsigned)dev->dev, (unsigned)dev->func,
                            (unsigned long long)dev->bar[dev->bar_num - 1]);
                        continue;
                }

                if ((bar & PCI_BASE_ADDRESS_MEM_TYPE_MASK) ==
                    PCI_BASE_ADDRESS_MEM_TYPE_32) {
                        dev->bar[dev->bar_num++] = bar;
                        LOG_DEBUG(
                            "PCI %04x:%02x:%02x.%x detected 32bit BAR address "
                            "0x%08llx\n",
                            (unsigned)dev->domain, (unsigned)dev->bus,
                            (unsigned)dev->dev, (unsigned)dev->func,
                            (unsigned long long)dev->bar[dev->bar_num - 1]);
                        continue;
                }

                if ((bar & PCI_BASE_ADDRESS_MEM_TYPE_MASK) ==
                    PCI_BASE_ADDRESS_MEM_TYPE_64) {
                        uint32_t val;

                        if (i == bar_num - 1) {
                                LOG_ERROR("Invalid 64-bit BAR address");
                                continue;
                        }

                        val =
                            pci_read_long(dev, PCI_BASE_ADDRESS_0 + (++i) * 4);
                        dev->bar[dev->bar_num++] = bar | ((uint64_t)val << 32);
                        LOG_DEBUG(
                            "PCI %04x:%02x:%02x.%x detected 64bit BAR address "
                            "0x%016llx\n",
                            (unsigned)dev->domain, (unsigned)dev->bus,
                            (unsigned)dev->dev, (unsigned)dev->func,
                            (unsigned long long)dev->bar[dev->bar_num - 1]);
                }
        }

        retval = pci_read_numa(dev);
        if (retval != PQOS_RETVAL_OK && retval != PQOS_RETVAL_RESOURCE) {
                LOG_ERROR("PCI %04x:%02x:%02x.%x failed to obtain numa node\n",
                          (unsigned)dev->domain, (unsigned)dev->bus,
                          (unsigned)dev->dev, (unsigned)dev->func);
                free(dev);
                return NULL;
        }

        return dev;
}

int
io_devs_get(struct pqos_pci_info *pci_info, uint16_t segment, uint16_t bdf)
{
        struct pci_dev *dev = NULL;
        unsigned int vendor_id = 0;
        unsigned int device_id = 0;
        unsigned int class_code = 0;
        unsigned int revision = 0;
        unsigned int ret = 0;
        unsigned int idx = 0;
        pqos_channel_t *channels = NULL;
        const struct pqos_channel *channel = NULL;
        const struct pqos_devinfo *devinfo = _pqos_get_dev();

        dev = pci_dev_get(segment, bdf);

        memset(pci_info, 0, sizeof(struct pqos_pci_info));

        pci_read_driver(pci_info, dev);
        pci_info->numa = dev->numa;
        pci_read_config(pci_info, dev);

        ret = pci_read_sysfs(&vendor_id, PCI_SYSFS_FILE_STR_VENDOR, dev);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Unable to open PCI sysfs file %s\n",
                          PCI_SYSFS_FILE_STR_VENDOR);

        ret = pci_read_sysfs(&device_id, PCI_SYSFS_FILE_STR_DEVICE, dev);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Unable to open PCI sysfs file %s\n",
                          PCI_SYSFS_FILE_STR_DEVICE);

        ret = pci_read_sysfs(&class_code, PCI_SYSFS_FILE_STR_CLASS, dev);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Unable to open PCI sysfs file %s\n",
                          PCI_SYSFS_FILE_STR_CLASS);

        /** Populate Vendor name, Device name and Subclass name */
        pci_parse_pci_ids(pci_info, class_code, vendor_id, device_id);

        ret = pci_read_sysfs(&revision, PCI_SYSFS_FILE_STR_REVISION, dev);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Unable to open PCI sysfs file %s\n",
                          PCI_SYSFS_FILE_STR_REVISION);
        pci_info->revision = revision;

        channels = pqos_devinfo_get_channel_ids(devinfo, segment, bdf,
                                                &pci_info->num_channels);
        if (channels == NULL)
                LOG_ERROR("Unable to get channels of %04x:%02x:%02x.%x\n",
                          segment, (bdf >> 8), ((bdf >> 3) & 0x1F),
                          (bdf & 0x7));

        for (idx = 0; idx < pci_info->num_channels && channels; idx++)
                pci_info->channels[idx] = channels[idx];
        free(channels);

        for (idx = 0; idx < pci_info->num_channels; idx++) {
                channel =
                    pqos_devinfo_get_channel(devinfo, pci_info->channels[idx]);
                if (channel != NULL)
                        pci_info->mmio_addr[idx] = channel->mmio_addr;
        }

        free(dev);

        return PQOS_RETVAL_OK;
}

int
hw_io_devs_get(struct pqos_pci_info *pci_info, uint16_t segment, uint16_t bdf)
{
        return io_devs_get(pci_info, segment, bdf);
}

int
mmio_io_devs_get(struct pqos_pci_info *pci_info, uint16_t segment, uint16_t bdf)
{
        unsigned int ret = 0;
        const struct pqos_devinfo *devinfo = _pqos_get_dev();

        ret = pqos_devinfo_get_domain_id(devinfo, segment, bdf,
                                         &pci_info->domain_id);
        if (ret != PQOS_RETVAL_OK)
                LOG_ERROR("Unable to get domain ID of %04x:%02x:%02x.%x\n",
                          segment, (bdf >> 8), ((bdf >> 3) & 0x1F),
                          (bdf & 0x7));

        return io_devs_get(pci_info, segment, bdf);
}

void
pci_dev_release(struct pci_dev *dev)
{
        ASSERT(dev != NULL);

        free(dev);
}

int
pci_read(struct pci_dev *dev, uint32_t offset, uint8_t *data, uint32_t count)
{
        uint32_t addr = PCI_BASE;
        uint32_t pos = PCI_CONFIG_ADDRESS + (offset & 3);

        ASSERT(dev != NULL);

        /* configuration address */
        addr |= dev->bus << 16;
        addr |= dev->dev << 11;
        addr |= dev->func << 8;
        addr |= offset & ~3;

        outl(addr, PCI_CONFIG_DATA);

        if (dev->domain != 0 || offset >= 256)
                return PQOS_RETVAL_PARAM;

        switch (count) {
        case 1:
                *data = inb(pos);
                break;
        case 2:
                *((uint16_t *)(void *)data) = __cpu_to_le16(inw(pos));
                break;
        case 4:
                *((uint32_t *)(void *)data) = __cpu_to_le32(inl(pos));
                break;
        default:
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

int
pci_write(struct pci_dev *dev,
          uint32_t offset,
          const uint8_t *data,
          uint32_t count)
{
        uint32_t addr = PCI_BASE;
        uint32_t pos = PCI_CONFIG_ADDRESS + (offset & 3);

        ASSERT(dev != NULL);

        /* configuration address */
        addr |= dev->bus << 16;
        addr |= dev->dev << 11;
        addr |= dev->func << 8;
        addr |= offset & ~3;

        outl(addr, PCI_CONFIG_DATA);

        if (dev->domain != 0 || offset >= 256)
                return PQOS_RETVAL_PARAM;

        switch (count) {
        case 1:
                outb(*data, pos);
                break;
        case 2:
                outw(__le16_to_cpu(*(const uint16_t *)(const void *)data), pos);
                break;
        case 4:
                outl(__le32_to_cpu(*(const uint32_t *)(const void *)data), pos);
                break;
        default:
                return PQOS_RETVAL_ERROR;
        }

        return PQOS_RETVAL_OK;
}

uint8_t
pci_read_byte(struct pci_dev *dev, uint32_t offset)
{
        uint8_t value = 0;

        (void)pci_read(dev, offset, &value, 1);

        return value;
}

uint16_t
pci_read_word(struct pci_dev *dev, uint32_t offset)
{
        uint16_t value = 0;

        (void)pci_read(dev, offset, (uint8_t *)&value, 2);

        return value;
}

uint32_t
pci_read_long(struct pci_dev *dev, uint32_t offset)
{
        uint32_t value = 0;

        (void)pci_read(dev, offset, (uint8_t *)&value, 4);

        return value;
}

void
pci_write_long(struct pci_dev *dev, uint32_t offset, uint32_t value)
{
        (void)pci_write(dev, offset, (uint8_t *)&value, 4);
}

uint64_t
pci_bar_get(struct pci_dev *dev, unsigned bar)
{
        ASSERT(dev != NULL);

        if (dev->bar_num > bar)
                return dev->bar[bar];

        return 0;
}
