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
  Component,
  EventEmitter,
  Input,
  OnChanges,
  OnInit,
  Output
} from '@angular/core';
import { MatDialog } from '@angular/material/dialog';

import { CacheAllocation } from '../../system-caps/system-caps.model';
import { EditDialogComponent } from '../edit-dialog/edit-dialog.component';
import { Pools } from '../overview.model';
import { LocalService } from 'src/app/services/local.service';
import { Subscription } from 'rxjs';
import { AutoUnsubscribe } from 'src/app/services/decorators';

@Component({
  selector: 'app-l3-cache-allocation',
  templateUrl: './l3-cache-allocation.component.html',
  styleUrls: ['./l3-cache-allocation.component.scss'],
})
@AutoUnsubscribe
export class L3CacheAllocationComponent implements OnInit, OnChanges {
  @Input() available?: boolean;
  @Input() pools!: Pools[];
  @Output() poolEvent = new EventEmitter<unknown>();
  poolsList!: Pools[];
  l3cat!: CacheAllocation | null;
  l3catSub!: Subscription;

  constructor(
    public dialog: MatDialog,
    private localService: LocalService
  ) { }

  ngOnInit(): void {
    this.l3catSub = this.localService.getL3CatEvent().subscribe((l3cat) => {
      this.l3cat = l3cat;
      this._convertToBitmask();
    });
  }

  ngOnChanges(): void {
    this._convertToBitmask();
  }

  private _convertToBitmask() {
    if (!this.l3cat) return;
    this.poolsList = this.pools.map((pool: Pools) => (
      (this.l3cat?.cdp_enabled) ? {
        ...pool,
        l3BitmaskCode: this.localService.convertToBitmask(pool.l3cbm_code, this.l3cat!.cw_num),
        l3BitmaskData: this.localService.convertToBitmask(pool.l3cbm_data, this.l3cat!.cw_num),
      } : {
        ...pool,
        l3Bitmask: this.localService.convertToBitmask(pool.l3cbm, this.l3cat!.cw_num),
      }
    ));
  }

  openDialog() {
    const dialogRef = this.dialog.open(EditDialogComponent, {
      height: 'auto',
      width: '50rem',
      data: {
        l3cbm: true,
        l3cdp: this.l3cat!.cdp_enabled,
        numCacheWays: this.l3cat!.cw_num
      },
    });

    dialogRef.afterClosed().subscribe(() => {
      this.poolEvent.emit();
    });
  }
}
