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
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND ITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.*/

import { MAT_DIALOG_DATA, MatDialogRef } from '@angular/material/dialog';
import { Router } from '@angular/router';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { EMPTY, of } from 'rxjs';
import { MatSliderModule } from '@angular/material/slider';

import { AppqosService } from 'src/app/services/appqos.service';
import { SharedModule } from 'src/app/shared/shared.module';
import { Pools } from '../overview.model';
import { EditDialogComponent } from './edit-dialog.component';

describe('Given EditDialogComponent', () => {
  beforeEach(() =>
    MockBuilder(EditDialogComponent)
      .mock(SharedModule)
      .mock(Router)
      .mock(MAT_DIALOG_DATA, {})
      .mock(MatDialogRef, {})
      .mock(AppqosService, {
        getPools: () => EMPTY,
      })
      .keep(MatSliderModule)
  );

  MockInstance.scope('case');

  describe('when initialized with CAT', () => {
    it('should display "L3 Cache Allocation (CAT)" title', async () => {
      const title = 'L3 Cache Allocation (CAT)';
      const fixture = MockRender(
        EditDialogComponent,
        {},
        {
          providers: [
            {
              provide: MAT_DIALOG_DATA,
              useValue: { l3cbm: true, numCacheWays: 12 },
            },
          ],
        }
      );

      await fixture.whenStable();

      const expectedTitle = ngMocks.formatText(ngMocks.find('h2'));

      expect(expectedTitle).toEqual(title);
    });

    it('should display pool name', async () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const poolsSpy = jasmine.createSpy('getPools');

      MockInstance(AppqosService, 'getPools', poolsSpy).and.returnValue(
        of(mockedPool)
      );

      const fixture = MockRender(
        EditDialogComponent,
        {},
        {
          providers: [
            {
              provide: MAT_DIALOG_DATA,
              useValue: { l3cbm: true, numCacheWays: 12 },
            },
          ],
        }
      );

      await fixture.whenStable();

      const expectedPoolName = ngMocks
        .findAll('.pool-name')
        .map((a) => ngMocks.formatText(a));

      expect(expectedPoolName.toString()).toEqual(mockedPool[0].name);
    });

    it('should display l3cbm converted to binary', async () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const poolsSpy = jasmine.createSpy('getPools');

      MockInstance(AppqosService, 'getPools', poolsSpy).and.returnValue(
        of(mockedPool)
      );

      const fixture = MockRender(
        EditDialogComponent,
        {},
        {
          providers: [
            {
              provide: MAT_DIALOG_DATA,
              useValue: { l3cbm: true, numCacheWays: 12 },
            },
          ],
        }
      );

      await fixture.whenStable();

      const expectedCbm = ngMocks
        .findAll('.pool-l3cbm')
        .map((a) => ngMocks.formatText(a));

      expect(expectedCbm).toEqual(['0 1 1 1 1 1 1 1 1 1 1 1']);
    });
  });

  describe('when saveL3CBM method is executed', () => {
    it('should save pool l3cbm by id', async () => {
      const mockResponse = 'POOL 0 updated';
      const poolsSpy = jasmine.createSpy('poolPut');
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      MockInstance(AppqosService, 'getPools', () => of(mockedPool));
      MockInstance(AppqosService, 'poolPut', poolsSpy).and.returnValue(
        of(mockResponse)
      );

      const fixture = MockRender(
        EditDialogComponent,
        {},
        {
          providers: [
            {
              provide: MAT_DIALOG_DATA,
              useValue: { l3cbm: true, numCacheWays: 12 },
            },
          ],
        }
      );

      await fixture.whenStable();

      const cbmButton = ngMocks.find('.apply-button');
      cbmButton.triggerEventHandler('click', null);

      expect(poolsSpy).toHaveBeenCalledWith({ l3cbm: 2047 }, 0);
    });
  });

  describe('when onChangeL3CBM method is executed', () => {
    it('should update state of pool cbm by index', async () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      MockInstance(AppqosService, 'getPools', () => of(mockedPool));

      const fixture = MockRender(
        EditDialogComponent,
        {},
        {
          providers: [
            {
              provide: MAT_DIALOG_DATA,
              useValue: { l3cbm: true, numCacheWays: 12 },
            },
          ],
        }
      );
      const component = fixture.point.componentInstance;
      await fixture.whenStable();

      const l3cbmButton = ngMocks.find('.l3cbm-button');
      l3cbmButton.triggerEventHandler('click', null);

      expect(component.pools[0].l3Bitmask).toEqual([
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      ]);
    });
  });
});
