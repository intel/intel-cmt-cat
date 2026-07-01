// Copyright(c) 2025 Zededa, Inc.
// SPDX-License-Identifier: BSD-3-Clause

package pqos

/*
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -lpqos
#cgo CFLAGS: -I${SRCDIR}/../../../lib
#include <stdlib.h>
#include <pqos.h>

// Helper functions to access union members (CGO doesn't handle unions well)
static inline struct pqos_cap_l3ca* get_l3ca_cap(struct pqos_capability *cap) {
	return cap->u.l3ca;
}

static inline struct pqos_cap_l2ca* get_l2ca_cap(struct pqos_capability *cap) {
	return cap->u.l2ca;
}

static inline struct pqos_cap_mba* get_mba_cap(struct pqos_capability *cap) {
	return cap->u.mba;
}

static inline struct pqos_cap_mon* get_mon_cap(struct pqos_capability *cap) {
	return cap->u.mon;
}

// Helper to access flexible array member in pqos_cap_mon
static inline struct pqos_monitor* get_mon_event(struct pqos_cap_mon *mon, unsigned int idx) {
	return &mon->events[idx];
}
*/
import "C"
import (
	"fmt"
)

// Capability types
const (
	// CapTypeMon represents monitoring capability
	CapTypeMon = C.PQOS_CAP_TYPE_MON
	// CapTypeL3CA represents L3 cache allocation capability
	CapTypeL3CA = C.PQOS_CAP_TYPE_L3CA
	// CapTypeL2CA represents L2 cache allocation capability
	CapTypeL2CA = C.PQOS_CAP_TYPE_L2CA
	// CapTypeMBA represents memory bandwidth allocation capability
	CapTypeMBA = C.PQOS_CAP_TYPE_MBA
	// CapTypeSMBA represents slow memory bandwidth allocation capability
	CapTypeSMBA = C.PQOS_CAP_TYPE_SMBA
)

// MonEvent represents monitoring event types
const (
	// MonEventL3Occup monitors LLC occupancy
	MonEventL3Occup = C.PQOS_MON_EVENT_L3_OCCUP
	// MonEventLMemBW monitors local memory bandwidth
	MonEventLMemBW = C.PQOS_MON_EVENT_LMEM_BW
	// MonEventTMemBW monitors total memory bandwidth
	MonEventTMemBW = C.PQOS_MON_EVENT_TMEM_BW
	// MonEventRMemBW monitors remote memory bandwidth (virtual event)
	MonEventRMemBW = C.PQOS_MON_EVENT_RMEM_BW
	// PerfEventLLCMiss monitors LLC misses
	PerfEventLLCMiss = C.PQOS_PERF_EVENT_LLC_MISS
	// PerfEventIPC monitors instructions per clock
	PerfEventIPC = C.PQOS_PERF_EVENT_IPC
	// PerfEventLLCRef monitors LLC references
	PerfEventLLCRef = C.PQOS_PERF_EVENT_LLC_REF
)

// Capability represents PQoS capabilities
type Capability struct {
	cap *C.struct_pqos_cap
	cpu *C.struct_pqos_cpuinfo
}

// L3CACapability represents L3 Cache Allocation capability
type L3CACapability struct {
	// MemSize is the byte size of the structure
	MemSize uint
	// NumClasses is the number of classes of service
	NumClasses uint
	// NumWays is the number of cache ways
	NumWays uint
	// WaySize is the way size in bytes
	WaySize uint
	// WayContention is the ways contention bit mask
	WayContention uint64
	// CDP indicates if Code Data Prioritization is supported
	CDP bool
	// CDPOn indicates if CDP is currently enabled
	CDPOn bool
	// NonContiguousCBM indicates if non-contiguous CBM is supported
	NonContiguousCBM bool
	// IORDT indicates if I/O RDT is supported
	IORDT bool
	// IORDTOn indicates if I/O RDT is currently enabled
	IORDTOn bool
}

// L2CACapability represents L2 Cache Allocation capability
type L2CACapability struct {
	// MemSize is the byte size of the structure
	MemSize uint
	// NumClasses is the number of classes of service
	NumClasses uint
	// NumWays is the number of cache ways
	NumWays uint
	// WaySize is the way size in bytes
	WaySize uint
	// WayContention is the ways contention bit mask
	WayContention uint64
	// CDP indicates if Code Data Prioritization is supported
	CDP bool
	// CDPOn indicates if CDP is currently enabled
	CDPOn bool
	// NonContiguousCBM indicates if non-contiguous CBM is supported
	NonContiguousCBM bool
}

// MBACapability represents Memory Bandwidth Allocation capability
type MBACapability struct {
	// MemSize is the byte size of the structure
	MemSize uint
	// NumClasses is the number of classes of service
	NumClasses uint
	// ThrottleMax is the maximum MBA can be throttled
	ThrottleMax uint
	// ThrottleStep is the MBA granularity
	ThrottleStep uint
	// IsLinear indicates if MBA is linear (true) or nonlinear (false)
	IsLinear bool
	// Ctrl indicates if MBA controller is supported
	Ctrl bool
	// CtrlOn indicates if MBA controller is enabled
	CtrlOn bool
	// MBA40 indicates if MBA 4.0 extensions are supported
	MBA40 bool
	// MBA40On indicates if MBA 4.0 extensions are enabled
	MBA40On bool
}

// Monitor represents a single monitoring capability
type Monitor struct {
	// Type is the monitoring event type
	Type uint
	// MaxRMID is the maximum RMID supported for this event
	MaxRMID uint
	// ScaleFactor is the factor to scale RMID value to bytes
	ScaleFactor uint32
	// CounterLength is the counter bit length
	CounterLength uint
	// IORDT indicates if I/O RDT monitoring is supported
	IORDT bool
}

// MonCapability represents monitoring capability
type MonCapability struct {
	// MemSize is the byte size of the structure
	MemSize uint
	// MaxRMID is the maximum RMID supported by socket
	MaxRMID uint
	// L3Size is the L3 cache size in bytes
	L3Size uint
	// NumEvents is the number of supported events
	NumEvents uint
	// IORDT indicates if I/O RDT monitoring is supported
	IORDT bool
	// IORDTOn indicates if I/O RDT monitoring is enabled
	IORDTOn bool
	// SNCNum is the number of monitoring clusters
	SNCNum uint
	// SNCMode is the SNC mode
	SNCMode int
	// Events is the list of supported monitoring events
	Events []Monitor
}

// GetCapability retrieves PQoS capabilities and CPU information
func (p *PQoS) GetCapability() (*Capability, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return nil, fmt.Errorf("PQoS not initialized")
	}

	var cap *C.struct_pqos_cap
	var cpu *C.struct_pqos_cpuinfo

	ret := C.pqos_cap_get(&cap, &cpu)
	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_cap_get")
	}

	return &Capability{cap: cap, cpu: cpu}, nil
}

// GetL3CA retrieves L3 Cache Allocation capability
func (c *Capability) GetL3CA() (*L3CACapability, error) {
	if c.cap == nil {
		return nil, fmt.Errorf("capability not initialized")
	}

	var l3cap *C.struct_pqos_capability
	ret := C.pqos_cap_get_type(c.cap, C.PQOS_CAP_TYPE_L3CA, &l3cap)
	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_cap_get_type(L3CA)")
	}

	l3 := C.get_l3ca_cap(l3cap)
	if l3 == nil {
		return nil, fmt.Errorf("L3CA capability not available")
	}
	return &L3CACapability{
		MemSize:          uint(l3.mem_size),
		NumClasses:       uint(l3.num_classes),
		NumWays:          uint(l3.num_ways),
		WaySize:          uint(l3.way_size),
		WayContention:    uint64(l3.way_contention),
		CDP:              int(l3.cdp) != 0,
		CDPOn:            int(l3.cdp_on) != 0,
		NonContiguousCBM: uint(l3.non_contiguous_cbm) != 0,
		IORDT:            int(l3.iordt) != 0,
		IORDTOn:          int(l3.iordt_on) != 0,
	}, nil
}

// GetL2CA retrieves L2 Cache Allocation capability
func (c *Capability) GetL2CA() (*L2CACapability, error) {
	if c.cap == nil {
		return nil, fmt.Errorf("capability not initialized")
	}

	var l2cap *C.struct_pqos_capability
	ret := C.pqos_cap_get_type(c.cap, C.PQOS_CAP_TYPE_L2CA, &l2cap)
	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_cap_get_type(L2CA)")
	}

	l2 := C.get_l2ca_cap(l2cap)
	if l2 == nil {
		return nil, fmt.Errorf("L2CA capability not available")
	}
	return &L2CACapability{
		MemSize:          uint(l2.mem_size),
		NumClasses:       uint(l2.num_classes),
		NumWays:          uint(l2.num_ways),
		WaySize:          uint(l2.way_size),
		WayContention:    uint64(l2.way_contention),
		CDP:              int(l2.cdp) != 0,
		CDPOn:            int(l2.cdp_on) != 0,
		NonContiguousCBM: uint(l2.non_contiguous_cbm) != 0,
	}, nil
}

// GetMBA retrieves Memory Bandwidth Allocation capability
func (c *Capability) GetMBA() (*MBACapability, error) {
	if c.cap == nil {
		return nil, fmt.Errorf("capability not initialized")
	}

	var mbacap *C.struct_pqos_capability
	ret := C.pqos_cap_get_type(c.cap, C.PQOS_CAP_TYPE_MBA, &mbacap)
	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_cap_get_type(MBA)")
	}

	mba := C.get_mba_cap(mbacap)
	if mba == nil {
		return nil, fmt.Errorf("MBA capability not available")
	}
	return &MBACapability{
		MemSize:      uint(mba.mem_size),
		NumClasses:   uint(mba.num_classes),
		ThrottleMax:  uint(mba.throttle_max),
		ThrottleStep: uint(mba.throttle_step),
		IsLinear:     int(mba.is_linear) != 0,
		Ctrl:         int(mba.ctrl) != 0,
		CtrlOn:       int(mba.ctrl_on) != 0,
		MBA40:        int(mba.mba40) != 0,
		MBA40On:      int(mba.mba40_on) != 0,
	}, nil
}

// GetMon retrieves monitoring capability
func (c *Capability) GetMon() (*MonCapability, error) {
	if c.cap == nil {
		return nil, fmt.Errorf("capability not initialized")
	}

	var moncap *C.struct_pqos_capability
	ret := C.pqos_cap_get_type(c.cap, C.PQOS_CAP_TYPE_MON, &moncap)
	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_cap_get_type(MON)")
	}

	mon := C.get_mon_cap(moncap)
	if mon == nil {
		return nil, fmt.Errorf("MON capability not available")
	}
	capability := &MonCapability{
		MemSize:   uint(mon.mem_size),
		MaxRMID:   uint(mon.max_rmid),
		L3Size:    uint(mon.l3_size),
		NumEvents: uint(mon.num_events),
		IORDT:     int(mon.iordt) != 0,
		IORDTOn:   int(mon.iordt_on) != 0,
		SNCNum:    uint(mon.snc_num),
		SNCMode:   int(mon.snc_mode),
		Events:    make([]Monitor, 0, mon.num_events),
	}

	// Convert event array to Go slice
	if mon.num_events > 0 {
		for i := C.uint(0); i < mon.num_events; i++ {
			event := C.get_mon_event(mon, i)
			capability.Events = append(capability.Events, Monitor{
				Type:          uint(event._type),
				MaxRMID:       uint(event.max_rmid),
				ScaleFactor:   uint32(event.scale_factor),
				CounterLength: uint(event.counter_length),
				IORDT:         int(event.iordt) != 0,
			})
		}
	}

	return capability, nil
}

// HasL3CA checks if L3 Cache Allocation is supported
func (c *Capability) HasL3CA() bool {
	if c.cap == nil {
		return false
	}
	var l3cap *C.struct_pqos_capability
	ret := C.pqos_cap_get_type(c.cap, C.PQOS_CAP_TYPE_L3CA, &l3cap)
	return ret == C.PQOS_RETVAL_OK && l3cap != nil
}

// HasL2CA checks if L2 Cache Allocation is supported
func (c *Capability) HasL2CA() bool {
	if c.cap == nil {
		return false
	}
	var l2cap *C.struct_pqos_capability
	ret := C.pqos_cap_get_type(c.cap, C.PQOS_CAP_TYPE_L2CA, &l2cap)
	return ret == C.PQOS_RETVAL_OK && l2cap != nil
}

// HasMBA checks if Memory Bandwidth Allocation is supported
func (c *Capability) HasMBA() bool {
	if c.cap == nil {
		return false
	}
	var mbacap *C.struct_pqos_capability
	ret := C.pqos_cap_get_type(c.cap, C.PQOS_CAP_TYPE_MBA, &mbacap)
	return ret == C.PQOS_RETVAL_OK && mbacap != nil
}

// HasMon checks if monitoring is supported
func (c *Capability) HasMon() bool {
	if c.cap == nil {
		return false
	}
	var moncap *C.struct_pqos_capability
	ret := C.pqos_cap_get_type(c.cap, C.PQOS_CAP_TYPE_MON, &moncap)
	return ret == C.PQOS_RETVAL_OK && moncap != nil
}

// CapTypeToString converts a capability type to a string
func CapTypeToString(capType int) string {
	switch capType {
	case CapTypeMon:
		return "MON"
	case CapTypeL3CA:
		return "L3CA"
	case CapTypeL2CA:
		return "L2CA"
	case CapTypeMBA:
		return "MBA"
	case CapTypeSMBA:
		return "SMBA"
	default:
		return fmt.Sprintf("UNKNOWN(%d)", capType)
	}
}

// MonEventToString converts a monitoring event to a string
func MonEventToString(event uint) string {
	switch event {
	case MonEventL3Occup:
		return "L3_OCCUP"
	case MonEventLMemBW:
		return "LMEM_BW"
	case MonEventTMemBW:
		return "TMEM_BW"
	case MonEventRMemBW:
		return "RMEM_BW"
	case PerfEventLLCMiss:
		return "LLC_MISS"
	case PerfEventIPC:
		return "IPC"
	case PerfEventLLCRef:
		return "LLC_REF"
	default:
		return fmt.Sprintf("UNKNOWN(0x%x)", event)
	}
}
