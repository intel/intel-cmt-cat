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
import { MatSlideToggleChange } from '@angular/material/slide-toggle';

import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { SystemTopology, MBACTRL, resMessage, PowerProfiles, SSTBF } from '../system-caps/system-caps.model';
import { Pools } from './overview.model';
import { AutoUnsubscribe } from 'src/app/services/decorators';
import { Subscription } from 'rxjs';

@Component({
  selector: 'app-overview',
  templateUrl: './overview.component.html',
  styleUrls: ['./overview.component.scss'],
})
@AutoUnsubscribe
export class OverviewComponent implements OnInit {
  pools!: Pools[];
  mbaCtrl!: MBACTRL | null;
  caps!: string[];
  topology!: SystemTopology | null;
  pwrProfiles!: PowerProfiles[];
  sstbf!: SSTBF | null;
  subs: Subscription[] = [];
  hideUnsupported = false;
  rdtSupported = false;
  sstSupported = false;

  constructor(
    private service: AppqosService,
    private snackBar: SnackBarService,
    private localService: LocalService
  ) { }

  ngOnInit(): void {
    this.getTopology();

    const capsSub = this.localService.getCapsEvent().subscribe((caps) => {
      this.caps = caps;
      this.rdtSupported = ['l3cat', 'l2cat', 'mba'].some((feat) => this.caps.includes(feat));
      this.sstSupported = ['sstbf', 'power'].some((feat) => this.caps.includes(feat));
    });

    const mbaCtrlSub = this.localService.getMbaCtrlEvent().subscribe((mbaCtrl) => {
      this.mbaCtrl = mbaCtrl;
    });

    const poolsSub = this.localService.getPoolsEvent().subscribe((pools) => {
      this.pools = pools;
    });

    const pwrProfilesSub = this.localService.getPowerProfilesEvent().subscribe((pwrProfiles) => {
      this.pwrProfiles = pwrProfiles;
    });

    const sstbfSub = this.localService.getSstbfEvent().subscribe((sstbf) => {
      this.sstbf = sstbf;
    });

    this.subs.push(capsSub, mbaCtrlSub, poolsSub, pwrProfilesSub, sstbfSub);
  }

  getMbaCtrl(): void {
    this.service.getMbaCtrl().subscribe((mbaCtrl: MBACTRL) => {
      this.mbaCtrl = mbaCtrl;
    });
  }

  getSstbf(): void {
    this.service.getSstbf().subscribe({
      next: (sstbf: SSTBF) => {
        this.sstbf = sstbf;
      },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    });
  }

  getTopology(): void {
    this.service.getSystemTopology().subscribe((topo: SystemTopology) => {
      this.topology = topo;
    });
  }

  mbaOnChange(event: MatSlideToggleChange) {
    this.service.mbaCtrlPut(event.checked).subscribe({
      next: (res: resMessage) => {
        this.snackBar.displayInfo(res.message);
        this.getMbaCtrl();
        this._getPools();
      },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      },
    });
  }

  sstbfOnChange(event: MatSlideToggleChange) {
    this.service.sstbfPut(event.checked).subscribe({
      next: (res: resMessage) => {
        this.snackBar.displayInfo(res.message);
        this.getSstbf();
      },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      },
    });
  }

  private _getPools() {
    this.service.getPools().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    });
  }

  hideUnsupportedEvent(event: MatSlideToggleChange) {
    event.source.checked = this.hideUnsupported;
    this.hideUnsupported = !this.hideUnsupported;
  }
}
