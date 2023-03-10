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

import {
  ChangeDetectionStrategy,
  Component,
  EventEmitter,
  Input,
  OnChanges,
  Output,
} from '@angular/core';
import { FormControl, Validators } from '@angular/forms';
import { MatOptionSelectionChange } from '@angular/material/core';
import { MatDialog } from '@angular/material/dialog';
import { MatSliderChange } from '@angular/material/slider';

import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { Apps, Pools } from '../../overview/overview.model';
import {
  CacheAllocation,
  resMessage,
  Standards,
} from '../../system-caps/system-caps.model';
import { CoresEditDialogComponent } from './cores-edit-dialog/cores-edit-dialog.component';
import { PoolAddDialogComponent } from './pool-add-dialog/pool-add-dialog.component';

@Component({
  selector: 'app-pool-config',
  templateUrl: './pool-config.component.html',
  styleUrls: ['./pool-config.component.scss'],
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class PoolConfigComponent implements OnChanges {
  @Input() apps!: Apps[];
  @Input() pools!: Pools[];
  @Output() poolEvent = new EventEmitter<unknown>();

  selected!: string;
  pool!: Pools;
  poolApps!: Apps[];
  poolName!: string;
  poolId!: number | undefined;
  l3numCacheWays!: number;
  l2numCacheWays!: number;
  mbaBwDefNum = Math.pow(2, 32) - 1;
  mbaBwControl = new FormControl('', [
    Validators.required,
    Validators.min(1),
    Validators.max(this.mbaBwDefNum),
    Validators.pattern('^[0-9]+$'),
  ]);
  nameControl = new FormControl('', [
    Validators.pattern('^[0-9a-zA-Z]+[ -~]+$'),
    Validators.required,
    Validators.maxLength(Standards.MAX_CHARS),
  ]);

  constructor(
    private service: AppqosService,
    private localService: LocalService,
    private snackBar: SnackBarService,
    public dialog: MatDialog
  ) { }

  ngOnChanges(): void {
    this.getData(this.poolId);
  }

  getData(id?: number) {
    this.pools.map((pool) => pool.cores.sort((a, b) => a - b));
    const selectedPool = this.pools[this.pools.length - 1];

    if (id === undefined) {
      this.getPool(selectedPool.id);
      this.selected = selectedPool.name;
    } else {
      this.getPool(id);
    }
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
    this.mbaBwControl.setValue(
      this.pool.mba_bw === this.mbaBwDefNum ? '' : this.pool.mba_bw
    );
    this.nameControl.setValue(this.pool.name);

    if (this.pool.l3cbm) {
      this.localService
        .getL3CatEvent()
        .subscribe((l3cat: CacheAllocation | null) => {
          this.l3numCacheWays = l3cat!.cw_num;
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
          this.l2numCacheWays = l2cat!.cw_num;
          this.pool.l2Bitmask = this.pool.l2cbm
            ?.toString(2)
            .padStart(l2cat!.cw_num, '0')
            .split('')
            .map(Number);
        });
    }
  }

  savePoolName() {
    this.service
      .poolPut(
        {
          name: this.nameControl.value,
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

  onChangeL3CBM(value: number, i: number) {
    if (value) {
      this.pool.l3Bitmask![i] = 0;
    } else {
      this.pool.l3Bitmask![i] = 1;
    }
  }

  onChangeL2CBM(value: number, i: number) {
    if (value) {
      this.pool.l2Bitmask![i] = 0;
    } else {
      this.pool.l2Bitmask![i] = 1;
    }
  }

  onChangeMBA(event: MatSliderChange) {
    this.pool.mba = event.value!;
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

  saveMBABW(reset: boolean) {
    if (reset) this.mbaBwControl.markAsUntouched();

    this.service
      .poolPut(
        {
          mba_bw: reset ? this.mbaBwDefNum : Number(this.mbaBwControl.value),
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
    this.poolEvent.emit();
  }

  errorHandler(error: Error) {
    this.snackBar.handleError(error.message);
    this.poolEvent.emit();
  }

  coresEditDialog() {
    const dialogRef = this.dialog.open(CoresEditDialogComponent, {
      height: 'auto',
      width: '40rem',
      data: this.pool,
    });

    dialogRef.afterClosed().subscribe((result) => {
      if (result) this.poolEvent.emit();
    });
  }

  deletePool() {
    if (!this.pool) return;

    this.service.deletePool(this.pool.id).subscribe({
      next: (response) => {
        this.snackBar.displayInfo(response.message);
        this.poolId = undefined;
        this.poolEvent.emit();
      },
      error: (error) => {
        this.snackBar.handleError(error.message);
      },
    });
  }

  poolAddDialog() {
    const dialogRef = this.dialog.open(PoolAddDialogComponent, {
      height: 'auto',
      width: '40rem',
      data: { l2cwNum: this.l2numCacheWays, l3cwNum: this.l3numCacheWays },
    });

    dialogRef.afterClosed().subscribe((response) => {
      if (response.id) {
        this.poolId = response.id;
        this.poolEvent.emit();
      }
    });
  }
}
