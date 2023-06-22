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

import { Apps, Pools } from '../../overview/overview.model';
import { SimpleChange } from '@angular/core';
import { MatTableModule } from '@angular/material/table';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { EMPTY, of, throwError } from 'rxjs';
import { AppqosService } from 'src/app/services/appqos.service';
import { SharedModule } from 'src/app/shared/shared.module';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { AppsConfigComponent } from './apps-config.component';
import { MatTableHarness } from '@angular/material/table/testing';
import { TestbedHarnessEnvironment } from '@angular/cdk/testing/testbed';
import { HarnessLoader } from '@angular/cdk/testing';
import { MatDialog, MatDialogModule } from '@angular/material/dialog';
import { AppsAddDialogComponent } from './apps-add-dialog/apps-add-dialog.component';
import { AppsEditDialogComponent } from './apps-edit-dialog/apps-edit-dialog.component';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';

describe('Given AppConfigComponent', () => {
  beforeEach(() => {
    return MockBuilder(AppsConfigComponent)
      .mock(SharedModule)
      .mock(AppqosService, {
        deleteApp: () => EMPTY,
      })
      .mock(MatDialogModule)
      .mock(SnackBarService)
      .mock(AppsAddDialogComponent)
      .mock(AppsEditDialogComponent)
      .keep(MatTableModule)
      .keep(BrowserAnimationsModule);
  });

  const mockedPools: Pools[] = [
    { id: 0, name: 'pool 0', cores: [1, 2, 3] },
    { id: 1, name: 'pool 1', cores: [4, 5, 6] },
  ];

  const mockedApps: Apps[] = [
    {
      name: 'app 0',
      pids: [1234, 4321],
      cores: [0, 1, 2, 3],
      id: 1,
      pool_id: 0,
    },
    {
      name: 'app 1',
      pids: [5678, 8765],
      cores: [10, 11, 12, 13],
      id: 2,
      pool_id: 1,
    },
  ];

  const params = {
    pools: mockedPools,
    apps: mockedApps,
  };

  MockInstance.scope('case');

  describe('when initialized with no configured apps', () => {
    it('should display message to user', () => {
      const empty_apps_params = {
        pools: mockedPools,
        apps: [],
      };

      const fixture = MockRender(AppsConfigComponent, empty_apps_params);
      const msg = ngMocks.formatText(ngMocks.find(fixture, 'span'));
      expect(msg).toBe('No applications currently configured');
    });
  });

  describe('when initialized with existing apps', () => {
    it('should display correct app details after init', async () => {
      // initialize component with mocked initail apps and pools
      const fixture = MockRender(AppsConfigComponent, params);
      const loader: HarnessLoader = TestbedHarnessEnvironment.loader(fixture);

      // assert expected table values
      const cells = await loader
        .getHarness(MatTableHarness)
        .then((table) => table.getCellTextByColumnName());

      mockedApps.forEach((app, i) => {
        expect(cells['name'].text[i]).toBe(app.name);
        expect(cells['cores'].text[i]).toBe(String(app.cores));
        expect(cells['pids'].text[i]).toBe(String(app.pids));
        expect(cells['pool'].text[i]).toBe(
          String(
            params.pools.find((pool: Pools) => pool.id === app.pool_id)?.name
          )
        );
      });

      // check no update occurs if apps is null
      fixture.point.componentInstance.ngOnChanges({
        apps: new SimpleChange(null, null, false),
      });
      fixture.detectChanges();
    });

    it('should display correct app details after update', async () => {
      const params = {
        pools: [...mockedPools],
        apps: [...mockedApps],
      };

      // initialize component with mocked initail apps and pools
      const fixture = MockRender(AppsConfigComponent, params);
      const loader: HarnessLoader = TestbedHarnessEnvironment.loader(fixture);

      // assert expected table values
      const cells = await loader
        .getHarness(MatTableHarness)
        .then((table) => table.getCellTextByColumnName());

      mockedApps.forEach((app, i) => {
        expect(cells['name'].text[i]).toBe(app.name);
        expect(cells['cores'].text[i]).toBe(String(app.cores));
        expect(cells['pids'].text[i]).toBe(String(app.pids));
        expect(cells['pool'].text[i]).toBe(
          String(
            params.pools.find((pool: Pools) => pool.id === app.pool_id)?.name
          )
        );
      });

      // updated app and pool values
      const updatedPools: Pools[] = Array.from(mockedPools);
      const updatedApps: Apps[] = [
        {
          name: 'updated app 0',
          pids: [1111, 2222],
          cores: [4, 5, 6, 7],
          id: 1,
          pool_id: 1,
        },
        {
          name: 'updated app 1',
          pids: [3333, 4444],
          cores: undefined, // test getting pool cores branch
          id: 2,
          pool_id: 0,
        },
      ];

      // update component values
      fixture.componentInstance.pools = updatedPools;
      fixture.componentInstance.apps = updatedApps;

      // assert updated table values
      const updatedCells = await loader
        .getHarness(MatTableHarness)
        .then((table) => table.getCellTextByColumnName());

      updatedApps.forEach((app, i) => {
        expect(updatedCells['name'].text[i]).toBe(app.name);
        /*
         * If no app cores defined
         * default to pool cores
         */
        if (!app.cores) {
          expect(updatedCells['cores'].text[i]).toBe(
            String(
              mockedPools.find((pool: Pools) => pool.id === app.pool_id)?.cores
            )
          );
        } else {
          expect(updatedCells['cores'].text[i]).toBe(String(app.cores));
        }
        expect(updatedCells['pids'].text[i]).toBe(String(app.pids));
        expect(updatedCells['pool'].text[i]).toBe(
          String(
            updatedPools.find((pool: Pools) => pool.id === app.pool_id)?.name
          )
        );
      });
    });
  });

  describe('when add app button clicked', () => {
    it('should open app add dialog', () => {
      const dialogSpy = jasmine.createSpy('dialog.open');

      MockInstance(MatDialog, 'open', dialogSpy).and.returnValue({
        afterClosed: () => of(null),
      });

      MockRender(AppsConfigComponent, params);

      const addAppButton = ngMocks.find('#add-app-button');

      addAppButton.triggerEventHandler('click', {});

      expect(dialogSpy).toHaveBeenCalledWith(AppsAddDialogComponent, {
        height: 'auto',
        width: '35rem',
        data: mockedPools,
      });
    });
  });

  describe('when edit app button clicked', () => {
    it('should open edit add dialog', () => {
      const dialogSpy = jasmine.createSpy('dialog.open');

      MockInstance(MatDialog, 'open', dialogSpy).and.returnValue({
        afterClosed: () => of(null),
      });

      const fixture = MockRender(AppsConfigComponent, params);
      const editAppButton = ngMocks.find(fixture, '#edit-app-button');

      editAppButton.triggerEventHandler('click', {});

      const coresList = String(mockedApps[0].cores);
      const poolName = mockedPools.find(
        (pool: Pools) => pool.id === mockedApps[0].pool_id
      )?.name;

      expect(dialogSpy).toHaveBeenCalledWith(AppsEditDialogComponent, {
        height: 'auto',
        width: '35rem',
        data: {
          pools: mockedPools,
          app: {
            ...mockedApps[0],
            coresList,
            poolName,
          },
        },
      });
    });
  });

  describe('when delete app button clicked', () => {
    it('should display a response', () => {
      const mockedResponse = {
        status: 200,
        message: `APP ${mockedApps[0].id} deleted`,
      };

      const deleteAppSpy = jasmine
        .createSpy('deleteApp')
        .and.returnValue(of(mockedResponse));
      const displayInfoSpy = jasmine.createSpy('displayInfo');

      MockInstance(AppqosService, 'deleteApp', deleteAppSpy);
      MockInstance(SnackBarService, 'displayInfo', displayInfoSpy);

      const {
        point: { componentInstance: component },
      } = MockRender(AppsConfigComponent, params);

      component.appEvent.subscribe((event) => {
        expect(event).toBeUndefined();
      });

      const deleteAppButton = ngMocks.find('#delete-app-button');
      deleteAppButton.triggerEventHandler('click', null);

      expect(deleteAppSpy).toHaveBeenCalledOnceWith(mockedApps[0].id);
      expect(displayInfoSpy).toHaveBeenCalledOnceWith(mockedResponse.message);
    });

    it('should handle error', () => {
      const mockedError: Error = {
        name: 'Error',
        message: 'rest API error',
      };

      const deleteAppSpy = jasmine
        .createSpy('deleteApp')
        .and.returnValue(throwError(() => mockedError));
      const handleErrorSpy = jasmine.createSpy('handleError');

      MockInstance(AppqosService, 'deleteApp', deleteAppSpy);
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      MockRender(AppsConfigComponent, params);

      const deleteAppButton = ngMocks.find('#delete-app-button');
      deleteAppButton.triggerEventHandler('click', null);

      expect(deleteAppSpy).toHaveBeenCalledWith(mockedApps[0].id);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    });
  });
});

