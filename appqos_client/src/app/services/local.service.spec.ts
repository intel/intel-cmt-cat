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
import { skip } from 'rxjs';
import { MbaAllocationComponent } from '../components/overview/mba-allocation/mba-allocation.component';
import { CacheAllocation, MBACTRL } from '../components/system-caps/system-caps.model';

import { LocalService } from './local.service';

describe('Given LocalService', () => {
  beforeEach(() => MockBuilder(LocalService));

  MockInstance.scope('case');

  describe('when saveData method is executed', () => {
    it('should store data to the LocalStorage', () => {
      const {
        point: { componentInstance: service },
      } = MockRender(LocalService);

      service.saveData('api_url', 'localhost:5000');

      expect(localStorage.getItem('api_url')).toBe('localhost:5000');
    });
  });

  describe('when getData method is executed', () => {
    it('should return data from LocalStorage', () => {
      const {
        point: { componentInstance: service },
      } = MockRender(LocalService);

      localStorage.setItem('api_url', 'localhost:5000');

      const expectedValue = service.getData('api_url');

      expect(expectedValue).toBe('localhost:5000');
    });
  });

  describe('when clearData method is executed', () => {
    it('should clear data from LocalStorage', () => {
      const {
        point: { componentInstance: service },
      } = MockRender(LocalService);

      localStorage.setItem('api_url', 'localhost:5000');

      service.clearData();

      expect(service.getData('api_url')).toBeNull();
    });
  });

  describe('when isLoggedIn method is executed and LocalStorage is null', () => {
    it('should return false', () => {
      const {
        point: { componentInstance: service },
      } = MockRender(LocalService);

      service.clearData();

      const expectedValue = service.isLoggedIn();

      expect(expectedValue).toBeFalse();
    });
  });

  describe('when isLoggedIn method is executed and LocalStorage is NOT null', () => {
    it('should return true', () => {
      const {
        point: { componentInstance: service },
      } = MockRender(LocalService);

      service.saveData('api_url', 'localhost:5000');

      const expectedValue = service.isLoggedIn();

      expect(expectedValue).toBeTrue();
    });
  });

  describe('when setIfaceEvent method is executed', () => {
    it('should emit ifaceEvent', (done: DoneFn) => {
      const {
        point: { componentInstance: service },
      } = MockRender(LocalService);

      service.ifaceEvent.subscribe((event) => {
        expect(event).toBeUndefined();

        done();
      });

      service.setIfaceEvent();
    });
  });

  describe('when getIfaceEvent method is executed', () => {
    it('should detect changes', (done: DoneFn) => {
      const {
        point: { componentInstance: service },
      } = MockRender(LocalService);

      service.getIfaceEvent().subscribe((event) => {
        expect(event).toBeUndefined();

        done();
      });

      service.ifaceEvent.next();
    });
  });

  describe('when setL3CatEvent method is executed', () => {
    it('should emit L3cat', (done: DoneFn) => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const mockedL3cat: CacheAllocation = {
        cache_size: 20,
        cdp_enabled: true,
        cdp_supported: false,
        clos_num: 23,
        cw_num: 10,
        cw_size: 30
      }

      service.l3cat.pipe(skip(1)).subscribe((event) => {
        expect(event).toBe(mockedL3cat);

        done();
      })

      service.setL3CatEvent(mockedL3cat);
    })
  })

  describe('when getL3CatEvent method is excuted', () => {
    it('should detect change', (done: DoneFn) => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const mockedL3cat: CacheAllocation = {
        cache_size: 20,
        cdp_enabled: true,
        cdp_supported: false,
        clos_num: 23,
        cw_num: 10,
        cw_size: 30
      }

      service.getL3CatEvent().pipe(skip(1)).subscribe((event) => {
        expect(event).toBe(mockedL3cat);

        done();
      });

      service.l3cat.next(mockedL3cat);
    })
  });

  describe('when setL2CatEvent method is excuted', () => {
    it('it should emit', (done: DoneFn) => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const mockedL2cat: CacheAllocation = {
        cache_size: 20,
        cdp_enabled: true,
        cdp_supported: false,
        clos_num: 23,
        cw_num: 10,
        cw_size: 30
      }

      service.l2cat.pipe(skip(1)).subscribe((event) => {
        expect(event).toBe(mockedL2cat);

        done();
      })

      service.setL2CatEvent(mockedL2cat);
    })
  })

  describe('when getL2CatEvent method is excuted', () => {
    it('should detect change', (done: DoneFn) => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const mockedL2cat: CacheAllocation = {
        cache_size: 20,
        cdp_enabled: true,
        cdp_supported: false,
        clos_num: 23,
        cw_num: 10,
        cw_size: 30
      }

      service.getL2CatEvent().pipe(skip(1)).subscribe((event) => {
        expect(event).toBe(mockedL2cat);

        done();
      })

      service.l2cat.next(mockedL2cat);
    })
  })

  describe('when setCapsEvent method is excuted', () => {
    it('it should emit', (done: DoneFn) => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const mockedCaps: string[] = [
        'L3Cat',
        'L2Cat',
        'mba',
      ]

      service.caps.pipe(skip(1)).subscribe((event) => {
        expect(event).toBe(mockedCaps);

        done();
      })

      service.setCapsEvent(mockedCaps);
    })
  })

  describe('when getCapsEvent method is excuted', () => {
    it('should detect change', (done: DoneFn) => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const mockedCaps: string[] = [
        'L3Cat',
        'L2Cat',
        'mba',
      ]

      service.getCapsEvent().pipe(skip(1)).subscribe((event) => {
        expect(event).toBe(mockedCaps);

        done();
      })

      service.caps.next(mockedCaps);
    })
  })

  describe('when setMbaCtrlEvent method is excuted', () => {
    it('it should emit', (done: DoneFn) => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      service.mbaCtrl.pipe(skip(1)).subscribe((event) => {
        expect(event).toBeTrue;

        done();
      })

      service.setMbaCtrlEvent(true);
    })
  })

  describe('when getMbaCtrlEvent method is excuted', () => {
    it('should detect change', (done: DoneFn) => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      service.getMbaCtrlEvent().pipe(skip(1)).subscribe((event) => {
        expect(event).toBeTrue;

        done();
      })

      service.mbaCtrl.next(true);
    })
  })

  describe('when getCoresDash method is called', () => {
    it('should return array of numbers 1 to 11 when given "1-10,11"', () => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const testString = '1-10,11';
      const numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];

      expect(
        service.getCoresDash(testString)
      ).toEqual(numbers);
    })

    it('should return array of numbers 1 to 11 when given "1,2,3,4-11"', () => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const testString = '1,2,3,4-11';
      const numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];

      expect(
        service.getCoresDash(testString).sort((a, b) => { return a - b })
      ).toEqual(numbers);
    })

    it('should return array of numbers 1 to 11 when given "11-1"', () => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const testString = '11-1'
      const numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];

      expect(
        service.getCoresDash(testString).sort((a, b) => { return a - b })
      ).toEqual(numbers);
    })
  })

  describe('when getPidsDash method is called', () => {
    it('should return array of numbers 1 to 11 when given "1-10,11"', () => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const testString = '1-10,11';
      const numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];

      expect(
        service.getPidsDash(testString).sort((a, b) => { return a - b })
      ).toEqual(numbers);
    })

    it('should return array of numbers 1 to 11 when given "1,2,3,4-11"', () => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const testString = '1,2,3,4-11';
      const numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11];

      expect(
        service.getPidsDash(testString).sort((a, b) => { return a - b })
      ).toEqual(numbers);
    })

    it('should return an array of numbers 1-11 when given "11-1"', () => {
      const {
        point: { componentInstance: service }
      } = MockRender(LocalService);

      const testString = '11-1';
      const numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]

      expect(
        service.getPidsDash(testString).sort((a, b) => { return a - b })
      ).toEqual(numbers);
    })
  })
})