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

import { Component } from '@angular/core';
import { AppqosService } from 'src/app/services/appqos.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { Router } from '@angular/router';
import { MatDialog } from '@angular/material/dialog';
import { ErrorDialogComponent } from '../error-dialog/error-dialog.component';
import { forkJoin, of } from 'rxjs';

@Component({
  selector: 'app-dashboard-page',
  templateUrl: './dashboard-page.component.html',
  styleUrls: ['./dashboard-page.component.scss'],
})

/* Component used to display Dashboard page*/
export class DashboardPageComponent {
  showContent = true;

  constructor(
    private service: AppqosService,
    public dialog: MatDialog,
    private snackBar: SnackBarService,
    private router: Router
  ) {
    this._getCaps();
  }

  switchContent(event: boolean): void {
    this.showContent = event;
  }

  private _getCaps() {
    this.service.getCaps().subscribe(
      {
        next: (caps) => {
          forkJoin({
            pools: this.service.getPools(),
            l3cat: (caps.capabilities.includes('l3cat'))
              ? this.service.getL3cat() : of([]),
            l2cat: (caps.capabilities.includes('l2cat'))
              ? this.service.getL2cat() : of([]),
            mbaCtrl: (caps.capabilities.includes('mba'))
              ? this.service.getMbaCtrl() : of([])
          }).subscribe((data) => {
            this._checkPools(data, caps.capabilities);
          });

          this._getRdtIface();
          this._getApps();

          if (caps.capabilities.includes('sstbf'))
            this._getSstbf();

          if (caps.capabilities.includes('mba')) {
            this._getMba();
          }

          if (caps.capabilities.includes('power'))
            this._getPwrProfiles();

        },
        error: (error: Error) => {
          this.snackBar.handleError(error.message);
          this.router.navigate(['/login']);
        }
      });
  }

  private _getMba() {
    this.service.getMba().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    });
  }

  private _getSstbf() {
    this.service.getSstbf().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    });
  }

  private _getRdtIface() {
    this.service.getRdtIface().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    });
  }

  private _getApps() {
    this.service.getApps().subscribe();
  }

  private _getPwrProfiles() {
    this.service.getPowerProfile().subscribe();
  }

  private _checkPools(data: any, caps: string[]) {

    const fields = ['name'];

    if (caps.includes('l3cat')) {
      if (data.l3cat.cdp_enabled) {
        fields.push('l3cbm_code', 'l3cbm_data');
      } else {
        fields.push('l3cbm');
      }
    }

    if (caps.includes('l2cat')) {
      if (data.l2cat.cdp_enabled) {
        fields.push('l2cbm_code', 'l2cbm_data');
      } else {
        fields.push('l2cbm');
      }
    }

    if (caps.includes('mba')) {
      if (data.mbaCtrl?.enabled) {
        fields.push('mba_bw');
      } else {
        fields.push('mba');
      }
    }

    const config: { id: number, fields: string[] }[] = [];

    data.pools.forEach((pool: any) => {
      const obj: { id: number, fields: string[] } = {
        id: pool.id,
        fields: []
      };
      fields.forEach((field) => {
        if (!(field in pool)) obj.fields.push(field);
      });

      if (obj.fields.length !== 0) config.push(obj);
    });

    if (config.length === 0) return;

    this.dialog.open(ErrorDialogComponent, {
      height: 'auto',
      width: '26rem',
      data: {
        pools: config
      }
    });

    this.router.navigate(['/login']);
  }
}
