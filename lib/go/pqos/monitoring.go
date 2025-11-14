// Copyright(c) 2025 Zededa, Inc.
// SPDX-License-Identifier: BSD-3-Clause

package pqos

/*
#cgo LDFLAGS: -L${SRCDIR}/../../../lib -lpqos
#cgo CFLAGS: -I${SRCDIR}/../../../lib
#include <stdlib.h>
#include <sys/types.h>
#include <pqos.h>

// Helper to get number of cores from mon_data
static inline unsigned get_mon_num_cores(struct pqos_mon_data *group) {
	return group->num_cores;
}

// Helper to get core from mon_data cores array
static inline unsigned get_mon_core(struct pqos_mon_data *group, unsigned idx) {
	return group->cores[idx];
}

// Helper to get number of PIDs from mon_data
static inline unsigned get_mon_num_pids(struct pqos_mon_data *group) {
	return group->num_pids;
}

// Helper to get PID from mon_data pids array
static inline pid_t get_mon_pid(struct pqos_mon_data *group, unsigned idx) {
	return group->pids[idx];
}
*/
import "C"
import (
	"fmt"
)

// EventValues represents monitoring event values
type EventValues struct {
	// LLC is the L3 cache occupancy in bytes
	LLC uint64
	// MBMLocal is the local memory bandwidth reading in bytes
	MBMLocal uint64
	// MBMTotal is the total memory bandwidth reading in bytes
	MBMTotal uint64
	// MBMRemote is the remote memory bandwidth reading in bytes
	MBMRemote uint64
	// MBMLocalDelta is the local memory bandwidth delta in bytes
	MBMLocalDelta uint64
	// MBMTotalDelta is the total memory bandwidth delta in bytes
	MBMTotalDelta uint64
	// MBMRemoteDelta is the remote memory bandwidth delta in bytes
	MBMRemoteDelta uint64
	// IPCRetired is the instructions retired reading
	IPCRetired uint64
	// IPCRetiredDelta is the instructions retired delta
	IPCRetiredDelta uint64
	// IPCUnhalted is the unhalted cycles reading
	IPCUnhalted uint64
	// IPCUnhaltedDelta is the unhalted cycles delta
	IPCUnhaltedDelta uint64
	// LLCMissTotal is the LLC miss counter
	LLCMissTotal uint64
	// LLCMissDelta is the LLC miss delta
	LLCMissDelta uint64
	// LLCRefTotal is the LLC reference counter
	LLCRefTotal uint64
	// LLCRefDelta is the LLC reference delta
	LLCRefDelta uint64
}

// MonData represents a monitoring group
type MonData struct {
	// Valid indicates if the structure is valid
	Valid bool
	// Event is the combination of monitored events
	Event uint
	// Values contains the monitoring event values
	Values EventValues
	// Cores is the list of monitored core IDs (core monitoring)
	Cores []uint
	// PIDs is the list of monitored process IDs (process monitoring)
	PIDs []int

	// Internal C pointer (not exposed to users)
	group *C.struct_pqos_mon_data
}

// MonStartCores starts resource monitoring on selected cores
//
// Parameters:
//   - cores: array of logical core IDs to monitor
//   - event: combination of monitoring events (MonEventL3Occup, MonEventTMemBW, etc.)
//
// Returns MonData structure and error if operation fails
func (p *PQoS) MonStartCores(cores []uint, event uint) (*MonData, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return nil, ErrNotInitialized
	}

	if len(cores) == 0 {
		return nil, fmt.Errorf("empty cores array")
	}

	// Convert Go slice to C array
	cCores := make([]C.uint, len(cores))
	for i, core := range cores {
		cCores[i] = C.uint(core)
	}

	var group *C.struct_pqos_mon_data
	ret := C.pqos_mon_start_cores(
		C.uint(len(cores)),
		&cCores[0],
		C.enum_pqos_mon_event(event),
		nil, // context
		&group,
	)

	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_mon_start_cores")
	}

	return cMonDataToGo(group), nil
}

// MonStartPIDs starts resource monitoring on selected processes
//
// Parameters:
//   - pids: array of process IDs to monitor
//   - event: combination of monitoring events
//
// Returns MonData structure and error if operation fails
func (p *PQoS) MonStartPIDs(pids []int, event uint) (*MonData, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return nil, ErrNotInitialized
	}

	if len(pids) == 0 {
		return nil, fmt.Errorf("empty pids array")
	}

	// Convert Go slice to C array
	cPIDs := make([]C.pid_t, len(pids))
	for i, pid := range pids {
		cPIDs[i] = C.pid_t(pid)
	}

	var group *C.struct_pqos_mon_data
	ret := C.pqos_mon_start_pids2(
		C.uint(len(pids)),
		&cPIDs[0],
		C.enum_pqos_mon_event(event),
		nil, // context
		&group,
	)

	if ret != C.PQOS_RETVAL_OK {
		return nil, retvalToError(ret, "pqos_mon_start_pids2")
	}

	return cMonDataToGo(group), nil
}

// MonAddPIDs adds PIDs to an existing monitoring group
//
// Parameters:
//   - group: monitoring group to add PIDs to
//   - pids: array of process IDs to add
//
// Returns error if operation fails
func (p *PQoS) MonAddPIDs(group *MonData, pids []int) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	if group == nil || group.group == nil {
		return fmt.Errorf("invalid monitoring group")
	}

	if len(pids) == 0 {
		return fmt.Errorf("empty pids array")
	}

	// Convert Go slice to C array
	cPIDs := make([]C.pid_t, len(pids))
	for i, pid := range pids {
		cPIDs[i] = C.pid_t(pid)
	}

	ret := C.pqos_mon_add_pids(C.uint(len(pids)), &cPIDs[0], group.group)
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_mon_add_pids")
	}

	// Update Go structure
	updateMonDataFromC(group)

	return nil
}

// MonRemovePIDs removes PIDs from an existing monitoring group
//
// Parameters:
//   - group: monitoring group to remove PIDs from
//   - pids: array of process IDs to remove
//
// Returns error if operation fails
func (p *PQoS) MonRemovePIDs(group *MonData, pids []int) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	if group == nil || group.group == nil {
		return fmt.Errorf("invalid monitoring group")
	}

	if len(pids) == 0 {
		return fmt.Errorf("empty pids array")
	}

	// Convert Go slice to C array
	cPIDs := make([]C.pid_t, len(pids))
	for i, pid := range pids {
		cPIDs[i] = C.pid_t(pid)
	}

	ret := C.pqos_mon_remove_pids(C.uint(len(pids)), &cPIDs[0], group.group)
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_mon_remove_pids")
	}

	// Update Go structure
	updateMonDataFromC(group)

	return nil
}

// MonStop stops resource monitoring for a group
//
// Parameters:
//   - group: monitoring group to stop
//
// Returns error if operation fails
func (p *PQoS) MonStop(group *MonData) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	if group == nil || group.group == nil {
		return fmt.Errorf("invalid monitoring group")
	}

	ret := C.pqos_mon_stop(group.group)
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_mon_stop")
	}

	// Invalidate the group
	group.Valid = false
	group.group = nil

	return nil
}

// MonPoll polls monitoring data for groups
//
// Parameters:
//   - groups: array of monitoring groups to poll
//
// Returns error if operation fails
func (p *PQoS) MonPoll(groups []*MonData) error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	if len(groups) == 0 {
		return fmt.Errorf("empty groups array")
	}

	// Convert Go slice to C array of pointers
	cGroups := make([]*C.struct_pqos_mon_data, len(groups))
	for i, group := range groups {
		if group == nil || group.group == nil {
			return fmt.Errorf("invalid monitoring group at index %d", i)
		}
		cGroups[i] = group.group
	}

	ret := C.pqos_mon_poll(&cGroups[0], C.uint(len(groups)))
	if ret != C.PQOS_RETVAL_OK && ret != C.PQOS_RETVAL_OVERFLOW {
		return retvalToError(ret, "pqos_mon_poll")
	}

	// Update all groups from C data
	for _, group := range groups {
		updateMonDataFromC(group)
	}

	if ret == C.PQOS_RETVAL_OVERFLOW {
		return fmt.Errorf("MBM counter overflow detected")
	}

	return nil
}

// MonReset resets monitoring by binding all cores with RMID0
func (p *PQoS) MonReset() error {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return ErrNotInitialized
	}

	ret := C.pqos_mon_reset()
	if ret != C.PQOS_RETVAL_OK {
		return retvalToError(ret, "pqos_mon_reset")
	}

	return nil
}

// MonAssocGet reads RMID association of a logical core
//
// Parameters:
//   - lcore: logical core ID
//
// Returns RMID and error if operation fails
func (p *PQoS) MonAssocGet(lcore uint) (uint32, error) {
	p.mu.Lock()
	defer p.mu.Unlock()

	if !p.initialized {
		return 0, ErrNotInitialized
	}

	var rmid C.pqos_rmid_t
	ret := C.pqos_mon_assoc_get(C.uint(lcore), &rmid)
	if ret != C.PQOS_RETVAL_OK {
		return 0, retvalToError(ret, "pqos_mon_assoc_get")
	}

	return uint32(rmid), nil
}

// Helper function to convert C mon_data to Go MonData
func cMonDataToGo(cGroup *C.struct_pqos_mon_data) *MonData {
	if cGroup == nil {
		return nil
	}

	group := &MonData{
		Valid: int(cGroup.valid) != 0,
		Event: uint(cGroup.event),
		group: cGroup,
	}

	// Copy event values
	group.Values = EventValues{
		LLC:              uint64(cGroup.values.llc),
		MBMLocal:         uint64(cGroup.values.mbm_local),
		MBMTotal:         uint64(cGroup.values.mbm_total),
		MBMRemote:        uint64(cGroup.values.mbm_remote),
		MBMLocalDelta:    uint64(cGroup.values.mbm_local_delta),
		MBMTotalDelta:    uint64(cGroup.values.mbm_total_delta),
		MBMRemoteDelta:   uint64(cGroup.values.mbm_remote_delta),
		IPCRetired:       uint64(cGroup.values.ipc_retired),
		IPCRetiredDelta:  uint64(cGroup.values.ipc_retired_delta),
		IPCUnhalted:      uint64(cGroup.values.ipc_unhalted),
		IPCUnhaltedDelta: uint64(cGroup.values.ipc_unhalted_delta),
		LLCMissTotal:     uint64(cGroup.values.llc_misses),
		LLCMissDelta:     uint64(cGroup.values.llc_misses_delta),
		LLCRefTotal:      uint64(cGroup.values.llc_references),
		LLCRefDelta:      uint64(cGroup.values.llc_references_delta),
	}

	// Copy cores if present
	numCores := C.get_mon_num_cores(cGroup)
	if numCores > 0 {
		group.Cores = make([]uint, numCores)
		for i := C.uint(0); i < numCores; i++ {
			group.Cores[i] = uint(C.get_mon_core(cGroup, i))
		}
	}

	// Copy PIDs if present
	numPIDs := C.get_mon_num_pids(cGroup)
	if numPIDs > 0 {
		group.PIDs = make([]int, numPIDs)
		for i := C.uint(0); i < numPIDs; i++ {
			group.PIDs[i] = int(C.get_mon_pid(cGroup, i))
		}
	}

	return group
}

// Helper function to update MonData from C structure
func updateMonDataFromC(group *MonData) {
	if group == nil || group.group == nil {
		return
	}

	cGroup := group.group
	group.Valid = int(cGroup.valid) != 0
	group.Event = uint(cGroup.event)

	// Update event values
	group.Values = EventValues{
		LLC:              uint64(cGroup.values.llc),
		MBMLocal:         uint64(cGroup.values.mbm_local),
		MBMTotal:         uint64(cGroup.values.mbm_total),
		MBMRemote:        uint64(cGroup.values.mbm_remote),
		MBMLocalDelta:    uint64(cGroup.values.mbm_local_delta),
		MBMTotalDelta:    uint64(cGroup.values.mbm_total_delta),
		MBMRemoteDelta:   uint64(cGroup.values.mbm_remote_delta),
		IPCRetired:       uint64(cGroup.values.ipc_retired),
		IPCRetiredDelta:  uint64(cGroup.values.ipc_retired_delta),
		IPCUnhalted:      uint64(cGroup.values.ipc_unhalted),
		IPCUnhaltedDelta: uint64(cGroup.values.ipc_unhalted_delta),
		LLCMissTotal:     uint64(cGroup.values.llc_misses),
		LLCMissDelta:     uint64(cGroup.values.llc_misses_delta),
		LLCRefTotal:      uint64(cGroup.values.llc_references),
		LLCRefDelta:      uint64(cGroup.values.llc_references_delta),
	}

	// Update cores
	numCores := C.get_mon_num_cores(cGroup)
	if numCores > 0 {
		group.Cores = make([]uint, numCores)
		for i := C.uint(0); i < numCores; i++ {
			group.Cores[i] = uint(C.get_mon_core(cGroup, i))
		}
	}

	// Update PIDs
	numPIDs := C.get_mon_num_pids(cGroup)
	if numPIDs > 0 {
		group.PIDs = make([]int, numPIDs)
		for i := C.uint(0); i < numPIDs; i++ {
			group.PIDs[i] = int(C.get_mon_pid(cGroup, i))
		}
	}
}

// GetLLCOccupancy returns the L3 cache occupancy in bytes
func (m *MonData) GetLLCOccupancy() uint64 {
	return m.Values.LLC
}

// GetMBMLocalBandwidth returns the local memory bandwidth in bytes
func (m *MonData) GetMBMLocalBandwidth() uint64 {
	return m.Values.MBMLocalDelta
}

// GetMBMTotalBandwidth returns the total memory bandwidth in bytes
func (m *MonData) GetMBMTotalBandwidth() uint64 {
	return m.Values.MBMTotalDelta
}

// GetMBMRemoteBandwidth returns the remote memory bandwidth in bytes
func (m *MonData) GetMBMRemoteBandwidth() uint64 {
	return m.Values.MBMRemoteDelta
}

// GetIPC returns the instructions per cycle
func (m *MonData) GetIPC() float64 {
	if m.Values.IPCUnhaltedDelta == 0 {
		return 0.0
	}
	return float64(m.Values.IPCRetiredDelta) / float64(m.Values.IPCUnhaltedDelta)
}

// GetLLCMissRate returns the LLC miss rate
func (m *MonData) GetLLCMissRate() float64 {
	if m.Values.LLCRefDelta == 0 {
		return 0.0
	}
	return float64(m.Values.LLCMissDelta) / float64(m.Values.LLCRefDelta)
}
