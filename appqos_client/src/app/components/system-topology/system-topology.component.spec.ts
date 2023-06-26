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
import { SystemTopologyComponent } from './system-topology.component';
import { SharedModule } from 'src/app/shared/shared.module';
import { LocalService } from 'src/app/services/local.service';
import { of, EMPTY } from 'rxjs';
import {
  MatSlideToggle,
  MatSlideToggleChange,
} from '@angular/material/slide-toggle';

describe('Given SystemTopologyComponent', () => {
  beforeEach(() =>
    MockBuilder(SystemTopologyComponent)
      .keep(SharedModule)
      .mock(LocalService, {
        getSstbfEvent: () => EMPTY,
      })
  );

  const mockedTopology = {
    vendor: 'INTEL',
    cache: [
      {
        level: 2,
        num_ways: 20,
        num_sets: 1024,
        num_partitions: 1,
        line_size: 64,
        total_size: 1310720,
        way_size: 65536,
      },
      {
        level: 3,
        num_ways: 12,
        num_sets: 57344,
        num_partitions: 1,
        line_size: 64,
        total_size: 44040192,
        way_size: 3670016,
      },
    ],
    core: [
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
        socket: 1,
        lcore: 8,
        L2ID: 8,
        L3ID: 1,
      },
      {
        socket: 1,
        lcore: 9,
        L2ID: 9,
        L3ID: 1,
      },
      {
        socket: 1,
        lcore: 10,
        L2ID: 10,
        L3ID: 1,
      },
      {
        socket: 1,
        lcore: 11,
        L2ID: 11,
        L3ID: 1,
      },
      {
        socket: 1,
        lcore: 12,
        L2ID: 12,
        L3ID: 1,
      },
      {
        socket: 1,
        lcore: 13,
        L2ID: 13,
        L3ID: 1,
      },
      {
        socket: 1,
        lcore: 14,
        L2ID: 14,
        L3ID: 1,
      },
      {
        socket: 1,
        lcore: 15,
        L2ID: 15,
        L3ID: 1,
      },
    ],
  };

  const mockedSstbf = {
    configured: false,
    hp_cores: [2, 3, 5, 7, 9, 12],
    std_cores: [0, 1, 4, 6, 8, 10, 11, 13, 14, 15],
  };

  MockInstance.scope('case');

  describe('when initialized', () => {
    it('should populate nodes', async () => {
      MockInstance(LocalService, 'getSstbfEvent', () => of(mockedSstbf));

      const component = MockRender(SystemTopologyComponent, {
        systemTopology: mockedTopology,
      }).point.componentInstance;

      expect(component.nodes.length).toBe(2);
    });

    it('should populate node cores', async () => {
      MockInstance(LocalService, 'getSstbfEvent', () => of(mockedSstbf));

      const component = MockRender(SystemTopologyComponent, {
        systemTopology: mockedTopology,
      }).point.componentInstance;

      // nodes currently based on L3ID
      component.nodes.forEach((node) => {
        expect(node.cores).toEqual(
          mockedTopology.core.filter(
            (core) => core.L3ID === node.nodeID
          )
        );
      });
    });

    it('should set SST-BF high priority cores', async () => {
      const mockedSstbfConfigured = { ...mockedSstbf, configured: true };

      MockInstance(LocalService, 'getSstbfEvent', () =>
        of(mockedSstbfConfigured)
      );

      const component = MockRender(SystemTopologyComponent, {
        systemTopology: mockedTopology,
      }).point.componentInstance;

      let sstbfHPCores: number[] = [];

      component.nodes.forEach((node) => {
        const nodeCores = node.cores
          ?.filter((core) => core.sstbfHP)
          .map((core) => core.lcore);

        sstbfHPCores = sstbfHPCores.concat(nodeCores ?? []);
      });
      expect(sstbfHPCores).toEqual(mockedSstbfConfigured.hp_cores);
    });
  });

  describe('when detailed view slide toggle is clicked', () => {
    it('should toggle detailedView value', () => {
      const event: MatSlideToggleChange = {
        checked: false,
        source: {} as MatSlideToggle,
      };

      MockInstance(LocalService, 'getSstbfEvent', () => of(mockedSstbf));

      const component = MockRender(SystemTopologyComponent, {
        systemTopology: mockedTopology,
      }).point.componentInstance;

      expect(component.detailedView).toBe(false);

      // toggle and check value changed
      const toggle = ngMocks.find('mat-slide-toggle');
      toggle.triggerEventHandler('change', event);

      expect(component.detailedView).toBe(true);
    });
  });
});
