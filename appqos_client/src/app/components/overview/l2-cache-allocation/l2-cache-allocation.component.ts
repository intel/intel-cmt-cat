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
  Output,
  SimpleChanges,
} from '@angular/core';
import { MatDialog } from '@angular/material/dialog';

import { AppqosService } from 'src/app/services/appqos.service';
import { EditDialogComponent } from '../edit-dialog/edit-dialog.component';
import { Pools } from '../overview.model';

@Component({
  selector: 'app-l2-cache-allocation',
  templateUrl: './l2-cache-allocation.component.html',
  styleUrls: ['./l2-cache-allocation.component.scss'],
})
export class L2CacheAllocationComponent implements OnChanges {
  @Input() pools!: Pools[];
  @Output() poolEvent = new EventEmitter<unknown>();
  poolsList!: Pools[];
  numCacheWays!: number;

  constructor(public dialog: MatDialog, private service: AppqosService) {}

  ngOnChanges(changes: SimpleChanges): void {
    this.service.getL2cat().subscribe((l2cat) => {
      this.numCacheWays = l2cat.cw_num;
      this._convertToBitmask();
    });
  }

  private _convertToBitmask() {
    this.poolsList = this.pools.map((pool: Pools) => ({
      ...pool,
      l2Bitmask: pool.l2cbm
        ?.toString(2)
        .padStart(this.numCacheWays, '0')
        .split('')
        .map(Number),
    }));
  }

  openDialog() {
    const dialogRef = this.dialog.open(EditDialogComponent, {
      height: 'auto',
      width: '50rem',
      data: { l2cbm: true, numCacheWays: this.numCacheWays },
    });

    dialogRef.afterClosed().subscribe((_) => {
      this.poolEvent.emit();
    });
  }
}
