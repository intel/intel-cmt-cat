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
import { MatDialog } from '@angular/material/dialog';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { of } from 'rxjs';

import { SharedModule } from 'src/app/shared/shared.module';
import { EditDialogComponent } from '../edit-dialog/edit-dialog.component';
import { Pools } from '../overview.model';
import { L3CacheAllocationComponent } from './l3-cache-allocation.component';
import { AppqosService } from 'src/app/services/appqos.service';

describe('Given OverviewComponent', () => {
  beforeEach(() =>
    MockBuilder(L3CacheAllocationComponent)
      .mock(SharedModule)
      .mock(Router)
      .mock(AppqosService, {
        getL3cat: () =>
          of({
            cache_size: 44040192,
            cdp_enabled: false,
            cdp_supported: false,
            clos_num: 15,
            cw_num: 12,
            cw_size: 3670016,
          }),
      })
  );

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should display "L3 Cache Allocation (CAT)" title', () => {
      const title = 'L3 Cache Allocation (CAT)';
      MockRender(L3CacheAllocationComponent);

      const expectedTitle = ngMocks.formatText(ngMocks.find('mat-card-title'));

      expect(expectedTitle).toEqual(title);
    });

    it('should get Cache way number', () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const {
        point: { componentInstance: component },
      } = MockRender(L3CacheAllocationComponent, { pools: mockedPool });

      expect(component.numCacheWays).toEqual(12);
    });

    it('should display pool name', () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
        {
          id: 1,
          mba_bw: 4294967295,
          l3cbm: 15,
          name: 'HP',
          cores: [10, 12, 15],
        },
        {
          id: 2,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'LP',
          cores: [16, 17, 14],
        },
      ];

      MockRender(L3CacheAllocationComponent, { pools: mockedPool });

      const expectedPoolName = ngMocks
        .findAll('.pool-name')
        .map((a) => ngMocks.formatText(a));

      expect(expectedPoolName.toString()).toEqual('Default,HP,LP');
    });

    it('should display l3cbm converted to binary', () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
        {
          id: 1,
          mba_bw: 4294967295,
          l3cbm: 15,
          name: 'HP',
          cores: [10, 12, 15],
        },
        {
          id: 2,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'LP',
          cores: [16, 17, 14],
        },
      ];

      MockRender(L3CacheAllocationComponent, {
        pools: mockedPool,
      });

      const expectedCbm = ngMocks
        .findAll('.pool-cbm')
        .map((pool) => ngMocks.formatText(pool));

      expect(expectedCbm).toEqual([
        '0 1 1 1 1 1 1 1 1 1 1 1',
        '0 0 0 0 0 0 0 0 1 1 1 1',
        '0 1 1 1 1 1 1 1 1 1 1 1',
      ]);
    });
  });

  describe('when click Edit button', () => {
    it('should open modal dialog', () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const dialogSpy = jasmine.createSpy('dialog.open');

      MockInstance(MatDialog, 'open', dialogSpy).and.returnValue({
        afterClosed: () => of(null),
      });

      MockRender(L3CacheAllocationComponent, { pools: mockedPool });

      const expandButton = ngMocks.find('.action-button');

      expandButton.triggerEventHandler('click', {});

      expect(dialogSpy).toHaveBeenCalledWith(EditDialogComponent, {
        height: 'auto',
        width: '50rem',
        data: { l3cbm: true, numCacheWays: 12 },
      });
    });
  });

  describe('when click "Go to wikipedia"', () => {
    it('should redirect to wiki page', () => {
      MockRender(L3CacheAllocationComponent);

      const infoUrl = ngMocks.find('a').nativeElement.getAttribute('href');

      expect(infoUrl).toEqual(
        'https://www.intel.com/content/www/us/en/developer/articles/technical/introduction-to-cache-allocation-technology.html?wapkw=introduction%20to%20cache%20allocation'
      );
    });
  });
});
