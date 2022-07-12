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

import { HttpClientModule } from '@angular/common/http';
import { TestBed } from '@angular/core/testing';
import { MockBuilder, MockRender, ngMocks } from 'ng-mocks';

import {
  HttpClientTestingModule,
  HttpTestingController,
} from '@angular/common/http/testing';

import { Caps } from '../components/system-caps/system-caps.model';
import { AppqosService } from './appqos.service';
import { LocalService } from './local.service';

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
      const mockedCaps = { capabilities: ['cat', 'mba', 'sstbf', 'power'] };

      const {
        point: { componentInstance: service },
      } = MockRender(AppqosService);

      const httpMock = TestBed.inject(HttpTestingController);

      service.login(hostName, portNumber).subscribe((result) => {
        expect(result).toBe(mockedCaps);
      });

      const req = httpMock.expectOne(`${hostName}:${portNumber}/caps`);
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

      const req = httpMock.expectOne(`${hostName}:${portNumber}/caps`);
      req.flush(false);
      httpMock.verify();
    });
  });

  describe('when getCaps method is called', () => {
    it('it should return response', () => {
      const api_url = 'https://localhost:5000';

      const mockedCaps: Caps = {
        capabilities: ['cat', 'mba', 'sstbf', 'power'],
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
});
