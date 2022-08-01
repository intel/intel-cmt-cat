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

import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { of } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { SharedModule } from 'src/app/shared/shared.module';
import { MBACTRL } from '../system-caps.model';
import { MbaComponent } from './mba.component';

describe('Given MbaComponent', () => {
  beforeEach(() => MockBuilder(MbaComponent).mock(SharedModule));

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should display "MBA" title', () => {
      const mockedMbaCtrl: MBACTRL = { enabled: false, supported: false };

      MockInstance(AppqosService, 'getMbaCtrl', () => of(mockedMbaCtrl));

      MockRender(MbaComponent, {
        isSupported: true,
      });

      const expectValue = ngMocks.formatText(ngMocks.find('div'));

      expect(expectValue).toEqual('MBA');
    });
  });

  describe('when initialized and MBA is supported', () => {
    it('should display MBA controller details', () => {
      const mockedMbaCtrl: MBACTRL = { enabled: false, supported: false };

      MockInstance(AppqosService, 'getMbaCtrl', () => of(mockedMbaCtrl));

      const {
        point: { componentInstance: component },
      } = MockRender(MbaComponent, {
        isSupported: true,
      });

      component.mbaCtrl$.subscribe((mba: MBACTRL) =>
        expect(mockedMbaCtrl).toEqual(mba)
      );
    });

    it('should display "Supported Yes" text', () => {
      const mockedMbaCtrl: MBACTRL = { enabled: false, supported: false };

      MockInstance(AppqosService, 'getMbaCtrl', () => of(mockedMbaCtrl));

      MockRender(MbaComponent, {
        isSupported: true,
      });

      const template = ngMocks.formatText(ngMocks.find('.positive'));

      expect(template).toEqual('Yes');
    });
  });

  describe('when initialized and MBA is NOT supported', () => {
    it('should NOT display MBA controller details', () => {
      const mockedMbaCtrl: MBACTRL = { enabled: false, supported: false };

      MockInstance(AppqosService, 'getMbaCtrl', () => of(mockedMbaCtrl));

      MockRender(MbaComponent, {
        isSupported: false,
      });

      const template = ngMocks.find('mat-list-item', null);

      expect(template).toBeNull();
    });

    it('should display "Supported No" text', () => {
      const mockedMbaCtrl: MBACTRL = { enabled: false, supported: false };

      MockInstance(AppqosService, 'getMbaCtrl', () => of(mockedMbaCtrl));

      MockRender(MbaComponent, {
        isSupported: false,
      });

      const template = ngMocks.formatText(ngMocks.find('.negative'));

      expect(template).toEqual('No');
    });
  });

  describe('when MBA controller is supported', () => {
    it('should display "MBA controller Yes" text', () => {
      const mockedMbaCtrl: MBACTRL = { enabled: false, supported: true };

      MockInstance(AppqosService, 'getMbaCtrl', () => of(mockedMbaCtrl));

      MockRender(MbaComponent, {
        isSupported: true,
      });

      const template = ngMocks.formatText(ngMocks.find('.positive'));

      expect(template).toEqual('Yes');
    });

    it('should display "MBA controller enabled" toggle', () => {
      const mockedMbaCtrl: MBACTRL = { enabled: false, supported: true };

      MockInstance(AppqosService, 'getMbaCtrl', () => of(mockedMbaCtrl));

      MockRender(MbaComponent, {
        isSupported: true,
      });

      const template = ngMocks.find('mat-slide-toggle');

      expect(template).toBeTruthy();
    });
  });

  describe('when MBA controller is NOT supported', () => {
    it('should display "MBA controller No" text', () => {
      const mockedMbaCtrl: MBACTRL = { enabled: false, supported: false };

      MockInstance(AppqosService, 'getMbaCtrl', () => of(mockedMbaCtrl));

      MockRender(MbaComponent, {
        isSupported: true,
      });

      const template = ngMocks.formatText(ngMocks.find('.negative'));

      expect(template).toEqual('No');
    });

    it('should NOT display "MBA controller enabled" toggle', () => {
      const mockedMbaCtrl: MBACTRL = { enabled: false, supported: false };

      MockInstance(AppqosService, 'getMbaCtrl', () => of(mockedMbaCtrl));

      MockRender(MbaComponent, {
        isSupported: true,
      });

      const template = ngMocks.find('mat-slide-toggle', null);

      expect(template).toBeNull();
    });
  });
});
