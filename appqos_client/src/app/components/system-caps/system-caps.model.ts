/*BSD LICENSE

Copyright(c) 2022 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
  * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.
    
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.*/

import { Apps, Pools } from "../overview/overview.model";

export interface Caps {
  capabilities: string[];
}

export interface CacheAllocation {
  cache_size: number;
  cdp_enabled: boolean;
  cdp_supported: boolean;
  clos_num: number;
  cw_num: number;
  cw_size: number;
}

export interface MBA {
  clos_num: number;
  mba_enabled: boolean;
  mba_bw_enabled: boolean;
}

export interface MBACTRL {
  enabled: boolean;
  supported: boolean;
}

export interface RDTIface {
  interface: string;
  interface_supported: string[];
}

export interface SSTBF {
  configured: boolean;
  hp_cores: number[];
  std_cores: number[];
}

export interface resMessage {
  message: string;
}

export enum Standards {
  MAX_CHARS = 80,
  MAX_CHARS_CORES = 4096,
  MAX_CHARS_PIDS = 4096,
  MAX_CORES = 1024,
  MIN_FREQ = 400,
  MAX_FREQ = 5000
}

/* cache information */
export interface CacheInfo {
  level: number;
  num_ways: number;
  num_sets: number;
  num_partitions: number;
  line_size: number;
  total_size: number;
  way_size: number;
}

/* logical core information */
export interface CoreInfo {
  socket: number;
  lcore: number;
  L2ID: number;
  L3ID?: number;
  sstbfHP?: boolean;
}

/* system information  */
export interface SystemTopology {
  vendor: string;
  cache: CacheInfo[];
  core: CoreInfo[];
}

/* node specific information */
export interface Node {
  nodeID: number;
  cores?: CoreInfo[];
}

/* power profile information */
export interface PowerProfiles {
  id: number,
  name: string,
  min_freq: number,
  max_freq: number,
  epp: string,
}

export interface AppqosConfig {
  rdt_iface: {"interface": string},
  mba_ctrl?: { "enabled": boolean },
  apps: Apps[],
  pools: Pools[],
  power_profiles?: PowerProfiles[],
  sstbf?: { "configured": boolean },
  power_profiles_expert_mode?: boolean,
  power_profiles_verify?: boolean
}

export type PostProfile = Omit<PowerProfiles, 'id'>

/* energy performance preference (EPP) */
export enum EnergyPerformPref {
  performance = 0,
  balancePerformance = 1,
  balancePower = 2,
  power = 3,
}

export const eppDisplayStr: string[] = [
  'Performance',
  'Balance Performance',
  'Balance Power',
  'Power'
];

export const eppPostStr: string[] = [
  'performance',
  'balance_performance',
  'balance_power',
  'power'
];
