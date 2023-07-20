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
import { MatDialog } from '@angular/material/dialog';
import { Router } from '@angular/router';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { first, of, tap, throwError } from 'rxjs';
import { LocalService } from 'src/app/services/local.service';

import { SharedModule } from 'src/app/shared/shared.module';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { Apps, Pools } from '../overview/overview.model';
import {
  AppqosConfig,
  MBACTRL,
  PowerProfiles,
  RDTIface,
  SSTBF,
} from '../system-caps/system-caps.model';
import { ToolbarComponent } from './toolbar.component';

describe('Given ToolbarComponent', () => {
  beforeEach(() =>
    MockBuilder(ToolbarComponent)
      .mock(Router)
      .mock(SharedModule)
      .mock(SnackBarService)
      .mock(LocalService, {
        getCapsEvent: () => of([]),
        getRdtIfaceEvent: () => of(mockedIface),
        getMbaCtrlEvent: () => of(null),
        getPoolsEvent: () => of(mockedPools),
        getPowerProfilesEvent: () => of([]),
        getAppsEvent: () => of([]),
        getSstbfEvent: () => of(null),
      }));

  const mockedIface: RDTIface = {
    interface: 'msr',
    interface_supported: ['msr', 'os'],
  };

  const mockedSSTBF: SSTBF = {
    configured: false,
    hp_cores: [2, 3],
    std_cores: [0, 1],
  };

  const mockedApps: Apps[] = [
    {
      id: 1,
      name: 'app1',
      pids: [1, 2, 3],
      pool_id: 0,
    },
  ];

  const mockedPools: Pools[] = [
    {
      id: 0,
      l3cbm: 4095,
      name: 'Default',
      cores: [0, 1, 2, 3],
    },
  ];

  const mockedPowerProfiles: PowerProfiles[] = [
    {
      id: 0,
      name: 'profile_0',
      min_freq: 1000,
      max_freq: 1200,
      epp: 'balance_power',
    },
  ];

  const mockedMbaCtrl: MBACTRL = {
    enabled: false,
    supported: false,
  };

  const baseConfig: AppqosConfig = {
    rdt_iface: {
      interface: 'msr',
    },
    apps: [],
    pools: [
      {
        id: 0,
        l3cbm: 4095,
        name: 'Default',
        cores: [0, 1, 2, 3],
      },
    ],
  };

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

  describe('when show config button is clicked with only l3cat support', () => {
    it('should display l3cat only config', () => {
      MockInstance(LocalService, 'getCapsEvent', () => of(['l3cat']));
      // verify changes dispayed
      const matDialogSpy = jasmine.createSpy('open');
      MockInstance(MatDialog, 'open', matDialogSpy);

      const fixture = MockRender(ToolbarComponent);
      const component = fixture.componentInstance;

      spyOn(component, 'openDialog').and.callThrough();
      spyOn(component, 'showConfig').and.callThrough();

      const showConfigButton = ngMocks.find('#show-config-button');
      showConfigButton.triggerEventHandler('click', null);

      expect(matDialogSpy).toHaveBeenCalledTimes(1);
      expect(component.showConfig).toHaveBeenCalledTimes(1);
      expect(component.openDialog).toHaveBeenCalledWith(
        JSON.stringify(baseConfig, null, 2)
      );
    });
  });

  describe('when show config button is clicked with power support (no profiles defined)', () => {
    it('should display config with power settings (without profiles)', () => {
      MockInstance(LocalService, 'getCapsEvent', () => of(['l3cat', 'power']));

      // add expected output with power support when no profiles defined
      const config: AppqosConfig = {
        ...baseConfig,
        power_profiles_expert_mode: true,
        power_profiles_verify: true
      };

      // verify changes dispayed
      const fixture = MockRender(ToolbarComponent);
      const component = fixture.componentInstance;

      spyOn(component, 'openDialog').and.callThrough();

      const showConfigButton = ngMocks.find('#show-config-button');
      showConfigButton.triggerEventHandler('click', null);

      expect(component.openDialog).toHaveBeenCalledWith(
        JSON.stringify(config, null, 2)
      );
    });
  });

  describe('when show config button is clicked with power support', () => {
    it('should display config with power settings (with profiles)', () => {
      MockInstance(LocalService, 'getCapsEvent', () => of(['l3cat', 'power']));
      MockInstance(LocalService, 'getPowerProfilesEvent', () =>
        of(mockedPowerProfiles)
      );

      // add expected output with power support when profiles are defined
      const config: AppqosConfig = {
        ...baseConfig,
        power_profiles: [...mockedPowerProfiles],
        power_profiles_expert_mode: true,
        power_profiles_verify: true
      };

      // verify changes dispayed
      const fixture = MockRender(ToolbarComponent);
      const component = fixture.componentInstance;

      spyOn(component, 'openDialog').and.callThrough();

      const showConfigButton = ngMocks.find('#show-config-button');
      showConfigButton.triggerEventHandler('click', null);

      expect(component.openDialog).toHaveBeenCalledWith(
        JSON.stringify(config, null, 2)
      );
    });
  });

  describe('when show config button is clicked with sstbf support', () => {
    it('should display config with sstbf settings', () => {
      MockInstance(LocalService, 'getCapsEvent', () => of(['l3cat', 'sstbf']));
      MockInstance(LocalService, 'getSstbfEvent', () =>
        of(mockedSSTBF)
      );

      // add sstbf support to expected output
      const config: AppqosConfig = {
        ...baseConfig,
        sstbf: {
          configured: mockedSSTBF.configured
        }
      };

      // verify changes dispayed
      const fixture = MockRender(ToolbarComponent);
      const component = fixture.componentInstance;

      spyOn(component, 'openDialog').and.callThrough();

      const showConfigButton = ngMocks.find('#show-config-button');
      showConfigButton.triggerEventHandler('click', null);

      expect(component.openDialog).toHaveBeenCalledWith(
        JSON.stringify(config, null, 2)
      );
    });
  });

  describe('when show config button is clicked with mba controller support', () => {
    it('should display config with mba controller settings', () => {
      MockInstance(LocalService, 'getCapsEvent', () => of(['l3cat', 'sstbf', 'mba']));
      MockInstance(LocalService, 'getMbaCtrlEvent', () => of(mockedMbaCtrl));

      // add mba ctrl support to expected output
      const config: AppqosConfig = {
        ...baseConfig,
        mba_ctrl: { enabled: mockedMbaCtrl.enabled }
      };

      // verify changes dispayed
      const fixture = MockRender(ToolbarComponent);
      const component = fixture.componentInstance;

      spyOn(component, 'openDialog').and.callThrough();

      const showConfigButton = ngMocks.find('#show-config-button');
      showConfigButton.triggerEventHandler('click', null);

      expect(component.openDialog).toHaveBeenCalledWith(
        JSON.stringify(config, null, 2)
      );
    });
  });

  describe('when show config button is clicked with apps defined', () => {
    it('should display config with app configs', () => {
      MockInstance(LocalService, 'getCapsEvent', () => of(['l3cat']));
      MockInstance(LocalService, 'getAppsEvent', () => of(mockedApps));

      // add app definitions to expected output
      const config: AppqosConfig = {
        ...baseConfig,
        apps: mockedApps
      };

      // verify changes dispayed
      const fixture = MockRender(ToolbarComponent);
      const component = fixture.componentInstance;

      spyOn(component, 'openDialog').and.callThrough();

      const showConfigButton = ngMocks.find('#show-config-button');
      showConfigButton.triggerEventHandler('click', null);

      expect(component.openDialog).toHaveBeenCalledWith(
        JSON.stringify(config, null, 2)
      );
    });
  });

  describe('when show config button is clicked with support for multiple technologies', () => {
    it('should display config with settings for all supported technologies', () => {
      MockInstance(LocalService, 'getCapsEvent', () => of(['l3cat', 'sstbf', 'mba', 'sstbf', 'power']));
      MockInstance(LocalService, 'getMbaCtrlEvent', () => of(mockedMbaCtrl));
      MockInstance(LocalService, 'getSstbfEvent', () => of(mockedSSTBF));
      MockInstance(LocalService, 'getPowerProfilesEvent', () => of(mockedPowerProfiles));
      MockInstance(LocalService, 'getAppsEvent', () => of(mockedApps));

      // extend expected output
      const config: AppqosConfig = {
        ...baseConfig,
        apps: mockedApps,
        power_profiles: [...mockedPowerProfiles],
        power_profiles_expert_mode: true,
        power_profiles_verify: true,
        sstbf: {
          configured: mockedSSTBF.configured
        },
        mba_ctrl: { enabled: mockedMbaCtrl.enabled }
      };

      // verify changes dispayed
      const fixture = MockRender(ToolbarComponent);
      const component = fixture.componentInstance;

      spyOn(component, 'openDialog').and.callThrough();

      const showConfigButton = ngMocks.find('#show-config-button');
      showConfigButton.triggerEventHandler('click', null);

      expect(component.openDialog).toHaveBeenCalledWith(
        JSON.stringify(config, null, 2)
      );
    });
  });

  describe('when show config button is clicked and an error occurs', () => {
    it('should display error message to the user', () => {
      const handleErrorSpy = jasmine.createSpy();
      MockInstance(LocalService, 'getCapsEvent', () => of(['l3cat', 'sstbf', 'mba']));
      MockInstance(LocalService, 'getMbaCtrlEvent', () => throwError(() => new Error('error')));
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      // verify error message dispayed
      const fixture = MockRender(ToolbarComponent);
      const component = fixture.componentInstance;

      spyOn(component, 'openDialog').and.callThrough();

      const showConfigButton = ngMocks.find('#show-config-button');
      showConfigButton.triggerEventHandler('click', null);

      expect(handleErrorSpy).toHaveBeenCalledOnceWith('Failed to generate configuration');

      expect(component.openDialog).not.toHaveBeenCalled();
    });
  });
});
