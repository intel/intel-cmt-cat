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

import { MockBuilder, MockInstance, MockRender, ngMocks } from "ng-mocks"
import { EMPTY, of, throwError } from "rxjs";
import { MatDialogRef, MAT_DIALOG_DATA } from "@angular/material/dialog";
import { AppqosService } from "src/app/services/appqos.service";
import { LocalService } from "src/app/services/local.service";
import { SharedModule } from "src/app/shared/shared.module";
import { SnackBarService } from "src/app/shared/snack-bar.service";
import { PoolAddDialogComponent } from "./pool-add-dialog.component";

describe('Given poolAddDialogComponent', () => {
  let parseNumberListSpy: any;

  beforeEach(() => {
    parseNumberListSpy = spyOn(
      LocalService.prototype,
      'parseNumberList'
    ).and.callThrough();
    MockInstance(LocalService, 'parseNumberList', parseNumberListSpy);

    return MockBuilder(PoolAddDialogComponent)
      .mock(SharedModule)
      .mock(LocalService, {
        getCapsEvent: () => EMPTY,
        getMbaCtrlEvent: () => EMPTY,
      })
      .mock(AppqosService, {
        postPool: () => EMPTY,
      })
      .mock(SnackBarService)
      .mock(MatDialogRef);
  });

  const mockedCaps = [
    'l3cat',
    'l2cat',
    'mba'
  ]

  const mockedMbaCtrl = false;
  const mockedData = {
    l2cwNum: 10,
    l3cwNum: 12
  }

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('it should populate the caps array & set mbaCtrl', () => {
      const getCapsEventSpy = jasmine.createSpy().and.returnValue(of(mockedCaps));
      const getMbaCtrlEventSpy = jasmine.createSpy().and.returnValue(of(mockedMbaCtrl));

      MockInstance(LocalService, 'getCapsEvent', getCapsEventSpy);
      MockInstance(LocalService, 'getMbaCtrlEvent', getMbaCtrlEventSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolAddDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedData
          }
        ]
      });

      expect(getCapsEventSpy).toHaveBeenCalledTimes(1);
      expect(component.caps).toEqual(mockedCaps);

      expect(getMbaCtrlEventSpy).toHaveBeenCalledTimes(1);
      expect(component.mbaCtrl).toBe(mockedMbaCtrl);
    })
  })

  describe('when savePool method is called', () => {
    MockInstance(LocalService, 'getCapsEvent', () => of(mockedCaps));
    MockInstance(LocalService, 'getMbaCtrlEvent', () => of(mockedMbaCtrl));

    it('it should create a pool', () => {
      const name = 'pool_0';
      const cores = '0-11';
      const mockedCoresList = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];
      const mockedPool = {
        name: name,
        cores: mockedCoresList,
        l3cbm: (1 << mockedData.l3cwNum) - 1,
        l2cbm: (1 << mockedData.l2cwNum) - 1,
        mba: 100
      }

      const {
        point: { componentInstance: component }
      } = MockRender(PoolAddDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedData
          }
        ]
      });

      const postPoolSpy = spyOn(component, 'postPool');

      component.form.patchValue({ name, cores });

      const savePoolButton = ngMocks.find('#save-pool-button');
      savePoolButton.triggerEventHandler('click', null);

      expect(component.form.valid).toBeTrue();
      expect(parseNumberListSpy).toHaveBeenCalledWith(cores);
      expect(component.coresList).toEqual(mockedCoresList);
      expect(postPoolSpy).toHaveBeenCalledWith(mockedPool);
    })

    it('it should return if the form is not valid', () => {
      const name = '';
      const cores = '';
      const coresErrorMessage = 'Cores is required!';
      const nameErrorMessage = 'Name is required!';

      const fixture = MockRender(PoolAddDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedData
          }
        ]
      });
      const component = fixture.point.componentInstance;
      const postPoolSpy = spyOn(component, 'postPool');

      component.form.patchValue({ name, cores });
      fixture.detectChanges();

      const coresReqError = ngMocks.formatText(
        ngMocks.find('#cores-req-error')
      );

      const nameReqError = ngMocks.formatText(
        ngMocks.find('#name-req-error')
      );

      const savePoolButton = ngMocks.find('#save-pool-button');
      savePoolButton.triggerEventHandler('click', null);

      expect(coresReqError).toBe(coresErrorMessage);
      expect(component.form.controls['cores'].errors?.['required']).toBeTrue();

      expect(nameReqError).toBe(nameErrorMessage);
      expect(component.form.controls['name'].errors?.['required']).toBeTrue();

      expect(parseNumberListSpy).not.toHaveBeenCalled();
      expect(postPoolSpy).not.toHaveBeenCalled();
    })

    it('it should throw error if the name input field exceeds 80 characters', () => {
      const cores = '11,22-43';
      const coresErrorMessage = 'max length 80 characters';

      let name = 'p';
      for (let i = 0; i < 80; i++){
        name += 'p';
      }
      
      const fixture = MockRender(PoolAddDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedData
          }
        ]
      });
      const component = fixture.point.componentInstance;
      component.form.patchValue({ name, cores });
      fixture.detectChanges();

      const coresError = ngMocks.formatText(
        ngMocks.find('#max-name-error')
      )

      expect(coresError).toBe(coresErrorMessage);
      expect(component.form.controls['name'].hasError('maxlength')).toBeTrue();
    })

    it('it should throw error if cores exceeds MAX_CORES', () => {
      const name = 'pool_0';
      const cores = '1024,1025';
      const maxCoresErrorText = 'limit cores to maximum number of 1024';

      const fixture = MockRender(
        PoolAddDialogComponent,
        {},
        {
          providers: [
            {
              provide: MAT_DIALOG_DATA,
              useValue: mockedData,
            },
          ],
        }
      );
      const component = fixture.point.componentInstance;
      const postPoolSpy = spyOn(component, 'postPool');

      component.form.patchValue({ name, cores });

      const savePoolButton = ngMocks.find('#save-pool-button');
      savePoolButton.triggerEventHandler('click', null);

      fixture.detectChanges();

      const maxCoresError = ngMocks.formatText(
        ngMocks.find('#max-cores-error')
      );

      expect(maxCoresError).toBe(maxCoresErrorText);
      expect(postPoolSpy).not.toHaveBeenCalled();
      expect(component.form.invalid).toBeTrue();
    });

    it('it should throw error if cores input field exceeds 4096 characters', () => {
      const name = 'pool_0';
      const maxCoresErrorText = 'max length 4096 characters';
      
      let cores: string = '0';
      for (let i = 1; i < 5000; i++){
        cores = `${cores}, ${i}`;
      }
      
      const fixture = MockRender(PoolAddDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedData
          }
        ]
      });
      const component = fixture.point.componentInstance;
      const postPoolSpy = spyOn(component, 'postPool');

      component.form.patchValue({ name, cores });
      fixture.detectChanges();

      const maxCoresError = ngMocks.formatText(
        ngMocks.find('#max-char-cores-error')
      );
      
      expect(maxCoresError).toBe(maxCoresErrorText);
      expect(postPoolSpy).not.toHaveBeenCalled();
      expect(component.form.invalid).toBeTrue();
      expect(component.form.controls['cores'].hasError('maxlength')).toBeTrue();
    })

    it('it should throw error if cores are not formatted correctly', () => {
      const name = 'pool_0';
      const cores = ',1,11-14,45-75';
      const coresErrorMessage = 'List of cores e.g. 1,2 or 1,2-5 or 1-5';

      const fixture = MockRender(PoolAddDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedData
          }
        ]
      });
      const component = fixture.point.componentInstance;

      component.form.patchValue({ name, cores });
      fixture.detectChanges();

      const coresError = ngMocks.formatText(
        ngMocks.find('#cores-pattern-error')
      );

      expect(coresError).toBe(coresErrorMessage);
      expect(component.form.controls['cores'].hasError('pattern')).toBeTrue();
    })
 
    it('it should create a pool', () => {
      const name = 'pool_0';
      const cores = '11,12';
      const mockedCoresList = [11, 12];
      const mockedPool = {
        name: name,
        cores: mockedCoresList,
        l3cbm: (1 << mockedData.l3cwNum) - 1,
        l2cbm: (1 << mockedData.l2cwNum) - 1,
        mba_bw: Math.pow(2, 32) - 1
      }

      const {
        point: { componentInstance: component }
      } = MockRender(PoolAddDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedData
          }
        ]
      });

      const postPoolSpy = spyOn(component, 'postPool');

      component.mbaCtrl = true;
      component.form.patchValue({ name, cores });

      const savePoolButton = ngMocks.find('#save-pool-button');
      savePoolButton.triggerEventHandler('click', null);

      expect(postPoolSpy).toHaveBeenCalledWith(mockedPool);
      expect(component.form.valid).toBeTrue();
    })
  });

  describe('when postPool method is called', () => {
    const mockedPool = {
      name: 'pool_0',
      cores: [1, 2, 3, 4, 5, 6, 7],
      mba: 100,
      l3cbm: 12,
      l2cbm: 10
    }

    MockInstance(LocalService, 'getCapsEvent', () => of(mockedCaps));
    MockInstance(LocalService, 'getMbaCtrlEvent', () => of(mockedMbaCtrl));

    it('it should display response', () => {
      const mockedResponse = {
        status: 200,
        message: 'NEW POOL 0 added'
      }

      const postPoolSpy = jasmine.createSpy().and.returnValue(of(mockedResponse));
      const displayInfoSpy = jasmine.createSpy();
      const closeSpy = jasmine.createSpy();

      MockInstance(AppqosService, 'postPool', postPoolSpy);
      MockInstance(SnackBarService, 'displayInfo', displayInfoSpy);
      MockInstance(MatDialogRef, 'close', closeSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolAddDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedData
          }
        ]
      });

      component.postPool(mockedPool);

      expect(postPoolSpy).toHaveBeenCalledOnceWith(mockedPool);
      expect(displayInfoSpy).toHaveBeenCalledOnceWith(mockedResponse.message);
      expect(closeSpy).toHaveBeenCalledOnceWith(mockedResponse);
    })

    it('it should handle error', () => {
      const mockedError: Error = {
        name: 'error',
        message: 'poolPut error'
      }

      const postPoolSpy = jasmine.createSpy().and.returnValue(
        throwError(() => mockedError)
      );
      const handleErrorSpy = jasmine.createSpy();

      MockInstance(AppqosService, 'postPool', postPoolSpy);
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);

      const {
        point: { componentInstance: component }
      } = MockRender(PoolAddDialogComponent);

      component.postPool(mockedPool);

      expect(postPoolSpy).toHaveBeenCalledOnceWith(mockedPool);
      expect(handleErrorSpy).toHaveBeenCalledOnceWith(mockedError.message);
    })
  })
});
