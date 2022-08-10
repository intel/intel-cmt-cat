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
import {
  AfterContentChecked,
  AfterContentInit,
  AfterViewInit,
  ChangeDetectionStrategy,
  Component,
  DoCheck,
  OnDestroy,
  OnInit,
  ViewChild,
  ViewEncapsulation,
} from '@angular/core';
import { MatButtonToggleChange } from '@angular/material/button-toggle';
import { MatSlideToggleChange } from '@angular/material/slide-toggle';
import { Router } from '@angular/router';
import {
  catchError,
  combineLatest,
  EMPTY,
  map,
  Observable,
  Subscription,
  tap,
} from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { L3catComponent } from './l3cat/l3cat.component';
import { MbaComponent } from './mba/mba.component';
import { SstcpComponent } from './sstcp/sstcp.component';

import {
  CacheAllocation,
  Caps,
  MBA,
  MBACTRL,
  RDTIface,
  resMessage,
  SSTBF,
} from './system-caps.model';

@Component({
  selector: 'app-system-caps',
  templateUrl: './system-caps.component.html',
  styleUrls: ['./system-caps.component.scss'],
  encapsulation: ViewEncapsulation.None,
})

/* Component used to show System Capabilities and capability details*/
export class SystemCapsComponent implements OnInit {
  caps!: string[];
  loading: boolean = false;
  interface!: string;
  mba!: MBACTRL & MBA;
  rdtIface!: RDTIface;
  sstbf!: SSTBF;
  l3cat!: CacheAllocation;
  l2cat!: CacheAllocation;

  constructor(
    private service: AppqosService,
    private snackBar: SnackBarService
  ) {}

  ngOnInit(): void {
    this.loading = true;

    this._getRdtIface();
    this._getCaps();
  }

  private _getCaps(): void {
    this.service.getCaps().subscribe({
      next: (caps) => {
        this.caps = caps.capabilities;
        this._getMbaData();
        this._getSstbf();
        this._getL3cat();
        this._getL2cat();
        this.loading = false;
      },
      error: (error: HttpErrorResponse) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      },
    });
  }

  onChangeIface(event: MatButtonToggleChange) {
    this.loading = true;
    this.service.rdtIfacePut(event.value).subscribe({
      next: (res: resMessage) => {
        this.snackBar.displayInfo(res.message);
        this._getCaps();
        this._getRdtIface();
        this.loading = false;
      },
      error: (error: HttpErrorResponse) => {
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
      error: (error: HttpErrorResponse) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      },
    });
  }

  mbaOnChange(event: MatSlideToggleChange) {
    this.loading = true;
    this.service.mbaCtrlPut(event.checked).subscribe({
      next: (res: resMessage) => {
        this.snackBar.displayInfo(res.message);
        this._getMbaData();
        this.loading = false;
      },
      error: (error: HttpErrorResponse) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      },
    });
  }

  private _getMbaData() {
    if (!this.caps.includes('mba')) return;

    combineLatest([this.service.getMba(), this.service.getMbaCtrl()]).subscribe(
      {
        next: ([mba, mbaCtrl]) => (this.mba = { ...mba, ...mbaCtrl }),
        error: (error: HttpErrorResponse) => {
          this.snackBar.handleError(error.message);
          this.loading = false;
        },
      }
    );
  }

  private _getRdtIface(): void {
    this.service.getRdtIface().subscribe({
      next: (rdtIface) => (this.rdtIface = rdtIface),
      error: (error: HttpErrorResponse) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      },
    });
  }

  private _getSstbf(): void {
    if (!this.caps.includes('sstbf')) return;

    this.service.getSstbf().subscribe({
      next: (sstbf) => (this.sstbf = sstbf),
      error: (error: HttpErrorResponse) => {
        this.snackBar.handleError(error.message);
        this.loading = false;
      },
    });
  }

  private _getL3cat(): void {
    if (!this.caps.includes('l3cat')) return;

    this.service
      .getL3cat()
      .pipe(
        map((cat: CacheAllocation) => ({
          ...cat,
          cache_size: Math.round((cat.cache_size / 1024 ** 2) * 100) / 100,
          cw_size: Math.round((cat.cw_size / 1024 ** 2) * 100) / 100,
        }))
      )
      .subscribe({
        next: (l3cat) => (this.l3cat = l3cat),
        error: (error: HttpErrorResponse) => {
          this.snackBar.handleError(error.message);
          this.loading = false;
        },
      });
  }

  private _getL2cat(): void {
    if (!this.caps.includes('l2cat')) return;

    this.service
      .getL2cat()
      .pipe(
        map((cat: CacheAllocation) => ({
          ...cat,
          cache_size: Math.round((cat.cache_size / 1024 ** 2) * 100) / 100,
          cw_size: Math.round((cat.cw_size / 1024 ** 2) * 100) / 100,
        }))
      )
      .subscribe({
        next: (l2cat) => (this.l2cat = l2cat),
        error: (error: HttpErrorResponse) => {
          this.snackBar.handleError(error.message);
          this.loading = false;
        },
      });
  }
}
