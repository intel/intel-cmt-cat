/*BSD LICENSE

Copyright(c) 2023 Intel Corporation. All rights reserved.

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
  OnInit,
  Input,
  OnChanges,
  SimpleChanges,
  ChangeDetectionStrategy,
} from '@angular/core';
import { Node } from '../../system-caps/system-caps.model';

const DEFAULT_COLS = 8;
const ROW_HEIGHT_SMALL = '20px';
const ROW_HEIGHT_LARGE = '30px';

@Component({
  selector: 'app-node',
  templateUrl: './node.component.html',
  styleUrls: ['./node.component.scss'],
  changeDetection: ChangeDetectionStrategy.OnPush,
})
export class NodeComponent implements OnInit, OnChanges {
  @Input() node!: Node;
  @Input() detailedView!: boolean;
  L2IDS: number[] = [];
  L3IDS: number[] = [];
  // set default row/col values
  numCols: number = DEFAULT_COLS;
  rowHeight: string = ROW_HEIGHT_SMALL;

  ngOnInit(): void {
    if (!this.node.cores) return;

    // get list of L2 cache ID's
    this.node.cores.forEach((core) => {
      if (this.L2IDS.find((l2ID) => l2ID === core.L2ID) === undefined) {
        this.L2IDS.push(core.L2ID);
      }
    });

    // get list of L3 cache ID's
    for (let i = 0; i < this.node.cores.length; i++) {
      const core = this.node.cores[i];

      if (core.L3ID === undefined) continue;

      if (this.L3IDS.find((L3ID) => L3ID === core.L3ID) === undefined) {
        this.L3IDS.push(core.L3ID);
      }
    }

    // calculate columns
    this.numCols = this.getNumCols();
  }

  ngOnChanges(changes: SimpleChanges): void {
    if (changes['detailedView'].currentValue) {
      this.rowHeight = ROW_HEIGHT_LARGE;
    } else {
      this.rowHeight = ROW_HEIGHT_SMALL;
    }
  }

  /*
   * determine number of colums that results
   * in all rows and columns occupied
   */
  getNumCols(): number {
    const numL2IDS = this.L2IDS.length;
    const numCores = this.node.cores?.length;
    let maxCols = DEFAULT_COLS;

    // increase columns when single thread per core
    if (numL2IDS === numCores) maxCols = maxCols * 2;

    // just single row of cores
    if (numL2IDS < maxCols) {
      return numL2IDS;
    }

    // calculate number of colums
    for (let i = maxCols; i > 0; i--) {
      if (numL2IDS % i === 0) {
        return i;
      }
    }
    return DEFAULT_COLS;
  }
}
