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

import { MockBuilder, MockRender, ngMocks } from "ng-mocks";
import { PowerProfilesComponent } from "./power-profiles.component";
import { SharedModule } from "src/app/shared/shared.module";
import { Router } from "@angular/router";
import { EnergyPerformPref, PowerProfiles } from "../system-caps/system-caps.model";
import { HarnessLoader } from "@angular/cdk/testing";
import { TestbedHarnessEnvironment } from "@angular/cdk/testing/testbed";
import { MatTableHarness } from "@angular/material/table/testing";
import { MatTableModule } from "@angular/material/table";

describe('Given PowerProfilesComponent', () => {
  beforeEach(() =>
    MockBuilder(PowerProfilesComponent)
      .mock(SharedModule)
      .mock(Router)
      .keep(MatTableModule)
  );

  const mockedPwrProfiles: PowerProfiles[] = [
    {
      id: 0,
      name: 'profile_0',
      min_freq: 1000,
      max_freq: 1200,
      epp: 'balance_performance'
    },
    {
      id: 1,
      name: 'profile_1',
      min_freq: 1000,
      max_freq: 1200,
      epp: 'balance_power'
    },
    {
      id: 2,
      name: 'profile_2',
      min_freq: 1000,
      max_freq: 1200,
      epp: 'power'
    },
    {
      id: 3,
      name: 'profile_3',
      min_freq: 1000,
      max_freq: 1200,
      epp: 'performance'
    }
  ];

  const params = {
    pwrProfiles: mockedPwrProfiles
  };

  describe('when initialized', () => {
    it('it should display correct title', () => {
      const title = "Speed Select Technology - Core Power (SST-CP)";

      MockRender(PowerProfilesComponent, params);

      const expectValue = ngMocks.formatText(ngMocks.find('mat-card-title'));
      expect(expectValue).toEqual(title);
    });

    it('it should display add button', () => {
      MockRender(PowerProfilesComponent, params);

      const expectValue = ngMocks.find('mat-mini-fab', null);

      expect(expectValue).toBeDefined();
    });

    it('it should display table if there are power profiles', async () => {
      const fixture = MockRender(PowerProfilesComponent, params);
      const loader: HarnessLoader = TestbedHarnessEnvironment.loader(fixture);

      const cells = await loader
        .getHarness(MatTableHarness)
        .then((table) => table.getCellTextByColumnName());

      mockedPwrProfiles.forEach((profile, i) => {
        expect(cells['id'].text[i]).toBe(String(profile.id));
        expect(cells['name'].text[i]).toBe(profile.name);
        expect(cells['minFreq'].text[i]).toBe(String(profile.min_freq) + 'Mhz');
        expect(cells['maxFreq'].text[i]).toBe(String(profile.max_freq) + 'Mhz');
        expect(
          cells['epp'].text[i].replace(' ', '_').toLowerCase()
        ).toBe(profile.epp);
      });
    });

    it('it should not display table if there are no power profiles', () => {
      const params = {
        pwrProfiles: []
      };
      const msg = 'No profiles currently configured';

      MockRender(PowerProfilesComponent, params);

      const expectedMsg = ngMocks.formatText(ngMocks.find('span'));
      const table = ngMocks.find('table', null);

      expect(expectedMsg).toEqual(msg);
      expect(table).toBeNull();
    });
  });

  describe('when click "See more"', () => {
    it('should redirect to wiki page', () => {
      MockRender(PowerProfilesComponent, params);

      const infoUrl = ngMocks.find('a').nativeElement.getAttribute('href');

      expect(infoUrl).toEqual(
        'https://www.intel.com/content/www/us/en/architecture-and-technology/speed-select-technology-article.html'
      );
    });
  });
});