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
import { NodeComponent } from './node.component';
import { CoreInfo, Node } from '../../system-caps/system-caps.model';
import { SharedModule } from 'src/app/shared/shared.module';

describe('Given NodeComponent', () => {
  beforeEach(() => MockBuilder(NodeComponent).keep(SharedModule));

  const mockedCores: CoreInfo[] = [
    {
      socket: 0,
      lcore: 0,
      L2ID: 0,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 1,
      L2ID: 1,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 2,
      L2ID: 2,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 3,
      L2ID: 3,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 4,
      L2ID: 4,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 5,
      L2ID: 5,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 6,
      L2ID: 6,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 7,
      L2ID: 7,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 8,
      L2ID: 0,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 9,
      L2ID: 1,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 10,
      L2ID: 2,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 11,
      L2ID: 3,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 12,
      L2ID: 4,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 13,
      L2ID: 5,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 14,
      L2ID: 6,
      L3ID: 0,
    },
    {
      socket: 0,
      lcore: 15,
      L2ID: 7,
      L3ID: 0,
    },
  ];

  const mockedNode: Node = {
    nodeID: 0,
    cores: mockedCores,
  };

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should group cores by L2 and L3 ID', async () => {
      const fixture = MockRender(NodeComponent, {
        node: mockedNode,
        detailedView: false,
      });

      expect(fixture.point.componentInstance.L3IDS).toEqual([0]);
      expect(fixture.point.componentInstance.L2IDS).toEqual([
        0, 1, 2, 3, 4, 5, 6, 7,
      ]);
    });

    it('should set number of columns to 8 when 16 cores and 8 L2IDs', async () => {
      const fixture = MockRender(NodeComponent, {
        node: mockedNode,
        detailedView: false,
      });

      expect(fixture.point.componentInstance.numCols).toBe(8);
    });

    it('should set number of columns to 16 with 16 cores and 16 L2IDs', async () => {
      const localMockedCores = mockedCores.map((core) => {
        const coreInfo = { ...core };

        coreInfo.L2ID = core.lcore;
        return coreInfo;
      });

      const mockedNode: Node = {
        nodeID: 0,
        cores: localMockedCores,
      };

      const fixture = MockRender(NodeComponent, {
        node: mockedNode,
        detailedView: false,
      });

      expect(fixture.point.componentInstance.numCols).toBe(16);
    });

    it('should match number of columns to number of cores when less than 8', async () => {
      const mockedNode: Node = {
        nodeID: 0,
        cores: mockedCores.slice(0, 4),
      };

      const fixture = MockRender(NodeComponent, {
        node: mockedNode,
        detailedView: false,
      });

      expect(fixture.point.componentInstance.numCols).toBe(4);
    });

    it('should use large row height when detailedView is true', async () => {
      const mockedNode: Node = {
        nodeID: 0,
        cores: mockedCores,
      };

      const fixture = MockRender(NodeComponent, {
        node: mockedNode,
        detailedView: true,
      });

      expect(fixture.point.componentInstance.rowHeight).toBe('30px');
    });

    it('should display the correct node ID', async () => {
      const mockedNode: Node = {
        nodeID: 0,
        cores: mockedCores.slice(0, 8),
      };

      const fixture = MockRender(NodeComponent, {
        node: mockedNode,
        detailedView: false,
      });

      const loader = TestbedHarnessEnvironment.loader(fixture);
      const grid = await loader.getHarness(MatGridListHarness);
      const tiles = await grid.getTiles();

      expect(await tiles[0].host().then((elem) => elem.text())).toBe(
        'Node ' + mockedNode.nodeID.toString()
      );
    });

    it('should display the correct L3ID if detailedView is true', async () => {
      const mockedNode: Node = {
        nodeID: 0,
        cores: mockedCores.slice(0, 8),
      };

      const fixture = MockRender(NodeComponent, {
        node: mockedNode,
        detailedView: true,
      });

      const loader = TestbedHarnessEnvironment.loader(fixture);
      const grid = await loader.getHarness(MatGridListHarness);
      const tiles = await grid.getTiles();

      expect(
        await tiles[tiles.length - 1].host().then((elem) => elem.text())
      ).toBe('L3ID: ' + mockedCores[0].L3ID?.toString());
    });

    it('should not display if cores are undefined', async () => {
      let errorMessage = '';

      const mockedNode: Node = {
        nodeID: 0,
        cores: undefined,
      };

      const fixture = MockRender(NodeComponent, {
        node: mockedNode,
        detailedView: false,
      });

      expect(fixture.point.componentInstance.L2IDS).toEqual([]);
      expect(fixture.point.componentInstance.L3IDS).toEqual([]);

      const loader = TestbedHarnessEnvironment.loader(fixture);
      try {
        await loader.getHarness(MatGridListHarness);
      } catch (e: any) {
        errorMessage = e.message;
      }
      expect(errorMessage).toContain('Failed to find element');
    });
  });
});
