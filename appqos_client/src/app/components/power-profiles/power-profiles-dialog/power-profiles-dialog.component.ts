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

import { Component, OnInit } from '@angular/core';
import { FormControl, FormGroup, Validators } from '@angular/forms';
import { EnergyPerformPref, PowerProfiles, Standards, eppDisplayStr, eppPostStr } from '../../system-caps/system-caps.model';
import { AppqosService } from 'src/app/services/appqos.service';
import { MatDialogRef } from '@angular/material/dialog';
import { SnackBarService } from 'src/app/shared/snack-bar.service';

type PostProfile = Omit<PowerProfiles, 'id'>

@Component({
  selector: 'app-power-profiles-dialog',
  templateUrl: './power-profiles-dialog.component.html',
  styleUrls: ['./power-profiles-dialog.component.scss']
})
export class PowerProfileDialogComponent implements OnInit {
  form!: FormGroup;
  epp: string[] = eppDisplayStr;
  
  MIN_FREQ: Standards = Standards.MIN_FREQ;
  MAX_FREQ: Standards = Standards.MAX_FREQ;

  constructor(
    private service: AppqosService,
    private snackbar: SnackBarService,
    public dialogRef: MatDialogRef<PowerProfileDialogComponent>,
  ) { }

  ngOnInit(): void {
    this.form = new FormGroup({
      name: new FormControl('', [
        Validators.required,
        Validators.pattern('^[ -~]+$'),
        Validators.maxLength(Standards.MAX_CHARS)
      ]),
      minFreq: new FormControl('', [
        Validators.required,
        Validators.pattern('^[0-9]+$'),
        Validators.min(Standards.MIN_FREQ),
        Validators.max(Standards.MAX_FREQ),
      ]),
      maxFreq: new FormControl('', [
        Validators.required,
        Validators.pattern('^[0-9]+$'),
        Validators.min(Standards.MIN_FREQ),
        Validators.max(Standards.MAX_FREQ),
      ]),
      epp: new FormControl(this.epp[EnergyPerformPref.balancePerformance], [
        Validators.required,
      ])
    });
  }

  savePwrProfile() {
    if (this.form.invalid) return;

    const profile: PostProfile = {
      name: this.form.controls['name'].value,
      min_freq: this.form.controls['minFreq'].value,
      max_freq: this.form.controls['maxFreq'].value,
      epp: eppPostStr[
        eppDisplayStr.indexOf(this.form.controls['epp'].value)
      ]
    }

    this.service.postPowerProfiles(profile).subscribe({
      next: (res) => {
        this.snackbar.displayInfo(res.message);
        this.service.getPowerProfile().subscribe();
        this.dialogRef.close();
      },
      error: (error: Error) => {
        this.snackbar.handleError(error.message);
      }
    })
  }

  checkFreq() {
    const maxFreq = this.form.controls['maxFreq'];
    const minFreq = this.form.controls['minFreq'];

    if ((maxFreq.value && minFreq.value) && (minFreq.value > maxFreq.value)) {
      maxFreq.setErrors({ lessThanMin: true })
    }
  }
}