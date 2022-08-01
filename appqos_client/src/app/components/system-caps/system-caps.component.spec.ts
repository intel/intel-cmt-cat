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
import { fakeAsync } from '@angular/core/testing';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { of } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { SharedModule } from 'src/app/shared/shared.module';
import { L3catComponent } from './l3cat/l3cat.component';
import { SystemCapsComponent } from './system-caps.component';

import {
  CacheAllocation,
  Caps,
  MBACTRL,
  RDTIface,
  SSTBF,
} from './system-caps.model';

describe('Given SystemCapsComponent', () => {
  beforeEach(() =>
    MockBuilder(SystemCapsComponent)
      .replace(HttpClientModule, HttpClientTestingModule)
      .mock(SharedModule)
      .mock(AppqosService)
  );

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should display title property in card ', () => {
      const title = 'System Capabilities';
      const mockedCaps: Caps = {
        capabilities: ['l3cat', 'mba', 'sstbf', 'power'],
      };

      const capsSpy = jasmine.createSpy('getCaps');

      MockInstance(AppqosService, 'getCaps', capsSpy).and.returnValue(
        of(mockedCaps)
      );

      MockRender(SystemCapsComponent);

      const expectedTitle = ngMocks.formatText(ngMocks.find('mat-card-title'));

      expect(expectedTitle).toBe(title);
    });

    it('should get capabilities', () => {
      const mockedCaps: Caps = {
        capabilities: ['l3cat', 'mba', 'sstbf', 'power'],
      };

      const capsSpy = jasmine.createSpy('getCaps');

      MockInstance(AppqosService, 'getCaps', capsSpy).and.returnValue(
        of(mockedCaps)
      );

      const {
        point: { componentInstance: component },
      } = MockRender(SystemCapsComponent);

      expect(component.caps).toEqual(mockedCaps.capabilities);
    });
  });

  describe('when request is sent to back', () => {
    it('it should show display loading', () => {
      const mockedCaps: Caps = {
        capabilities: ['l3cat', 'mba', 'sstbf', 'power'],
      };

      const capsSpy = jasmine.createSpy('getCaps');

      MockInstance(AppqosService, 'getCaps', capsSpy).and.returnValue(
        of(mockedCaps)
      );

      const fixture = MockRender(SystemCapsComponent);
      const component = fixture.point.componentInstance;

      component.loading = true;
      fixture.detectChanges();

      const content = ngMocks.find('.loading', null);

      expect(content).toBeTruthy();
    });
  });

  describe('when request is finished', () => {
    it('it should should NOT display loading', () => {
      const mockedCaps: Caps = {
        capabilities: ['l3cat', 'mba', 'sstbf', 'power'],
      };

      const capsSpy = jasmine.createSpy('getCaps');

      MockInstance(AppqosService, 'getCaps', capsSpy).and.returnValue(
        of(mockedCaps)
      );

      const fixture = MockRender(SystemCapsComponent);
      const component = fixture.point.componentInstance;

      component.loading = false;
      fixture.detectChanges();

      const content = ngMocks.find('.loading', null);

      expect(content).toBeNull();
    });
  });
});
