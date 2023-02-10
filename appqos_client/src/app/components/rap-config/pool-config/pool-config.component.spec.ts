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
import { PoolConfigComponent } from './pool-config.component';
import { CacheAllocation } from '../../system-caps/system-caps.model';
import { Pools, Apps } from '../../overview/overview.model';
import { SharedModule } from 'src/app/shared/shared.module';
import { AppqosService } from 'src/app/services/appqos.service';
import { BehaviorSubject, EMPTY, of } from 'rxjs';
import { LocalService } from 'src/app/services/local.service';

describe('Given poolConfigComponent', () => {
  beforeEach(() => {
    return MockBuilder(PoolConfigComponent)
      .mock(SharedModule)
      .mock(AppqosService, {
        getPools: () => EMPTY
      })
      .mock(LocalService, {
        getL3CatEvent: () => EMPTY,
        getL2CatEvent: () => EMPTY
      })
  })

  const mockedPools: Pools[] = [
    { id: 0, name: 'pool_0', cores: [1, 2, 3] },
    { id: 1, name: 'pool_1', cores: [4, 5, 6] },
    { id: 2, name: 'pool_2', cores: [7, 8, 9] },
    { id: 3, name: 'pool_3', cores: [10, 11, 12] }
  ];

  const mockedCache: CacheAllocation = {
    cache_size: 44040192,
    cdp_enabled: false,
    cdp_supported: false,
    clos_num: 15,
    cw_num: 12,
    cw_size: 3670016
  }

  const params = {
    pools: mockedPools,
    apps: []
  }

  MockInstance.scope('case');

  describe('when getPool method is called', () => {
    it('it should update selected pool', () => {
      const poolID = 0;
      const mockedL3Bitmask = [0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1];
      const mockedL2BitMask = [0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1];

      const mockedPools: Pools[] = [
        { id: 0, name: 'pool_0', cores: [1, 2, 3], l3cbm: 127, l2cbm: 7 },
      ];

      const params = {
        pools: mockedPools,
        apps: []
      }

      const catEvent = new BehaviorSubject<CacheAllocation>(mockedCache);

      MockInstance(LocalService, 'getL3CatEvent', () => catEvent);
      MockInstance(LocalService, 'getL2CatEvent', () => catEvent);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.getPool(poolID);
      expect(component.selected).toBe(mockedPools[poolID].name);
      expect(component.pool.l3Bitmask).toEqual(mockedL3Bitmask);
      expect(component.pool.l2Bitmask).toEqual(mockedL2BitMask);
    })
  })
});