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

import { HttpClient, HttpErrorResponse } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { catchError, map, Observable, of, tap, throwError } from 'rxjs';

import {
  CacheAllocation,
  Caps,
  SystemTopology,
  MBA,
  MBACTRL,
  RDTIface,
  resMessage,
  SSTBF,
  PowerProfiles,
} from '../components/system-caps/system-caps.model';
import { LocalService } from './local.service';
import { Apps, Pools } from '../components/overview/overview.model';

type poolPutType = {
  l2cbm?: number;
  l2cbm_code?: number;
  l2cbm_data?: number;
  l3cbm?: number;
  l3cbm_code?: number;
  l3cbm_data?: number;
  mba?: number;
  mba_bw?: number;
  name?: string;
  cores?: number[];
};

@Injectable({
  providedIn: 'root',
})

/* Service used to get data from backend */
export class AppqosService {
  constructor(private http: HttpClient, private local: LocalService) { }

  login(host: string, port: string) {
    return this.http
      .get(`${host}:${port}/caps/cpu`)
      .pipe(
        catchError(this.handleError),
        tap((topo) => this.local.saveData(
          'system_topology', JSON.stringify(topo)
        )
      ));
  }

  /**
   * Retrieve system capabilities
   */
  getCaps(): Observable<Caps> {
    const api_url = this.local.getData('api_url');
    return this.http.get<Caps>(`${api_url}/caps`)
      .pipe(
        catchError(this.handleError),
        tap((caps) => this.local.setCapsEvent(caps.capabilities))
      );
  }

  /**
   * Retrieve system topology information
   */
  getSystemTopology(): Observable<SystemTopology> {
    // first try to read from storage
    const topo = this.local.getData('system_topology');
    if (topo) return of(JSON.parse(topo));

    // otherwise retrieve from server and store
    const api_url = this.local.getData('api_url');
    return this.http.get<SystemTopology>(`${api_url}/caps/cpu`)
      .pipe(tap((topo) => this.local.saveData(
        'system_topology', JSON.stringify(topo)
      )),
        catchError(this.handleError));
  }

  /**
   * Retrieve L3 Cache Allocation
   */
  getL3cat(): Observable<CacheAllocation> {
    const api_url = this.local.getData('api_url');
    return this.http.get<CacheAllocation>(`${api_url}/caps/l3cat`)
      .pipe(
        catchError(this.handleError),
        map((l3cat: CacheAllocation) => {
          l3cat.cache_size = Math.round((l3cat.cache_size / 1024 ** 2) * 100) / 100;
          l3cat.cw_size = Math.round((l3cat.cw_size / 1024 ** 2) * 100) / 100;
          return l3cat;
        }),
        tap((l3cat: CacheAllocation) => {
          this.local.setL3CatEvent(l3cat);
        }),
      );
  }

  /**
   * Retrieve L2 Cache Allocation
   */
  getL2cat(): Observable<CacheAllocation> {
    const api_url = this.local.getData('api_url');
    return this.http.get<CacheAllocation>(`${api_url}/caps/l2cat`)
      .pipe(
        catchError(this.handleError),
        map((l2cat: CacheAllocation) => {
          l2cat.cache_size = Math.round((l2cat.cache_size / 1024 ** 2) * 100) / 100;
          l2cat.cw_size = Math.round((l2cat.cw_size / 1024 ** 2) * 100) / 100;
          return l2cat;
        }),
        tap((l2cat) => {
          this.local.setL2CatEvent(l2cat);
        }),
      );
  }

  /**
   * Retrieve Memory Bandwidth Allocation
   */
  getMba(): Observable<MBA> {
    const api_url = this.local.getData('api_url');
    return this.http.get<MBA>(`${api_url}/caps/mba`)
      .pipe(
        catchError(this.handleError),
        tap((mba) => this.local.setMbaEvent(mba)),
      );
  }

  /**
   * Retrieve Memory Bandwidth Allocation controller
   */
  getMbaCtrl(): Observable<MBACTRL> {
    const api_url = this.local.getData('api_url');
    return this.http.get<MBACTRL>(`${api_url}/caps/mba_ctrl`)
      .pipe(
        catchError(this.handleError),
        tap((mbaCtrl) => this.local.setMbaCtrlEvent(mbaCtrl)),
      );
  }

  /**
   * Retrieve RDT Interface
   */
  getRdtIface(): Observable<RDTIface> {
    const api_url = this.local.getData('api_url');
    return this.http.get<RDTIface>(`${api_url}/caps/rdt_iface`)
      .pipe(
        catchError(this.handleError),
        tap((rdtIface) => this.local.setRdtIfaceEvent(rdtIface))
      );
  }

  /**
   * Retrieve SST - Base Frequency
   */
  getSstbf(): Observable<SSTBF> {
    const api_url = this.local.getData('api_url');
    return this.http.get<SSTBF>(`${api_url}/caps/sstbf`)
      .pipe(
        catchError(this.handleError),
        tap((sstbf) => this.local.setSstbfEvent(sstbf)),
      );
  }

  rdtIfacePut(value: string): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.put<resMessage>(`${api_url}/caps/rdt_iface`, {
      interface: value,
    }).pipe(catchError(this.handleError));
  }

  sstbfPut(value: boolean): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.put<resMessage>(`${api_url}/caps/sstbf`, {
      configured: value,
    }).pipe(catchError(this.handleError));
  }

  mbaCtrlPut(value: boolean): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.put<resMessage>(`${api_url}/caps/mba_ctrl`, {
      enabled: value,
    }).pipe(catchError(this.handleError));
  }

  /**
   * Enable/disable L3 CAT CDP 
   */
  l3CdpPut(value: boolean): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.put<resMessage>(`${api_url}/caps/l3cat`, {
      cdp_enabled: value
    }).pipe(catchError(this.handleError));
  }

  /**
   * Enable/disable L2 CAT CDP
   */
  l2CdpPut(value: Boolean): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.put<resMessage>(`${api_url}/caps/l2cat`, {
      cdp_enabled: value
    }).pipe(catchError(this.handleError));
  }

  /**
   * Retrieve Pools
   */
  getPools(): Observable<Pools[]> {
    const api_url = this.local.getData('api_url');
    return this.http.get<Pools[]>(`${api_url}/pools`)
      .pipe(
        catchError(this.handleError),
        tap((pools) => this.local.setPoolsEvent(pools))
      );
  }

  poolPut(data: poolPutType, id: number): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.put<resMessage>(`${api_url}/pools/${id}`, data)
      .pipe(catchError(this.handleError));
  }

  deletePool(id: number): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.delete<resMessage>(`${api_url}/pools/${id}`)
      .pipe(catchError(this.handleError));
  }

  postPool(pool: any): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.post<resMessage>(`${api_url}/pools/`, pool)
      .pipe(catchError(this.handleError));
  }

  getApps(): Observable<Apps[]> {
    const api_url = this.local.getData('api_url');
    return this.http.get<Apps[]>(`${api_url}/apps`)
      .pipe(
        catchError(() => of([])),
        tap((apps)=> this.local.setAppsEvent(apps))
      );
  }

  postApp(app: any): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.post<resMessage>(`${api_url}/apps/`, app)
      .pipe(catchError(this.handleError));
  }

  appPut(app: any, id: number): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.put<resMessage>(`${api_url}/apps/${id}`, app)
      .pipe(catchError(this.handleError));
  }

  deleteApp(id: number): Observable<resMessage> {
    const api_url = this.local.getData('api_url');
    return this.http.delete<resMessage>(`${api_url}/apps/${id}`)
      .pipe(catchError(this.handleError));
  }

  getPowerProfile(): Observable<PowerProfiles[]>{
    const api_url = this.local.getData('api_url');
    return this.http.get<PowerProfiles[]>(`${api_url}/power_profiles`)
      .pipe(
        catchError(() => of([])),
        tap((pwrProfiles) => this.local.setPowerProfilesEvent(pwrProfiles))
      );
  }

  handleError(error: HttpErrorResponse): Observable<any> {
    if (!error.status) {
      //client / network error
      return throwError(() => Error(error.statusText))
    }
    else {
      //server error
      return throwError(() => Error(error.error.message))
    }
  }
}
