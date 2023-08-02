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

import { MockBuilder, MockInstance, MockRender } from 'ng-mocks';
import { TestbedHarnessEnvironment } from '@angular/cdk/testing/testbed';
import { MatGridListHarness } from '@angular/material/grid-list/testing';
import { CoreComponent } from './core.component';
import { Node, CoreInfo } from '../../system-caps/system-caps.model';
import { SharedModule } from 'src/app/shared/shared.module';
import { By } from '@angular/platform-browser';

describe('Given NodeComponent', () => {
  beforeEach(() =>
    MockBuilder(CoreComponent).keep(SharedModule)
  );

  const mockedCores: CoreInfo[] = [
    {
      "socket": 0,
      "lcore": 2,
      "L2ID": 1,
      "L3ID": 0,
      "sstbfHP": false
    },
    {
      "socket": 0,
      "lcore": 3,
      "L2ID": 1,
      "L3ID": 0,
      "sstbfHP": true
    },
  ];

  const mockedNode: Node = {
    nodeID: 0,
    cores: mockedCores,
  };

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should display the correct lcore IDs', async () => {

      const fixture = MockRender(CoreComponent, {
        L2ID: mockedCores[0].L2ID,
        node: mockedNode,
        detailedView: false,
      });
      const loader = TestbedHarnessEnvironment.loader(fixture);
      const grid = await loader.getHarness(MatGridListHarness);
      const tiles = await grid.getTiles();

      expect(tiles.length).toBe(mockedCores.length);

      tiles.forEach(async (tile, i) => {
        const tileText = await tile.host().then((elem) => elem.text());
        expect(tileText).toBe(mockedCores[i].lcore.toString());
      });
    });

    it('should display the correct L2ID if detailedView is true', async () => {

      const fixture = MockRender(CoreComponent, {
        L2ID: mockedCores[0].L2ID,
        node: mockedNode,
        detailedView: true,
      });
      const loader = TestbedHarnessEnvironment.loader(fixture);
      const grid = await loader.getHarness(MatGridListHarness);
      const lcoreTileRows = await grid.getTiles().then((tiles) => tiles[0].getRowspan());
      const l2idTile = await grid.getTileAtPosition({ row: lcoreTileRows + 1, column: 0 });

      const tileText = await l2idTile.host().then((elem) => elem.text());
      expect(tileText).toBe('L2ID: ' + fixture.componentInstance.L2ID.toString());
    });

    it('should not display the L2ID if detailedView is false', async () => {

      let errorMessage = '';
      const fixture = MockRender(CoreComponent, {
        L2ID: mockedCores[0].L2ID,
        node: mockedNode,
        detailedView: false,
      });
      const loader = TestbedHarnessEnvironment.loader(fixture);
      const grid = await loader.getHarness(MatGridListHarness);
      const lcoreTileRows = await grid.getTiles().then((tiles) => tiles[0].getRowspan());

      try {
        await grid.getTileAtPosition({ row: lcoreTileRows + 1, column: 0 });
      } catch (e: any) {
        errorMessage = e.message;
      }
      expect(errorMessage).toBe("Could not find tile at given position.");
    });

    it('should highlight SST-BF high priority cores', async () => {
      const fixture = MockRender(CoreComponent, {
        L2ID: mockedCores[0].L2ID,
        node: mockedNode,
        detailedView: false,
      });

      const debugElem = fixture.debugElement;
      const componentElem = debugElem.queryAll(By.css('.lcore'));

      componentElem.forEach((elem, i) => {
        if (mockedCores[i].sstbfHP) {
          expect(elem.styles['borderBottom']?.toString().includes('solid')).toBeTrue();
        } else {
          expect(elem.styles['borderBottom']).toBe('1px solid lightgrey');
        }
      });
    });
  });

});
