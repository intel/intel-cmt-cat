/*BSD LICENSE

Copyright(c) 2023 Intel Corporation. All rights reserved.

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
import { of } from 'rxjs';

import {
  MatSlideToggle,
  MatSlideToggleChange,
} from '@angular/material/slide-toggle';

import { AppqosService } from 'src/app/services/appqos.service';
import { SharedModule } from 'src/app/shared/shared.module';
import { SSTBF } from '../../system-caps/system-caps.model';
import { SstBfComponent } from './sst-bf.component';

describe('Given SstBfComponent', () => {
  beforeEach(() => MockBuilder(SstBfComponent).mock(SharedModule));

  MockInstance.scope('case');

  describe('when initialized and available', () => {
    it('should display "SST-BF" title', () => {
      const mockedSSTBF: SSTBF = {
        configured: true,
        hp_cores: [1, 2],
        std_cores: [1, 2],
      };

      MockRender(SstBfComponent, {
        available: true,
        sstbf: mockedSSTBF,
      });

      const expectValue = ngMocks.formatText(ngMocks.find('mat-card-title'));

      expect(expectValue).toEqual('Speed Select Technology - Base Frequency (SST-BF)');
    });
  });

  describe('when initialized and NOT available', () => {
    it('should display "Not currently available or supported" message', () => {
      const message = 'Not currently available or supported...';
      MockRender(SstBfComponent, { available: false });

      const expectedTitle = ngMocks.formatText(ngMocks.find('h2'));

      expect(expectedTitle).toEqual(message);
    });

    it('should disabled SST-BF enable toggle', () => {
      MockRender(SstBfComponent, { available: false });

      const template = ngMocks.find('mat-slide-toggle');

      expect(template.attributes['ng-reflect-disabled']).toEqual('true');
    });
  });

  describe('when slide toggle is clicked', () => {
    it('should emit "onChange" event with correct value', (done) => {
      const mockedSSTBF: SSTBF = {
        configured: true,
        hp_cores: [1, 2],
        std_cores: [1, 2],
      };

      const event: MatSlideToggleChange = {
        checked: false,
        source: {} as MatSlideToggle,
      };

      const RDTSpy = jasmine.createSpy('getSstbf');

      MockInstance(AppqosService, 'getSstbf', RDTSpy).and.returnValue(
        of(mockedSSTBF)
      );

      const fixture = MockRender(SstBfComponent, {
        available: true,
        sstbf: mockedSSTBF,
      });

      const component = fixture.point.componentInstance;
      const toggle = ngMocks.find('mat-slide-toggle');

      component.sstbfEvent.subscribe((value) => {
        expect(value.checked).toBeFalse();

        done();
      });

      toggle.triggerEventHandler('change', event);
    });
  });
});
