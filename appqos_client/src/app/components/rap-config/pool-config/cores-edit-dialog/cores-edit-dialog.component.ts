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
import { Standards } from 'src/app/components/system-caps/system-caps.model';

import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { Pools } from '../../../overview/overview.model';

@Component({
  selector: 'app-cores-edit-dialog',
  templateUrl: './cores-edit-dialog.component.html',
  styleUrls: ['./cores-edit-dialog.component.scss'],
})
export class CoresEditDialogComponent implements OnInit {
  cores!: string;
  isSaved = false;
  form!: FormGroup;
  coresList!: number[];

  constructor(
    @Inject(MAT_DIALOG_DATA) public data: Pools,
    private service: AppqosService,
    private localService: LocalService,
    private snackBar: SnackBarService,
    public dialogRef: MatDialogRef<CoresEditDialogComponent>
  ) {}

  ngOnInit(): void {
    this.form = new FormGroup({
      cores: new FormControl(String(this.data.cores), [
        Validators.required,
        Validators.maxLength(Standards.MAX_CHARS_CORES),
        Validators.pattern(
          '^[0-9]+(?:,[0-9]+)+(?:-[0-9]+)?$|^[0-9]+(?:-[0-9]+)?$|^[0-9]+(?:-[0-9]+)+(?:,[0-9]+)+(?:,[0-9]+)?$'
        ),
      ]),
    });
  }

  saveCores(): void {
    if (!this.form.valid) return;

    if (this.form.value.cores.includes('-')) {
      this.coresList = this.localService.getCoresDash(this.form.value.cores);
    } else {
      this.coresList = this.form.value.cores.split(',').map(Number);
    }

    if (Math.max(...this.coresList) > Standards.MAX_CORES) {
      this.form.controls['cores'].setErrors({ incorrect: true });
      return;
    }

    this.service
      .poolPut(
        {
          cores: this.coresList,
        },
        this.data.id
      )
      .subscribe({
        next: (response) => {
          this.snackBar.displayInfo(response.message);
          this.dialogRef.close(true);
        },
        error: (error) => {
          this.snackBar.handleError(error.message);
        },
      });
  }
}
