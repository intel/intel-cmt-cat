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
import { of } from 'rxjs';

import { AppqosService } from 'src/app/services/appqos.service';
import { SharedModule } from 'src/app/shared/shared.module';
import { SSTBF } from '../system-caps.model';
import { SstcpComponent } from './sstcp.component';

describe('Given SstpcComponent', () => {
  beforeEach(() => MockBuilder(SstcpComponent).mock(SharedModule));

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should display "SST-CP" title', () => {
      const mockedSSTBF: SSTBF = {
        configured: true,
        hp_cores: [1, 2],
        std_cores: [1, 2],
      };

      MockInstance(AppqosService, 'getSstbf', () => of(mockedSSTBF));

      MockRender(SstcpComponent, {
        isSupported: true,
      });

      const expectValue = ngMocks.formatText(ngMocks.find('div'));

      expect(expectValue).toEqual('SST-CP');
    });
  });

  describe('when initialized and SST-CP is NOT supported', () => {
    it('should NOT display SST-CP details', () => {
      const mockedSSTBF: SSTBF = {
        configured: true,
        hp_cores: [1, 2],
        std_cores: [1, 2],
      };

      const RDTSpy = jasmine.createSpy('getSstbf');

      MockInstance(AppqosService, 'getSstbf', RDTSpy).and.returnValue(
        of(mockedSSTBF)
      );

      MockRender(SstcpComponent, {
        isSupported: false,
      });

      const template = ngMocks
        .findAll('span')
        .map((span) => ngMocks.formatText(span));

      expect(template).toEqual(['Available', 'No']);
    });
  });

  describe('when SST-CP is supported and SST-BF configured is true', () => {
    it('should display SST-CP in NOT Available', () => {
      const mockedSSTBF: SSTBF = {
        configured: true,
        hp_cores: [1, 2],
        std_cores: [1, 2],
      };

      const RDTSpy = jasmine.createSpy('getSstbf');

      MockInstance(AppqosService, 'getSstbf', RDTSpy).and.returnValue(
        of(mockedSSTBF)
      );

      MockRender(SstcpComponent, {
        isSupported: true,
      });

      const template = ngMocks
        .findAll('span')
        .map((span) => ngMocks.formatText(span));

      expect(template).toEqual(['Available', 'No']);
    });
  });

  describe('when SST-CP is supported and SST-BF configured is false', () => {
    it('should display SST-CP is Available', () => {
      const mockedSSTBF: SSTBF = {
        configured: false,
        hp_cores: [1, 2],
        std_cores: [1, 2],
      };

      const RDTSpy = jasmine.createSpy('getSstbf');

      MockInstance(AppqosService, 'getSstbf', RDTSpy).and.returnValue(
        of(mockedSSTBF)
      );

      MockRender(SstcpComponent, {
        isSupported: true,
      });

      const template = ngMocks
        .findAll('span')
        .map((span) => ngMocks.formatText(span));

      expect(template).toEqual(['Available', 'Yes']);
    });
  });
});
