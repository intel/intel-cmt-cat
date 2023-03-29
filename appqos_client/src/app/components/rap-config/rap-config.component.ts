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
import { catchError, combineLatest, of, take } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { Apps, Pools } from '../overview/overview.model';

@Component({
  selector: 'app-rap-config',
  templateUrl: './rap-config.component.html',
  styleUrls: ['./rap-config.component.scss'],
})
export class RapConfigComponent implements OnInit {
  apps!: Apps[];
  pools!: Pools[];

  constructor(
    private service: AppqosService,
    private localService: LocalService
  ) {}

  ngOnInit(): void {
    this.getConfigData();

    this.localService.getIfaceEvent().subscribe(() => this.getConfigData());
    this.localService.getL3CatEvent().subscribe(() => this.getConfigData());
  }

  getConfigData(): void {
    const pools$ = this.service.getPools().pipe(
      take(1),
      catchError(() => of([]))
    );

    const apps$ = this.service.getApps().pipe(
      take(1),
      catchError(() => of([]))
    );

    combineLatest([pools$, apps$]).subscribe(([pools, apps]) => {
      this.pools = pools;
      this.apps = apps;
    });
  }
}
