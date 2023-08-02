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

import { Clipboard } from '@angular/cdk/clipboard';
import { MatDialogRef, MAT_DIALOG_DATA } from '@angular/material/dialog';
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';
import { MockBuilder, MockInstance, MockRender, ngMocks } from 'ng-mocks';
import { SnackBarService } from 'src/app/shared/snack-bar.service';
import { AppqosConfig } from '../../components/system-caps/system-caps.model';
import { SharedModule } from '../../shared/shared.module';
import { DisplayConfigComponent } from './display-config.component';

describe('Given DisplayConfigComponent', () => {
  beforeEach(() =>
    MockBuilder(DisplayConfigComponent)
      .mock(SharedModule)
      .mock(MAT_DIALOG_DATA, {})
      .mock(MatDialogRef, {})
      .mock(Clipboard)
      .mock(SnackBarService)
      .keep(BrowserAnimationsModule)
  );

  const baseConfig: AppqosConfig = {
    rdt_iface: {
      interface: 'msr',
    },
    apps: [],
    pools: [
      {
        id: 0,
        l3cbm: 4095,
        name: 'Default',
        cores: [0, 1, 2, 3],
      },
    ],
  };

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should display the config', async () => {
      const fixture = MockRender(
        DisplayConfigComponent,
        {},
        {
          providers: [
            {
              provide: MAT_DIALOG_DATA,
              useValue: { config: JSON.stringify(baseConfig, null, 2) },
            },
          ],
        }
      );

      await fixture.whenStable();

      const expectedConfig = ngMocks.formatText(ngMocks.find('div.config'));

      expect(expectedConfig).toContain('"interface": "msr"');
      expect(expectedConfig).toContain('"name": "Default"');
      expect(expectedConfig).toContain('"l3cbm": 4095');
    });
  });

  describe('when Copy to clipboard button is clicked and copy succeeds', () => {
    it('should display Config copied to clipboard message', async () => {
      const displayInfoSpy = jasmine.createSpy('displayInfoSpy');
      MockInstance(SnackBarService, 'displayInfo', displayInfoSpy);
      MockInstance(Clipboard, 'copy', () => true);

      const fixture = MockRender(
        DisplayConfigComponent,
        {},
        {
          providers: [
            {
              provide: MAT_DIALOG_DATA,
              useValue: { config: JSON.stringify(baseConfig, null, 2) },
            },
          ],
        }
      );
      await fixture.whenStable();

      const copyButton = ngMocks.find('button');
      copyButton.triggerEventHandler('click', null);

      expect(displayInfoSpy).toHaveBeenCalledWith('Config copied to clipboard');
    });
  });

  describe('when Copy to clipboard button is clicked and copy fails', () => {
    it('should display Config copied to clipboard message', async () => {
      const handleErrorSpy = jasmine.createSpy('handleErrorSpy');
      MockInstance(SnackBarService, 'handleError', handleErrorSpy);
      MockInstance(Clipboard, 'copy', () => false);

      const fixture = MockRender(
        DisplayConfigComponent,
        {},
        {
          providers: [
            {
              provide: MAT_DIALOG_DATA,
              useValue: { config: JSON.stringify(baseConfig, null, 2) },
            },
          ],
        }
      );
      await fixture.whenStable();

      const copyButton = ngMocks.find('button');
      copyButton.triggerEventHandler('click', null);

      expect(handleErrorSpy).toHaveBeenCalledWith(
        'Failed to copy to clipboard'
      );
    });
  });
});
