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

import { Component, Inject, OnInit } from '@angular/core';
import { FormControl, FormGroup, Validators } from '@angular/forms';
import { MatDialogRef, MAT_DIALOG_DATA } from '@angular/material/dialog';

import { Apps, Pools } from 'src/app/components/overview/overview.model';
import { Standards } from 'src/app/components/system-caps/system-caps.model';
import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';

type PostApp = Omit<Apps, 'id'>;

@Component({
  selector: 'app-apps-add-dialog',
  templateUrl: './apps-add-dialog.component.html',
  styleUrls: ['./apps-add-dialog.component.scss'],
})
export class AppsAddDialogComponent implements OnInit {
  form!: FormGroup;
  poolsList!: Pools[];
  coresList!: number[];
  pidsList!: number[];

  constructor(
    @Inject(MAT_DIALOG_DATA) public data: Pools[],
    private snackBar: SnackBarService,
    private localService: LocalService,
    public dialogRef: MatDialogRef<AppsAddDialogComponent>,
    private service: AppqosService
  ) {}

  ngOnInit(): void {
    this.form = new FormGroup({
      name: new FormControl('', [
        Validators.pattern('^[ -~]+$'),
        Validators.required,
        Validators.maxLength(Standards.MAX_CHARS),
      ]),
      pids: new FormControl('', [
        Validators.required,
        Validators.pattern(
          '^[0-9]+(?:,[0-9]+)+(?:-[0-9]+)?$|^[0-9]+(?:-[0-9]+)?$|^[0-9]+(?:-[0-9]+)+(?:,[0-9]+)+(?:,[0-9]+)?$'
        ),
        Validators.maxLength(Standards.MAX_CHARS_PIDS),
      ]),
      cores: new FormControl('', [
        Validators.pattern(
          '^[0-9]+(?:,[0-9]+)+(?:-[0-9]+)?$|^[0-9]+(?:-[0-9]+)?$|^[0-9]+(?:-[0-9]+)+(?:,[0-9]+)+(?:,[0-9]+)?$'
        ),
        Validators.maxLength(Standards.MAX_CHARS_CORES),
      ]),
      pool: new FormControl('', [Validators.required]),
    });
  }

  saveApp(): void {
    if (!this.form.valid) return;
    if (this.form.value.cores) this.getCores(this.form.value.cores);

    this.getPids(this.form.value.pids);

    const app: PostApp = {
      name: this.form.value.name,
      pids: this.pidsList,
      pool_id: this.form.value.pool.id,
    };

    if (!this.form.value.cores) {
      app.cores = this.data.find(
        (pool: Pools) => pool.id === this.form.value.pool.id
      )!.cores;
    } else {
      app.cores = this.coresList;
    }

    this.postApp(app);
  }

  postApp(app: PostApp): void {
    this.service.postApp(app).subscribe({
      next: (response) => {
        this.snackBar.displayInfo(response.message);
        this.dialogRef.close();
      },
      error: (error) => {
        this.snackBar.handleError(error.message);
      },
    });
  }

  getPids(pids: string) {
    if (pids.includes('-')) {
      this.pidsList = this.localService.getPidsDash(pids);
    } else {
      this.pidsList = pids.split(',').map(Number);
    }
  }

  getCores(cores: string) {
    if (cores.includes('-')) {
      this.coresList = this.localService.getCoresDash(cores);
    } else {
      this.coresList = cores.split(',').map(Number);
    }

    if (Math.max(...this.coresList) > Standards.MAX_CORES) {
      this.form.controls['cores'].setErrors({ incorrect: true });
    }
  }
}
