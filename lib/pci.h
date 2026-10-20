/*
 * BSD LICENSE
 *
 * Copyright(c) 2023-2026 Intel Corporation. All rights reserved.
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

#ifndef PCI_H
#define PCI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pqos.h"
#include "types.h"

#define PCI_NUMA_INVALID ((unsigned)-1)

/**
 * PCI device structure
 */
struct pci_dev {
        uint16_t domain; /**< PCI domain */
        uint16_t bdf;    /**< Bus Device Function */
        uint16_t bus;    /**< PCI bus */
        uint16_t dev;    /**< device */
        uint16_t func;   /**< function */

        unsigned bar_num; /**< number of BAR addresses */
        uint64_t bar[6];  /**< BAR addresses */

        unsigned numa; /**< numa node */
};

/**
 * @brief Initialize PCI module
 *
 * @return Operational status
 * @retval PQOS_RETVAL_OK success
 */
int pci_init(void);

/**
 * @brief Shuts down PCI module
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pci_fini(void);

/**
 * Where distributions keep the PCI ID database, pci.ids, in the order the
 * library looks for it: /usr/share/misc on Debian and the distributions built
 * on it, /usr/share/hwdata (the hwdata package) on Fedora and RHEL, and
 * /usr/share, pciutils' own default
 */
extern PQOS_LOCAL const char *const pci_ids_files[3];

/**
 * @brief Opens the first of a list of files that is a regular file
 *
 * A name is passed over for the next if it does not exist, if it is not a
 * regular file - a directory, a FIFO or a symbolic link, which pqos_fopen()
 * refuses - or if the file does not open.
 *
 * @param [in] files the names, in the order they are tried
 * @param [in] num_files how many there are
 * @param [out] unusable if not NULL, the first name passed over although it
 *              exists, or NULL if there was none
 *
 * @return the stream of the first that opens, NULL if none does
 */
PQOS_LOCAL FILE *pci_ids_open(const char *const *files,
                              unsigned num_files,
                              const char **unusable);

/**
 * @brief Opens pci.ids from a list of places, saying why if it cannot
 *
 * The file is opened by pci_ids_open(). If none of the places has it, the
 * error names the places. If one has something that cannot be read, such as a
 * symbolic link, the error names that instead.
 *
 * @param [in] files the places, in the order they are tried
 * @param [in] num_files how many there are
 *
 * @return the stream, NULL if no place gives one
 */
PQOS_LOCAL FILE *pci_ids_find(const char *const *files, unsigned num_files);

/**
 * @brief Reads a device's vendor, device and subclass names from pci.ids
 *
 * A name the database does not have is left as it was.
 *
 * @param [in] fd the database, read to its end
 * @param [out] info the device's information, whose names are filled in
 * @param [in] class_code the device's class code: class, subclass and
 *             programming interface, a byte each
 * @param [in] vendor_id the device's vendor ID
 * @param [in] device_id the device's device ID
 */
PQOS_LOCAL void pci_ids_read(FILE *fd,
                             struct pqos_pci_info *info,
                             unsigned int class_code,
                             unsigned int vendor_id,
                             unsigned int device_id);

/**
 * @brief Fills in a device's vendor, device and subclass names from pci.ids
 *
 * The file is looked for in pci_ids_files by pci_ids_find(). A name it does
 * not have is left as it was.
 *
 * @param [out] info the device's information, whose names are filled in
 * @param [in] class_code the device's class code: class, subclass and
 *             programming interface, a byte each
 * @param [in] vendor_id the device's vendor ID
 * @param [in] device_id the device's device ID
 */
PQOS_LOCAL void pci_parse_pci_ids(struct pqos_pci_info *info,
                                  unsigned int class_code,
                                  unsigned int vendor_id,
                                  unsigned int device_id);

/**
 * @brief Initialize PCI device
 *
 * @param [in] domain PCI domain
 * @param [in] bdf Bus Device Function
 *
 * @return PCI device structure
 */
struct pci_dev *pci_dev_get(uint16_t domain, uint16_t bdf);

/**
 * @brief Get I/O devices' PCI information
 *
 * @param [out] pci_info pqos_pci_info structure instance
 * @param [in] segment PCI segment
 * @param [in] bdf Bus Device Function
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE if the PCI information of the device cannot be
 *         read, e.g. when no such device is present
 */
PQOS_LOCAL int
io_devs_get(struct pqos_pci_info *pci_info, uint16_t segment, uint16_t bdf);

/**
 * @brief Get I/O devices' PCI information by msr interface
 *
 * @param [out] pci_info pqos_pci_info structure instance
 * @param [in] segment PCI segment
 * @param [in] bdf Bus Device Function
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE if the PCI information of the device cannot be
 *         read, e.g. when no such device is present
 */
PQOS_LOCAL int
hw_io_devs_get(struct pqos_pci_info *pci_info, uint16_t segment, uint16_t bdf);

/**
 * @brief Get I/O devices' PCI information by mmio interface
 *
 * @param [out] pci_info pqos_pci_info structure instance
 * @param [in] segment PCI segment
 * @param [in] bdf Bus Device Function
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 * @retval PQOS_RETVAL_RESOURCE if the PCI information of the device cannot be
 *         read, e.g. when no such device is present
 */
PQOS_LOCAL int mmio_io_devs_get(struct pqos_pci_info *pci_info,
                                uint16_t segment,
                                uint16_t bdf);

/**
 * @brief Deallocate memory used for PCI device
 *
 * @param [in] dev PCI device allocated by \a pci_dev_get
 */
void pci_dev_release(struct pci_dev *dev);

/**
 * @brief Read PCI device memory
 *
 * @param [in] dev PCI device allocated by \a pci_dev_get
 * @param [in] offset memory offset
 * @param [out] data read buffer
 * @param [in] count read data length
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int
pci_read(struct pci_dev *dev, uint32_t offset, uint8_t *data, uint32_t count);

/**
 * @brief Write to PCI device memory
 *
 * @param [in] dev PCI device allocated by \a pci_dev_get
 * @param [in] offset memory offset
 * @param [in] data write buffer
 * @param [in] count write data length
 *
 * @return Operation status
 * @retval PQOS_RETVAL_OK on success
 */
int pci_write(struct pci_dev *dev,
              uint32_t offset,
              const uint8_t *data,
              uint32_t count);

/**
 * @brief Read byte from PCI memory
 *
 * @param [in] dev PCI device allocated by \a pci_dev_get
 * @param [in] offset memory offset
 *
 * @return Obtained value
 */
uint8_t pci_read_byte(struct pci_dev *dev, uint32_t offset);

/**
 * @brief Read word from PCI memory
 *
 * @param [in] dev PCI device allocated by \a pci_dev_get
 * @param [in] offset memory offset
 *
 * @return Obtained value
 */
uint16_t pci_read_word(struct pci_dev *dev, uint32_t offset);

/**
 * @brief Read long from PCI memory
 *
 * @param [in] dev PCI device allocated by \a pci_dev_get
 * @param [in] offset memory offset
 *
 * @return Obtained value
 */
uint32_t pci_read_long(struct pci_dev *dev, uint32_t offset);

/**
 * @brief Write long to PCI memory
 *
 * @param [in] dev PCI device allocated by \a pci_dev_get
 * @param [in] offset memory offset
 * @param [in] value New value
 *
 * @return Obtained value
 */
void pci_write_long(struct pci_dev *dev, uint32_t offset, uint32_t value);

/**
 * @brief Obtain PCI device bar address
 *
 * @param [in] dev PCI device allocated by \a pci_dev_get
 * @param [in] bar Address index
 *
 * @return BAR address
 */
uint64_t pci_bar_get(struct pci_dev *dev, unsigned bar);

#ifdef __cplusplus
}
#endif

#endif /* PCI_H */
