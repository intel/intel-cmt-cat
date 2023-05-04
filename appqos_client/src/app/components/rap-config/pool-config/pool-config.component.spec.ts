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
import { CacheAllocation, resMessage } from '../../system-caps/system-caps.model';
import { Pools } from '../../overview/overview.model';
import { SharedModule } from 'src/app/shared/shared.module';
import { AppqosService } from 'src/app/services/appqos.service';
import { BehaviorSubject, EMPTY, of, throwError } from 'rxjs';
import { LocalService } from 'src/app/services/local.service';
import { MatOptionSelectionChange, MatOption } from '@angular/material/core';
import { MatSliderModule } from '@angular/material/slider';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { MatDialog } from '@angular/material/dialog';
import { TestbedHarnessEnvironment } from '@angular/cdk/testing/testbed';
import { MatSliderHarness } from '@angular/material/slider/testing';

describe('Given poolConfigComponent', () => {
  beforeEach(() => {
    return MockBuilder(PoolConfigComponent)
      .mock(SharedModule)
      .mock(AppqosService, {
        getPools: () => EMPTY,
        poolPut: () => EMPTY
      })
      .mock(LocalService, {
        getL3CatEvent: () => EMPTY,
        getL2CatEvent: () => EMPTY,
        getL3CatCurrentValue: () => mockedCache,
        getL2CatCurrentValue: () => mockedCache,
        convertToBitmask: LocalService.prototype.convertToBitmask
      })
      .keep(MatSliderModule)
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

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.getPool(poolID);
      expect(component.selected).toBe(mockedPools[poolID].name);
      expect(component.pool.l3Bitmask).toEqual(mockedL3Bitmask);
      expect(component.pool.l2Bitmask).toEqual(mockedL2BitMask);
    })

    it('it should update correct bitmasks when cdp is enabled', () => {
      const poolId = 0;
      const mockedL3BitmaskCode = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1];
      const mockedL3BitmaskData = [0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0];
      const mockedL2BitmaskCode = [0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1];
      const mockedL2BitmaskData = [0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0];

      const mockedPools: Pools[] = [
        {
          id: 0,
          name: 'pool_0',
          cores: [1, 2, 3],
          l3cbm_code: 15,
          l3cbm_data: 504,
          l2cbm_code: 63,
          l2cbm_data: 240,
        },
      ];

      const params = {
        pools: mockedPools,
        apps: []
      }

      const mockedCache: CacheAllocation = {
        cache_size: 44040192,
        cdp_enabled: true,
        cdp_supported: true,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3670016
      }

      MockInstance(LocalService, 'getL3CatCurrentValue', () => mockedCache);
      MockInstance(LocalService, 'getL2CatCurrentValue', () => mockedCache);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.getPool(poolId);
      expect(component.selected).toBe(mockedPools[poolId].name);
      expect(component.pool.l3BitmaskCode).toEqual(mockedL3BitmaskCode);
      expect(component.pool.l3BitmaskData).toEqual(mockedL3BitmaskData);
      expect(component.pool.l2BitmaskCode).toEqual(mockedL2BitmaskCode);
      expect(component.pool.l2BitmaskData).toEqual(mockedL2BitmaskData);
    })
  })

  describe('when getData method is called', () => {
    it('it should call getPool method', () => {
      const poolID = 0;

      const fixture = MockRender(PoolConfigComponent, params);
      const component = fixture.point.componentInstance;

      const getPoolSpy = spyOn(component, 'getPool');

      component.getData(poolID);
      expect(getPoolSpy).toHaveBeenCalledTimes(1);
    })
  })

  describe('when selectedPool method is called', () => {
    it('it should call getPool method if there is user input', () => {
      const poolID = 0;

      const event: MatOptionSelectionChange = {
        source: {} as MatOption,
        isUserInput: true
      }

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const getPoolSpy = spyOn(component, 'getPool');

      component.selectedPool(event, poolID);
      expect(getPoolSpy).toHaveBeenCalledTimes(1);
    })

    it('it should not call getPool method if there is not user input', () => {
      const poolID = 0;

      const event: MatOptionSelectionChange = {
        source: {} as MatOption,
        isUserInput: false
      }

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params)

      const getPoolSpy = spyOn(component, 'getPool');

      component.selectedPool(event, poolID);
      expect(getPoolSpy).not.toHaveBeenCalled();
    })
  })

  describe('when savePoolName method is called', () => {
    const poolName = 'pool_0';

    const mockedPools: Pools[] = [
      { id: 0, name: poolName, cores: [1, 2, 3] }
    ]

    const params = {
      pools: mockedPools,
      apps: []
    }

    it('it should update the pool name', () => {
      const poolID = 0;

      const mockedResponse = {
        status: 200,
        message: `POOL ${poolID} updated`
      }

      MockInstance(AppqosService, 'poolPut', () => of(mockedResponse));

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const nextHandlerSpy = spyOn(component, 'nextHandler');

      component.nameControl.setValue(poolName);
      component.poolId = poolID;

      const renamePoolButton = ngMocks.find('#rename-pool-button');
      renamePoolButton.triggerEventHandler('click', null);

      expect(nextHandlerSpy).toHaveBeenCalledWith(mockedResponse);
      expect(component.poolName).toBe('');
    })

    it('it should handle errors', () => {
      const invaildPoolID = -1;
      const mockedError: Error = {
        name: 'Error',
        message: 'rest API error'
      }

      const poolPutSpy = jasmine.createSpy().and.returnValue(
        throwError(() => mockedError)
      );

      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const errorHandlerSpy = spyOn(component, 'errorHandler');

      component.nameControl.setValue(poolName);
      component.poolId = invaildPoolID;

      const renamePoolButton = ngMocks.find('#rename-pool-button');
      renamePoolButton.triggerEventHandler('click', null);

      expect(poolPutSpy).toHaveBeenCalledTimes(1);
      expect(errorHandlerSpy).toHaveBeenCalledTimes(1);

      expect(component.nameControl.value).toBe(poolName);
    })
  })

  describe('when onChangeL3CBM is called', () => {
    it('it should update l3Bitmask', () => {
      const mockedL3Bitmask = [0, 1, 1, 1, 1, 1, 1, 1, 1];

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);


      component.pool.l3Bitmask = mockedL3Bitmask;
      component.onChangeL3CBM(1, 8);
      expect(component.pool.l3Bitmask).toEqual([0, 1, 1, 1, 1, 1, 1, 1, 0]);

      component.onChangeL3CBM(0, 0);
      expect(component.pool.l3Bitmask).toEqual([1, 1, 1, 1, 1, 1, 1, 1, 0]);
    })
  })

  describe('when onChangeL3CdpCode is called', () => {
    it('it should update l3bitmaskCode', () => {
      const mockedL3BitmaskCode = [1, 1, 0, 1, 1, 1, 1, 1, 1];

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.pool.l3BitmaskCode = mockedL3BitmaskCode;
      component.onChangeL3CdpCode(1, 6);
      expect(component.pool.l3BitmaskCode).toEqual([1, 1, 0, 1, 1, 1, 0, 1, 1]);

      component.onChangeL3CdpCode(0, 2);
      expect(component.pool.l3BitmaskCode).toEqual([1, 1, 1, 1, 1, 1, 0, 1, 1])
    })
  })

  describe('when onChangeL3CdpData is called', () => {
    it('it should update l3bitmaskData', () => {
      const mockedL3BitmaskData = [1, 1, 1, 1, 1, 1, 1, 0, 1];

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.pool.l3BitmaskData = mockedL3BitmaskData;
      component.onChangeL3CdpData(0, 7);
      expect(component.pool.l3BitmaskData).toEqual([1, 1, 1, 1, 1, 1, 1, 1, 1]);

      component.onChangeL3CdpData(1, 4);
      expect(component.pool.l3BitmaskData).toEqual([1, 1, 1, 1, 0, 1, 1, 1, 1])
    })
  })

  describe('when onChangeL2CBM method is called', () => {
    it('it should update l2Bitmask', () => {
      const mockedL3Bitmask = [0, 1, 1, 1, 1, 1, 1, 1];

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.pool.l2Bitmask = mockedL3Bitmask;
      component.onChangeL2CBM(0, 0);
      expect(component.pool.l2Bitmask).toEqual([1, 1, 1, 1, 1, 1, 1, 1])

      component.onChangeL2CBM(1, 7);
      expect(component.pool.l2Bitmask).toEqual([1, 1, 1, 1, 1, 1, 1, 0])
    })
  })

  describe('when onChangeL2CdpCode method is called', () => {
    it('it should update l2BitmaskCode', () => {
      const mockedL2BitmaskCode = [1, 1, 1, 1, 1, 1, 1, 0];

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.pool.l2BitmaskCode = mockedL2BitmaskCode;
      component.onChangeL2CdpCode(0, 7);
      expect(component.pool.l2BitmaskCode).toEqual([1, 1, 1, 1, 1, 1, 1, 1])

      component.onChangeL2CdpCode(1, 5);
      expect(component.pool.l2BitmaskCode).toEqual([1, 1, 1, 1, 1, 0, 1, 1])
    });
  });

  describe('when onChangeL2CdpData method is called', () => {
    it('it should update l2BitmaskData', () => {
      const mockedL2BitmaskData = [1, 1, 1, 0, 1, 1, 1, 1];

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.pool.l2BitmaskData = mockedL2BitmaskData;
      component.onChangeL2CdpData(0, 3);
      expect(component.pool.l2BitmaskData).toEqual([1, 1, 1, 1, 1, 1, 1, 1])

      component.onChangeL2CdpData(1, 6);
      expect(component.pool.l2BitmaskData).toEqual([1, 1, 1, 1, 1, 1, 0, 1])
    });
  });

  describe('when onChangeMBA is called', () => {
    it('it should update mba', async () => {
      const oldMba = 20;
      const newMba = 100;

      const mockedPools: Pools[] = [
        { id: 0, name: 'pool_0', cores: [1, 2, 3], mba: oldMba },
      ];

      const params = {
        pools: mockedPools,
        apps: []
      }

      const fixture = MockRender(PoolConfigComponent, params);
      const component = fixture.point.componentInstance;

      const loader = TestbedHarnessEnvironment.loader(fixture);
      await fixture.whenStable();
      const slider = await loader.getHarness(MatSliderHarness);

      expect(await slider.getValue()).toBe(oldMba);
      expect(component.pool.mba).toBe(oldMba);

      await slider.setValue(newMba);

      expect(await slider.getValue()).toBe(newMba);
      expect(component.pool.mba).toBe(newMba);
    })
  })

  describe('when saveL2CBM method is called', () => {
    const poolID = 0;
    const mockedL2Bitmask = [0, 1, 1, 1, 1, 1, 1, 1];

    const mockedPools: Pools[] = [
      { id: 0, name: 'pool_0', cores: [1, 2, 3], l2cbm: 7 },
    ];

    const params = {
      pools: mockedPools,
      apps: []
    }

    it('it should update l2cbm', () => {
      const mockedResponse = {
        status: 200,
        message: `POOL ${poolID} updated`
      }

      MockInstance(AppqosService, 'poolPut', () => of(mockedResponse));

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const nextHandlerSpy = spyOn(component, 'nextHandler');

      component.pool.l2Bitmask = mockedL2Bitmask;
      component.poolId = poolID;

      const saveL2CBMButton = ngMocks.find('#save-l2cbm-button');
      saveL2CBMButton.triggerEventHandler('click', null);

      expect(nextHandlerSpy).toHaveBeenCalledWith(mockedResponse);
      expect(nextHandlerSpy).toHaveBeenCalledTimes(1);
    });

    it('it should update the correct properties when cdp is enabled', () => {
      const mockedCache: CacheAllocation = {
        cache_size: 44040192,
        cdp_enabled: true,
        cdp_supported: true,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3670016
      }

      const mockResponse = {
        status: 200,
        message: `POOL ${poolID} updated`
      }
      const mockedcbm = 127;

      const poolPutSpy = jasmine.createSpy().and.returnValue(of(mockResponse));
      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const nextHandlerSpy = spyOn(component, 'nextHandler');
      component.poolId = poolID;
      component.pool.l2BitmaskCode = mockedL2Bitmask;
      component.pool.l2BitmaskData = mockedL2Bitmask;
      component.l2cat = mockedCache;

      const saveL2CBMButton = ngMocks.find('#save-l2cbm-button');
      saveL2CBMButton.triggerEventHandler('click', null);

      expect(nextHandlerSpy).toHaveBeenCalledTimes(1);
      expect(nextHandlerSpy).toHaveBeenCalledWith(mockResponse);
      expect(poolPutSpy).toHaveBeenCalledOnceWith(
        { l2cbm_code: mockedcbm, l2cbm_data: mockedcbm }, poolID
      );
    });

    it('it should handle error', () => {
      const mockedError: Error = {
        name: 'Error',
        message: `POOL ${poolID} not updated`
      }

      const poolPutSpy = jasmine.createSpy('poolPut').and.returnValue(
        throwError(() => mockedError)
      );

      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const errorHandlerSpy = spyOn(component, 'errorHandler');
      component.pool.l2Bitmask = mockedL2Bitmask;
      component.poolId = poolID;

      const saveL2CBMButton = ngMocks.find('#save-l2cbm-button');
      saveL2CBMButton.triggerEventHandler('click', null);

      expect(errorHandlerSpy).toHaveBeenCalledTimes(1);
    })
  })

  describe('when saveL3CBM method is called', () => {
    const poolID = 0;
    const mockedL3Bitmask = [0, 1, 1, 1, 1, 1, 1, 1];

    const mockedPools: Pools[] = [
      { id: 0, name: 'pool_0', cores: [1, 2, 3], l3cbm: 127 },
    ];

    const params = {
      pools: mockedPools,
      apps: []
    }

    it('it should update l3cbm', () => {
      const mockResponse = {
        status: 200,
        message: `POOL ${poolID} updated`
      }

      MockInstance(AppqosService, 'poolPut', () => of(mockResponse));

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const nextHandlerSpy = spyOn(component, 'nextHandler');
      component.poolId = poolID;
      component.pool.l3Bitmask = mockedL3Bitmask;

      const saveL3CBMButton = ngMocks.find('#save-l3cbm-button');
      saveL3CBMButton.triggerEventHandler('click', null);

      expect(nextHandlerSpy).toHaveBeenCalledTimes(1);
      expect(nextHandlerSpy).toHaveBeenCalledWith(mockResponse);
    })

    it('it should update the correct properties when cdp is enabled', () => {
      const mockedCache: CacheAllocation = {
        cache_size: 44040192,
        cdp_enabled: true,
        cdp_supported: true,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3670016
      }

      const mockResponse = {
        status: 200,
        message: `POOL ${poolID} updated`
      }
      const mockedcbm = 127;

      const poolPutSpy = jasmine.createSpy().and.returnValue(of(mockResponse));
      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const nextHandlerSpy = spyOn(component, 'nextHandler');
      component.poolId = poolID;
      component.pool.l3BitmaskCode = mockedL3Bitmask;
      component.pool.l3BitmaskData = mockedL3Bitmask;
      component.l3cat = mockedCache;

      const saveL3CBMButton = ngMocks.find('#save-l3cbm-button');
      saveL3CBMButton.triggerEventHandler('click', null);

      expect(nextHandlerSpy).toHaveBeenCalledTimes(1);
      expect(nextHandlerSpy).toHaveBeenCalledWith(mockResponse);
      expect(poolPutSpy).toHaveBeenCalledOnceWith(
        { l3cbm_code: mockedcbm, l3cbm_data: mockedcbm }, poolID
      );
    });

    it('it should handle error', () => {
      const mockedError: Error = {
        name: 'Error',
        message: `POOL ${poolID} not updated`
      }
      const poolPutSpy = jasmine.createSpy('poolPut').and.returnValue(
        throwError(() => mockedError)
      )

      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const errorHandlerSpy = spyOn(component, 'errorHandler');

      component.pool.l3Bitmask = mockedL3Bitmask;
      component.poolId = poolID;

      const saveL3CBMButton = ngMocks.find('#save-l3cbm-button');
      saveL3CBMButton.triggerEventHandler('click', null);

      expect(errorHandlerSpy).toHaveBeenCalledTimes(1);
    })
  })

  describe('when saveMBA method is called', () => {
    const poolID = 0;
    const oldMba = 30;
    const newMba = 90;

    const mockedPools: Pools[] = [
      { id: 0, name: 'pool_0', cores: [1, 2, 3], mba: oldMba }
    ]

    const params = {
      pools: mockedPools,
      apps: []
    }

    it('it should save mba', async () => {
      const mockedResponse = {
        status: 200,
        message: `POOL ${poolID} updated`
      }

      const poolPutSpy = jasmine.createSpy().and.returnValue(of(mockedResponse))
      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const fixture = MockRender(PoolConfigComponent, params);
      const component = fixture.point.componentInstance;

      const loader = TestbedHarnessEnvironment.loader(fixture);
      await fixture.whenStable();
      const slider = await loader.getHarness(MatSliderHarness);

      expect(await slider.getValue()).toBe(oldMba);

      const nextHandlerSpy = spyOn(component, 'nextHandler');

      component.poolId = poolID;
      await slider.setValue(newMba);

      const saveMBAButton = ngMocks.find('#save-mba-button');
      saveMBAButton.triggerEventHandler('click', null);

      expect(await slider.getValue()).toBe(newMba);
      expect(nextHandlerSpy).toHaveBeenCalledOnceWith(mockedResponse);
      expect(poolPutSpy).toHaveBeenCalledOnceWith({ mba: newMba }, poolID);
    })

    it('it should handle error', () => {
      const mockedError: Error = {
        name: 'Error',
        message: `POOL ${poolID} not updated`
      }

      const poolPutSpy = jasmine.createSpy('poolPut').and.returnValue(
        throwError(() => mockedError)
      );

      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const errorHandlerSpy = spyOn(component, 'errorHandler');

      component.poolId = poolID;

      const saveMBAButton = ngMocks.find('#save-mba-button');
      saveMBAButton.triggerEventHandler('click', null);

      expect(errorHandlerSpy).toHaveBeenCalledTimes(1);
      expect(errorHandlerSpy).toHaveBeenCalledWith(mockedError);
    })
  })

  describe('when saveMBABW method is called', () => {
    const poolID = 0;
    const mbaBw = 2147483648;
    const mbaBwDefault = Math.pow(2, 32) - 1
    const mockedPools: Pools[] = [
      { id: 0, name: 'pool_0', cores: [1, 2, 3], mba_bw: 1 }
    ]

    const params = {
      pools: mockedPools,
      apps: []
    }

    it('it should save mba_bw when apply button is pressed', () => {
      const mockedResponse = {
        status: 200,
        message: `POOL ${poolID} updated`
      }

      const poolPutSpy = jasmine.createSpy('poolPut').and.returnValue(of(mockedResponse));

      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const nextHandlerSpy = spyOn(component, 'nextHandler');

      component.poolId = poolID;
      component.mbaBwControl.setValue(mbaBw);

      const saveMBABWButton = ngMocks.find('#mbabw-apply-button');
      saveMBABWButton.triggerEventHandler('click', null);

      expect(poolPutSpy).toHaveBeenCalledWith({ mba_bw: mbaBw }, poolID);
      expect(nextHandlerSpy).toHaveBeenCalledOnceWith(mockedResponse);
    })

    it('it should use default value when reset button is pressed', () => {
      const mockedResponse = {
        status: 200,
        message: `POOL ${poolID} updated`
      }

      const poolPutSpy = jasmine.createSpy('poolPut').and.returnValue(of(mockedResponse));

      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const nextHandlerSpy = spyOn(component, 'nextHandler');

      component.poolId = poolID;
      component.mbaBwControl.setValue(mbaBw);

      const saveMBABWButton = ngMocks.find('#mbabw-reset-button');
      saveMBABWButton.triggerEventHandler('click', null);

      expect(poolPutSpy).toHaveBeenCalledOnceWith({ mba_bw: mbaBwDefault }, poolID);
      expect(nextHandlerSpy).toHaveBeenCalledOnceWith(mockedResponse);
    })

    it('it should handle error', () => {
      const mockedError: Error = {
        name: 'Error',
        message: `POOL ${poolID} not updated`
      }

      const poolPutSpy = jasmine.createSpy('poolPut').and.returnValue(throwError(() => mockedError));

      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const errorHandlerSpy = spyOn(component, 'errorHandler');

      component.poolId = poolID;
      component.mbaBwControl.setValue(mbaBw);

      const saveMBABWButton = ngMocks.find('#mbabw-apply-button');
      saveMBABWButton.triggerEventHandler('click', null);

      expect(poolPutSpy).toHaveBeenCalledOnceWith({ mba_bw: mbaBw }, poolID);
      expect(errorHandlerSpy).toHaveBeenCalledOnceWith(mockedError);
    })
  })

  describe('when nextHandler method is called', () => {
    it('it should display a response and emit a pool event', () => {
      const mockedResponse: resMessage = {
        message: 'POOL 0 updated'
      }

      const displayInfoSpy = jasmine.createSpy('displayInfo');

      MockInstance(SnackBarService, 'displayInfo', displayInfoSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.poolEvent.subscribe((event) => {
        expect(event).toBeUndefined();
      });

      component.nextHandler(mockedResponse);

      expect(displayInfoSpy).toHaveBeenCalledOnceWith(mockedResponse.message);
    })
  })

  describe('when errorHandler method is called', () => {
    it('it should display a error and emit a pool event', () => {
      const mockedError: Error = {
        name: 'Error',
        message: 'rest API error'
      }

      const handleErrorSpy = jasmine.createSpy('handleError');

      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.poolEvent.subscribe((event) => {
        expect(event).toBeUndefined();
      })

      component.errorHandler(mockedError);

      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    })
  })

  describe('when coresEditDialog method is called', () => {
    it('it should render CoresEditDialogComponent', () => {
      const matDialogSpy = jasmine.createSpy('open').and.returnValue(
        { afterClosed: () => of(true), close: null }
      );

      MockInstance(MatDialog, 'open', matDialogSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      const editCoresButton = ngMocks.find('#edit-cores-button');
      editCoresButton.triggerEventHandler('click', null);

      component.poolEvent.subscribe((event) => {
        expect(event).toBeUndefined();
      })

      expect(matDialogSpy).toHaveBeenCalledTimes(1);
    })
  })

  describe('when deletePool method is called', () => {
    const poolID = 0;

    const mockedPools: Pools[] = [
      { id: poolID, name: 'pool_0', cores: [1, 2, 3] }
    ]

    const params = {
      pools: mockedPools,
      apps: []
    }

    it('it should display a response', () => {
      const mockedResponse = {
        status: 200,
        message: `POOL ${poolID} deleted`
      }

      const deletePoolSpy = jasmine.createSpy('deletePool').and.returnValue(of(mockedResponse));
      const displayInfoSpy = jasmine.createSpy('displayInfo');

      MockInstance(AppqosService, 'deletePool', deletePoolSpy);
      MockInstance(SnackBarService, 'displayInfo', displayInfoSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.pool = mockedPools[0];
      component.poolId = poolID;

      component.poolEvent.subscribe((event) => {
        expect(event).toBeUndefined();
      })

      const deletePoolButton = ngMocks.find('#delete-pool-button');
      deletePoolButton.triggerEventHandler('click', null);

      expect(deletePoolSpy).toHaveBeenCalledOnceWith(poolID);
      expect(displayInfoSpy).toHaveBeenCalledOnceWith(mockedResponse.message);
      expect(component.poolId).toBeUndefined();
    })

    it('it should handle error', () => {
      const mockedError: Error = {
        name: 'Error',
        message: 'rest API error'
      }

      const deletePoolSpy = jasmine.createSpy('deletePool').and.returnValue(
        throwError(() => mockedError)
      );
      const handleErrorSpy = jasmine.createSpy('handleError');

      MockInstance(AppqosService, 'deletePool', deletePoolSpy);
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.poolId = poolID;

      const deletePoolButton = ngMocks.find('#delete-pool-button');
      deletePoolButton.triggerEventHandler('click', null);

      expect(deletePoolSpy).toHaveBeenCalledWith(poolID);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    })
  })

  describe('when poolAddDialog method is called', () => {
    it('it should render poolAddDialog component', () => {
      const poolID = 1;

      const mockedResponse = {
        id: poolID
      }

      const dialogRef = {
        afterClosed: () => of(mockedResponse)
      }

      const matDialogSpy = jasmine.createSpy('open').and.returnValue(dialogRef);
      MockInstance(MatDialog, 'open', matDialogSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolConfigComponent, params);

      component.poolEvent.subscribe((event) => {
        expect(event).toBeUndefined();
      })

      const addPoolButton = ngMocks.find('#add-pool-button');
      addPoolButton.triggerEventHandler('click', null);

      expect(matDialogSpy).toHaveBeenCalledTimes(1);
      expect(component.poolId).toBe(poolID);
    })
  })

  describe('when cdp is enabled', () => {
    it('it should display code and data label', async () => {
      const mockedCache: CacheAllocation = {
        cache_size: 44040192,
        cdp_enabled: true,
        cdp_supported: true,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3670016
      }

      const catEvent = new BehaviorSubject<CacheAllocation>(mockedCache);

      MockInstance(LocalService, 'getL3CatEvent', () => catEvent);
      MockRender(PoolConfigComponent, params);
      
      const label = ngMocks.formatText(
        ngMocks.find('.cbm-label')
      );

      expect(label).toContain('Code');
      expect(label).toContain('Data');
    })
  })
});