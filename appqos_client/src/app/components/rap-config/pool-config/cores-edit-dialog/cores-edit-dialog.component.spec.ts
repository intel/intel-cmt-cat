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

import { MatDialogRef, MAT_DIALOG_DATA } from "@angular/material/dialog"
import { MockBuilder, MockInstance, MockRender, ngMocks } from "ng-mocks"
import { AppqosService } from "src/app/services/appqos.service"
import { LocalService } from "src/app/services/local.service"
import { SharedModule } from "src/app/shared/shared.module"
import { SnackBarService } from "src/app/shared/snack-bar.service"
import { CoresEditDialogComponent } from "./cores-edit-dialog.component"
import { EMPTY, of, throwError } from 'rxjs';
import { Pools } from "src/app/components/overview/overview.model"
import { MatError } from "@angular/material/form-field"

describe('Given coresEditDialog component', () => {
  beforeEach(() => {
    return MockBuilder(CoresEditDialogComponent)
      .mock(SharedModule)
      .mock(AppqosService, {
        poolPut: () => EMPTY
      })
      .mock(LocalService)
      .mock(SnackBarService)
      .mock(MatDialogRef)
  })

  const mockedPool: Pools = {
    id: 0,
    name: 'pool_0',
    cores: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
  }

  MockInstance.scope('case');

  describe('when saveCores method is called', () => {
    it('it should return if the form is invalid', () => {
      const invalidCores = '';
      const coresError = 'Cores is required!';

      const poolPutSpy = jasmine.createSpy();
      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const fixture = MockRender(CoresEditDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedPool
          }
        ]
      });

      const component = fixture.point.componentInstance;
      component.form.patchValue({ cores: invalidCores });
      fixture.detectChanges();

      const expectCoresError = ngMocks.formatText(
        ngMocks.findAll(MatError)[0]
      );

      const saveCoresButton = ngMocks.find('#save-cores-button');
      saveCoresButton.triggerEventHandler('click', null);

      expect(expectCoresError).toBe(coresError);
      expect(poolPutSpy).not.toHaveBeenCalled();
      expect(component.form.invalid).toBeTrue();
    })

    it('it should return if one of the cores exceed MAX_CORES', () => {
      const mockedCores = '1024,1025';
      const coresError = 'limit cores to maximum number of 1024';

      const poolPutSpy = jasmine.createSpy();
      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const fixture = MockRender(CoresEditDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedPool
          }
        ]
      });

      const component = fixture.point.componentInstance;
      component.form.patchValue({ cores: mockedCores });

      const saveCoresButton = ngMocks.find('#save-cores-button');
      saveCoresButton.triggerEventHandler('click', null);

      fixture.detectChanges();

      const expectCoresError = ngMocks.formatText(
        ngMocks.findAll(MatError)[0]
      );

      expect(expectCoresError).toBe(coresError);
      expect(component.form.controls['cores'].hasError('incorrect')).toBeTrue();
      expect(poolPutSpy).not.toHaveBeenCalled();
    })

    it('it should throw error if cores are not formatted correctly', () => {
      const mockedCores = '25,35-42,50-67';
      const coresError = 'List of cores e.g. 1,2 or 1,2-5 or 1-5';

      const poolPutSpy = jasmine.createSpy();
      MockInstance(AppqosService, 'poolPut', poolPutSpy);

      const fixture = MockRender(CoresEditDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedPool
          }
        ]
      });
      const component = fixture.point.componentInstance;

      component.form.patchValue({ cores: mockedCores });
      fixture.detectChanges();

      const expectCoresError = ngMocks.formatText(
        ngMocks.findAll(MatError)[0]
      )

      expect(expectCoresError).toBe(coresError);
      expect(component.form.controls['cores'].hasError('pattern')).toBeTrue();
    })

    it('it should throw error if the input field exceeds 4096 characters', () => {
      const coresError = 'max length 4096 characters';
      let cores: string = '0';
      for (let i = 1; i < 5000; i++){
        cores = `${cores}, ${i}`;
      }

      const fixture = MockRender(CoresEditDialogComponent, {}, {
        providers: [
          {
            provide: MAT_DIALOG_DATA,
            useValue: mockedPool
          }
        ]
      });
      const component = fixture.point.componentInstance;

      component.form.patchValue({ cores });
      fixture.detectChanges();

      const expectCoresError = ngMocks.formatText(
        ngMocks.findAll(MatError)[1]
      )

      expect(expectCoresError).toBe(coresError);
      expect(component.form.controls['cores'].hasError('maxlength'));
    })

    it('it should handle PUT request response and close the dialog', () => {
      const poolId = 0;
      const mockedCores = '22-30';
      const mockedCoresList = [22, 23, 24, 25, 26, 27, 28, 29, 30];
      const mockedResponse = {
        message: `POOL ${poolId} updated`
      }

      const getCoresDashSpy = jasmine.createSpy().and.returnValue(mockedCoresList);
      const poolPutSpy = jasmine.createSpy().and.returnValue(of(mockedResponse));
      const snackBarSpy = jasmine.createSpy();
      const closeSpy = jasmine.createSpy();

      MockInstance(LocalService, 'getCoresDash', getCoresDashSpy);
      MockInstance(AppqosService, 'poolPut', poolPutSpy);
      MockInstance(SnackBarService, 'displayInfo', snackBarSpy);
      MockInstance(MatDialogRef, 'close', closeSpy);

      const fixture = MockRender(CoresEditDialogComponent,
        {},
        {
          providers: [
            { provide: MAT_DIALOG_DATA, useValue: mockedPool }
          ]
        }
      )

      const component = fixture.point.componentInstance;
      component.form.patchValue({ cores: mockedCores });

      const saveCoresButton = ngMocks.find('#save-cores-button');
      saveCoresButton.triggerEventHandler('click', null);

      expect(getCoresDashSpy).toHaveBeenCalledOnceWith(mockedCores);
      expect(poolPutSpy).toHaveBeenCalledOnceWith({ cores: mockedCoresList }, poolId);
      expect(snackBarSpy).toHaveBeenCalledOnceWith(mockedResponse.message);
      expect(closeSpy).toHaveBeenCalledOnceWith(true);
    })

    it('it should handle PUT request error', () => {
      const poolId = 0;
      const mockedCores = '15-21';
      const mockedCoresList = [15, 16, 17, 18, 19, 20, 21];
      const mockedError = {
        message: `POOL ${poolId} not updated`
      }

      const getCoresDashSpy = jasmine.createSpy().and.returnValue(mockedCoresList);
      const poolPutSpy = jasmine.createSpy().and.returnValue(
        throwError(() => mockedError)
      );
      const snackBarSpy = jasmine.createSpy();

      MockInstance(LocalService, 'getCoresDash', getCoresDashSpy);
      MockInstance(AppqosService, 'poolPut', poolPutSpy);
      MockInstance(SnackBarService, 'handleError', snackBarSpy);

      const fixture = MockRender(CoresEditDialogComponent,
        {},
        {
          providers: [
            { provide: MAT_DIALOG_DATA, useValue: mockedPool }
          ]
        }
      )

      const component = fixture.point.componentInstance;
      component.form.patchValue({ cores: mockedCores });

      const saveCoresButton = ngMocks.find('#save-cores-button');
      saveCoresButton.triggerEventHandler('click', null);

      expect(getCoresDashSpy).toHaveBeenCalledOnceWith(mockedCores);
      expect(poolPutSpy).toHaveBeenCalledOnceWith({ cores: mockedCoresList }, poolId);
      expect(snackBarSpy).toHaveBeenCalledOnceWith(mockedError.message);
    })
  })
})