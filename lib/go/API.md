# Intel(R) RDT PQoS Go Bindings - Complete API Reference

**Version:** 1.0.0  
**Package:** `github.com/intel/intel-cmt-cat/lib/go/pqos`

## Table of Contents

- [Overview](#overview)
- [Core Library](#core-library)
- [Capability Detection](#capability-detection)
- [CPU Information](#cpu-information)
- [Cache Allocation](#cache-allocation)
- [Memory Bandwidth Allocation](#memory-bandwidth-allocation)
- [Resource Monitoring](#resource-monitoring)
- [Constants](#constants)
- [Examples](#examples)

---

## Overview

This document provides a complete reference for all public APIs in the PQoS Go bindings.

### Import

```go
import "github.com/intel/intel-cmt-cat/lib/go/pqos"
```

---

## Core Library

### PQoS Type

Main library handle implementing singleton pattern.

```go
type PQoS struct {
    // Private fields
}
```

### GetInstance

Returns the singleton PQoS instance.

```go
func GetInstance() *PQoS
```

**Example:**
```go
p := pqos.GetInstance()
```

### Init

Initializes the PQoS library.

```go
func (p *PQoS) Init(cfg *Config) error
```

**Parameters:**
- `cfg` - Configuration (can be nil for defaults)

**Returns:** Error if initialization fails

**Example:**
```go
cfg := &pqos.Config{
    Interface: pqos.InterOS,
    Verbose:   pqos.LogVerbosityDefault,
}
err := p.Init(cfg)
```

### Fini

Shuts down the PQoS library.

```go
func (p *PQoS) Fini() error
```

**Returns:** Error if finalization fails

**Example:**
```go
defer p.Fini()
```

### IsInitialized

Checks if library is initialized.

```go
func (p *PQoS) IsInitialized() bool
```

**Returns:** True if initialized

### GetInterface

Gets the currently active interface.

```go
func (p *PQoS) GetInterface() (int, error)
```

**Returns:** Interface type constant and error

**Example:**
```go
iface, _ := p.GetInterface()
fmt.Println(pqos.InterfaceToString(iface))
```

### Config Type

```go
type Config struct {
    Interface int       // InterMSR, InterOS, InterAuto
    Verbose   int       // Log verbosity level
    LogFile   *os.File  // Optional log file
}
```

---

## Capability Detection

### GetCapability

Retrieves system capabilities.

```go
func (p *PQoS) GetCapability() (*Capability, error)
```

**Returns:** Capability structure and error

**Example:**
```go
cap, err := p.GetCapability()
if err != nil {
    log.Fatal(err)
}
```

### Capability Type

```go
type Capability struct {
    // Private fields
}
```

**Methods:**

#### HasL3CA

```go
func (c *Capability) HasL3CA() bool
```

Checks if L3 Cache Allocation is supported.

#### HasL2CA

```go
func (c *Capability) HasL2CA() bool
```

Checks if L2 Cache Allocation is supported.

#### HasMBA

```go
func (c *Capability) HasMBA() bool
```

Checks if Memory Bandwidth Allocation is supported.

#### HasMon

```go
func (c *Capability) HasMon() bool
```

Checks if resource monitoring is supported.

#### GetL3CA

```go
func (c *Capability) GetL3CA() (*L3CACapability, error)
```

Gets L3 CAT capability details.

**Returns:** L3CACapability structure

```go
type L3CACapability struct {
    MemSize          uint    // Structure size
    NumClasses       uint    // Number of COS
    NumWays          uint    // Number of cache ways
    WaySize          uint    // Way size in bytes
    WayContention    uint64  // Contention bitmask
    CDP              bool    // CDP supported
    CDPOn            bool    // CDP enabled
    NonContiguousCBM bool    // Non-contiguous CBM support
    IORDT            bool    // I/O RDT supported
    IORDTOn          bool    // I/O RDT enabled
}
```

#### GetL2CA

```go
func (c *Capability) GetL2CA() (*L2CACapability, error)
```

Gets L2 CAT capability details.

**Returns:** L2CACapability structure

```go
type L2CACapability struct {
    MemSize          uint
    NumClasses       uint
    NumWays          uint
    WaySize          uint
    WayContention    uint64
    CDP              bool
    CDPOn            bool
    NonContiguousCBM bool
}
```

#### GetMBA

```go
func (c *Capability) GetMBA() (*MBACapability, error)
```

Gets MBA capability details.

**Returns:** MBACapability structure

```go
type MBACapability struct {
    MemSize      uint  // Structure size
    NumClasses   uint  // Number of COS
    ThrottleMax  uint  // Max throttle value
    ThrottleStep uint  // Throttle granularity
    IsLinear     bool  // Linear throttling
    Ctrl         bool  // Controller supported
    CtrlOn       bool  // Controller enabled
    MBA40        bool  // MBA 4.0 supported
    MBA40On      bool  // MBA 4.0 enabled
}
```

#### GetMon

```go
func (c *Capability) GetMon() (*MonCapability, error)
```

Gets monitoring capability details.

**Returns:** MonCapability structure

```go
type MonCapability struct {
    MemSize   uint       // Structure size
    MaxRMID   uint       // Max RMID
    L3Size    uint       // L3 cache size
    NumEvents uint       // Number of events
    IORDT     bool       // I/O RDT monitoring
    IORDTOn   bool       // I/O RDT enabled
    SNCNum    uint       // SNC clusters
    SNCMode   int        // SNC mode
    Events    []Monitor  // Supported events
}

type Monitor struct {
    Type          uint    // Event type
    MaxRMID       uint    // Max RMID for event
    ScaleFactor   uint32  // Scale factor
    CounterLength uint    // Counter bits
    IORDT         bool    // I/O RDT support
}
```

---

## CPU Information

### GetCPUInfo

Gets CPU topology information.

```go
func (c *Capability) GetCPUInfo() (*CPUInfo, error)
```

**Returns:** CPUInfo structure

```go
type CPUInfo struct {
    MemSize  uint        // Structure size
    L2       CacheInfo   // L2 cache info
    L3       CacheInfo   // L3 cache info
    Vendor   int         // CPU vendor
    NumCores uint        // Number of cores
    Cores    []CoreInfo  // Core information
}

type CoreInfo struct {
    LCore   uint  // Logical core ID
    Socket  uint  // Socket ID
    L3ID    uint  // L3 cluster ID
    L2ID    uint  // L2 cluster ID
    L3CATID uint  // L3 CAT ID
    MBAID   uint  // MBA ID
    NUMA    uint  // NUMA node
    SMBAID  uint  // SMBA ID
}

type CacheInfo struct {
    Detected      bool  // Cache detected
    NumWays       uint  // Number of ways
    NumSets       uint  // Number of sets
    NumPartitions uint  // Number of partitions
    LineSize      uint  // Line size (bytes)
    TotalSize     uint  // Total size (bytes)
    WaySize       uint  // Way size (bytes)
}
```

**Methods:**

```go
func (c *CPUInfo) GetNumCores() uint
func (c *CPUInfo) GetNumSockets() uint
func (c *CPUInfo) GetSocketIDs() []uint
func (c *CPUInfo) GetL3IDs() []uint
func (c *CPUInfo) GetL3CATIDs() []uint
func (c *CPUInfo) GetMBAIDs() []uint
func (c *CPUInfo) GetCoresBySocket(socket uint) []CoreInfo
func (c *CPUInfo) GetCoresByL3ID(l3ID uint) []CoreInfo
func (c *CPUInfo) GetCoresByNUMA(numa uint) []CoreInfo
func (c *CPUInfo) FindCore(lcore uint) *CoreInfo
func (c *CPUInfo) GetL3CacheSize() uint
func (c *CPUInfo) GetL2CacheSize() uint
```

---

## Cache Allocation

### L3CA Type

L3 Cache Allocation configuration.

```go
type L3CA struct {
    ClassID  uint    // Class of Service
    CDP      bool    // Code/Data Prioritization
    WaysMask uint64  // Cache ways mask (non-CDP)
    CodeMask uint64  // Code mask (CDP)
    DataMask uint64  // Data mask (CDP)
}
```

### L3CASet

Configures L3 cache allocation.

```go
func (p *PQoS) L3CASet(l3CatID uint, cos []L3CA) error
```

**Parameters:**
- `l3CatID` - L3 cache cluster ID
- `cos` - Array of L3CA configurations

**Example:**
```go
cos := []pqos.L3CA{
    {
        ClassID:  1,
        CDP:      false,
        WaysMask: 0x0F, // Use first 4 ways
    },
}
err := p.L3CASet(0, cos)
```

### L3CAGet

Retrieves L3 cache allocation configuration.

```go
func (p *PQoS) L3CAGet(l3CatID uint, maxNumCOS uint) ([]L3CA, error)
```

**Parameters:**
- `l3CatID` - L3 cache cluster ID
- `maxNumCOS` - Maximum number of classes to retrieve

**Returns:** Array of L3CA configurations and error

**Example:**
```go
cos, err := p.L3CAGet(0, 16)
```

### L3CAGetMinCBMBits

Gets minimum CBM bits required.

```go
func (p *PQoS) L3CAGetMinCBMBits() (uint, error)
```

**Returns:** Minimum bits and error

### L2CA Type

L2 Cache Allocation configuration.

```go
type L2CA struct {
    ClassID  uint    // Class of Service
    CDP      bool    // Code/Data Prioritization
    WaysMask uint64  // Cache ways mask (non-CDP)
    CodeMask uint64  // Code mask (CDP)
    DataMask uint64  // Data mask (CDP)
}
```

### L2CASet

Configures L2 cache allocation.

```go
func (p *PQoS) L2CASet(l2ID uint, cos []L2CA) error
```

**Parameters:**
- `l2ID` - L2 cache cluster ID
- `cos` - Array of L2CA configurations

### L2CAGet

Retrieves L2 cache allocation configuration.

```go
func (p *PQoS) L2CAGet(l2ID uint, maxNumCOS uint) ([]L2CA, error)
```

### L2CAGetMinCBMBits

Gets minimum CBM bits required.

```go
func (p *PQoS) L2CAGetMinCBMBits() (uint, error)
```

---

## Memory Bandwidth Allocation

### MBA Type

Memory Bandwidth Allocation configuration.

```go
type MBA struct {
    ClassID uint  // Class of Service
    MBMax   uint  // Max bandwidth (% or MBps)
    Ctrl    bool  // Controller mode
    SMBA    bool  // Slow memory bandwidth
}
```

### MBASet

Configures memory bandwidth allocation.

```go
func (p *PQoS) MBASet(mbaID uint, requested []MBA) ([]MBA, error)
```

**Parameters:**
- `mbaID` - MBA cluster ID
- `requested` - Requested MBA configurations

**Returns:** Actual MBA configurations applied and error

**Example:**
```go
requested := []pqos.MBA{
    {
        ClassID: 1,
        MBMax:   50, // 50% bandwidth
        Ctrl:    false,
    },
}
actual, err := p.MBASet(0, requested)
```

### MBAGet

Retrieves memory bandwidth allocation configuration.

```go
func (p *PQoS) MBAGet(mbaID uint, maxNumCOS uint) ([]MBA, error)
```

**Parameters:**
- `mbaID` - MBA cluster ID
- `maxNumCOS` - Maximum number of classes

**Returns:** Array of MBA configurations and error

---

## Core and Process Association

### AllocAssocSet

Associates a logical core with a Class of Service.

```go
func (p *PQoS) AllocAssocSet(lcore uint, classID uint) error
```

**Parameters:**
- `lcore` - Logical core ID
- `classID` - Class of Service ID

**Example:**
```go
err := p.AllocAssocSet(0, 1) // Associate core 0 with COS 1
```

### AllocAssocGet

Gets the COS associated with a core.

```go
func (p *PQoS) AllocAssocGet(lcore uint) (uint, error)
```

**Returns:** Class ID and error

### AllocAssocSetPID

Associates a process with a COS (OS interface).

```go
func (p *PQoS) AllocAssocSetPID(pid int, classID uint) error
```

**Parameters:**
- `pid` - Process ID
- `classID` - Class of Service ID

### AllocAssocGetPID

Gets the COS associated with a process.

```go
func (p *PQoS) AllocAssocGetPID(pid int) (uint, error)
```

**Returns:** Class ID and error

### AllocAssign

Automatically assigns an available COS to cores.

```go
func (p *PQoS) AllocAssign(technology uint, cores []uint) (uint, error)
```

**Parameters:**
- `technology` - Technology bitmask (1 << CapType)
- `cores` - Array of core IDs

**Returns:** Assigned class ID and error

**Example:**
```go
tech := (1 << pqos.CapTypeL3CA) | (1 << pqos.CapTypeMBA)
classID, err := p.AllocAssign(tech, []uint{0, 1, 2, 3})
```

### AllocRelease

Releases cores back to default COS#0.

```go
func (p *PQoS) AllocRelease(cores []uint) error
```

### AllocAssignPID

Automatically assigns an available COS to processes.

```go
func (p *PQoS) AllocAssignPID(technology uint, pids []int) (uint, error)
```

### AllocReleasePID

Releases processes back to default COS#0.

```go
func (p *PQoS) AllocReleasePID(pids []int) error
```

### AllocReset

Resets all allocation configuration to defaults.

```go
func (p *PQoS) AllocReset() error
```

---

## Resource Monitoring

### MonData Type

Monitoring group data.

```go
type MonData struct {
    Valid  bool         // Structure validity
    Event  uint         // Monitored events
    Values EventValues  // Event metrics
    Cores  []uint       // Monitored cores
    PIDs   []int        // Monitored processes
}

type EventValues struct {
    LLC              uint64  // LLC occupancy (bytes)
    MBMLocal         uint64  // Local mem BW reading
    MBMTotal         uint64  // Total mem BW reading
    MBMRemote        uint64  // Remote mem BW reading
    MBMLocalDelta    uint64  // Local mem BW delta
    MBMTotalDelta    uint64  // Total mem BW delta
    MBMRemoteDelta   uint64  // Remote mem BW delta
    IPCRetired       uint64  // Instructions retired
    IPCRetiredDelta  uint64  // Instructions delta
    IPCUnhalted      uint64  // Unhalted cycles
    IPCUnhaltedDelta uint64  // Unhalted cycles delta
    LLCMissTotal     uint64  // LLC misses
    LLCMissDelta     uint64  // LLC misses delta
    LLCRefTotal      uint64  // LLC references
    LLCRefDelta      uint64  // LLC references delta
}
```

**Methods:**

```go
func (m *MonData) GetLLCOccupancy() uint64
func (m *MonData) GetMBMLocalBandwidth() uint64
func (m *MonData) GetMBMTotalBandwidth() uint64
func (m *MonData) GetMBMRemoteBandwidth() uint64
func (m *MonData) GetIPC() float64
func (m *MonData) GetLLCMissRate() float64
```

### MonStartCores

Starts resource monitoring on cores.

```go
func (p *PQoS) MonStartCores(cores []uint, event uint) (*MonData, error)
```

**Parameters:**
- `cores` - Array of logical core IDs
- `event` - Combination of monitoring events

**Returns:** MonData structure and error

**Example:**
```go
event := pqos.MonEventL3Occup | pqos.MonEventTMemBW
group, err := p.MonStartCores([]uint{0, 1}, event)
```

### MonStartPIDs

Starts resource monitoring on processes.

```go
func (p *PQoS) MonStartPIDs(pids []int, event uint) (*MonData, error)
```

**Parameters:**
- `pids` - Array of process IDs
- `event` - Combination of monitoring events

**Returns:** MonData structure and error

**Example:**
```go
event := pqos.MonEventL3Occup | pqos.MonEventTMemBW
group, err := p.MonStartPIDs([]int{1234, 5678}, event)
```

### MonAddPIDs

Adds PIDs to an existing monitoring group.

```go
func (p *PQoS) MonAddPIDs(group *MonData, pids []int) error
```

### MonRemovePIDs

Removes PIDs from a monitoring group.

```go
func (p *PQoS) MonRemovePIDs(group *MonData, pids []int) error
```

### MonPoll

Polls monitoring data for groups.

```go
func (p *PQoS) MonPoll(groups []*MonData) error
```

**Parameters:**
- `groups` - Array of monitoring groups to poll

**Example:**
```go
err := p.MonPoll([]*pqos.MonData{group1, group2})
if err != nil {
    log.Printf("Poll error: %v", err)
}
fmt.Printf("LLC: %d bytes\n", group1.GetLLCOccupancy())
```

### MonStop

Stops resource monitoring for a group.

```go
func (p *PQoS) MonStop(group *MonData) error
```

### MonReset

Resets all monitoring by binding cores to RMID0.

```go
func (p *PQoS) MonReset() error
```

### MonAssocGet

Gets RMID association of a core.

```go
func (p *PQoS) MonAssocGet(lcore uint) (uint32, error)
```

**Returns:** RMID and error

---

## Constants

### Interface Types

```go
const (
    InterMSR           // MSR interface
    InterOS            // OS interface (resctrl)
    InterOSResctrlMon  // OS with resctrl monitoring
    InterAuto          // Auto-detect
)
```

### Log Verbosity

```go
const (
    LogVerbositySilent        = -1
    LogVerbosityDefault       = 0
    LogVerbosityVerbose       = 1
    LogVerbositySuperVerbose  = 2
)
```

### Capability Types

```go
const (
    CapTypeMon   // Monitoring
    CapTypeL3CA  // L3 Cache Allocation
    CapTypeL2CA  // L2 Cache Allocation
    CapTypeMBA   // Memory Bandwidth Allocation
    CapTypeSMBA  // Slow Memory Bandwidth Allocation
)
```

### Monitoring Events

```go
const (
    MonEventL3Occup    // LLC occupancy
    MonEventLMemBW     // Local memory bandwidth
    MonEventTMemBW     // Total memory bandwidth
    MonEventRMemBW     // Remote memory bandwidth
    PerfEventLLCMiss   // LLC misses
    PerfEventIPC       // Instructions per clock
    PerfEventLLCRef    // LLC references
)
```

### CPU Vendors

```go
const (
    VendorUnknown
    VendorIntel
    VendorAMD
)
```

### Return Values

```go
const (
    RetvalOK
    RetvalError
    RetvalParam
    RetvalResource
    RetvalInit
    RetvalTransport
    RetvalPerfCtr
    RetvalBusy
    RetvalInter
    RetvalOverflow
)
```

---

## Helper Functions

### String Conversion Functions

```go
func InterfaceToString(iface int) string
func VerbosityToString(verbose int) string
func VendorToString(vendor int) string
func CapTypeToString(capType int) string
func MonEventToString(event uint) string
```

---

## Examples

### Complete Workflow Example

```go
package main

import (
    "fmt"
    "log"
    "time"
    
    "github.com/intel/intel-cmt-cat/lib/go/pqos"
)

func main() {
    // Initialize
    p := pqos.GetInstance()
    cfg := &pqos.Config{
        Interface: pqos.InterAuto,
        Verbose:   pqos.LogVerbosityDefault,
    }
    
    if err := p.Init(cfg); err != nil {
        log.Fatal(err)
    }
    defer p.Fini()
    
    // Get capabilities
    cap, _ := p.GetCapability()
    
    // Configure L3 CAT
    if cap.HasL3CA() {
        cos := []pqos.L3CA{
            {ClassID: 1, WaysMask: 0x0F}, // 4 ways
            {ClassID: 2, WaysMask: 0xF0}, // 4 ways
        }
        p.L3CASet(0, cos)
        
        // Associate cores with COS
        p.AllocAssocSet(0, 1) // Core 0 -> COS 1
        p.AllocAssocSet(1, 2) // Core 1 -> COS 2
    }
    
    // Start monitoring
    if cap.HasMon() {
        event := pqos.MonEventL3Occup | pqos.MonEventTMemBW
        group, _ := p.MonStartCores([]uint{0, 1}, event)
        defer p.MonStop(group)
        
        // Poll for 10 seconds
        for i := 0; i < 10; i++ {
            time.Sleep(time.Second)
            p.MonPoll([]*pqos.MonData{group})
            
            fmt.Printf("LLC: %d KB, BW: %d MB/s\n",
                group.GetLLCOccupancy()/1024,
                group.GetMBMTotalBandwidth()/(1024*1024))
        }
    }
    
    // Reset to defaults
    p.AllocReset()
}
```

---

## Error Handling

All functions that can fail return an error. Always check errors:

```go
if err := p.Init(cfg); err != nil {
    log.Fatalf("Initialization failed: %v", err)
}
```

Common errors:
- `"PQoS not initialized"` - Call Init() first
- `"initialization error"` - Check permissions, hardware support
- `"parameter error"` - Invalid parameters
- `"interface not supported"` - Interface not available

---

## Thread Safety

The PQoS type is thread-safe. Multiple goroutines can safely call methods on the same instance.

```go
go func() {
    cos, _ := p.L3CAGet(0, 16)
    // Process...
}()

go func() {
    group, _ := p.MonStartCores([]uint{0}, pqos.MonEventL3Occup)
    // Monitor...
}()
```

---

## License

BSD 3-Clause License - Copyright(c) 2025 Zededa, Inc.
