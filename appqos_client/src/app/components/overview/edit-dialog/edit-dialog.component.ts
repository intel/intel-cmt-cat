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

import { AfterContentInit, Component, Inject } from '@angular/core';
import { MAT_DIALOG_DATA } from '@angular/material/dialog';
import { MatSliderChange } from '@angular/material/slider';
import { map } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { Pools } from '../overview.model';

type dialogDataType = {
  mba?: boolean;
  l2cbm?: boolean;
  l3cbm?: boolean;
  numCacheWays: number;
};

@Component({
  selector: 'app-edit-dialog',
  templateUrl: './edit-dialog.component.html',
  styleUrls: ['./edit-dialog.component.scss'],
})
export class EditDialogComponent implements AfterContentInit {
  pools!: Pools[];
  loading = false;
  mbaBwDefNum = Math.pow(2, 32) - 1;

  constructor(
    @Inject(MAT_DIALOG_DATA) public data: dialogDataType,
    private service: AppqosService,
    private snackBar: SnackBarService
  ) {}

  ngAfterContentInit(): void {
    this._getPools();
  }

  onChangeL3CBM(value: number, i: number, j: number) {
    if (value) {
      this.pools[i].l3Bitmask![j] = 0;
    } else {
      this.pools[i].l3Bitmask![j] = 1;
    }
  }

  onChangeL2CBM(value: number, i: number, j: number) {
    if (value) {
      this.pools[i].l2Bitmask![j] = 0;
    } else {
      this.pools[i].l2Bitmask![j] = 1;
    }
  }

  onChangeMbaBw(event: any, i: number) {
    if (event.target.value === '') return;

    this.pools[i]['mba_bw'] = event.target.value;
  }

  onChangeMBA(event: MatSliderChange, i: number) {
    this.pools[i].mba = event.value!;
  }

  private _getPools(): void {
    this.loading = true;
    this.service.getPools().subscribe((pools: Pools[]) => {
      (this.pools = this._convertToBitmask(pools)), (this.loading = false);
    });
  }

  private _convertToBitmask(pools: Pools[]): Pools[] {
    if (this.data.l3cbm) {
      return pools.map((pool: Pools) => ({
        ...pool,
        l3Bitmask: pool.l3cbm
          ?.toString(2)
          .padStart(this.data.numCacheWays, '0')
          .split('')
          .map(Number),
      }));
    } else {
      return pools.map((pool: Pools) => ({
        ...pool,
        l2Bitmask: pool.l2cbm
          ?.toString(2)
          .padStart(this.data.numCacheWays, '0')
          .split('')
          .map(Number),
      }));
    }
  }

  saveL3CBM(i: number, id: number) {
    this.service
      .poolPut(
        {
          l3cbm: parseInt(this.pools[i].l3Bitmask!.join(''), 2),
        },
        id
      )
      .subscribe({
        next: (response) => {
          this.snackBar.displayInfo(response.message);
          this._getPools();
        },
        error: (error) => {
          this.snackBar.handleError(error.error.message);
          this._getPools();
        },
      });
  }

  saveL2CBM(i: number, id: number) {
    this.service
      .poolPut(
        {
          l2cbm: parseInt(this.pools[i].l2Bitmask!.join(''), 2),
        },
        id
      )
      .subscribe({
        next: (response) => {
          this.snackBar.displayInfo(response.message);
          this._getPools();
        },
        error: (error) => {
          this.snackBar.handleError(error.error.message);
          this._getPools();
        },
      });
  }

  saveMBA(i: number, id: number) {
    this.service
      .poolPut(
        {
          mba: this.pools[i].mba,
        },
        id
      )
      .subscribe({
        next: (response) => {
          this.snackBar.displayInfo(response.message);
          this._getPools();
        },
        error: (error) => {
          this.snackBar.handleError(error.error.message);
          this._getPools();
        },
      });
  }

  updateMBABW(i: number, id: number) {
      this.setMBABW(Number(this.pools[i].mba_bw), id);
  }

  resetMBABW(id: number) {
      this.setMBABW(this.mbaBwDefNum, id);
  }

  setMBABW(bwMbps: number, id: number) {
    this.service
      .poolPut(
        {
          mba_bw: bwMbps,
        },
        id
      )
      .subscribe({
        next: (response) => {
          this.snackBar.displayInfo(response.message);
          this._getPools();
        },
        error: (error) => {
          this.snackBar.handleError(error.error.message);
          this._getPools();
        },
      });
  }
}
