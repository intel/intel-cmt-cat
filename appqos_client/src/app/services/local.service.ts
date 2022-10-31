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

import { Injectable } from '@angular/core';
import { BehaviorSubject, Observable, Subject } from 'rxjs';

import { CacheAllocation } from '../components/system-caps/system-caps.model';

@Injectable({
  providedIn: 'root',
})

/* Service used to store data to localStorage */
export class LocalService {
  ifaceEvent = new Subject<void>();
  l3cat = new BehaviorSubject<CacheAllocation | null>(null);
  l2cat = new BehaviorSubject<CacheAllocation | null>(null);

  setIfaceEvent(): void {
    this.ifaceEvent.next();
  }

  getIfaceEvent(): Observable<void> {
    return this.ifaceEvent.asObservable();
  }

  setL3CatEvent(l3cat: CacheAllocation) {
    this.l3cat.next(l3cat);
  }

  getL3CatEvent(): Observable<CacheAllocation | null> {
    return this.l3cat.asObservable();
  }

  setL2CatEvent(l2cat: CacheAllocation) {
    this.l2cat.next(l2cat);
  }

  getL2CatEvent(): Observable<CacheAllocation | null> {
    return this.l2cat.asObservable();
  }

  public saveData(key: string, value: string): void {
    window.localStorage.setItem(key, value);
  }

  public getData(key: string): string | null {
    return window.localStorage.getItem(key);
  }

  public clearData(): void {
    window.localStorage.clear();
  }

  /**
   * Check if app is in "logged in" state
   */
  public isLoggedIn(): boolean {
    if (window.localStorage.getItem('api_url') === null) {
      return false;
    }
    return true;
  }
}
