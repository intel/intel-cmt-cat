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
  caps = new BehaviorSubject<string[] | null>(null);
  mbaCtrl = new BehaviorSubject<boolean | null>(null);

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

  setCapsEvent(caps: string[]) {
    this.caps.next(caps);
  }

  getCapsEvent(): Observable<string[] | null> {
    return this.caps.asObservable();
  }

  setMbaCtrlEvent(mbaCtrl: boolean) {
    this.mbaCtrl.next(mbaCtrl);
  }

  getMbaCtrlEvent(): Observable<boolean | null> {
    return this.mbaCtrl.asObservable();
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

  /**
   * Parse and sort string of numbers
   *
   * @param {string} numbers String of comma separated numbers where each
   * value should be a single number or a range of numbers e.g. 0,1-5
   *
   * @returns {number[]} List of sorted unique numbers
   */
  public parseNumberList(numbers: string): number[] {
    const numStrs = numbers.split(',');
    let numberList: number[] = [];

    if (!numStrs.length) {
      return numberList;
    }

    // convert string to list of numbers
    numStrs.forEach((numStr: string) => {
      if (numStr.includes('-')) {
        const range = numStr.split('-').map(Number);
        let start = range[0];
        let end = range[1];

        // swap if needed to start from the smallest number
        if (start > end) [start, end] = [end, start];

        for (let i = start; i <= end; i++) {
          numberList.push(i);
        }
      } else {
        numberList.push(Number(numStr));
      }
    })

    // sort list and return unique set of numbers
    numberList.sort((a, b) => a - b);

    return [ ...new Set(numberList)]
  }

  public getPidsDash(pids: string): number[] {
    const splitedPids = pids.split(/[,-]/).map(Number);
    let start = 0;
    let end = 0;
    const pidsList = [];

    if (pids.indexOf('-') > pids.indexOf(',')) {
      start = splitedPids[splitedPids.length - 2];
      end = splitedPids[splitedPids.length - 1];
      splitedPids.splice(splitedPids.length - 2, 2);
    } else {
      start = splitedPids[0];
      end = splitedPids[1];
      splitedPids.splice(0, 2);
    }

    if (start > end) [start, end] = [end, start];

    for (let i = start; i <= end; i++) {
      pidsList.push(i);
    }

    return [...pidsList, ...splitedPids];
  }
}
