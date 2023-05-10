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
import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { CacheAllocation } from '../system-caps/system-caps.model';
import { catchError, map, of } from 'rxjs';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { Router } from '@angular/router';
import { Apps } from '../overview/overview.model';

@Component({
  selector: 'app-dashboard-page',
  templateUrl: './dashboard-page.component.html',
  styleUrls: ['./dashboard-page.component.scss'],
})

/* Component used to display Dashboard page*/
export class DashboardPageComponent implements OnInit {
  showContent: boolean = true;

  constructor(
    private service: AppqosService,
    private localService: LocalService,
    private snackBar: SnackBarService,
    private router: Router
  ) { }

  ngOnInit(): void {
    this._getCaps();
  }

  switchContent(event: boolean): void {
    this.showContent = event;
  }

  private _getCaps() {
    this.service.getCaps().subscribe(
      {
        next: (caps) => {
          this._getRdtIface();
          this._getPools();
          this._getApps();

          if (caps.capabilities.includes('l3cat'))
            this._getL3Cat();

          if (caps.capabilities.includes('l2cat'))
            this._getL2Cat();

          if (caps.capabilities.includes('sstbf'))
            this._getSstbf();

          if (caps.capabilities.includes('mba')) {
            this._getMba();
            this._getMbaCtrl();
          }

        },
        error: (error: Error) => {
          this.snackBar.handleError(error.message);
          this.router.navigate(['/login']);
        }
      });
  }

  private _getL3Cat() {
    this.service.getL3cat().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    });
  }

  private _getL2Cat() {
    this.service.getL2cat().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    });
  }

  private _getMba() {
    this.service.getMba().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    });
  }

  private _getMbaCtrl() {
    this.service.getMbaCtrl().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    });
  }

  private _getSstbf() {
    this.service.getSstbf().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    })
  }

  private _getRdtIface() {
    this.service.getRdtIface().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    })
  }

  private _getPools() {
    this.service.getPools().subscribe({
      error: (error: Error) => {
        this.snackBar.handleError(error.message);
      }
    })
  }

  private _getApps() {
    this.service.getApps().subscribe();
  }
}
