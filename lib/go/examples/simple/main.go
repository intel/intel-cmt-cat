// Copyright(c) 2025 Zededa, Inc.
// SPDX-License-Identifier: BSD-3-Clause

// Simple example demonstrating basic usage of PQoS Go bindings
package main

import (
	"fmt"
	"log"

	"github.com/intel/intel-cmt-cat/lib/go/pqos"
)

func main() {
	fmt.Println("Intel(R) RDT PQoS - Simple Example")
	fmt.Println("===================================\n")

	// Step 1: Get the PQoS singleton instance
	p := pqos.GetInstance()

	// Step 2: Configure initialization
	// Use InterAuto to automatically select the best interface
	cfg := &pqos.Config{
		Interface: pqos.InterAuto,
		Verbose:   pqos.LogVerbosityDefault,
	}

	// Step 3: Initialize the library
	fmt.Println("Initializing PQoS library...")
	if err := p.Init(cfg); err != nil {
		log.Fatalf("Failed to initialize PQoS: %v", err)
	}
	// Always clean up when done
	defer func() {
		fmt.Println("\nShutting down PQoS library...")
		if err := p.Fini(); err != nil {
			log.Printf("Warning: Failed to finalize PQoS: %v", err)
		}
	}()

	// Step 4: Get the active interface
	iface, err := p.GetInterface()
	if err != nil {
		log.Printf("Warning: Could not get interface: %v", err)
	} else {
		fmt.Printf("Active interface: %s\n\n", pqos.InterfaceToString(iface))
	}

	// Step 5: Get system capabilities
	fmt.Println("Detecting capabilities...")
	cap, err := p.GetCapability()
	if err != nil {
		log.Fatalf("Failed to get capabilities: %v", err)
	}

	// Step 6: Check what features are supported
	fmt.Println("\nSupported Features:")
	fmt.Printf("  L3 Cache Allocation (CAT): %v\n", cap.HasL3CA())
	fmt.Printf("  L2 Cache Allocation (CAT): %v\n", cap.HasL2CA())
	fmt.Printf("  Memory Bandwidth Alloc:    %v\n", cap.HasMBA())
	fmt.Printf("  Resource Monitoring:       %v\n", cap.HasMon())

	// Step 7: Get CPU information
	cpuInfo, err := cap.GetCPUInfo()
	if err != nil {
		log.Printf("Warning: Could not get CPU info: %v", err)
		return
	}

	fmt.Println("\nSystem Information:")
	fmt.Printf("  CPU Vendor:   %s\n", pqos.VendorToString(cpuInfo.Vendor))
	fmt.Printf("  Total Cores:  %d\n", cpuInfo.NumCores)
	fmt.Printf("  Sockets:      %d\n", cpuInfo.GetNumSockets())

	if cpuInfo.L3.Detected {
		fmt.Printf("  L3 Cache:     %d MB total\n", cpuInfo.GetL3CacheSize()/(1024*1024))
	}

	// Step 8: If L3 CAT is supported, show details
	if cap.HasL3CA() {
		fmt.Println("\nL3 Cache Allocation Details:")
		l3ca, err := cap.GetL3CA()
		if err != nil {
			log.Printf("  Error: %v", err)
		} else {
			fmt.Printf("  Classes of Service: %d\n", l3ca.NumClasses)
			fmt.Printf("  Cache Ways:         %d\n", l3ca.NumWays)
			fmt.Printf("  Way Size:           %d KB\n", l3ca.WaySize/1024)
			fmt.Printf("  CDP Support:        %v\n", l3ca.CDP)
			if l3ca.CDP {
				fmt.Printf("  CDP Enabled:        %v\n", l3ca.CDPOn)
			}
		}
	}

	// Step 9: If monitoring is supported, show event details
	if cap.HasMon() {
		fmt.Println("\nMonitoring Capabilities:")
		mon, err := cap.GetMon()
		if err != nil {
			log.Printf("  Error: %v", err)
		} else {
			fmt.Printf("  Max RMID:        %d\n", mon.MaxRMID)
			fmt.Printf("  Supported Events: %d\n", mon.NumEvents)
			if len(mon.Events) > 0 {
				fmt.Println("  Events:")
				for _, event := range mon.Events {
					fmt.Printf("    - %s\n", pqos.MonEventToString(event.Type))
				}
			}
		}
	}

	fmt.Println("\nSimple example completed successfully!")
}
