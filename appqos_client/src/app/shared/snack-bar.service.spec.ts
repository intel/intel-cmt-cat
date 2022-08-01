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

import { TestBed } from '@angular/core/testing';
import { MatSnackBar } from '@angular/material/snack-bar';
import { Router } from '@angular/router';
import { MockBuilder, MockInstance, MockRender } from 'ng-mocks';

import { SharedModule } from './shared.module';
import { SnackBarService } from './snack-bar.service';

describe('Given SnackBarService', () => {
  beforeEach(() =>
    MockBuilder(SnackBarService)
      .mock(SharedModule)
      .mock(MatSnackBar)
      .mock(Router)
  );

  MockInstance.scope('case');

  describe('when handleError method is executed', () => {
    it('should display error message snackbar', () => {
      const openSnackbarSpy = jasmine.createSpy('snackbar.open');
      const errorMessage = 'Failed connection!';

      MockInstance(MatSnackBar, 'open', openSnackbarSpy);
      const {
        point: { componentInstance: service },
      } = MockRender(SnackBarService);

      service.handleError(errorMessage);

      expect(openSnackbarSpy).toHaveBeenCalledWith(errorMessage, '', {
        duration: 3000,
        horizontalPosition: 'end',
        verticalPosition: 'top',
      });
    });
  });

  describe('when handleError method is executed with failSilent', () => {
    it('should display error message snackbar', () => {
      const openSnackbarSpy = jasmine.createSpy('snackbar.open');
      const errorMessage = 'Failed connection!';

      MockInstance(MatSnackBar, 'open', openSnackbarSpy);

      const {
        point: { componentInstance: service },
      } = MockRender(SnackBarService);

      const router = TestBed.inject(Router);
      const routerSpy = spyOn(router, 'navigate');

      service.handleError(errorMessage, true);

      expect(routerSpy).toHaveBeenCalledWith(['/login']);
    });
  });
});
