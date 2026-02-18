// Copyright(c) 2025 Zededa, Inc.
// SPDX-License-Identifier: BSD-3-Clause

package main

import (
	"fmt"
	"log"
	"os"

	"github.com/intel/intel-cmt-cat/lib/go/pqos"
)

func main() {
	// Initialize PQoS library
	p := pqos.GetInstance()
	cfg := &pqos.Config{
		Interface: pqos.InterAuto,
		Verbose:   pqos.LogVerbosityDefault,
	}

	fmt.Println("Initializing Intel(R) RDT PQoS library...")
	if err := p.Init(cfg); err != nil {
		log.Fatalf("Failed to initialize PQoS: %v", err)
	}
	defer func() {
		if err := p.Fini(); err != nil {
			log.Printf("Failed to finalize PQoS: %v", err)
		}
	}()

	// Get active interface
	iface, err := p.GetInterface()
	if err != nil {
		log.Fatalf("Failed to get interface: %v", err)
	}
	fmt.Printf("Active interface: %s\n\n", pqos.InterfaceToString(iface))

	// Get capabilities
	cap, err := p.GetCapability()
	if err != nil {
		log.Fatalf("Failed to get capabilities: %v", err)
	}

	// Display CPU information
	fmt.Println("=== CPU Information ===")
	cpuInfo, err := cap.GetCPUInfo()
	if err != nil {
		log.Printf("Warning: Failed to get CPU info: %v", err)
	} else {
		displayCPUInfo(cpuInfo)
	}

	fmt.Println("\n=== Intel(R) RDT Capabilities ===")

	// Check L3 Cache Allocation
	if cap.HasL3CA() {
		fmt.Println("\n--- L3 Cache Allocation Technology (CAT) ---")
		l3ca, err := cap.GetL3CA()
		if err != nil {
			log.Printf("Error getting L3CA details: %v", err)
		} else {
			displayL3CA(l3ca)
		}
	} else {
		fmt.Println("\nL3 Cache Allocation: Not supported")
	}

	// Check L2 Cache Allocation
	if cap.HasL2CA() {
		fmt.Println("\n--- L2 Cache Allocation Technology (CAT) ---")
		l2ca, err := cap.GetL2CA()
		if err != nil {
			log.Printf("Error getting L2CA details: %v", err)
		} else {
			displayL2CA(l2ca, cpuInfo)
		}
	} else {
		fmt.Println("\nL2 Cache Allocation: Not supported")
	}

	// Check Memory Bandwidth Allocation
	if cap.HasMBA() {
		fmt.Println("\n--- Memory Bandwidth Allocation (MBA) ---")
		mba, err := cap.GetMBA()
		if err != nil {
			log.Printf("Error getting MBA details: %v", err)
		} else {
			displayMBA(mba)
		}
	} else {
		fmt.Println("\nMemory Bandwidth Allocation: Not supported")
	}

	// Check Monitoring
	if cap.HasMon() {
		fmt.Println("\n--- Resource Monitoring ---")
		mon, err := cap.GetMon()
		if err != nil {
			log.Printf("Error getting MON details: %v", err)
		} else {
			displayMon(mon)
		}
	} else {
		fmt.Println("\nResource Monitoring: Not supported")
	}

	fmt.Println("\n=== Summary ===")
	fmt.Printf("L3 CAT:   %s\n", supportStatus(cap.HasL3CA()))
	fmt.Printf("L2 CAT:   %s\n", supportStatus(cap.HasL2CA()))
	fmt.Printf("MBA:      %s\n", supportStatus(cap.HasMBA()))
	fmt.Printf("MON:      %s\n", supportStatus(cap.HasMon()))
}

func displayCPUInfo(cpu *pqos.CPUInfo) {
	fmt.Printf("Vendor:       %s\n", pqos.VendorToString(cpu.Vendor))
	fmt.Printf("Total Cores:  %d\n", cpu.NumCores)
	fmt.Printf("Sockets:      %d\n", cpu.GetNumSockets())

	if cpu.L3.Detected {
		fmt.Printf("L3 Cache:     %d KB (%d ways, %d sets)\n",
			cpu.L3.TotalSize/1024, cpu.L3.NumWays, cpu.L3.NumSets)
	}
	if cpu.L2.Detected {
		fmt.Printf("L2 Cache:     %d KB (%d ways, %d sets)\n",
			cpu.L2.TotalSize/1024, cpu.L2.NumWays, cpu.L2.NumSets)
	}

	// Display socket topology
	fmt.Printf("\nSocket Topology:\n")
	for _, socketID := range cpu.GetSocketIDs() {
		cores := cpu.GetCoresBySocket(socketID)
		fmt.Printf("  Socket %d: %d cores ", socketID, len(cores))
		if len(cores) <= 16 {
			fmt.Printf("(")
			for i, core := range cores {
				if i > 0 {
					fmt.Printf(", ")
				}
				fmt.Printf("%d", core.LCore)
			}
			fmt.Printf(")")
		}
		fmt.Println()
	}
}

func displayL3CA(l3ca *pqos.L3CACapability) {
	fmt.Printf("Number of Classes:    %d\n", l3ca.NumClasses)
	fmt.Printf("Number of Ways:       %d\n", l3ca.NumWays)
	fmt.Printf("Way Size:             %d KB\n", l3ca.WaySize/1024)
	fmt.Printf("Total L3 Cache:       %d MB\n", (l3ca.NumWays*l3ca.WaySize)/(1024*1024))
	fmt.Printf("CDP Support:          %s\n", yesNo(l3ca.CDP))
	fmt.Printf("CDP Enabled:          %s\n", yesNo(l3ca.CDPOn))
	fmt.Printf("Non-Contiguous CBM:   %s\n", yesNo(l3ca.NonContiguousCBM))
	fmt.Printf("I/O RDT Support:      %s\n", yesNo(l3ca.IORDT))
	if l3ca.IORDT {
		fmt.Printf("I/O RDT Enabled:      %s\n", yesNo(l3ca.IORDTOn))
	}
	if l3ca.WayContention != 0 {
		fmt.Printf("Way Contention Mask:  0x%x\n", l3ca.WayContention)
	}
}

func displayL2CA(l2ca *pqos.L2CACapability, cpuInfo *pqos.CPUInfo) {
	fmt.Printf("Number of Classes:    %d\n", l2ca.NumClasses)
	fmt.Printf("Number of Ways:       %d\n", l2ca.NumWays)
	fmt.Printf("Way Size:             %d KB\n", l2ca.WaySize/1024)
	fmt.Printf("Total L2 Cache:       %d KB\n", (l2ca.NumWays*l2ca.WaySize)/1024)
	fmt.Printf("CDP Support:          %s\n", yesNo(l2ca.CDP))
	fmt.Printf("CDP Enabled:          %s\n", yesNo(l2ca.CDPOn))
	fmt.Printf("Non-Contiguous CBM:   %s\n", yesNo(l2ca.NonContiguousCBM))
	if l2ca.WayContention != 0 {
		fmt.Printf("Way Contention Mask:  0x%x\n", l2ca.WayContention)
	}

	// Display L2 topology
	if cpuInfo != nil {
		l2IDs := cpuInfo.GetL2IDs()
		if len(l2IDs) > 0 {
			fmt.Printf("\nL2 Cache Domains: %d\n", cpuInfo.GetNumL2Domains())
			// Show first few domains as examples
			maxDisplay := 8
			if len(l2IDs) > maxDisplay {
				fmt.Printf("Showing first %d of %d domains:\n", maxDisplay, len(l2IDs))
				l2IDs = l2IDs[:maxDisplay]
			}
			for _, l2ID := range l2IDs {
				cores := cpuInfo.GetCoresByL2ID(l2ID)
				fmt.Printf("  L2 Domain %d: %d cores ", l2ID, len(cores))
				if len(cores) <= 8 {
					fmt.Printf("(")
					for i, core := range cores {
						if i > 0 {
							fmt.Printf(", ")
						}
						fmt.Printf("%d", core.LCore)
					}
					fmt.Printf(")")
				}
				fmt.Println()
			}
		}
	}
}

func displayMBA(mba *pqos.MBACapability) {
	fmt.Printf("Number of Classes:    %d\n", mba.NumClasses)
	fmt.Printf("Throttle Maximum:     %d%%\n", mba.ThrottleMax)
	fmt.Printf("Throttle Step:        %d%%\n", mba.ThrottleStep)
	fmt.Printf("Linear Mode:          %s\n", yesNo(mba.IsLinear))
	fmt.Printf("Controller Support:   %s\n", yesNo(mba.Ctrl))
	if mba.Ctrl {
		fmt.Printf("Controller Enabled:   %s\n", yesNo(mba.CtrlOn))
	}
	fmt.Printf("MBA 4.0 Support:      %s\n", yesNo(mba.MBA40))
	if mba.MBA40 {
		fmt.Printf("MBA 4.0 Enabled:      %s\n", yesNo(mba.MBA40On))
	}
}

func displayMon(mon *pqos.MonCapability) {
	fmt.Printf("Maximum RMID:         %d\n", mon.MaxRMID)
	fmt.Printf("L3 Cache Size:        %d MB\n", mon.L3Size/(1024*1024))
	fmt.Printf("Number of Events:     %d\n", mon.NumEvents)
	fmt.Printf("I/O RDT Support:      %s\n", yesNo(mon.IORDT))
	if mon.IORDT {
		fmt.Printf("I/O RDT Enabled:      %s\n", yesNo(mon.IORDTOn))
	}
	if mon.SNCNum > 0 {
		fmt.Printf("SNC Clusters:         %d\n", mon.SNCNum)
		fmt.Printf("SNC Mode:             %d\n", mon.SNCMode)
	}

	if len(mon.Events) > 0 {
		fmt.Println("\nSupported Events:")
		for _, event := range mon.Events {
			fmt.Printf("  - %-12s (Max RMID: %d, Scale Factor: %d, Counter Length: %d bits)\n",
				pqos.MonEventToString(event.Type), event.MaxRMID, event.ScaleFactor, event.CounterLength)
		}
	}
}

func yesNo(b bool) string {
	if b {
		return "Yes"
	}
	return "No"
}

func supportStatus(supported bool) string {
	if supported {
		return "Supported"
	}
	return "✗ Not supported"
}

func init() {
	// Check if running with appropriate privileges
	if os.Geteuid() != 0 {
		fmt.Fprintf(os.Stderr, "Warning: This program may require root privileges to access MSR interface.\n")
		fmt.Fprintf(os.Stderr, "         Consider running with sudo or using OS interface.\n\n")
	}
}
