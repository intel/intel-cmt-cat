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

import {
  MatSlideToggle,
  MatSlideToggleChange,
} from '@angular/material/slide-toggle';
import { Router } from '@angular/router';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { EMPTY, of, throwError } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { LocalService } from 'src/app/services/local.service';
import { SharedModule } from 'src/app/shared/shared.module';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { MBACTRL, SSTBF } from '../system-caps/system-caps.model';
import { OverviewComponent } from './overview.component';
import { Pools } from './overview.model';

describe('Given OverviewComponent', () => {
  beforeEach(() =>
    MockBuilder(OverviewComponent)
      .mock(SharedModule)
      .mock(Router)
      .mock(AppqosService, {
        getSystemTopology: () => EMPTY,
        getPools: () => EMPTY,
        getMbaCtrl: () => EMPTY,
        getCaps: () => EMPTY,
        getSstbf: () => EMPTY,
      })
      .mock(LocalService, {
        getCapsEvent: () => EMPTY,
        getMbaCtrlEvent: () => EMPTY,
        getPoolsEvent: () => EMPTY,
        getPowerProfilesEvent: () => EMPTY,
        getSstbfEvent: () => EMPTY
      })
  );

  const mockedError: Error = {
    name: 'Error',
    message: 'rest API error',
  };

  const mockedSSTBF: SSTBF = {
    configured: false,
    hp_cores: [1, 2],
    std_cores: [1, 2],
  };

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should get Memory Bandwidth Allocation controller', () => {
      const mockedMbaCtrlData: MBACTRL = {
        enabled: true,
        supported: true,
      };

      MockInstance(LocalService, 'getMbaCtrlEvent', () => of(mockedMbaCtrlData));

      const {
        point: { componentInstance: component },
      } = MockRender(OverviewComponent);

      expect(component.mbaCtrl).toEqual(mockedMbaCtrlData);
    });

    it('should get Capabilities', () => {
      const mockedCaps = ['l2cat', 'mba', 'sstbf', 'power'];

      MockInstance(LocalService, 'getCapsEvent', () => of(mockedCaps));

      const {
        point: { componentInstance: component },
      } = MockRender(OverviewComponent);

      expect(component.caps).toEqual(mockedCaps);
    });

    it('should get Pools', () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      MockInstance(LocalService, 'getPoolsEvent', () => of(mockedPool));

      const {
        point: { componentInstance: component },
      } = MockRender(OverviewComponent);

      expect(component.pools).toEqual(mockedPool);
    });
  });

  describe('when initialized and L3 CAT is supported', () => {
    it('should render L3CacheAllocationComponent', () => {
      const mockedCaps = ['l3cat', 'mba', 'sstbf', 'power'];

      MockInstance(LocalService, 'getCapsEvent', () => of(mockedCaps));
      MockRender(OverviewComponent);

      const expectValue = ngMocks.find('app-l3-cache-allocation');

      expect(expectValue).toBeTruthy();
    });
  });

  describe('when initialized and L3 CAT is NOT supported', () => {
    it('should NOT render L3CacheAllocationComponent', () => {
      const mockedCaps = ['l2cat', 'mba', 'sstbf', 'power'];

      MockInstance(LocalService, 'getCapsEvent', () => of(mockedCaps));
      MockRender(OverviewComponent);

      const expectValue = ngMocks.find('app-l3-cache-allocation', null);

      expect(expectValue).toBeNull();
    });
  });

  describe('when initialized and L2 CAT is supported', () => {
    it('should render L2CacheAllocationComponent', () => {
      const mockedCaps = ['l2cat', 'mba', 'sstbf', 'power'];

      MockInstance(LocalService, 'getCapsEvent', () => of(mockedCaps));
      MockRender(OverviewComponent);

      const expectValue = ngMocks.find('app-l2-cache-allocation');

      expect(expectValue).toBeTruthy();
    });
  });

  describe('when initialized and L2 CAT is NOT supported', () => {
    it('should NOT render L2CacheAllocationComponent', () => {
      const mockedCaps = ['l3cat', 'mba', 'sstbf', 'power'];

      MockInstance(LocalService, 'getCapsEvent', () => of(mockedCaps));
      MockRender(OverviewComponent);

      const expectValue = ngMocks.find('app-l2-cache-allocation', null);

      expect(expectValue).toBeNull();
    });
  });

  describe('when initialized and MBA is supported', () => {
    it('should render MbaAllocationComponent', () => {
      const mockedCaps = ['l3cat', 'mba', 'sstbf', 'power'];

      MockInstance(LocalService, 'getCapsEvent', () => of(mockedCaps));
      MockRender(OverviewComponent);

      const expectValue = ngMocks.find('app-mba-allocation');

      expect(expectValue).toBeTruthy();
    });
  });

  describe('when initialized and MBA is NOT supported', () => {
    it('should NOT render MbaAllocationComponent', () => {
      const mockedCaps = ['l3cat', 'l2cat', 'sstbf', 'power'];

      MockInstance(LocalService, 'getCapsEvent', () => of(mockedCaps));
      MockRender(OverviewComponent);

      const expectValue = ngMocks.find('app-mba-allocation', null);

      expect(expectValue).toBeNull();
    });
  });

  describe('when mbaOnChange method is called', () => {
    it('it should call mbaCtrlPut with correct value', () => {
      const mockResponse = 'MBA controller modified';
      const mbaCtrlSpy = jasmine.createSpy('mbaCtrlPut');
      const event: MatSlideToggleChange = {
        source: {} as MatSlideToggle,
        checked: true,
      };

      MockInstance(AppqosService, 'mbaCtrlPut', mbaCtrlSpy)
        .withArgs(event.checked)
        .and.returnValue(of(mockResponse));

      const {
        point: { componentInstance: component },
      } = MockRender(OverviewComponent);

      component.mbaOnChange(event);

      expect(mbaCtrlSpy).toHaveBeenCalledWith(event.checked);
    });

    it('it should handle error', () => {
      const mockedError: Error = {
        name: 'Error',
        message: 'rest API error'
      };

      const handleErrorSpy = jasmine.createSpy('handleError');

      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      const mbaCtrlSpy = jasmine.createSpy('mbaCtrlPut');
      const event: MatSlideToggleChange = {
        source: {} as MatSlideToggle,
        checked: true,
      };

      MockInstance(AppqosService, 'mbaCtrlPut', mbaCtrlSpy)
        .withArgs(event.checked)
        .and.returnValue(throwError(() => mockedError));

      const {
        point: { componentInstance: component },
      } = MockRender(OverviewComponent);

      component.mbaOnChange(event);

      expect(mbaCtrlSpy).toHaveBeenCalledWith(event.checked);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
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

      const fixture = MockRender(OverviewComponent);
      const component = fixture.point.componentInstance;

      component.sstbfOnChange(event);

      expect(sstbfPutSpy).toHaveBeenCalledWith(event.checked);
    });

    it('it should catch error', () => {
      const handleErrorSpy = jasmine.createSpy();
      const sstbfPutSpy = jasmine.createSpy();
      const event: MatSlideToggleChange = {
        source: {} as MatSlideToggle,
        checked: true,
      };

      MockInstance(SnackBarService, 'handleError', handleErrorSpy);
      MockInstance(AppqosService, 'sstbfPut', sstbfPutSpy)
        .withArgs(event.checked)
        .and.returnValue(throwError(() => mockedError));

      const fixture = MockRender(OverviewComponent);
      const component = fixture.point.componentInstance;

      component.sstbfOnChange(event);

      expect(sstbfPutSpy).toHaveBeenCalledWith(event.checked);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    });

    it('it should call getSstbf', () => {
      const mockResponse = 'SST-BF caps modified';
      const sstbfPutSpy = jasmine.createSpy('sstbfPutSpy');
      const getSstbfSpy = jasmine.createSpy('getSstbfSpy')
        .and.returnValue(of(mockedSSTBF));
      const event: MatSlideToggleChange = {
        source: {} as MatSlideToggle,
        checked: true,
      };

      // MockInstance(LocalService, 'getCapsEvent', () => of(mockedCaps));
      MockInstance(AppqosService, 'getSstbf', getSstbfSpy);
      MockInstance(AppqosService, 'sstbfPut', sstbfPutSpy)
        .withArgs(event.checked)
        .and.returnValue(of(mockResponse));

      const fixture = MockRender(OverviewComponent);
      const component = fixture.point.componentInstance;

      component.sstbfOnChange(event);

      expect(sstbfPutSpy).toHaveBeenCalledWith(event.checked);
      expect(getSstbfSpy).toHaveBeenCalledTimes(1);
    });

    it('it should handle getSstbf error', () => {
      const mockResponse = 'SST-BF caps modified';
      const sstbfPutSpy = jasmine.createSpy('sstbfPutSpy');
      const getSstbfSpy = jasmine.createSpy('getSstbfSpy');
      const handleErrorSpy = jasmine.createSpy('handleErrorSpy');

      const event: MatSlideToggleChange = {
        source: {} as MatSlideToggle,
        checked: true,
      };

      MockInstance(SnackBarService, 'handleError', handleErrorSpy);
      MockInstance(AppqosService, 'getSstbf', getSstbfSpy)
        .and.returnValue(throwError(() => mockedError));
      MockInstance(AppqosService, 'sstbfPut', sstbfPutSpy)
        .withArgs(event.checked)
        .and.returnValue(of(mockResponse));

      const fixture = MockRender(OverviewComponent);
      const component = fixture.point.componentInstance;

      component.sstbfOnChange(event);

      expect(sstbfPutSpy).toHaveBeenCalledWith(event.checked);
      expect(getSstbfSpy).toHaveBeenCalledTimes(1);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    });
  });
});
