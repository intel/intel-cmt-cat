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

#ifndef __PQOS_ACPI_TABLE_H__
#define __PQOS_ACPI_TABLE_H__

#include <stdint.h>

#define ACPI_TABLE_RSDP_SIZE 20

#ifndef ACPI_TABLE_SIG_IRDT
#define ACPI_TABLE_SIG_IRDT "IRDT"
#endif

/**
 * Root System Description Pointer (RSDP)
 */
struct __attribute__((__packed__)) acpi_table_rsdp {
        char signature[8];         /**< Table signature, contains "RSD PTR " */
        uint8_t checksum;          /**< ACPI 1.0 table checksum */
        char oem_id[6];            /**< An OEM-supplied string that identifies
                                        the OEM */
        uint8_t revision;          /**< The revision of this structure */
        uint32_t rsdt_address;     /**< 32 bit physical address of the RSDT */
        uint32_t length;           /**< Length of table in bytes */
        uint64_t xsdt_address;     /**< 64 bit physical address of the XSDT */
        uint8_t extended_checksum; /**< Entire table checksum */
        char reserved[3];          /**< Reserved */
};

/**
 * ACPI table header
 */
struct __attribute__((__packed__)) acpi_table_header {
        char signature[4];         /**< Table signature */
        uint32_t length;           /**< Length of table in bytes */
        uint8_t revision;          /**< Table specification version */
        uint8_t checksum;          /**< Table checksum */
        char oem_id[6];            /**< OEM identification */
        char oem_table_id[8];      /**< OEM table identification */
        uint32_t oem_revision;     /**< OEM revision number */
        uint32_t creator_id;       /**< Vendor ID of utility that created
                                        the table */
        uint32_t creator_revision; /**< Revision of utility that created
                                        the table */
};

/**
 * Root System Description Table (RSDT)
 */
struct __attribute__((__packed__)) acpi_table_rsdt {
        struct acpi_table_header header;
        uint32_t entry[0]; /**< An array of 32-bit physical addresses */
};

/**
 * Extended System Description Table (XSDT)
 */
struct __attribute__((__packed__)) acpi_table_xsdt {
        struct acpi_table_header header;
        uint64_t entry[0]; /**< An array of 64-bit physical addresses */
};

#define ACPI_TABLE_IRDT_CHMS_CHAN_SHARED (0x1 << 6)
#define ACPI_TABLE_IRDT_CHMS_CHAN_VALID  (0x1 << 7)

#define ACPI_TABLE_IRDT_CHMS_CHAN_MASK                                         \
        (ACPI_TABLE_IRDT_CHMS_CHAN_SHARED | ACPI_TABLE_IRDT_CHMS_CHAN_VALID)
/**
 * I/O RDT Channel Management Structure (CHMS) Table
 */
struct __attribute__((__packed__)) acpi_table_irdt_chms {
        /**
         * RCS Enumeration ID controlling this link.
         *
         * Corresponds to the enumeration ID of the RCS structure under this DSS
         */
        uint8_t rcs_enum_id;

        /**
         * Channels' map for VCx
         *
         * Represents the index into the "RCS-CFG-Table" used by the
         * corresponding VC.
         * Byte 1 represents the channel for VC0,
         * Byte 2 represents the channel for VC1, etc.
         * In this field, bit 7 is a valid bit (entry is not valid if enable bit
         * is cleared). Bit 6 indicates that this channel is shared with
         * another DSS. The number of valid bytes in this field is define
         * in the per-RCS "Channel Count" field, any unused bytes
         * (e.g., for a single-Channel CXL link) are Reserved.
         */
        uint8_t vc_map[8];

        uint8_t reserved[7];
};

#define ACPI_TABLE_IRDT_TYPE_DSS 0
#define ACPI_TABLE_IRDT_TYPE_RCS 1

#define RCS_FLAGS_AQ   1
#define RCS_FLAGS_RTS  (1 << 1)
#define RCS_FLAGS_CTS  (1 << 2)
#define RCS_FLAGS_REGW (1 << 3)
#define RCS_FLAGS_REF  (1 << 4)
#define RCS_FLAGS_CEF  (1 << 5)

/**
 * I/O RDT Device Scope Structure (DSS), Resource Control Structure (RCS) Table
 */
struct __attribute__((__packed__)) acpi_table_irdt_device {
        uint16_t type;   /**< Type for the table DSS or RCS */
        uint16_t length; /**< Length of table in bytes */
        union {
                struct __attribute__((__packed__)) {
                        /**
                         * Device Type.
                         *
                         * The following values are defined for this field.
                         * 0x01: PCI Endpoint Device - The device identified by
                         *       the ‘Path’ field is a PCI endpoint device.
                         *       This type must not be used in Device Scope of
                         *       DRHD structures with INCLUDE_PCI_ALL flag Set.
                         * 0x02: PCI Sub-hierarchy - The device identified by
                         *       the ‘Path’ field is a PCI-PCI bridge.
                         *       In this case, the specified bridge device and
                         *       all its downstream devices are included in the
                         *       scope. This type must not be in Device Scope of
                         *       DRHD structures with INCLUDE_PCI_ALL flag Set.
                         *
                         * Other values for this field are reserved for
                         * future use.
                         */
                        uint8_t device_type;

                        /**
                         * Enumeration ID
                         *
                         * If Type 1 or 2, this is the BDF.
                         */
                        uint16_t enumeration_id;

                        uint8_t reserved[1];

                        /**
                         * CHMS and RCS Enumeration
                         *
                         * One RCS may support multiple DSSes, and one DSS may
                         * have multiple RCSs (links), so this is an array,
                         * with size derivable from the DSS Length field.
                         */
                        struct acpi_table_irdt_chms chms_rcs_enumeration[0];
                } dss;
                struct __attribute__((__packed__)) {
                        /**
                         * Type of link controlled
                         * 0x0 = PCIe
                         * 0x1 = CXL.$
                         * 0x2 and above: Reserved
                         */
                        uint16_t link_interface_type;

                        /* A unique identifier for this RCS under this RMUD */
                        uint8_t rcs_enumeration_id;

                        /**
                         * Number of Channels defined for this link
                         * (affects the interpretation of the CHMS structure
                         * within the corresponding DSS).
                         */
                        uint8_t channel_count;

                        /**
                         * Bit 0: Reserved
                         *
                         * RCS Interface Parameter Flags:
                         * Bit 1: RTS: RMID Tagging supported
                         * Bit 2: CTS: CLOS Tagging supported
                         * Bit 3: REGW: if set, the registers defined the
                         *      RCS MMIO location should be accessed as 2B
                         *      fields, or accessed as 4B fields if clear,
                         *      subject to enumerated valid RMID and CLOS width
                         *      (from the RMUD structure)
                         *
                         * MMIO Interface Parameter Flags:
                         * Bits 15-4: Reserved
                         */
                        uint16_t flags;

                        /**
                         * Byte offset from RCS MMIO Location where the RMID
                         * tagging fields begin */
                        uint16_t rmid_block_offset;

                        /**
                         * Byte offset from RCS MMIO Location where the CLOS
                         * tagging fields begin */
                        uint16_t clos_block_offset;

                        uint8_t reserved[18];

                        /**
                         * RCS Hosting I/O Block MMIO BAR Location defines
                         * an MMIO physical address */
                        uint64_t rcs_block_mmio_location;
                } rcs;
        };
};

#define ACPI_TABLE_IRDT_TYPE_RMUD 0
/**
 * I/O RDT Resource Management Unit Descriptor (RMUD) Table
 */
struct __attribute__((__packed__)) acpi_table_irdt_rmud {

        /**
         * Type 0 = "RMUD".
         * Signature for the I/O RDT Resource Management Unit Descriptor.
         */
        uint8_t type;

        uint8_t reserved[3];

        uint32_t length; /**< Length of table in bytes */

        /**
         * The PCI Segment containing this RMUD,
         * and all of the devices that are within it.
         */
        uint16_t segment;

        uint8_t reserved_2[3];

        /**
         * List of devices behind this RMUD, with one IORTS-DSS table instance
         * per device.
         *
         * Contains a list of DSS control structures and RCS control structures,
         * identified by their Type field at offset zero in the sub-structures.
         */
        struct acpi_table_irdt_device device[0];
};

#define ACPI_TABLE_IRDT_PROTO_FLAGS_MON    (0x1 << 0)
#define ACPI_TABLE_IRDT_PROTO_FLAGS_CTL    (0x1 << 1)
#define ACPI_TABLE_IRDT_PROTO_FLAGS_BW_CTL (0x1 << 2)
/**
 * I/O RDT Top Level Description Table
 * Top-level descriptor table indicating the presence of I/O RDT on the platform
 */
struct __attribute__((__packed__)) acpi_table_irdt {
        struct acpi_table_header header;

        /**
         * IO Protocol Flags
         *
         * Bit 0: IO_PROTO_MON -- Set if supported somewhere on the platform
         * Bit 1: IO_PROTO_CTL -- Set if supported somewhere on the platform
         * Bit 2: IO_PROTO_BW_CTL -- Set if supported somewhere on the platform
         */
        uint16_t io_protocol_flags;

        /**
         * Cache Protocol Flags
         *
         * Bit 0: IO_COH_MON -- Set if supported somewhere on the platform
         * Bit 1: IO_COH_CTL -- Set if supported somewhere on the platform
         * Bit 2: IO_COH_BW_CTL -- Set if supported somewhere on the platform
         */
        uint16_t cache_protocol_flags;

        char reserved[8];

        /**
         * Resource Management Hardware Blocks list
         *
         * A list of structures. The list will contain one or more
         * Resource Management Unit Descriptors (RMUDs),
         * I/O Domain Descriptors (IODDs), etc.
         */
        struct acpi_table_irdt_rmud rmud[0];
};

#endif /* __PQOS_ACPI_TABLE_H__ */
