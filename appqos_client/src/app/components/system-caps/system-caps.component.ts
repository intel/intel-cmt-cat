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

import { Component, OnInit, ViewEncapsulation } from '@angular/core';
import { MatButtonToggleChange } from '@angular/material/button-toggle';
import { MatSlideToggleChange } from '@angular/material/slide-toggle';
import { combineLatest, Subscription } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';

import {
  CacheAllocation,
  MBA,
  MBACTRL,
  RDTIface,
  resMessage,
  SSTBF,
} from './system-caps.model';
import { AutoUnsubscribe } from 'src/app/services/decorators';

@Component({
  selector: 'app-system-caps',
  templateUrl: './system-caps.component.html',
  styleUrls: ['./system-caps.component.scss'],
  encapsulation: ViewEncapsulation.None,
})
@AutoUnsubscribe
/* Component used to show System Capabilities and capability details*/
export class SystemCapsComponent implements OnInit {
  caps!: string[] | null;
  loading = false;
  mba!: MBA & MBACTRL;
  mbaCtrl!: MBACTRL | null;
  rdtIface!: RDTIface | null;
  sstbf!: SSTBF | null;
  l3cat!: CacheAllocation | null;
  l2cat!: CacheAllocation | null;
  systemName: string | undefined;

  subs: Subscription[] = [];

  constructor(
    private service: AppqosService,
    private snackBar: SnackBarService,
    private localService: LocalService
  ) { }

  ngOnInit(): void {
    this.loading = true;

    this.systemName = this.localService
      .getData('api_url')
      ?.split('/')
      .pop()
      ?.split(':')
      .shift();

    const capsSub = this.localService.getCapsEvent().subscribe((caps) => {
      this.caps = caps;
    });

    const l3catSub = this.localService.getL3CatEvent().subscribe((l3cat) => {
      this.l3cat = l3cat;
    });

    const l2catSub = this.localService.getL2CatEvent().subscribe((l2cat) => {
      this.l2cat = l2cat;
    });

    const sstbfSub = this.localService.getSstbfEvent().subscribe((sstbf) => {
      this.sstbf = sstbf;
    });

    const rdtIfaceSub = this.localService.getRdtIfaceEvent().subscribe((rdtIFace) => {
      this.rdtIface = rdtIFace;
    });

    const mbaSub = combineLatest([
      this.localService.getMbaEvent(),
      this.localService.getMbaCtrlEvent()
    ]).subscribe(([mba, mbaCtrl]) => {
      this.mba = { ...mba, ...mbaCtrl } as MBA & MBACTRL;
    });

    this.loading = false;

    this.subs.push(capsSub, l3catSub, l2catSub, sstbfSub, rdtIfaceSub, mbaSub);
  }

  onChangeIface(event: MatButtonToggleChange) {
    this.loading = true;
    this.service.rdtIfacePut(event.value).subscribe({
      next: (res: resMessage) => {
        this.service.getCaps().subscribe(() => {
          this._getMbaData();
          this._getSstbf();
          this._getL3cat();
          this._getL2cat();
          this._getRdtIface();
        });
        this.snackBar.displayInfo(res.message);
        this.loading = false;
      },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      },
    });
  }

  sstbfOnChange(event: MatSlideToggleChange) {
    this.loading = true;
    this.service.sstbfPut(event.checked).subscribe({
      next: (res: resMessage) => {
        this.snackBar.displayInfo(res.message);
        this._getSstbf();
        this.loading = false;
      },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      },
    });
  }

  l3CdpOnChange(event: MatSlideToggleChange) {
    this.loading = true;
    this.service.l3CdpPut(event.checked).subscribe({
      next: (res: resMessage) => {
        this.snackBar.displayInfo(res.message);
        this._getL3cat();
        this._getPools();
        this.loading = false;
      },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      }
    });
  }

  l2CdpOnChange(event: MatSlideToggleChange) {
    this.loading = true;
    this.service.l2CdpPut(event.checked).subscribe({
      next: (res: resMessage) => {
        this.snackBar.displayInfo(res.message);
        this._getL2cat();
        this._getPools();
        this.loading = false;
      },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      }
    });
  }

  private _getMbaData() {
    if (!this.caps!.includes('mba')) return;

    combineLatest([this.service.getMba(), this.service.getMbaCtrl()]).subscribe(
      {
        next: ([mba, mbaCtrl]) => (this.mba = { ...mba, ...mbaCtrl }),
        error: (error: Error) => {
          this.snackBar.handleError(error.message);
          this.loading = false;
        },
      }
    );
  }

  private _getRdtIface(): void {
    this.service.getRdtIface().subscribe({
      next: (rdtIface) => {
        this.rdtIface = rdtIface;
        this._getPools();
        this.loading = false;
      },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      },
    });
  }

  private _getSstbf(): void {
    if (!this.caps!.includes('sstbf')) return;

    this.service.getSstbf().subscribe({
      next: (sstbf) => { this.sstbf = sstbf; },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      },
    });
  }

  private _getL3cat(): void {
    if (!this.caps!.includes('l3cat')) return;

    this.service.getL3cat().subscribe({
      next: (l3cat) => { this.l3cat = l3cat; },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      },
    });
  }

  private _getL2cat(): void {
    if (!this.caps!.includes('l2cat')) return;

    this.service.getL2cat().subscribe({
      next: (l2cat) => { this.l2cat = l2cat; },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
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
}
