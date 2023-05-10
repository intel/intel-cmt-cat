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

import { Component, OnInit } from '@angular/core';
import { FormControl, FormGroup, Validators } from '@angular/forms';
import { Router } from '@angular/router';

import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { Standards } from '../system-caps/system-caps.model';

@Component({
  selector: 'app-login',
  templateUrl: './login.component.html',
  styleUrls: ['./login.component.scss'],
})

/* Component used to display Login page with form fields*/
export class LoginComponent implements OnInit {
  form!: FormGroup;
  hasError = false;
  loading = false;

  constructor(
    private service: AppqosService,
    private localStore: LocalService,
    private router: Router
  ) {}

  ngOnInit(): void {
    this.localStore.clearData();
    this.form = new FormGroup({
      hostName: new FormControl('', [
        Validators.required,
        Validators.pattern('^https://+[!-~]+$'),
        Validators.maxLength(Standards.MAX_CHARS),
      ]),
      portNumber: new FormControl('', [
        Validators.required,
        Validators.min(0),
        Validators.max(65353),
        Validators.pattern('^[0-9]+$'),
      ]),
    });
  }

  loginToSystem(form: FormGroup) {
    const hostName = form.value.hostName.trim();
    const portNumber = form.value.portNumber.trim();
    this.loading = true;
    this.hasError = false;

    if (!form.valid) {
      return;
    }

    this.service.login(hostName, portNumber).subscribe({
      next: () => {
        this.hasError = false;
        this.loading = false;
        this.goToSystem(hostName, portNumber);
      },
      error: () => {
        this.hasError = true;
        this.loading = false;
      }
    });
  }

  private goToSystem(hostName: string, portNumber: string): void {
    this.localStore.saveData('api_url', `${hostName}:${portNumber}`);
    this.router.navigate(['/dashboard']);
  }
}
