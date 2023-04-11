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

import { AppqosService } from 'src/app/services/appqos.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { Pools } from '../overview.model';
import { LocalService } from 'src/app/services/local.service';

type dialogDataType = {
  mba?: boolean;
  l2cbm?: boolean;
  l2cdp?: boolean;
  l3cbm?: boolean;
  l3cdp?: boolean;
  numCacheWays: number;
};

type Pool = Pools & {
  isValid?: boolean;
};

@Component({
  selector: 'app-edit-dialog',
  templateUrl: './edit-dialog.component.html',
  styleUrls: ['./edit-dialog.component.scss'],
})
export class EditDialogComponent implements AfterContentInit {
  pools!: Pool[];
  loading = false;
  mbaBwDefNum = Math.pow(2, 32) - 1;

  constructor(
    @Inject(MAT_DIALOG_DATA) public data: dialogDataType,
    private service: AppqosService,
    private snackBar: SnackBarService,
    private localservice: LocalService
  ) { }

  ngAfterContentInit(): void {
    this._getPools();
  }

  onChangeL3CBM(value: number, poolIndex: number, bitIndex: number) {
    this.pools[poolIndex].l3Bitmask![bitIndex] = Number(!value);
  }

  onChangeL3CdpCode(value: number, poolIndex: number, bitIndex: number) {
    this.pools[poolIndex].l3BitmaskCode![bitIndex] = Number(!value);
  }

  onChangeL3CdpData(value: number, poolIndex: number, bitIndex: number) {
    this.pools[poolIndex].l3BitmaskData![bitIndex] = Number(!value);
  }

  onChangeL2CBM(value: number, poolIndex: number, bitIndex: number) {
    this.pools[poolIndex].l2Bitmask![bitIndex] = Number(!value);
  }

  onChangeL2CdpCode(value: number, poolIndex: number, bitIndex: number) {
    this.pools[poolIndex].l2BitmaskCode![bitIndex] = Number(!value);
  }

  onChangeL2CdpData(value: number, poolIndex: number, bitIndex: number) {
    this.pools[poolIndex].l2BitmaskData![bitIndex] = Number(!value);
  }

  onChangeMbaBw(event: any, i: number) {
    if (event.target.value === '') {
      this.pools[i].isValid = true;
      return;
    }

    const mbaBw = Number(event.target.value);
    if (mbaBw < 1 || mbaBw > this.mbaBwDefNum) {
      this.pools[i].isValid = true;
    } else {
      this.pools[i]['mba_bw'] = event.target.value;
      this.pools[i].isValid = false;
    }
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
      return pools.map((pool: Pools) => (
        (this.data.l3cdp) ? {
          ...pool,
          l3BitmaskCode: this.localservice.convertToBitmask(pool.l3cbm_code, this.data.numCacheWays),
          l3BitmaskData: this.localservice.convertToBitmask(pool.l3cbm_data, this.data.numCacheWays),
        } : {
          ...pool,
          l3Bitmask: this.localservice.convertToBitmask(pool.l3cbm, this.data.numCacheWays),
        }
      ));
    } else if (this.data.l2cbm) {
      return pools.map((pool: Pools) => (
        (this.data.l2cdp) ? {
          ...pool,
          l2BitmaskCode: this.localservice.convertToBitmask(pool.l2cbm_code, this.data.numCacheWays),
          l2BitmaskData: this.localservice.convertToBitmask(pool.l2cbm_data, this.data.numCacheWays),
        } : {
          ...pool,
          l2Bitmask: this.localservice.convertToBitmask(pool.l2cbm, this.data.numCacheWays),
        }
      ));
    } else {
      return pools;
    }
  }

  saveL3CBM(i: number, id: number) {
    this.service
      .poolPut(
        (this.data.l3cdp) ?
          {
            l3cbm_code: parseInt(this.pools[i].l3BitmaskCode!.join(''), 2),
            l3cbm_data: parseInt(this.pools[i].l3BitmaskData!.join(''), 2),
          } :
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
          this.snackBar.handleError(error.message);
          this._getPools();
        },
      });
  }

  saveL2CBM(i: number, id: number) {
    this.service
      .poolPut(
        (this.data.l2cdp) ?
          {
            l2cbm_code: parseInt(this.pools[i].l2BitmaskCode!.join(''), 2),
            l2cbm_data: parseInt(this.pools[i].l2BitmaskData!.join(''), 2)
          } :
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
          this.snackBar.handleError(error.message);
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
          this.snackBar.handleError(error.message);
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
          this.snackBar.handleError(error.message);
          this._getPools();
        },
      });
  }
}
