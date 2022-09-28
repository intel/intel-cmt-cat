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

import { HttpClient } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { catchError, Observable, of } from 'rxjs';

import {
  CacheAllocation,
  Caps,
  MBA,
  MBACTRL,
  RDTIface,
  resMessage,
  SSTBF,
} from '../components/system-caps/system-caps.model';

import { LocalService } from './local.service';
import { Pools } from '../components/overview/overview.model';

@Injectable({
  providedIn: 'root',
})

/* Service used to get data from backend */
export class AppqosService {
  constructor(private http: HttpClient, private local: LocalService) {}

  login(host: string, port: string) {
    return this.http
      .get(`${host}:${port}/caps`)
      .pipe(catchError((_) => of(false)));
  }

  /**
   * Retrieve system capabilities
   */
  getCaps(): Observable<Caps> {
    const api_url = this.local.getData('api_url');
    return this.http.get<Caps>(`${api_url}/caps`);
  }

  /**
   * Retrieve L3 Cache Allocation
   */
  getL3cat(): Observable<CacheAllocation> {
    const api_url = this.local.getData('api_url');
    return this.http.get<CacheAllocation>(`${api_url}/caps/l3cat`);
  }

  /**
   * Retrieve L2 Cache Allocation
   */
  getL2cat(): Observable<CacheAllocation> {
    const api_url = this.local.getData('api_url');
    return this.http.get<CacheAllocation>(`${api_url}/caps/l2cat`);
  }

  /**
   * Retrieve Memory Bandwidth Allocation
   */
  getMba(): Observable<MBA> {
    const api_url = this.local.getData('api_url');
    return this.http.get<MBA>(`${api_url}/caps/mba`);
  }

  /**
   * Retrieve Memory Bandwidth Allocation controller
   */
  getMbaCtrl(): Observable<MBACTRL> {
    const api_url = this.local.getData('api_url');
    return this.http.get<MBACTRL>(`${api_url}/caps/mba_ctrl`);
  }

  /**
   * Retrieve RDT Interface
   */
  getRdtIface(): Observable<RDTIface> {
    const api_url = this.local.getData('api_url');
    return this.http.get<RDTIface>(`${api_url}/caps/rdt_iface`);
  }

  /**
   * Retrieve SST - Base Frequency
   */
  getSstbf(): Observable<SSTBF> {
    const api_url = this.local.getData('api_url');
    return this.http.get<SSTBF>(`${api_url}/caps/sstbf`);
  }

  rdtIfacePut(value: string): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.put<resMessage>(`${api_url}/caps/rdt_iface`, {
      interface: value,
    });
  }

  sstbfPut(value: boolean): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.put<any>(`${api_url}/caps/sstbf`, {
      configured: value,
    });
  }

  mbaCtrlPut(value: boolean): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.put<any>(`${api_url}/caps/mba_ctrl`, {
      enabled: value,
    });
  }

  /**
   * Retrieve Pools
   */
  getPools(): Observable<Pools[]> {
    const api_url = this.local.getData('api_url');
    return this.http.get<Pools[]>(`${api_url}/pools`);
  }
}
