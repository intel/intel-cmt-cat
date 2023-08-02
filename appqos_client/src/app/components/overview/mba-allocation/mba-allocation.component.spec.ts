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

import { MatDialog } from '@angular/material/dialog';
import { MatInputModule } from '@angular/material/input';
import { MatSliderModule } from '@angular/material/slider';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';
import { TestbedHarnessEnvironment } from '@angular/cdk/testing/testbed';
import { MatInputHarness } from '@angular/material/input/testing';
import { MatTooltipHarness } from '@angular/material/tooltip/testing';
import { MatTooltipModule } from '@angular/material/tooltip';
import { Router } from '@angular/router';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { of } from 'rxjs';

import { SharedModule } from 'src/app/shared/shared.module';
import { Pools } from '../overview.model';
import { MbaAllocationComponent } from './mba-allocation.component';
import { EditDialogComponent } from '../edit-dialog/edit-dialog.component';
import {
  MatSlideToggle,
  MatSlideToggleChange,
} from '@angular/material/slide-toggle';
import { MBACTRL } from '../../system-caps/system-caps.model';

describe('Given MbaAllocationComponent', () => {
  beforeEach(() =>
    MockBuilder(MbaAllocationComponent)
      .mock(SharedModule)
      .mock(Router)
      .keep(MatSliderModule)
      .keep(MatInputModule)
      .keep(BrowserAnimationsModule)
      .keep(MatTooltipModule)
  );

  MockInstance.scope('case');

  describe('when initialized and MBA is available', () => {
    it('should display "Memory Bandwidth Allocation (MBA)" title', () => {
      const title = 'Memory Bandwidth Allocation (MBA)';
      MockRender(MbaAllocationComponent, { available: true });

      const expectedTitle = ngMocks.formatText(ngMocks.find('mat-card-title'));

      expect(expectedTitle).toEqual(title);
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

      MockRender(MbaAllocationComponent, { available: true, pools: mockedPool });

      const expectedPoolNameList = ngMocks
        .findAll('.pool-name')
        .map((a) => ngMocks.formatText(a));

      expect(expectedPoolNameList).toEqual(['Default', 'HP', 'LP']);
    });

    it('should display "MBA controller enabled" toggle', () => {
      const mockedMbaData: MBACTRL = {
        enabled: true,
        supported: true,
      };

      MockRender(MbaAllocationComponent, {
        mbaCtrl: mockedMbaData,
      });

      const template = ngMocks.find('mat-slide-toggle');

      expect(template).toBeTruthy();
    });
  });

  describe('when initialized and MBA is NOT available', () => {
    it('should display "Not currently available or supported" message', () => {
      const message = 'Not currently available or supported...';
      MockRender(MbaAllocationComponent, { available: false });

      const expectedTitle = ngMocks.formatText(ngMocks.find('h2'));

      expect(expectedTitle).toEqual(message);
    });

    it('should display "MBA controller disabled" toggle', () => {
      const mockedMbaData: MBACTRL = {
        enabled: false,
        supported: false,
      };

      MockRender(MbaAllocationComponent, {
        available: false,
        mbaCtrl: mockedMbaData,
      });

      const template = ngMocks.find('mat-slide-toggle');

      expect(template.attributes['ng-reflect-disabled']).toEqual('true');
    });

    it('should not display edit button', () => {
      MockRender(MbaAllocationComponent, { available: false });

      const editButton = ngMocks.find('.action-button', null);

      expect(editButton).toBeNull();
    });
  });

  describe('when initialized and MBA CTRL disabled', () => {
    it('should display pool mba', () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba: 70,
          l3cbm: 15,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
        {
          id: 0,
          mba: 50,
          l3cbm: 15,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      MockRender(MbaAllocationComponent, {
        available: true,
        pools: mockedPool
      });

      const expectedMbaList = ngMocks
        .findAll('.pool-mba')
        .map((mba) => ngMocks.formatText(mba));

      expect(expectedMbaList).toEqual(['70', '50']);
    });

    it('should display the text of a tooltip', async () => {
      const mockedMbaData: MBACTRL = {
        enabled: false,
        supported: false,
      };

      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 15,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const fixture = MockRender(MbaAllocationComponent, {
        pools: mockedPool,
        mbaCtrl: mockedMbaData,
      });

      const loader = TestbedHarnessEnvironment.loader(fixture);
      await fixture.whenStable();
      const tooltip = await loader.getHarness(MatTooltipHarness);
      await tooltip.show();

      expect(await tooltip.getTooltipText()).toEqual('MBA controller disabled');
    });
  });

  describe('when initialized and MBA CTRL enabled', () => {
    it('should display the text of a tooltip', async () => {
      const mockedMbaData: MBACTRL = {
        enabled: true,
        supported: true,
      };

      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 15,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const fixture = MockRender(MbaAllocationComponent, {
        pools: mockedPool,
        mbaCtrl: mockedMbaData,
      });

      const loader = TestbedHarnessEnvironment.loader(fixture);
      await fixture.whenStable();
      const tooltip = await loader.getHarness(MatTooltipHarness);
      await tooltip.show();

      expect(await tooltip.getTooltipText()).toEqual('MBA controller enabled');
    });
  });

  describe('when MBA CTRL enabled and mba_bw with default value', () => {
    it('should display "Unrestricted" text', async () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 15,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const fixture = MockRender(MbaAllocationComponent, {
        available: true,
        pools: mockedPool
      });
      const loader = TestbedHarnessEnvironment.loader(fixture);
      await fixture.whenStable();
      const inputs = await loader.getAllHarnesses(MatInputHarness);
      const input = inputs[0];

      expect((await input.getValue()).toString()).toEqual('Unrestricted');
    });
  });

  describe('when MBA CTRL enabled and MBA BW with NOT default value', () => {
    it('should display pool mba_bw value', async () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967,
          l3cbm: 15,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const fixture = MockRender(MbaAllocationComponent, {
        available: true,
        pools: mockedPool
      });
      const loader = TestbedHarnessEnvironment.loader(fixture);
      await fixture.whenStable();
      const inputs = await loader.getAllHarnesses(MatInputHarness);
      const input = inputs[0];

      expect((await input.getValue()).toString()).toEqual('4294967');
    });
  });

  describe('when clicked "See more"', () => {
    it('should redirect to wiki page', () => {
      MockRender(MbaAllocationComponent);

      const infoUrl = ngMocks.find('a').nativeElement.getAttribute('href');

      expect(infoUrl).toEqual(
        'https://www.intel.com/content/www/us/en/developer/articles/technical/introduction-to-memory-bandwidth-allocation.html'
      );
    });
  });

  describe('when click Edit button', () => {
    it('should open modal dialog', () => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba: 50,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const dialogSpy = jasmine.createSpy('dialog.open');

      MockInstance(MatDialog, 'open', dialogSpy).and.returnValue({
        afterClosed: () => of(null),
      });

      MockRender(MbaAllocationComponent, {
        available: true,
        pools: mockedPool,
        mbaCtrl: { enabled: true, supported: true },
      });

      const editButton = ngMocks.find('button');

      editButton.triggerEventHandler('click', {});

      expect(dialogSpy).toHaveBeenCalledWith(EditDialogComponent, {
        height: 'auto',
        width: '50rem',
        data: { mba: true },
      });
    });
  });

  describe('when slide toggle is clicked', () => {
    it('should emit "onChange" event with correct value', (done) => {
      const mockedPool: Pools[] = [
        {
          id: 0,
          mba: 50,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const event: MatSlideToggleChange = {
        checked: false,
        source: {} as MatSlideToggle,
      };

      const mockedMbaCtrkData: MBACTRL = {
        enabled: true,
        supported: true,
      };

      const fixture = MockRender(MbaAllocationComponent, {
        pools: mockedPool,
        mbaCtrl: mockedMbaCtrkData,
      });

      const component = fixture.point.componentInstance;
      const toggle = ngMocks.find('mat-slide-toggle');

      component.mbaCtrlEvent.subscribe((value: MatSlideToggleChange) => {
        expect(value.checked).toBeFalse();

        done();
      });

      toggle.triggerEventHandler('change', event);
    });
  });
});
