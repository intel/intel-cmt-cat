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

import { HttpClientModule, HttpErrorResponse } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';
import { MockBuilder, MockRender, ngMocks } from 'ng-mocks';

import {
  HttpClientTestingModule,
  HttpTestingController,
} from '@angular/common/http/testing';

import {
  CacheAllocation,
  Caps,
  MBA,
  MBACTRL,
  RDTIface,
  SSTBF,
} from '../components/system-caps/system-caps.model';
import { AppqosService } from './appqos.service';
import { LocalService } from './local.service';
import { first } from 'rxjs';
import { Pools, Apps } from '../components/overview/overview.model';

describe('Given AppqosService', () => {
  beforeEach(() =>
    MockBuilder(AppqosService)
      .replace(HttpClientModule, HttpClientTestingModule)
      .keep(LocalService)
  );

  describe('when login method is called with correct credentials', () => {
    it('it should return response from HTTP request', () => {
      const hostName = 'https://localhost';
      const portNumber = '5000';
      const mockedCaps = { capabilities: ['l3cat', 'mba', 'sstbf', 'power'] };

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);

      service.login(hostName, portNumber).subscribe((result) => {
        expect(result).toBe(mockedCaps);
      });

      const req = httpMock.expectOne(`${hostName}:${portNumber}/caps/cpu`);
      req.flush(mockedCaps);
      httpMock.verify();
    });
  });

  describe('when login method is called with incorrect credentials', () => {
    it('it should return error from HTTP request', () => {
      const hostName = 'https://errorName';
      const portNumber = '404';

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);

      service.login(hostName, portNumber).subscribe((result) => {
        expect(result).toBeFalse();
      });

      const req = httpMock.expectOne(`${hostName}:${portNumber}/caps/cpu`);
      req.flush(false);
      httpMock.verify();
    });
  });

  describe('when getCaps method is called', () => {
    it('it should return response', () => {
      const api_url = 'https://localhost:5000';

      const mockedCaps: Caps = {
        capabilities: ['l3cat', 'mba', 'sstbf', 'power'],
      };

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.getCaps().subscribe((caps: Caps) => {
        expect(caps).toBe(mockedCaps);
      });

      const req = httpMock.expectOne(`${api_url}/caps`);
      req.flush(mockedCaps);
      httpMock.verify();
    });
  });

  describe('when getL3cat method is called', () => {
    it('it should return response', () => {
      const api_url = 'https://localhost:5000';

      const mockedL3cat: CacheAllocation = {
        cache_size: 44040192,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3670016,
      };

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.getL3cat().subscribe((cat: CacheAllocation) => {
        expect(cat).toBe(mockedL3cat);
      });

      const req = httpMock.expectOne(`${api_url}/caps/l3cat`);
      req.flush(mockedL3cat);
      httpMock.verify();
    });
  });

  describe('when getMbaCtrl method is called', () => {
    it('it should return response', () => {
      const api_url = 'https://localhost:5000';

      const mockedMbaCtrl: MBACTRL = {
        enabled: false,
        supported: true,
      };

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.getMbaCtrl().subscribe((mba: MBACTRL) => {
        expect(mba).toBe(mockedMbaCtrl);
      });

      const req = httpMock.expectOne(`${api_url}/caps/mba_ctrl`);
      req.flush(mockedMbaCtrl);
      httpMock.verify();
    });
  });

  describe('when getRdtIface method is called', () => {
    it('it should return response', () => {
      const api_url = 'https://localhost:5000';

      const mockedRDT: RDTIface = {
        interface: 'os',
        interface_supported: ['msr', 'os'],
      };

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.getRdtIface().subscribe((rdt: RDTIface) => {
        expect(rdt).toBe(mockedRDT);
      });

      const req = httpMock.expectOne(`${api_url}/caps/rdt_iface`);
      req.flush(mockedRDT);
      httpMock.verify();
    });
  });

  describe('when getSstbf method is called', () => {
    it('it should return response', () => {
      const api_url = 'https://localhost:5000';

      const mockedSSTBF: SSTBF = {
        configured: false,
        hp_cores: [1, 2],
        std_cores: [1, 2],
      };

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.getSstbf().subscribe((sstbf: SSTBF) => {
        expect(sstbf).toBe(mockedSSTBF);
      });

      const req = httpMock.expectOne(`${api_url}/caps/sstbf`);
      req.flush(mockedSSTBF);
      httpMock.verify();
    });
  });

  describe('when getMba method is called', () => {
    it('it should return response', () => {
      const api_url = 'https://localhost:5000';

      const mockedMba: MBA = {
        clos_num: 12,
        mba_enabled: true,
        mba_bw_enabled: false,
      };

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.getMba().subscribe((mba: MBA) => {
        expect(mba).toBe(mockedMba);
      });

      const req = httpMock.expectOne(`${api_url}/caps/mba`);
      req.flush(mockedMba);
      httpMock.verify();
    });
  });

  describe('when getL2cat method is called', () => {
    it('it should return response', () => {
      const api_url = 'https://localhost:5000';

      const mockedL2cat: CacheAllocation = {
        cache_size: 44040192,
        cdp_enabled: false,
        cdp_supported: false,
        clos_num: 15,
        cw_num: 12,
        cw_size: 3670016,
      };

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.getL2cat().subscribe((l2cat: CacheAllocation) => {
        expect(l2cat).toBe(mockedL2cat);
      });

      const req = httpMock.expectOne(`${api_url}/caps/l2cat`);
      req.flush(mockedL2cat);
      httpMock.verify();
    });
  });

  describe('when rdtIfacePut method is called', () => {
    it('it should call correct REST API endpoint with PUT method', () => {
      const api_url = 'https://localhost:5000';
      const mockResponse = 'RDT Interface modified';
      const body = 'os';

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service
        .rdtIfacePut(body)
        .pipe(first())
        .subscribe((response: unknown) => {
          expect(response).toBe(mockResponse);
        });

      const req = httpMock.expectOne(`${api_url}/caps/rdt_iface`);
      req.flush(mockResponse);
      httpMock.verify();
    });
  });

  describe('when sstbfPut method is called', () => {
    it('it should call correct REST API endpoint with PUT method', () => {
      const api_url = 'https://localhost:5000';
      const mockResponse = 'SST-BF caps modified';
      const body = true;

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service
        .sstbfPut(body)
        .pipe(first())
        .subscribe((response: unknown) => {
          expect(response).toBe(mockResponse);
        });

      const req = httpMock.expectOne(`${api_url}/caps/sstbf`);
      req.flush(mockResponse);
      httpMock.verify();
    });
  });

  describe('when mbaCtrlPut method is called', () => {
    it('it should call correct REST API endpoint with PUT method', () => {
      const api_url = 'https://localhost:5000';
      const mockResponse = 'MBA CTRL status changed.';
      const body = true;

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service
        .mbaCtrlPut(body)
        .pipe(first())
        .subscribe((response: unknown) => {
          expect(response).toBe(mockResponse);
        });

      const req = httpMock.expectOne(`${api_url}/caps/mba_ctrl`);
      req.flush(mockResponse);
      httpMock.verify();
    });
  });

  describe('when getPools method is called', () => {
    it('it should return response', () => {
      const api_url = 'https://localhost:5000';

      const mockedPool: Pools[] = [
        {
          id: 0,
          mba_bw: 4294967295,
          l3cbm: 2047,
          name: 'Default',
          cores: [0, 1, 45, 46, 47],
        },
      ];

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.getPools().subscribe((pools: Pools[]) => {
        expect(pools).toBe(mockedPool);
      });

      const req = httpMock.expectOne(`${api_url}/pools`);
      req.flush(mockedPool);
      httpMock.verify();
    });
  });

  describe('when poolPut method is called', () => {
    it('it should call correct REST API endpoint with PUT method', () => {
      const api_url = 'https://localhost:5000';
      const mockResponse = 'Pool 0 updated.';
      const body = { l3cbm: 7 };
      const id = 0;

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service
        .poolPut(body, id)
        .pipe(first())
        .subscribe((response: unknown) => {
          expect(response).toBe(mockResponse);
        });

      const req = httpMock.expectOne(`${api_url}/pools/${id}`);
      req.flush(mockResponse);
      httpMock.verify();
    });
  });

  describe('when poolPut method is called with incorrect L3 CBM', () => {
    it('it should return error', () => {
      const api_url = 'https://localhost:5000';
      const mockErrorResponse = {
        message:
          'POOL 0 not updated, Pool 0, L3 CBM 0xcc/0b11001100 is not contiguous.',
      };
      const body = { l3cbm: 204 };
      const id = 0;

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service
        .poolPut(body, id)
        .pipe(first())
        .subscribe((response: unknown) => {
          expect(response).toBe(mockErrorResponse);
        });

      const req = httpMock.expectOne(`${api_url}/pools/${id}`);
      req.flush(mockErrorResponse);
      httpMock.verify();
    });
  });

  describe('when deletePool method called with pool id', () => {
    it('it should return "POOL 0 deleted" message', () => {
      const api_url = 'https://localhost:5000';
      const poolID = 0;

      const mockResponse = {
        message: `POOL ${poolID} deleted`
      };

      const {
        point: { componentInstance: service }
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.deletePool(poolID).subscribe((response) => {
        expect(response).toBe(mockResponse);
      });

      const req = httpMock.expectOne(`${api_url}/pools/${poolID}`);
      req.flush(mockResponse);
      httpMock.verify();
    });
  });

  describe('when postPool method is called', () => {
    it('it should return "New POOL 0 added" message', () => {
      const api_url = 'https://localhost:5000';

      const mockResponse = {
        status: 201,
        body: {
          id: 0,
          message: "New POOL 0 added"
        }
      };
      type PostPool = Omit<Pools, 'id'>;

      const mockPool: PostPool = {
        name: 'test',
        cores: [1, 2, 3]
      };

      const {
        point: { componentInstance: service }
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.postPool(mockPool).subscribe((response: unknown) => {
        expect(response).toBe(mockResponse);
      });

      const req = httpMock.expectOne(`${api_url}/pools/`);
      req.flush(mockResponse);
      httpMock.verify();
    });
  });

  describe('when getApps method is called ', () => {
    it('it should return a response', () => {
      const api_url = 'https://localhost:5000';

      const mockedApps: Apps[] = [
        { id: 1, name: 'test', pids: [1, 2, 3], pool_id: 0 }
      ];

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.getApps().subscribe((apps: Apps[]) => {
        expect(apps).toBe(mockedApps);
      });

      const req = httpMock.expectOne(`${api_url}/apps`);
      req.flush(mockedApps);
      httpMock.verify();
    });
  });

  describe('when postApp method is called', () => {
    it('it should return "New APP added to pool 1" message', () => {
      const api_url = 'https://localhost:5000';

      const mockedApps: Apps = {
        id: 1,
        name: 'test',
        pids: [1, 2, 3],
        pool_id: 2
      };

      const mockedResponse = {
        status: 201,
        body: {
          id: 1,
          message: 'New APP added to pool 1'
        }
      };

      const {
        point: { componentInstance: service }
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.postApp(mockedApps).subscribe((response: unknown) => {
        expect(response).toBe(mockedResponse);
      });

      const req = httpMock.expectOne(`${api_url}/apps/`);
      req.flush(mockedResponse);
      httpMock.verify();
    });
  });

  describe('when appPut method is called', () => {
    it('it should return "APP 1 updated" message', () => {
      const api_url = 'https://localhost:5000';
      const appID = 1;

      const mockedApps: Apps = {
        id: 1,
        name: 'test',
        pids: [1, 2, 3],
        pool_id: 2
      };

      const mockedResponse = {
        status: 200,
        body: {
          message: `APP ${appID} updated`
        }
      };

      const {
        point: { componentInstance: service }
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.appPut(mockedApps, appID).subscribe((response: unknown) => {
        expect(response).toBe(mockedResponse);
      });

      const req = httpMock.expectOne(`${api_url}/apps/${appID}`);
      req.flush(mockedResponse);
      httpMock.verify();
    });
  });

  describe('when deleteApp method is called with id', () => {
    it('it should return "APP 0 deleted" message', () => {
      const api_url = 'https://localhost:5000';
      const appID = 0;

      const mockedResponse = {
        status: 200,
        body: {
          message: `APP ${appID} deleted`
        }
      };

      const {
        point: { componentInstance: service }
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service.deleteApp(appID).subscribe((response: unknown) => {
        expect(response).toBe(mockedResponse);
      });

      const req = httpMock.expectOne(`${api_url}/apps/${appID}`);
      req.flush(mockedResponse);
      httpMock.verify();
    });
  });

  describe('when l3CdpPut method is called', () => {
    it('it should call correct REST API endpoint with PUT method', () => {
      const api_url = 'https://localhost:5000';
      const mockResponse = 'L3 CDP status changed.';
      const body = true;

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service
        .l3CdpPut(body)
        .pipe(first())
        .subscribe((response: unknown) => {
          expect(response).toBe(mockResponse);
        });

      const req = httpMock.expectOne(`${api_url}/caps/l3cat`);
      req.flush(mockResponse);
      httpMock.verify();
    });
  });

  describe('when l2CdpPut method is called', () => {
    it('it should call correct REST API endpoint with PUT method', () => {
      const api_url = 'https://localhost:5000';
      const mockResponse = 'L2 CDP status changed.';
      const body = true;

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);
      const local = ngMocks.findInstance(LocalService);
      local.saveData('api_url', api_url);

      service
        .l2CdpPut(body)
        .pipe(first())
        .subscribe((response: unknown) => {
          expect(response).toBe(mockResponse);
        });

      const req = httpMock.expectOne(`${api_url}/caps/l2cat`);
      req.flush(mockResponse);
      httpMock.verify();
    });
  });

  describe('when handleError() is called with client side error', () => {
    it('it should return error status text', () => {
      const mockErrorResponse = {
        status: 0,
        statusText: "Client Error!"
      };
      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      service.handleError(mockErrorResponse as HttpErrorResponse).subscribe({
        // error response should be called
        error: (err) => expect(err.message)
        .toBe(mockErrorResponse.statusText)
      });
    });
  });

  describe('when handleError() is called with server side error', () => {
    it('it should return error message', () => {
      const mockErrorResponse = {
        status: 400,
        error: {
          message: "Server Error!"
        }
      };
      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      service.handleError(mockErrorResponse as HttpErrorResponse).subscribe({
        // error response should be called
        error: (err) => expect(err.message)
        .toBe(mockErrorResponse.error.message)
      });
    });
  });
});
