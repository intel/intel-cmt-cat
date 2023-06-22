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

import { AppqosService } from 'src/app/services/appqos.service';
import { RapConfigComponent } from './rap-config.component';
import { MockBuilder, MockInstance, MockRender } from 'ng-mocks';
import { Pools, Apps } from '../overview/overview.model';
import { LocalService } from 'src/app/services/local.service';
import { of, EMPTY, throwError } from 'rxjs';
import { SharedModule } from 'src/app/shared/shared.module';

describe('Given RapConfigComponent', () => {
  MockInstance.scope('suite');

  // create test component and mock required dependencies
  beforeEach(() => {
    return MockBuilder(RapConfigComponent)
      .mock(SharedModule)
      .mock(AppqosService, {
        getPools: () => EMPTY,
        getApps: () => EMPTY,
      })
      .mock(LocalService, {
        getRdtIfaceEvent: () => EMPTY,
        getL3CatEvent: () => EMPTY,
        getL2CatEvent: () => EMPTY,
        getPoolsEvent: () => EMPTY,
        getAppsEvent: () => EMPTY
      });
  });

  // preserve initial component state for each child 'describe' function
  MockInstance.scope('case');

  describe('when initialized', () => {
    // define mocked API response data
    const mockedPools: Pools[] = [
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

    // set mock app and pool data to be returned from AppqosService
    MockInstance(LocalService, 'getPoolsEvent', () => of(mockedPools));
    MockInstance(LocalService, 'getAppsEvent', () => of(mockedApps));

    it('should populate list of apps and pools', () => {
      const component = MockRender(RapConfigComponent).point.componentInstance;

      expect(component.apps)
        .withContext('should get list of apps')
        .toEqual(mockedApps);

      expect(component.pools)
        .withContext('should get list of pools')
        .toEqual(mockedPools);
    });
  });

  describe('when initialized with errors', () => {
    it('should handle errors', () => {
      // set mock app and pool data to be returned from AppqosService
      const getPoolsSpy = jasmine.createSpy().and
        .returnValue(throwError(() => new Error('getPools error')));
      const getAppsSpy = jasmine.createSpy().and
        .returnValue(throwError(() => new Error('getApps error')));
      const setAppsEventSpy = jasmine.createSpy('setAppsEventSpy');
      const setPoolsEventSpy = jasmine.createSpy('setPoolsEventSpy');

      MockInstance(AppqosService, 'getPools', getPoolsSpy);
      MockInstance(AppqosService, 'getApps', getAppsSpy);
      MockInstance(LocalService, 'setAppsEvent', setAppsEventSpy);
      MockInstance(LocalService, 'setPoolsEvent', setPoolsEventSpy);

      const fixture = MockRender(RapConfigComponent);
      const component = fixture.point.componentInstance;
      const emptyApps: Apps[] = [];
      const emptyPools: Pools[] = [];

      component.getConfigData();

      // verify errors were handled correctly
      expect(setAppsEventSpy).toHaveBeenCalledOnceWith(emptyApps);

      expect(setPoolsEventSpy).toHaveBeenCalledOnceWith(emptyPools);
    });
  });
});
