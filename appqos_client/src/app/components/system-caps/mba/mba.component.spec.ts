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

import {
  MatSlideToggle,
  MatSlideToggleChange,
} from '@angular/material/slide-toggle';

import { SharedModule } from 'src/app/shared/shared.module';
import { MBA, MBACTRL } from '../system-caps.model';
import { MbaComponent } from './mba.component';

describe('Given MbaComponent', () => {
  beforeEach(() => MockBuilder(MbaComponent).mock(SharedModule));

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should display "MBA" title', () => {
      MockRender(MbaComponent, {
        isSupported: true,
      });

      const expectValue = ngMocks.formatText(ngMocks.find('div'));

      expect(expectValue).toEqual('MBA');
    });
  });

  describe('when initialized and MBA is supported', () => {
    it('should display MBA COS number', () => {
      const mockedMbaData: MBA & MBACTRL = {
        clos_num: 24,
        mba_enabled: true,
        mba_bw_enabled: false,
        enabled: true,
        supported: true,
      };

      MockRender(MbaComponent, {
        isSupported: true,
        mba: mockedMbaData,
      });

      const template = ngMocks.findAll('span')[3];

      expect(ngMocks.formatText(template)).toEqual(
        mockedMbaData.clos_num.toString()
      );
    });

    it('should display "Supported Yes" text', () => {
      MockRender(MbaComponent, {
        isSupported: true,
      });

      const template = ngMocks.formatText(ngMocks.find('.positive'));

      expect(template).toEqual('Yes');
    });
  });

  describe('when initialized and MBA is NOT supported', () => {
    it('should NOT display MBA controller details', () => {
      MockRender(MbaComponent, {
        isSupported: false,
      });

      const template = ngMocks.find('mat-list-item', null);

      expect(template).toBeNull();
    });

    it('should display "Supported No" text', () => {
      MockRender(MbaComponent, {
        isSupported: false,
      });

      const template = ngMocks.formatText(ngMocks.find('.negative'));

      expect(template).toEqual('No');
    });
  });

  describe('when MBA controller is supported', () => {
    it('should display "MBA controller Yes" text', () => {
      const mockedMbaData: MBA & MBACTRL = {
        clos_num: 24,
        mba_enabled: true,
        mba_bw_enabled: false,
        enabled: true,
        supported: true,
      };

      MockRender(MbaComponent, {
        isSupported: true,
        mba: mockedMbaData,
      });

      const template = ngMocks.formatText(ngMocks.find('.positive'));

      expect(template).toEqual('Yes');
    });

    it('should display "MBA controller enabled" toggle', () => {
      const mockedMbaData: MBA & MBACTRL = {
        clos_num: 24,
        mba_enabled: true,
        mba_bw_enabled: true,
        enabled: true,
        supported: true,
      };

      MockRender(MbaComponent, {
        isSupported: true,
        mba: mockedMbaData,
      });

      const template = ngMocks.find('mat-slide-toggle');

      expect(template).toBeTruthy();
    });
  });

  describe('when MBA controller is NOT supported', () => {
    it('should display "MBA controller No" text', () => {
      const mockedMbaData: MBA & MBACTRL = {
        clos_num: 24,
        mba_enabled: true,
        mba_bw_enabled: true,
        enabled: false,
        supported: false,
      };

      MockRender(MbaComponent, {
        isSupported: true,
        mba: mockedMbaData,
      });

      const template = ngMocks.formatText(ngMocks.find('.negative'));

      expect(template).toEqual('No');
    });

    it('should NOT display "MBA controller enabled" toggle', () => {
      const mockedMbaData: MBA & MBACTRL = {
        clos_num: 24,
        mba_enabled: true,
        mba_bw_enabled: true,
        enabled: false,
        supported: false,
      };

      MockRender(MbaComponent, {
        isSupported: true,
        mba: mockedMbaData,
      });

      const template = ngMocks.find('mat-slide-toggle', null);

      expect(template).toBeNull();
    });
  });

  describe('when slide toggle is clicked', () => {
    it('should emit "onChange" event with correct value', (done) => {
      const event: MatSlideToggleChange = {
        checked: false,
        source: {} as MatSlideToggle,
      };

      const mockedMbaData: MBA & MBACTRL = {
        clos_num: 24,
        mba_enabled: true,
        mba_bw_enabled: true,
        enabled: true,
        supported: true,
      };

      const fixture = MockRender(MbaComponent, {
        isSupported: true,
        mba: mockedMbaData,
      });

      const component = fixture.point.componentInstance;
      const toggle = ngMocks.find('mat-slide-toggle');

      component.changeEvent.subscribe((value) => {
        expect(value.checked).toBeFalse();

        done();
      });

      toggle.triggerEventHandler('change', event);
    });
  });
});
