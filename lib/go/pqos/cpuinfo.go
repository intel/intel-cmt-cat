// Copyright(c) 2025 Zededa, Inc.
// SPDX-License-Identifier: BSD-3-Clause

package pqos

/*
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -lpqos
#cgo CFLAGS: -I${SRCDIR}/../../../lib
#include <stdlib.h>
#include <pqos.h>

// Helper to access flexible array member in pqos_cpuinfo
static inline struct pqos_coreinfo* get_core_info(struct pqos_cpuinfo *cpu, unsigned int idx) {
	return &cpu->cores[idx];
}
*/
import "C"
import (
	"fmt"
)

// Vendor represents CPU vendor
const (
	VendorUnknown = C.PQOS_VENDOR_UNKNOWN
	VendorIntel   = C.PQOS_VENDOR_INTEL
	VendorAMD     = C.PQOS_VENDOR_AMD
)

// CoreInfo represents information about a single CPU core
type CoreInfo struct {
	// LCore is the logical core ID
	LCore uint
	// Socket is the socket ID in the system
	Socket uint
	// L3ID is the L3/LLC cluster ID
	L3ID uint
	// L2ID is the L2 cluster ID
	L2ID uint
	// L3CATID is the L3 CAT classes ID
	L3CATID uint
	// MBAID is the MBA ID
	MBAID uint
	// NUMA is the NUMA node in the system
	NUMA uint
	// SMBAID is the SMBA ID
	SMBAID uint
}

// CacheInfo represents cache information
type CacheInfo struct {
	// Detected indicates if cache was detected and is valid
	Detected bool
	// NumWays is the number of cache ways
	NumWays uint
	// NumSets is the number of sets
	NumSets uint
	// NumPartitions is the number of partitions
	NumPartitions uint
	// LineSize is the cache line size in bytes
	LineSize uint
	// TotalSize is the total cache size in bytes
	TotalSize uint
	// WaySize is the cache way size in bytes
	WaySize uint
}

// CPUInfo represents CPU topology information
type CPUInfo struct {
	// MemSize is the byte size of the structure
	MemSize uint
	// L2 contains L2 cache information
	L2 CacheInfo
	// L3 contains L3 cache information
	L3 CacheInfo
	// Vendor is the CPU vendor (Intel/AMD)
	Vendor int
	// NumCores is the number of cores in the system
	NumCores uint
	// Cores is the list of core information
	Cores []CoreInfo
}

// GetCPUInfo retrieves CPU topology information from the capability
func (c *Capability) GetCPUInfo() (*CPUInfo, error) {
	if c.cpu == nil {
		return nil, fmt.Errorf("CPU info not available")
	}

	cpuInfo := &CPUInfo{
		MemSize:  uint(c.cpu.mem_size),
		Vendor:   int(c.cpu.vendor),
		NumCores: uint(c.cpu.num_cores),
		L2: CacheInfo{
			Detected:      int(c.cpu.l2.detected) != 0,
			NumWays:       uint(c.cpu.l2.num_ways),
			NumSets:       uint(c.cpu.l2.num_sets),
			NumPartitions: uint(c.cpu.l2.num_partitions),
			LineSize:      uint(c.cpu.l2.line_size),
			TotalSize:     uint(c.cpu.l2.total_size),
			WaySize:       uint(c.cpu.l2.way_size),
		},
		L3: CacheInfo{
			Detected:      int(c.cpu.l3.detected) != 0,
			NumWays:       uint(c.cpu.l3.num_ways),
			NumSets:       uint(c.cpu.l3.num_sets),
			NumPartitions: uint(c.cpu.l3.num_partitions),
			LineSize:      uint(c.cpu.l3.line_size),
			TotalSize:     uint(c.cpu.l3.total_size),
			WaySize:       uint(c.cpu.l3.way_size),
		},
		Cores: make([]CoreInfo, 0, c.cpu.num_cores),
	}

	// Convert cores array to Go slice
	if c.cpu.num_cores > 0 {
		for i := C.uint(0); i < c.cpu.num_cores; i++ {
			core := C.get_core_info(c.cpu, i)
			cpuInfo.Cores = append(cpuInfo.Cores, CoreInfo{
				LCore:   uint(core.lcore),
				Socket:  uint(core.socket),
				L3ID:    uint(core.l3_id),
				L2ID:    uint(core.l2_id),
				L3CATID: uint(core.l3cat_id),
				MBAID:   uint(core.mba_id),
				NUMA:    uint(core.numa),
				SMBAID:  uint(core.smba_id),
			})
		}
	}

	return cpuInfo, nil
}

// GetNumCores returns the number of CPU cores
func (c *CPUInfo) GetNumCores() uint {
	return c.NumCores
}

// GetNumSockets returns the number of CPU sockets
func (c *CPUInfo) GetNumSockets() uint {
	sockets := make(map[uint]bool)
	for _, core := range c.Cores {
		sockets[core.Socket] = true
	}
	return uint(len(sockets))
}

// GetCoresBySocket returns all cores for a given socket
func (c *CPUInfo) GetCoresBySocket(socket uint) []CoreInfo {
	var cores []CoreInfo
	for _, core := range c.Cores {
		if core.Socket == socket {
			cores = append(cores, core)
		}
	}
	return cores
}

// GetCoresByL3ID returns all cores for a given L3 cluster
func (c *CPUInfo) GetCoresByL3ID(l3ID uint) []CoreInfo {
	var cores []CoreInfo
	for _, core := range c.Cores {
		if core.L3ID == l3ID {
			cores = append(cores, core)
		}
	}
	return cores
}

// GetCoresByL2ID returns all cores for a given L2 cluster
func (c *CPUInfo) GetCoresByL2ID(l2ID uint) []CoreInfo {
	var cores []CoreInfo
	for _, core := range c.Cores {
		if core.L2ID == l2ID {
			cores = append(cores, core)
		}
	}
	return cores
}

// GetCoresByNUMA returns all cores for a given NUMA node
func (c *CPUInfo) GetCoresByNUMA(numa uint) []CoreInfo {
	var cores []CoreInfo
	for _, core := range c.Cores {
		if core.NUMA == numa {
			cores = append(cores, core)
		}
	}
	return cores
}

// GetSocketIDs returns a list of all socket IDs
func (c *CPUInfo) GetSocketIDs() []uint {
	sockets := make(map[uint]bool)
	for _, core := range c.Cores {
		sockets[core.Socket] = true
	}

	ids := make([]uint, 0, len(sockets))
	for socket := range sockets {
		ids = append(ids, socket)
	}
	return ids
}

// GetL3IDs returns a list of all L3 cluster IDs
func (c *CPUInfo) GetL3IDs() []uint {
	l3IDs := make(map[uint]bool)
	for _, core := range c.Cores {
		l3IDs[core.L3ID] = true
	}

	ids := make([]uint, 0, len(l3IDs))
	for l3ID := range l3IDs {
		ids = append(ids, l3ID)
	}
	return ids
}

// GetL2IDs returns a list of all L2 cluster IDs
func (c *CPUInfo) GetL2IDs() []uint {
	l2IDs := make(map[uint]bool)
	for _, core := range c.Cores {
		l2IDs[core.L2ID] = true
	}

	ids := make([]uint, 0, len(l2IDs))
	for l2ID := range l2IDs {
		ids = append(ids, l2ID)
	}
	return ids
}

// GetL3CATIDs returns a list of all L3 CAT cluster IDs
func (c *CPUInfo) GetL3CATIDs() []uint {
	l3CATIDs := make(map[uint]bool)
	for _, core := range c.Cores {
		l3CATIDs[core.L3CATID] = true
	}

	ids := make([]uint, 0, len(l3CATIDs))
	for l3CATID := range l3CATIDs {
		ids = append(ids, l3CATID)
	}
	return ids
}

// GetMBAIDs returns a list of all MBA IDs
func (c *CPUInfo) GetMBAIDs() []uint {
	mbaIDs := make(map[uint]bool)
	for _, core := range c.Cores {
		mbaIDs[core.MBAID] = true
	}

	ids := make([]uint, 0, len(mbaIDs))
	for mbaID := range mbaIDs {
		ids = append(ids, mbaID)
	}
	return ids
}

// GetNumL2Domains returns the number of L2 cache domains
func (c *CPUInfo) GetNumL2Domains() uint {
	return uint(len(c.GetL2IDs()))
}

// VendorToString converts a vendor constant to a string
func VendorToString(vendor int) string {
	switch vendor {
	case VendorIntel:
		return "Intel"
	case VendorAMD:
		return "AMD"
	case VendorUnknown:
		return "Unknown"
	default:
		return fmt.Sprintf("Unknown(%d)", vendor)
	}
}

// FindCore finds a core by its logical core ID
func (c *CPUInfo) FindCore(lcore uint) *CoreInfo {
	for i := range c.Cores {
		if c.Cores[i].LCore == lcore {
			return &c.Cores[i]
		}
	}
	return nil
}

// GetL3CacheSize returns the total L3 cache size in bytes
func (c *CPUInfo) GetL3CacheSize() uint {
	if c.L3.Detected {
		return c.L3.TotalSize
	}
	return 0
}

// GetL2CacheSize returns the total L2 cache size in bytes
func (c *CPUInfo) GetL2CacheSize() uint {
	if c.L2.Detected {
		return c.L2.TotalSize
	}
	return 0
}
