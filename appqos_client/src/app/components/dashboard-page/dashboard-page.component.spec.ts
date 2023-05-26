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

import { Router } from '@angular/router';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';

import { SharedModule } from 'src/app/shared/shared.module';
import { DashboardPageComponent } from './dashboard-page.component';
import { AppqosService } from 'src/app/services/appqos.service';
import { EMPTY, of, throwError } from 'rxjs';
import { LocalService } from 'src/app/services/local.service';
import { Caps, CacheAllocation, MBA, MBACTRL, SSTBF, RDTIface } from '../system-caps/system-caps.model';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { Apps, Pools } from '../overview/overview.model';

describe('Given DashboardPageComponent', () => {
  beforeEach(() =>
    MockBuilder(DashboardPageComponent)
      .mock(SharedModule)
      .mock(Router)
      .mock(AppqosService, {
        getCaps: () => of(mockedCaps),
        getL2cat: () => of(mockedCache),
        getL3cat: () => of(mockedCache),
        getMba: () => of(mockedMba),
        getMbaCtrl: () => of(mockedMbaCtrl),
        getSstbf: () => of(mockedSSTBF),
        getRdtIface: () => EMPTY,
        getPools: () => EMPTY,
        getApps: () => EMPTY
      })
      .mock(LocalService)
  );

  const mockedCaps: Caps = {
    capabilities: ['l3cat', 'l2cat', 'mba', 'sstbf']
  };

  const mockedCache: CacheAllocation = {
    cache_size: 44040192,
    cdp_enabled: false,
    cdp_supported: false,
    clos_num: 15,
    cw_num: 12,
    cw_size: 3670016
  };

  const mockedMba: MBA = {
    mba_enabled: false,
    mba_bw_enabled: false,
    clos_num: 7
  };

  const mockedMbaCtrl: MBACTRL = {
    enabled: false,
    supported: true
  };

  const mockedSSTBF: SSTBF = {
    configured: true,
    hp_cores: [1, 2],
    std_cores: [1, 2],
  };

  const mockedError: Error = {
    name: 'Error',
    message: 'rest API error',
  };

  const mockedRDT: RDTIface = {
    interface: 'os',
    interface_supported: ['msr', 'os'],
  };

  const mockedPool: Pools[] = [
    {
      id: 0,
      mba_bw: 4294967295,
      l3cbm: 2047,
      name: 'Default',
      cores: [0, 1, 45, 46, 47],
    },
  ];

  const mockedApps: Apps[] = [
    {
      id: 1,
      name: 'test',
      pids: [1, 2, 3],
      pool_id: 0,
    },
  ];

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should render SystemCapsComponent', () => {
      MockRender(DashboardPageComponent);

      const expectValue = ngMocks.find('app-system-caps');

      expect(expectValue).toBeTruthy();
    });

    it('it should not render RapConfigComponent', () => {
      MockRender(DashboardPageComponent);

      const expectValue = ngMocks.find('app-rap-config', null);

      expect(expectValue).toBeNull();
    });

    it('it should call GET caps', () => {
      const getCapsSpy = jasmine.createSpy('getCapsSpy')
        .and.returnValue(of(mockedCaps));

      MockInstance(AppqosService, 'getCaps', getCapsSpy);

      MockRender(DashboardPageComponent);

      expect(getCapsSpy).toHaveBeenCalledTimes(1);
    });

    it('it should handle GET caps error', () => {
      const getCapsSpy = jasmine.createSpy('getCapsSpy')
        .and.returnValue(throwError(() => mockedError));
      const handleErrorSpy = jasmine.createSpy('handleErrorSpy');
      const navigateSpy = jasmine.createSpy('navigateSpy');

      MockInstance(AppqosService, 'getCaps', getCapsSpy);
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);
      MockInstance(Router, 'navigate', navigateSpy);

      MockRender(DashboardPageComponent);

      expect(getCapsSpy).toHaveBeenCalledTimes(1);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
      expect(navigateSpy).toHaveBeenCalledOnceWith(['/login']);
    });

    it('it should call GET rdt interface', () => {
      const getRdtIfaceSpy = jasmine.createSpy('getRdtIfaceSpy')
        .and.returnValue(of(mockedRDT));

      MockInstance(AppqosService, 'getRdtIface', getRdtIfaceSpy);

      MockRender(DashboardPageComponent);

      expect(getRdtIfaceSpy).toHaveBeenCalledTimes(1);
    });

    it('it should handle GET rdt interface error', () => {
      const getRdtIfaceSpy = jasmine.createSpy('getRdtIfaceSpy')
        .and.returnValue(throwError(() => mockedError));
      const handleErrorSpy = jasmine.createSpy('handleErrorSpy');

      MockInstance(AppqosService, 'getRdtIface', getRdtIfaceSpy);
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      MockRender(DashboardPageComponent);

      expect(getRdtIfaceSpy).toHaveBeenCalledTimes(1);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    });

    it('it should call GET pools', () => {
      const getPoolsSpy = jasmine.createSpy('getPoolsSpy')
        .and.returnValue(of(mockedPool));

      MockInstance(AppqosService, 'getPools', getPoolsSpy);

      MockRender(DashboardPageComponent);

      expect(getPoolsSpy).toHaveBeenCalledTimes(1);
    });

    it('it should handle GET pools error', () => {
      const getPoolsSpy = jasmine.createSpy('getPoolsSpy')
        .and.returnValue(throwError(() => mockedError));
      const handleErrorSpy = jasmine.createSpy('handleErrorSpy');

      MockInstance(AppqosService, 'getPools', getPoolsSpy);
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      MockRender(DashboardPageComponent);

      expect(getPoolsSpy).toHaveBeenCalledTimes(1);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    });

    it('it should call GET Apps', () => {
      const getAppsSpy = jasmine.createSpy('getAppsSpy')
        .and.returnValue(of(mockedApps));

      MockInstance(AppqosService, 'getApps', getAppsSpy);

      MockRender(DashboardPageComponent);

      expect(getAppsSpy).toHaveBeenCalledTimes(1);
    });

    it('it should call GET L3cat if supported', () => {
      const getL3catSpy = jasmine.createSpy('getL3catSpy')
        .and.returnValue(of(mockedCache));

      MockInstance(AppqosService, 'getL3cat', getL3catSpy);

      MockRender(DashboardPageComponent);

      expect(getL3catSpy).toHaveBeenCalledTimes(1);
    });

    it('it should not call GET L3cat if not supported', () => {
      const mockedCaps: Caps = {
        capabilities: ['l2cat', 'l2cdp', 'mba', 'sstbf']
      };

      const getL3catSpy = jasmine.createSpy('getL3catSpy')
        .and.returnValue(of(mockedCache));

      MockInstance(AppqosService, 'getCaps', () => of(mockedCaps));
      MockInstance(AppqosService, 'getL3cat', getL3catSpy);

      MockRender(DashboardPageComponent);

      expect(getL3catSpy).toHaveBeenCalledTimes(0);
    });

    it('it should handle GET L3cat error', () => {
      const getL3catSpy = jasmine.createSpy('getL3catSpy')
        .and.returnValue(throwError(() => mockedError));
      const handleErrorSpy = jasmine.createSpy('handleErrorSpy');

      MockInstance(AppqosService, 'getL3cat', getL3catSpy);
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      MockRender(DashboardPageComponent);

      expect(getL3catSpy).toHaveBeenCalledTimes(1);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    });

    it('it should call GET L2cat if supported', () => {
      const getL2catSpy = jasmine.createSpy('getL2catSpy')
        .and.returnValue(of(mockedCache));

      MockInstance(AppqosService, 'getL2cat', getL2catSpy);

      MockRender(DashboardPageComponent);

      expect(getL2catSpy).toHaveBeenCalledTimes(1);
    });

    it('it should not call GET L2cat if not supported', () => {
      const mockedCaps: Caps = {
        capabilities: ['l3cat', 'l3cdp', 'mba', 'sstbf']
      };

      const getL2catSpy = jasmine.createSpy('getL2catSpy')
        .and.returnValue(of(mockedCache));

      MockInstance(AppqosService, 'getCaps', () => of(mockedCaps));
      MockInstance(AppqosService, 'getL2cat', getL2catSpy);

      MockRender(DashboardPageComponent);

      expect(getL2catSpy).toHaveBeenCalledTimes(0);
    });

    it('it should handle GET L2cat error', () => {
      const getL2catSpy = jasmine.createSpy('getL2catSpy')
        .and.returnValue(throwError(() => mockedError));
      const handleErrorSpy = jasmine.createSpy('handleErrorSpy');

      MockInstance(AppqosService, 'getL2cat', getL2catSpy);
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      MockRender(DashboardPageComponent);

      expect(getL2catSpy).toHaveBeenCalledTimes(1);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    });

    it('it should call GET Sstbf if supported', () => {
      const getSstbfSpy = jasmine.createSpy('getSstbfSpy')
        .and.returnValue(of(mockedSSTBF));

      MockInstance(AppqosService, 'getSstbf', getSstbfSpy);

      MockRender(DashboardPageComponent);

      expect(getSstbfSpy).toHaveBeenCalledTimes(1);
    });

    it('it should not call GET Sstbf if not supported', () => {
      const mockedCaps: Caps = {
        capabilities: ['l3cat', 'l3cdp', 'l2cat', 'mba']
      };

      const getSstbfSpy = jasmine.createSpy('getSstbfSpy')
        .and.returnValue(of(mockedSSTBF));

      MockInstance(AppqosService, 'getCaps', () => of(mockedCaps));
      MockInstance(AppqosService, 'getSstbf', getSstbfSpy);

      MockRender(DashboardPageComponent);

      expect(getSstbfSpy).toHaveBeenCalledTimes(0);
    });

    it('it should handle GET Sstbf error', () => {
      const getSstbfSpy = jasmine.createSpy('getSstbfSpy')
        .and.returnValue(throwError(() => mockedError));
      const handleErrorSpy = jasmine.createSpy('handleErrorSpy');

      MockInstance(AppqosService, 'getSstbf', getSstbfSpy);
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      MockRender(DashboardPageComponent);

      expect(getSstbfSpy).toHaveBeenCalledTimes(1);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    });

    it('it should call GET Mba if supported', () => {
      const getMbaSpy = jasmine.createSpy('getMbaSpy')
        .and.returnValue(of(mockedMba));
      const getMbaCtrlSpy = jasmine.createSpy('getMbaCrtrlSpy')
        .and.returnValue(of(mockedMbaCtrl));

      MockInstance(AppqosService, 'getMba', getMbaSpy);
      MockInstance(AppqosService, 'getMbaCtrl', getMbaCtrlSpy);

      MockRender(DashboardPageComponent);

      expect(getMbaSpy).toHaveBeenCalledTimes(1);
      expect(getMbaCtrlSpy).toHaveBeenCalledTimes(1);
    });

    it('it should not call GET Mba if not supported', () => {
      const mockedCaps: Caps = {
        capabilities: ['l3cat', 'l3cdp', 'l2cat', 'l2cdp']
      };

      const getMbaSpy = jasmine.createSpy('getMbaSpy')
        .and.returnValue(of(mockedMba));
      const getMbaCtrlSpy = jasmine.createSpy('getMbaCrtrlSpy')
        .and.returnValue(of(mockedMbaCtrl));

      MockInstance(AppqosService, 'getCaps', () => of(mockedCaps));
      MockInstance(AppqosService, 'getMba', getMbaSpy);
      MockInstance(AppqosService, 'getMbaCtrl', getMbaCtrlSpy);

      MockRender(DashboardPageComponent);

      expect(getMbaSpy).toHaveBeenCalledTimes(0);
      expect(getMbaCtrlSpy).toHaveBeenCalledTimes(0);
    });

    it('it should handle GET Mba error', () => {
      const mockedMbaError: Error = {
        name: 'Error',
        message: 'Mba GET Error',
      };
      const mockedMbaCtrlError: Error = {
        name: 'Error',
        message: 'MbaCtrl GET Error',
      };

      const getMbaSpy = jasmine.createSpy('getMbaSpy')
        .and.returnValue(throwError(() => mockedMbaError));
      const getMbaCtrlSpy = jasmine.createSpy('getMbaCtrlSpy')
        .and.returnValue(throwError(() => mockedMbaCtrlError));

      const handleErrorSpy = jasmine.createSpy('handleErrorSpy');

      MockInstance(AppqosService, 'getMba', getMbaSpy);
      MockInstance(AppqosService, 'getMbaCtrl', getMbaCtrlSpy);
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      MockRender(DashboardPageComponent);

      expect(getMbaSpy).toHaveBeenCalledTimes(1);
      expect(getMbaCtrlSpy).toHaveBeenCalledTimes(1);

      expect(handleErrorSpy).toHaveBeenCalledWith(mockedMbaError.message);
      expect(handleErrorSpy).toHaveBeenCalledWith(mockedMbaCtrlError.message);
    });
  });

  describe('when OVERVIEW button clicked', () => {
    it('should display Overview content', () => {
      const fixture = MockRender(DashboardPageComponent);

      const toolbar = ngMocks.find('app-toolbar');

      toolbar.triggerEventHandler('switcher', true);

      fixture.detectChanges();

      const overview = ngMocks.find('app-overview');

      expect(overview).toBeTruthy();
    });

    it('should display NOT RAP Config content', () => {
      const fixture = MockRender(DashboardPageComponent);

      const toolbar = ngMocks.find('app-toolbar');

      toolbar.triggerEventHandler('switcher', true);

      fixture.detectChanges();

      const rapConfig = ngMocks.find('app-rap-config', null);

      expect(rapConfig).toBeNull();
    });
  });

  describe('when RAP CONFIG button clicked', () => {
    it('should display RAP Config content', () => {
      const fixture = MockRender(DashboardPageComponent);

      const toolbar = ngMocks.find('app-toolbar');

      toolbar.triggerEventHandler('switcher', false);

      fixture.detectChanges();

      const rapConfig = ngMocks.find('app-rap-config');

      expect(rapConfig).toBeTruthy();
    });

    it('should NOT display Overview content', () => {
      const fixture = MockRender(DashboardPageComponent);

      const toolbar = ngMocks.find('app-toolbar');

      toolbar.triggerEventHandler('switcher', false);

      fixture.detectChanges();

      const overview = ngMocks.find('app-overview', null);

      expect(overview).toBeNull();
    });
  });
});
