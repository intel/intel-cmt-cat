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
import { MatSlideToggleChange } from '@angular/material/slide-toggle';

import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { Caps, MBACTRL, resMessage } from '../system-caps/system-caps.model';
import { Pools } from './overview.model';

@Component({
  selector: 'app-overview',
  templateUrl: './overview.component.html',
  styleUrls: ['./overview.component.scss'],
})
export class OverviewComponent implements OnInit {
  pools!: Pools[];
  mbaCtrl!: MBACTRL;
  caps!: string[];

  constructor(
    private service: AppqosService,
    private snackBar: SnackBarService,
    private localService: LocalService
  ) {}

  ngOnInit(): void {
    this.getMbaCtrl();
    this.getPools();
    this.getCaps();

    this.localService.getIfaceEvent().subscribe((_) => {
      this.getMbaCtrl();
      this.getPools();
    });
  }

  getPools(): void {
    this.service
      .getPools()
      .subscribe((pools: Pools[]) => (this.pools = pools.slice(0, 4)));
  }

  getCaps(): void {
    this.service
      .getCaps()
      .subscribe((caps: Caps) => (this.caps = caps.capabilities));
  }

  getMbaCtrl(): void {
    this.service.getMbaCtrl().subscribe((mbaCtrl: MBACTRL) => {
      this.mbaCtrl = mbaCtrl;
    });
  }

  mbaOnChange(event: MatSlideToggleChange) {
    this.service.mbaCtrlPut(event.checked).subscribe({
      next: (res: resMessage) => {
        this.snackBar.displayInfo(res.message);
        this.getMbaCtrl();
        this.getPools();
      },
      error: (error: HttpErrorResponse) => {
        this.snackBar.handleError(error.message);
      },
    });
  }
}
