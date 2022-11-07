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

import { HttpErrorResponse } from '@angular/common/http';
import { Component, OnInit } from '@angular/core';
import { MatOptionSelectionChange } from '@angular/material/core';
import { MatSliderChange } from '@angular/material/slider';
import { catchError, combineLatest, of, take } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { Apps, Pools } from '../../overview/overview.model';
import {
  CacheAllocation,
  resMessage,
} from '../../system-caps/system-caps.model';

@Component({
  selector: 'app-pool-config',
  templateUrl: './pool-config.component.html',
  styleUrls: ['./pool-config.component.scss'],
})
export class PoolConfigComponent implements OnInit {
  selected!: string;
  pools!: Pools[];
  pool!: Pools;
  apps!: Apps[];
  poolApps!: Apps[];
  poolName!: string;
  mbaBW!: string;
  poolId!: number;
  l3numCacheWays!: number;
  mbaBwDefNum = 1 * Math.pow(2, 32) - 1;

  constructor(
    private service: AppqosService,
    private localService: LocalService,
    private snackBar: SnackBarService
  ) {}

  ngOnInit(): void {
    this.getData();

    this.localService.getIfaceEvent().subscribe((_) => this.getData());
  }

  getData(id?: number) {
    const pools$ = this.service.getPools().pipe(
      take(1),
      catchError((_) => of([]))
    );

    const apps$ = this.service.getApps().pipe(
      take(1),
      catchError((_) => of([]))
    );

    combineLatest([pools$, apps$]).subscribe(([pools, apps]) => {
      pools.map((pool) => pool.cores.sort((a, b) => a - b));
      this.pools = pools;
      this.apps = apps;
      if (id === undefined) {
        this.getPool(pools[0].id);
        this.selected = pools[0].name;
      } else {
        this.getPool(this.pool.id);
      }
    });
  }

  selectedPool(event: MatOptionSelectionChange, id: number) {
    if (event.isUserInput) {
      this.getPool(id);
    }
  }

  getPool(id: number) {
    this.poolId = id;
    this.pool = this.pools.find((pool: Pools) => pool.id === id) as Pools;
    this.poolApps = this.apps.filter((app) => app.pool_id === id);
    this.selected = this.pool.name;

    if (this.pool.l3cbm) {
      this.localService
        .getL3CatEvent()
        .subscribe((l3cat: CacheAllocation | null) => {
          this.pool.l3Bitmask = this.pool.l3cbm
            ?.toString(2)
            .padStart(l3cat!.cw_num, '0')
            .split('')
            .map(Number);
        });
    }

    if (this.pool.l2cbm) {
      this.localService
        .getL2CatEvent()
        .subscribe((l2cat: CacheAllocation | null) => {
          this.pool.l2Bitmask = this.pool.l2cbm
            ?.toString(2)
            .padStart(l2cat!.cw_num, '0')
            .split('')
            .map(Number);
        });
    }
  }

  onChangePoolName(event: any) {
    if (event.target.value === '') {
      this.snackBar.handleError('Invalid Pool name!');
    }

    this.poolName = event.target.value;
  }

  savePoolName() {
    if (this.poolName === '') return;

    this.service
      .poolPut(
        {
          name: this.poolName,
        },
        this.pool.id
      )
      .subscribe({
        next: (response) => {
          this.nextHandler(response);
          this.poolName = '';
        },
        error: (error) => {
          this.errorHandler(error);
        },
      });
  }

  onChangeL3CBM(value: Number, i: number) {
    if (value) {
      this.pool.l3Bitmask![i] = 0;
    } else {
      this.pool.l3Bitmask![i] = 1;
    }
  }

  onChangeL2CBM(value: Number, i: number) {
    if (value) {
      this.pool.l2Bitmask![i] = 0;
    } else {
      this.pool.l2Bitmask![i] = 1;
    }
  }

  onChangeMBA(event: MatSliderChange) {
    this.pool.mba = event.value!;
  }

  onChangeMbaBw(event: any) {
    if (event.target.value === '') return;

    this.pool.mba_bw = event.target.value;
  }

  saveL2CBM() {
    this.service
      .poolPut(
        {
          l2cbm: parseInt(this.pool.l2Bitmask!.join(''), 2),
        },
        this.pool.id
      )
      .subscribe({
        next: (response) => {
          this.nextHandler(response);
        },
        error: (error) => {
          this.errorHandler(error);
        },
      });
  }

  saveL3CBM() {
    this.service
      .poolPut(
        {
          l3cbm: parseInt(this.pool.l3Bitmask!.join(''), 2),
        },
        this.pool.id
      )
      .subscribe({
        next: (response) => {
          this.nextHandler(response);
        },
        error: (error) => {
          this.errorHandler(error);
        },
      });
  }

  saveMBA() {
    this.service
      .poolPut(
        {
          mba: this.pool.mba,
        },
        this.pool.id
      )
      .subscribe({
        next: (response) => {
          this.nextHandler(response);
        },
        error: (error) => {
          this.errorHandler(error);
        },
      });
  }

  saveMBABW() {
    this.service
      .poolPut(
        {
          mba_bw: Number(this.pool.mba_bw),
        },
        this.pool.id
      )
      .subscribe({
        next: (response) => {
          this.nextHandler(response);
        },
        error: (error) => {
          this.errorHandler(error);
        },
      });
  }

  nextHandler(response: resMessage) {
    this.snackBar.displayInfo(response.message);
    this.getData(this.pool.id);
  }

  errorHandler(error: HttpErrorResponse) {
    this.snackBar.handleError(error.error.message);
    this.getData(this.pool.id);
  }
}
