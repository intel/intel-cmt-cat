import { Injectable } from '@angular/core';
import { CanActivate, Router } from '@angular/router';

import { LocalService } from './local.service';

@Injectable({
  providedIn: 'root',
})
export class PermissionsGuard implements CanActivate {
  constructor(private router: Router, private local: LocalService) {}

  canActivate(): boolean {
    if (this.local.isLoggedIn()) return true;
    this.router.navigate(['/login']);
    return false;
  }
}
