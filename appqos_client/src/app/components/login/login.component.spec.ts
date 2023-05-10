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

import { HttpClientModule } from '@angular/common/http';
import { HttpClientTestingModule } from '@angular/common/http/testing';
import { DebugElement } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { By } from '@angular/platform-browser';
import { Router } from '@angular/router';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { of, throwError } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { SharedModule } from 'src/app/shared/shared.module';
import { LoginComponent } from './login.component';
import { LocalService } from 'src/app/services/local.service';

describe('Given LoginComponent', () => {
  beforeEach(() =>
    MockBuilder(LoginComponent)
      .replace(HttpClientModule, HttpClientTestingModule)
      .mock(Router)
      .mock(SharedModule)
      .mock(MatProgressSpinnerModule)
      .keep(LocalService)
  );

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should clear localStorage', () => {
      const localStorageSpy = spyOn(localStorage, 'clear');

      MockRender(LoginComponent);

      expect(localStorageSpy).toHaveBeenCalled();
    });
  });

  describe('when loginToSystem method is called with invalid form credentials', () => {
    it('should give error', () => {
      const hostName = 'https://errorName';
      const portNumber = '404';
      const mockedError = {
        message: 'REST API error',
        name: 'error'
      }

      const loginSpy = jasmine.createSpy('loginSpy').and.returnValue(throwError(() => mockedError))
      MockInstance(AppqosService, 'login', loginSpy);

      const fixture = MockRender(LoginComponent);
      const component = fixture.point.componentInstance;

      component.form.patchValue({ hostName, portNumber });
      fixture.detectChanges();

      const form: DebugElement = fixture.debugElement.query(By.css('form'));

      form.triggerEventHandler('submit', null);

      expect(loginSpy).toHaveBeenCalledTimes(1);
      expect(component.hasError).toBeTrue();
    });
  });

  describe('when host name starts without https://', () => {
    it('should give error that form invalid', () => {
      const hostName = 'localhost';
      const portNumber = '404';

      const fixture = MockRender(LoginComponent);
      const component = fixture.point.componentInstance;

      component.form.patchValue({ hostName, portNumber });
      fixture.detectChanges();

      expect(component.form.invalid).toBeTrue();
    });
  });

  describe('when host name starts with https://', () => {
    it('should NOT give error that form invalid', () => {
      const hostName = 'https://localhost';
      const portNumber = '404';

      const fixture = MockRender(LoginComponent);
      const component = fixture.point.componentInstance;

      component.form.patchValue({ hostName, portNumber });
      fixture.detectChanges();

      expect(component.form.valid).toBeTrue();
    });
  });

  describe('when loginToSystem method is called with valid form credentials', () => {
    it('it should NOT give error', () => {
      const hostName = 'localhost';
      const portNumber = '5000';
      const mockedCaps = { capabilities: ['l3cat', 'mba', 'sstbf', 'power'] };

      MockInstance(AppqosService, 'login', () => of(mockedCaps));

      const fixture = MockRender(LoginComponent);
      const component = fixture.point.componentInstance;

      component.form.patchValue({ hostName, portNumber });
      fixture.detectChanges();

      const form: DebugElement = fixture.debugElement.query(By.css('form'));

      form.triggerEventHandler('submit', null);

      expect(component.hasError).toBeFalse();
    });

    it('it should redirect to Dashboard page', () => {
      const hostName = 'https://localhost';
      const portNumber = '5000';
      const mockedCaps = { capabilities: ['l3cat', 'mba', 'sstbf', 'power'] };

      MockInstance(AppqosService, 'login', () => of(mockedCaps));

      const fixture = MockRender(LoginComponent);
      const component = fixture.point.componentInstance;
      const router = TestBed.inject(Router);

      component.form.patchValue({ hostName, portNumber });
      fixture.detectChanges();

      const form: DebugElement = fixture.debugElement.query(By.css('form'));
      const routerSpy = spyOn(router, 'navigate');

      form.triggerEventHandler('submit', null);

      expect(routerSpy).toHaveBeenCalledWith(['/dashboard']);
    });

    it('it should store credentials to localStorage', () => {
      const hostName = 'https://localhost';
      const portNumber = '5000';
      const mockedCaps = { capabilities: ['l3cat', 'mba', 'sstbf', 'power'] };

      MockInstance(AppqosService, 'login', () => of(mockedCaps));

      const fixture = MockRender(LoginComponent);
      const component = fixture.point.componentInstance;
      const form: DebugElement = fixture.debugElement.query(By.css('form'));

      component.form.patchValue({ hostName, portNumber });

      form.triggerEventHandler('submit', null);

      const localStore = ngMocks.findInstance(LocalService);

      expect(localStore.getData('api_url')).toBe('https://localhost:5000');
    });
  });

  describe('when request is sent to back', () => {
    it('it should show display loading', () => {
      const fixture = MockRender(LoginComponent);
      const component = fixture.point.componentInstance;

      component.loading = true;
      fixture.detectChanges();

      const content = ngMocks.find('.loading', null);

      expect(content).toBeTruthy();
    });
  });

  describe('when request is finished', () => {
    it('it should should NOT display loading', () => {
      const fixture = MockRender(LoginComponent);
      const component = fixture.point.componentInstance;

      component.loading = false;
      fixture.detectChanges();

      const content = ngMocks.find('.loading', null);

      expect(content).toBeNull();
    });
  });
});
