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

import { MockBuilder, MockInstance, MockRender } from 'ng-mocks';

import { LocalService } from './local.service';

describe('Given LocalService', () => {
  beforeEach(() =>
    MockBuilder(LocalService)
  );

  MockInstance.scope('case');

  describe('when saveData method is executed', () => {
    it('it should store data to the LocalStorage', () => {
      const { point: { componentInstance: service } } = MockRender(LocalService);

      service.saveData('hostName', 'localhost');

      expect(localStorage.getItem('hostName')).toBe('localhost');
    });
  });

  describe('when getData method is executed', () => {
    it('it should return data from LocalStorage', () => {
      const { point: { componentInstance: service } } = MockRender(LocalService);

      localStorage.setItem('portNumber', '5000');

      const expectedValue = service.getData('portNumber');

      expect(expectedValue).toBe('5000');
    });
  });

  describe('when clearData method is executed', () => {
    it('it should clear data from LocalStorage', () => {
      const { point: { componentInstance: service } } = MockRender(LocalService);

      localStorage.setItem('portNumber', '5000');

      service.clearData();

      expect(service.getData('portNumber')).toBeNull();
    });
  });
});
