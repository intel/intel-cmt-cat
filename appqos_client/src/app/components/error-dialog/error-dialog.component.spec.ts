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

import { MAT_DIALOG_DATA, MatDialogRef } from '@angular/material/dialog';
import { ErrorDialogComponent } from './error-dialog.component';
import { MockBuilder, MockRender, ngMocks } from 'ng-mocks';
import { SharedModule } from 'src/app/shared/shared.module';

describe('ErrorDialogComponent', () => {
  beforeEach(() => {
    return MockBuilder(ErrorDialogComponent)
      .mock(SharedModule)
      .mock(MatDialogRef);

  });

  describe('when initialized', () => {
    it('it should display header', () => {
      MockRender(ErrorDialogComponent, {}, {
        providers: [
          { provide: MAT_DIALOG_DATA, useValue: { pools: [] } }
        ]
      });

      const headerText = ngMocks.formatText(ngMocks.find('h2'));

      expect(headerText).toBe('Configuration file missing required fields');
    });

    it('it should display missing fields and pool ID', () => {
      const mockedPools = [
        { id: 0, fields: ['l3cbm', 'mba', 'l2cdp_data', 'l2cdp_code'] },
        { id: 1, fields: ['l3cbm','l2cdp_data'] },
        { id: 2, fields: ['mba', 'l3cdp_data', 'l3cdp_code'] },
        { id: 3, fields: ['l2cbm', 'mba', 'l3cdp_data'] },
      ];

      MockRender(ErrorDialogComponent, {}, {
        providers: [
          { provide: MAT_DIALOG_DATA, useValue: { pools: mockedPools } }
        ]
      });

      const pools = ngMocks.findAll('.pool');

      pools.forEach((pool, index) => {
        const text = ngMocks.formatText(pool);

        mockedPools[index].fields.forEach((field) => {
          expect(text.includes(field)).toBeTrue();
        });

        expect(text.includes(`Pool ${index}`)).toBeTrue();

      });
    });
  });
});
