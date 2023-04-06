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
import { of, EMPTY, Subject, throwError } from 'rxjs';
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
        getIfaceEvent: () => EMPTY,
        getL3CatEvent: () => EMPTY
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
    MockInstance(AppqosService, 'getPools', () => of(mockedPools));
    MockInstance(AppqosService, 'getApps', () => of(mockedApps));

    it('should populate list of apps and pools', () => {
      const component = MockRender(RapConfigComponent).point.componentInstance;

      expect(component.apps)
        .withContext('should get list of apps')
        .toEqual(mockedApps);

      expect(component.pools)
        .withContext('should get list of pools')
        .toEqual(mockedPools);
    });

    it('should subscribe to localService.getIfaceEvent()', () => {
      // create mock observable and spies
      const mockGetIFaceEvent$ = new Subject<void>();
      const getConfigDataSpy = jasmine.createSpy();
      const getIfaceEvent =
        jasmine.createSpy().and.returnValue(mockGetIFaceEvent$);

      // replace localService.getIFaceEvent() with spy
      MockInstance(LocalService, () => ({ getIfaceEvent }));

      const fixture = MockRender(RapConfigComponent);

      // replace getConfigData() with spy
      fixture.point.componentInstance.getConfigData = getConfigDataSpy;

      mockGetIFaceEvent$.next();

      expect(fixture.point.componentInstance.getConfigData)
        .withContext('should call getConfigData when event emitted')
        .toHaveBeenCalledTimes(1);
    })

    it('should subscribe to getL3CatEvent', () => {
      const mockGetl3CatEvent = new Subject<void>();
      const getConfigDataSpy = jasmine.createSpy();
      const getl3CatEventSpy =
        jasmine.createSpy().and.returnValue(mockGetl3CatEvent);

      MockInstance(LocalService, 'getL3CatEvent', getl3CatEventSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(RapConfigComponent);

      component.getConfigData = getConfigDataSpy;

      mockGetl3CatEvent.next();

      expect(component.getConfigData)
        .withContext('should call getConfigData when event emitted')
        .toHaveBeenCalledTimes(1);
    })
  });

  describe('when initialized with errors', () => {
    it('should handle errors', () => {
      // set mock app and pool data to be returned from AppqosService
      const getPoolsSpy = jasmine.createSpy().and
        .returnValue(throwError(() => new Error('getPools error')));
      const getAppsSpy = jasmine.createSpy().and
        .returnValue(throwError(() => new Error('getApps error')));

      MockInstance(AppqosService, 'getPools', getPoolsSpy);
      MockInstance(AppqosService, 'getApps', getAppsSpy);

      const fixture = MockRender(RapConfigComponent);
      const component = fixture.point.componentInstance;
      const emptyApps: Apps[] = [];
      const emptyPools: Pools[] = [];

      // very getPools() and getApps() called during gnOnInit()
      expect(getPoolsSpy).toHaveBeenCalledTimes(1);
      expect(getAppsSpy).toHaveBeenCalledTimes(1);

      // verify errors were handled correctly
      expect(component.apps)
        .withContext('should return empty apps list on error')
        .toEqual(emptyApps);

      expect(component.pools)
        .withContext('should return empty pools list on error')
        .toEqual(emptyPools);
    });
  });
});
