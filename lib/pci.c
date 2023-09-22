/*
 * BSD LICENSE
 *
 * Copyright(c) 2021 Intel Corporation. All rights reserved.
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

#include "common.h"
#include "log.h"
#include "pqos.h"

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
        char path[256];
        FILE *fd;
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
