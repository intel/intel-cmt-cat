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

import { Component, Input, OnChanges } from '@angular/core';
import { PowerProfiles, eppDisplayStr, eppPostStr, resMessage } from '../system-caps/system-caps.model';
import { MatDialog } from '@angular/material/dialog';
import { PowerProfileDialogComponent } from './power-profiles-dialog/power-profiles-dialog.component';
import { AppqosService } from 'src/app/services/appqos.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';

@Component({
  selector: 'app-power-profiles',
  templateUrl: './power-profiles.component.html',
  styleUrls: ['./power-profiles.component.scss']
})
export class PowerProfilesComponent implements OnChanges {
  @Input() available?: boolean;
  @Input() pwrProfiles!: PowerProfiles[];
  tableData!: PowerProfiles[];
  tableHeaders: string[] = ['id', 'name', 'minFreq', 'maxFreq', 'epp', 'action'];

  constructor(
    public dialog: MatDialog,
    private service: AppqosService,
    private snackBar: SnackBarService
  ) { }

  ngOnChanges(): void {
    this.tableData = this.pwrProfiles.map((profile) => ({
      ...profile,
      epp: eppDisplayStr[eppPostStr.indexOf(profile.epp)]
    }));
  }

  pwrProfileAddDialog() {
    this.dialog.open(PowerProfileDialogComponent, {
      height: 'auto',
      width: '35rem',
      data: {
        edit: false
      }
    });
  }

  pwrProfileEditDialog(profile: PowerProfiles) {
    this.dialog.open(PowerProfileDialogComponent, {
      height: 'auto',
      width: '35rem',
      data: {
        profile,
        edit: true
      }
    });
  }

  deletePwrProfile(id: number) {
    this.service.deletePowerProfile(id).subscribe({
      next: (res: resMessage) => {
        this.snackBar.displayInfo(res.message);
        this.service.getPowerProfile().subscribe();
      },
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    });
  }
}
