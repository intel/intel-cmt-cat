import { RouterModule } from '@angular/router';
import { MockBuilder, MockRender, NG_MOCKS_GUARDS } from 'ng-mocks';
import { RouterTestingModule } from '@angular/router/testing';

import { PermissionsGuard } from './permissions.guard';
import { SharedModule } from '../shared/shared.module';
import { LoginComponent } from '../components/login/login.component';

describe('TestRoutingGuard', () => {
  beforeEach(() =>
    MockBuilder(PermissionsGuard)
      .mock(RouterModule)
      .mock(SharedModule)
      .keep(RouterTestingModule.withRoutes([{ path: 'login', component: LoginComponent }]))
      .exclude(NG_MOCKS_GUARDS)
  );

  describe('when canActivate method is executed for a OTHER THAN Login and the user IS NOT logged in', () => {
    it('should return false to disallow activation', () => {
      const { point: { componentInstance: guard } } = MockRender(PermissionsGuard);

      localStorage.clear();

      expect(guard.canActivate()).toBeFalse();
    });
  });

  describe('when canActivate method is executed for a OTHER THAN Login and the user is logged in', () => {
    it('should return true to allow activation', () => {
      const { point: { componentInstance: guard } } = MockRender(PermissionsGuard);

      localStorage.setItem('hostName', 'localhost');

      expect(guard.canActivate()).toBeTrue();
    });
  });
});
