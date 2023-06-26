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

import { Component, Input, OnInit } from '@angular/core';
import { LocalService } from 'src/app/services/local.service';
import { Node, SystemTopology, SSTBF } from '../system-caps/system-caps.model';
import { MatSlideToggleChange } from '@angular/material/slide-toggle';
import { Subscription } from 'rxjs';
import { AutoUnsubscribe } from 'src/app/services/decorators';

@Component({
  selector: 'app-system-topology',
  templateUrl: './system-topology.component.html',
  styleUrls: ['./system-topology.component.scss'],
})
@AutoUnsubscribe
export class SystemTopologyComponent implements OnInit {
  @Input() systemTopology!: SystemTopology;
  nodes: Node[] = [];
  detailedView = false;
  sstbf!: SSTBF | null;
  sstbfSub!: Subscription;

  constructor(private local: LocalService) { }

  ngOnInit(): void {
    this.sstbfSub = this.local.getSstbfEvent().subscribe({
      next: (sstbf) => {
        this.sstbf = sstbf;
        this.setNodeInfo();
      },
    });
  }

  /*
   * parse system topology and populate node list
   */
  setNodeInfo() {
    // get node ID's (currently socket ID's)
    this.systemTopology.core.forEach((core) => {
      if (!this.nodes.find((node) => node.nodeID === core.socket)) {
        this.nodes.push({ nodeID: core.socket });
      }
    });

    // populate node cores
    this.nodes.forEach((node) => {
      node.cores = this.systemTopology.core.filter(
        (core) => core.socket === node.nodeID
      );
      // set SST-BF high priority cores if configured
      if (this.sstbf?.configured) {
        node.cores.forEach((core) => {
          core.sstbfHP = !!this.sstbf!.hp_cores.find(
            (sstbfHPCore) => sstbfHPCore === core.lcore
          );
        });
      } else {
        node.cores?.forEach((core) => {
          core.sstbfHP = false;
        });
      }
    });
  }

  /*
   * toggle showing topology details
   */
  changeView(event: MatSlideToggleChange) {
    this.detailedView = !this.detailedView;
    event.source.checked = this.detailedView;
  }
}
