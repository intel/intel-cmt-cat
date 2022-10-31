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

import { Component, OnInit } from '@angular/core';
import { MatOptionSelectionChange } from '@angular/material/core';
import { catchError, combineLatest, of, take } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { Apps, Pools } from '../../overview/overview.model';
import { CacheAllocation } from '../../system-caps/system-caps.model';

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
  poolApps!: any;
  l3numCacheWays!: number;
  mbaBwDefNum = 1 * Math.pow(2, 32) - 1;

  constructor(
    private service: AppqosService,
    private localService: LocalService
  ) {}

  ngOnInit(): void {
    this.getData();

    this.localService.getIfaceEvent().subscribe((_) => this.getData());
  }

  getData() {
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
      this.getPool(pools[0].id);
      this.selected = pools[0].name;
    });
  }

  selectedPool(event: MatOptionSelectionChange, id: number) {
    if (event.isUserInput) {
      this.getPool(id);
    }
  }

  getPool(id: number) {
    this.pool = this.pools.find((pool: Pools) => pool.id === id) as Pools;
    this.poolApps = this.apps.filter((app) => app.pool_id === id);

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
}
