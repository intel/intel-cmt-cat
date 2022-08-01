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

import { Component, Input, OnInit } from '@angular/core';
import { catchError, EMPTY, map, Observable, throwError } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { CacheAllocation } from '../system-caps.model';

@Component({
  selector: 'app-l3cat',
  templateUrl: './l3cat.component.html',
  styleUrls: ['./l3cat.component.scss'],
})
/* Component used to show L3CAT details*/
export class L3catComponent implements OnInit {
  @Input() isSupported!: boolean;
  l3cat$!: Observable<CacheAllocation>;

  constructor(
    private service: AppqosService,
    private snackBar: SnackBarService
  ) {}

  ngOnInit(): void {
    this.l3cat$ = this.service.getL3cat().pipe(
      map((cat: CacheAllocation) => ({
        ...cat,
        cache_size: Math.round((cat.cache_size / 1024 ** 2) * 100) / 100,
        cw_size: Math.round((cat.cw_size / 1024 ** 2) * 100) / 100,
      })),
      catchError((err, caught) => {
        this.snackBar.handleError(err.message);
        return EMPTY;
      })
    );
  }
}
