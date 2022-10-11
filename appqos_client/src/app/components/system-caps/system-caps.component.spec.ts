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
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { EMPTY, of } from 'rxjs';

import {
  MatButtonToggle,
  MatButtonToggleChange,
} from '@angular/material/button-toggle';
import {
  MatSlideToggle,
  MatSlideToggleChange,
} from '@angular/material/slide-toggle';

import { AppqosService } from 'src/app/services/appqos.service';
import { SharedModule } from 'src/app/shared/shared.module';
import { SstcpComponent } from './sstcp/sstcp.component';
import { SystemCapsComponent } from './system-caps.component';
import {
  CacheAllocation,
  Caps,
  MBA,
  MBACTRL,
  RDTIface,
  SSTBF,
} from './system-caps.model';
import { LocalService } from 'src/app/services/local.service';

describe('Given SystemCapsComponent', () => {
  beforeEach(() =>
    MockBuilder(SystemCapsComponent)
      .replace(HttpClientModule, HttpClientTestingModule)
      .mock(SharedModule)
      .mock(AppqosService, {
        getRdtIface: () => EMPTY,
        getCaps: () =>
          of({
            capabilities: ['l3cat', 'mba', 'sstbf', 'power'],
          }),
        getSstbf: () => EMPTY,
        getL3cat: () => EMPTY,
        getL2cat: () => EMPTY,
      })
      .mock(SstcpComponent)
      .keep(LocalService)
  );

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should display system name', () => {
      localStorage.setItem('api_url', 'https://localhost:5000');

      MockRender(SystemCapsComponent);

      const systemName = ngMocks.formatText(ngMocks.find('.system-name'));

      expect(systemName).toEqual('localhost');
    });

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

      const expectedTitle = ngMocks.formatText(ngMocks.find('.card-title'));

      expect(expectedTitle).toBe(title);
    });

    it('should get MBA data', () => {
      const mockedMba: MBA = {
        clos_num: 12,
        mba_enabled: true,
        mba_bw_enabled: false,
      };

      const mockedMbaCtrl: MBACTRL = { enabled: true, supported: true };

      MockInstance(AppqosService, 'getMba', () => of(mockedMba));
      MockInstance(AppqosService, 'getMbaCtrl', () => of(mockedMbaCtrl));

      const {
        point: { componentInstance: component },
      } = MockRender(SystemCapsComponent);

      expect(component.mba).toEqual({ ...mockedMba, ...mockedMbaCtrl });
    });

    it('should get RDT interface', () => {
      const mockedRDT: RDTIface = {
        interface: 'os',
        interface_supported: ['msr', 'os'],
      };

      MockInstance(AppqosService, 'getRdtIface', () => of(mockedRDT));

      const {
        point: { componentInstance: component },
      } = MockRender(SystemCapsComponent);

      expect(component.rdtIface).toEqual(mockedRDT);
    });

    it('should get L3 CAT', () => {
      const mockedL3cat: CacheAllocation = {
        cache_size: 44040192,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3670016,
      };

      MockInstance(AppqosService, 'getL3cat', () => of(mockedL3cat));

      const {
        point: { componentInstance: component },
      } = MockRender(SystemCapsComponent);

      expect(component.l3cat).toEqual({
        ...mockedL3cat,
        cache_size: 42,
        cw_size: 3.5,
      });
    });

    it('should get L2 CAT', () => {
      const mockedL2cat: CacheAllocation = {
        cache_size: 44040192,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3670016,
      };

      const mockedCaps: Caps = {
        capabilities: ['l3cat', 'l2cat', 'mba', 'sstbf', 'power'],
      };

      MockInstance(AppqosService, 'getCaps', () => of(mockedCaps));
      MockInstance(AppqosService, 'getL2cat', () => of(mockedL2cat));

      const {
        point: { componentInstance: component },
      } = MockRender(SystemCapsComponent);

      expect(component.l2cat).toEqual({
        ...mockedL2cat,
        cache_size: 42,
        cw_size: 3.5,
      });
    });

    it('should get SST-BF', () => {
      const mockedSSTBF: SSTBF = {
        configured: false,
        hp_cores: [1, 2],
        std_cores: [1, 2],
      };

      MockInstance(AppqosService, 'getSstbf', () => of(mockedSSTBF));

      const {
        point: { componentInstance: component },
      } = MockRender(SystemCapsComponent);

      expect(component.sstbf).toEqual(mockedSSTBF);
    });

    it('should get capabilities', () => {
      const mockedCaps: Caps = {
        capabilities: ['l3cat', 'mba', 'sstbf', 'power'],
      };

      MockInstance(AppqosService, 'getCaps', () => of(mockedCaps));

      const {
        point: { componentInstance: component },
      } = MockRender(SystemCapsComponent);

      expect(component.caps).toEqual(mockedCaps.capabilities);
    });
  });

  describe('when request is sent to back', () => {
    it('it should show display loading', () => {
      const fixture = MockRender(SystemCapsComponent);
      const component = fixture.point.componentInstance;

      component.loading = true;
      fixture.detectChanges();

      const content = ngMocks.find('.loading', null);

      expect(content).toBeTruthy();
    });
  });

  describe('when request is finished', () => {
    it('it should NOT display loading', () => {
      const fixture = MockRender(SystemCapsComponent);
      const component = fixture.point.componentInstance;

      component.loading = false;
      fixture.detectChanges();

      const content = ngMocks.find('.loading', null);

      expect(content).toBeNull();
    });
  });

  describe('when onChangeIface method is called', () => {
    it('it should call rdtIfacePut with correct value', () => {
      const mockResponse = 'RDT Interface modified';
      const rdtIfaceSpy = jasmine.createSpy('rdtIfacePut');
      const event: MatButtonToggleChange = {
        source: {} as MatButtonToggle,
        value: 'os',
      };

      MockInstance(AppqosService, 'rdtIfacePut', rdtIfaceSpy)
        .withArgs(event.value)
        .and.returnValue(of(mockResponse));

      const fixture = MockRender(SystemCapsComponent);
      const component = fixture.point.componentInstance;

      component.onChangeIface(event);

      expect(rdtIfaceSpy).toHaveBeenCalledWith(event.value);
    });
  });

  describe('when sstbfOnChange method is called', () => {
    it('it should call sstbfPut with correct value', () => {
      const mockResponse = 'SST-BF caps modified';
      const sstbfPutSpy = jasmine.createSpy('sstbfPut');
      const event: MatSlideToggleChange = {
        source: {} as MatSlideToggle,
        checked: true,
      };

      MockInstance(AppqosService, 'sstbfPut', sstbfPutSpy)
        .withArgs(event.checked)
        .and.returnValue(of(mockResponse));

      const fixture = MockRender(SystemCapsComponent);
      const component = fixture.point.componentInstance;

      component.sstbfOnChange(event);

      expect(sstbfPutSpy).toHaveBeenCalledWith(event.checked);
    });
  });
});
