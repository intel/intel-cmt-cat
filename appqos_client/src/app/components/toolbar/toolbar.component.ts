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
  Output,
  ViewEncapsulation,
} from '@angular/core';
import { Router } from '@angular/router';
import { first, forkJoin } from 'rxjs';
import { LocalService } from 'src/app/services/local.service';
import { MatDialog } from '@angular/material/dialog';
import { DisplayConfigComponent } from 'src/app/components/display-config/display-config.component';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { AppqosConfig } from '../system-caps/system-caps.model';

@Component({
  selector: 'app-toolbar',
  templateUrl: './toolbar.component.html',
  styleUrls: ['./toolbar.component.scss'],
  encapsulation: ViewEncapsulation.None,
})
export class ToolbarComponent {
  @Output() switcher = new EventEmitter<boolean>();
  enableOverview = true;

  constructor(private router: Router,
    private local: LocalService,
    private snackbar: SnackBarService,
    public dialog: MatDialog) { }

  logout(): void {
    this.router.navigate(['/login']);
  }

  switchContent(value: boolean): void {
    if (!value) {
      this.enableOverview = false;
    } else {
      this.enableOverview = true;
    }
    this.switcher.emit(value);
  }

  showConfig() {
    const config: AppqosConfig = {
      rdt_iface: {"interface": "os"},
      apps: [],
      pools: [],
    };

    forkJoin({
      rdt_iface: this.local.getRdtIfaceEvent().pipe(first()),
      mba_ctrl: this.local.getMbaCtrlEvent().pipe(first()),
      apps: this.local.getAppsEvent().pipe(first()),
      pools: this.local.getPoolsEvent().pipe(first()),
      power_profiles: this.local.getPowerProfilesEvent().pipe(first()),
      sstbf: this.local.getSstbfEvent().pipe(first()),
      caps: this.local.getCapsEvent().pipe(first())
    }).subscribe({
      next: result => {
        const caps: string[] = result.caps;

        // set required fields
        config.rdt_iface.interface = result.rdt_iface!.interface;
        config.pools = result.pools;
        config.apps = result.apps;

        // set power profile settings if supported
        if (caps.includes('power')) {
          if (result.power_profiles.length) {
            config.power_profiles = result.power_profiles;
          }
          config.power_profiles_expert_mode = true;
          config.power_profiles_verify = true;
        }

        // set sst-bf setting if supported
        if (caps.includes('sstbf')) {
          if (result.sstbf) {
            config.sstbf = { "configured": result.sstbf.configured };
          }
        }

        // set mba ctrl setting if mba supported
        if (caps.includes('mba')) {
          if (result.mba_ctrl) {
            config.mba_ctrl = { "enabled": result.mba_ctrl.enabled };
          }
        }

        this.openDialog(JSON.stringify(config, null, 2));
      },
      error: () => this.snackbar.handleError('Failed to generate configuration')
    });
  }

  openDialog(config: string): void {
    this.dialog.open(DisplayConfigComponent, {
      height: 'auto',
      width: '40rem',
      data: { config: config },
    });
  }
}
