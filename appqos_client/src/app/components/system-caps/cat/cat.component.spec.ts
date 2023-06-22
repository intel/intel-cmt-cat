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

import { MatSlideToggle, MatSlideToggleChange } from '@angular/material/slide-toggle';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { SharedModule } from 'src/app/shared/shared.module';
import { CacheAllocation } from '../system-caps.model';
import { CatComponent } from './cat.component';

describe('Given CatComponent', () => {
  beforeEach(() => MockBuilder(CatComponent).mock(SharedModule));

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should display "L2 CAT" title', () => {
      const mockedL2cat: CacheAllocation = {
        cache_size: 42,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3.5,
      };
      const title = 'L2 CAT';

      MockRender(CatComponent, {
        isSupported: true,
        cat: mockedL2cat,
        title: title
      });

      const expectValue = ngMocks.formatText(ngMocks.find('div'));

      expect(expectValue).toEqual(title);
    });

    it('should display "L3 CAT" title', () => {
      const mockedL3cat: CacheAllocation = {
        cache_size: 42,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3.5,
      };
      const title = 'L3 CAT';

      MockRender(CatComponent, {
        isSupported: true,
        cat: mockedL3cat,
        title: title
      });

      const expectValue = ngMocks.formatText(ngMocks.find('div'));

      expect(expectValue).toEqual(title);
    });
  });

  describe('when initialized and L2 CAT is supported', () => {
    it('should display CAT details', () => {
      const mockedL2cat: CacheAllocation = {
        cache_size: 42,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3.5,
      };
      const title = 'L2 CAT';

      MockRender(CatComponent, {
        isSupported: true,
        cat: mockedL2cat,
        title: title
      });

      const template = ngMocks.findAll('span');
      const expextedData = ngMocks.formatText(template);

      expect(expextedData).toEqual([
        'Supported',
        'Yes',
        'Cache size',
        '42MB',
        'Cache way size',
        '3.5MB',
        'Number of cache ways',
        '12',
        'Number of COS',
        '15',
        'CDP supported',
        'No',
      ]);
    });

    it('should display "Supported Yes" text', () => {
      const mockedL2cat: CacheAllocation = {
        cache_size: 42,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3.5,
      };
      const title = 'L2 CAT';

      MockRender(CatComponent, {
        isSupported: true,
        cat: mockedL2cat,
        title: title
      });

      const template = ngMocks.formatText(ngMocks.find('.positive'));

      expect(template).toEqual('Yes');
    });
  });

  describe('when initialized and L2 CAT is NOT supported', () => {
    it('should NOT display L2 CAT details', () => {
      const mockedL2cat: CacheAllocation = {
        cache_size: 42,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3.5,
      };
      const title = 'L2 CAT';

      MockRender(CatComponent, {
        isSupported: false,
        cat: mockedL2cat,
        title: title
      });

      const template = ngMocks.find('mat-list-item', null);

      expect(template).toBeNull();
    });

    it('should display "Supported No" text', () => {
      const mockedL2cat: CacheAllocation = {
        cache_size: 42,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3.5,
      };
      const title = 'L2 CAT';

      MockRender(CatComponent, {
        isSupported: false,
        cat: mockedL2cat,
        title: title
      });

      const template = ngMocks.formatText(ngMocks.find('.negative'));

      expect(template).toEqual('No');
    });
  });

  describe('when CAT CDP is supported', () => {
    it('should display "CDP enabled" toggle', () => {
      const mockedCat: CacheAllocation = {
        cache_size: 42,
        cdp_enabled: false,
        cdp_supported: true,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3.5,
      };
      const title = 'L2 CAT';

      MockRender(CatComponent, {
        isSupported: true,
        cat: mockedCat,
        title: title
      });

      const template = ngMocks.find('mat-slide-toggle');

      expect(template).toBeTruthy();
    });
  });

  describe('when CAT CDP is NOT supported', () => {
    it('should NOT display "CDP enabled" toggle', () => {
      const mockedCat: CacheAllocation = {
        cache_size: 42,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3.5,
      };
      const title = 'L2 CAT';

      MockRender(CatComponent, {
        isSupported: true,
        cat: mockedCat,
        title: title
      });

      const template = ngMocks.find('mat-slide-toggle', null);

      expect(template).toBeNull();
    });
  });

  describe('when slide toggle is clicked', () => {
    it('should emit "onChange" event with false', (done) => {
      const mockedCat: CacheAllocation = {
        cache_size: 42,
        cdp_enabled: false,
        cdp_supported: true,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3.5,
      };
      const title = 'L2 CAT';

      const event: MatSlideToggleChange = {
        checked: !mockedCat.cdp_enabled,
        source: {} as MatSlideToggle,
      };

      const {
        point: {componentInstance: component}
      } = MockRender(CatComponent, {
        isSupported: true,
        cat: mockedCat,
        title: title
      });

      const toggle = ngMocks.find('mat-slide-toggle');

      component.changeEvent.subscribe((value) => {
        expect(value.checked).toBeTrue();

        done();
      });

      toggle.triggerEventHandler('change', event);
    });

    it('should emit "onChange" event with true', (done) => {
      const mockedCat: CacheAllocation = {
        cache_size: 42,
        cdp_enabled: true,
        cdp_supported: true,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3.5,
      };
      const title = 'L2 CAT';

      const event: MatSlideToggleChange = {
        checked: !mockedCat.cdp_enabled,
        source: {} as MatSlideToggle,
      };

      const {
        point: {componentInstance: component}
      } = MockRender(CatComponent, {
        isSupported: true,
        cat: mockedCat,
        title: title
      });

      const toggle = ngMocks.find('mat-slide-toggle');

      component.changeEvent.subscribe((value) => {
        expect(value.checked).toBeFalse();

        done();
      });

      toggle.triggerEventHandler('change', event);
    });
  });
});

