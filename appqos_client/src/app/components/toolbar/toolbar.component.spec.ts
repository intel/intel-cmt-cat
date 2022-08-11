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
import { Router } from '@angular/router';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { first, tap } from 'rxjs';

import { SharedModule } from 'src/app/shared/shared.module';
import { ToolbarComponent } from './toolbar.component';

describe('Given ToolbarComponent', () => {
  beforeEach(() =>
    MockBuilder(ToolbarComponent).mock(Router).mock(SharedModule)
  );

  MockInstance.scope('case');

  describe('when logout method is called', () => {
    it('should redirect to Login page', () => {
      const fixture = MockRender(ToolbarComponent);
      const component = fixture.point.componentInstance;
      const router = TestBed.inject(Router);
      const routerSpy = spyOn(router, 'navigate');

      component.logout();
      expect(routerSpy).toHaveBeenCalledWith(['/login']);
    });
  });

  describe('when Overview button is clicked', () => {
    it('should emit "switcher" Event', (done: DoneFn) => {
      const fixture = MockRender(ToolbarComponent);
      const component = fixture.point.componentInstance;

      const switcherSpy = jasmine.createSpy('switcher');

      component.switcher
        .asObservable()
        .pipe(first(), tap(switcherSpy))
        .subscribe({
          complete: () => {
            expect(switcherSpy).toHaveBeenCalledTimes(1);

            done();
          },
        });

      const overviewButton = ngMocks.find('.active');

      overviewButton.triggerEventHandler('click', null);
    });
  });

  describe('when Rap config button is clicked', () => {
    it('should emit "switcher" Event', (done: DoneFn) => {
      const fixture = MockRender(ToolbarComponent);
      const component = fixture.point.componentInstance;

      const switcherSpy = jasmine.createSpy('switcher');

      component.switcher
        .asObservable()
        .pipe(first(), tap(switcherSpy))
        .subscribe({
          complete: () => {
            expect(switcherSpy).toHaveBeenCalledTimes(1);

            done();
          },
        });

      const overviewButton = ngMocks.find('.inactive');

      overviewButton.triggerEventHandler('click', null);
    });
  });
});
