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

import { Apps, Pools } from '../../overview/overview.model';
import {
  Component,
  EventEmitter,
  Input,
  OnChanges,
  Output,
  SimpleChanges,
} from '@angular/core';
import { MatDialog } from '@angular/material/dialog';

import { AppsAddDialogComponent } from './apps-add-dialog/apps-add-dialog.component';
import { AppsEditDialogComponent } from './apps-edit-dialog/apps-edit-dialog.component';
import { AppqosService } from 'src/app/services/appqos.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';

@Component({
  selector: 'app-apps-config',
  templateUrl: './apps-config.component.html',
  styleUrls: ['./apps-config.component.scss'],
})
export class AppsConfigComponent implements OnChanges {
  @Input() apps!: Apps[];
  @Input() pools!: Pools[];
  @Output() appEvent = new EventEmitter<unknown>();

  tableData!: Apps[] & { coresList: string; poolName: string };
  displayedColumns: string[] = ['name', 'pool', 'pids', 'cores', 'actions'];

  constructor(
    public dialog: MatDialog,
    private service: AppqosService,
    private snackBar: SnackBarService
  ) {}

  ngOnChanges(changes: SimpleChanges): void {
    if (!changes['apps'].currentValue) return;

    this.tableData = changes['apps'].currentValue.map((app: Apps) => ({
      ...app,
      coresList:
        app.cores === undefined
          ? String(
              changes['pools'].currentValue.find(
                (pool: Pools) => pool.id === app.pool_id
              )?.cores
            )
          : String(app.cores),
      poolName: changes['pools'].currentValue.find(
        (pool: Pools) => pool.id === app.pool_id
      )?.name,
    }));
  }

  appAddDialog(): void {
    const dialogRef = this.dialog.open(AppsAddDialogComponent, {
      height: 'auto',
      width: '35rem',
      data: this.pools,
    });

    dialogRef.afterClosed().subscribe(() => {
      this.appEvent.emit();
    });
  }

  appEditDialog(app: Apps): void {
    const dialogRef = this.dialog.open(AppsEditDialogComponent, {
      height: 'auto',
      width: '35rem',
      data: { pools: this.pools, app: app },
    });

    dialogRef.afterClosed().subscribe(() => {
      this.appEvent.emit();
    });
  }

  deleteApp(app: Apps): void {
    this.service.deleteApp(app.id).subscribe({
      next: (response) => {
        this.snackBar.displayInfo(response.message);
        this.appEvent.emit();
      },
      error: (error) => {
        this.snackBar.handleError(error.error.message);
      },
    });
  }
}
