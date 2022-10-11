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

import { NgModule } from '@angular/core';
import { MatInputModule } from '@angular/material/input';
import { MatButtonModule } from '@angular/material/button';
import { ReactiveFormsModule } from '@angular/forms';
import { CommonModule } from '@angular/common';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { HttpClientModule } from '@angular/common/http';
import { MatCardModule } from '@angular/material/card';
import { MatGridListModule } from '@angular/material/grid-list';
import { MatListModule } from '@angular/material/list';
import { MatDividerModule } from '@angular/material/divider';
import { MatSnackBarModule } from '@angular/material/snack-bar';
import { MatToolbarModule } from '@angular/material/toolbar';
import { MatSlideToggleModule } from '@angular/material/slide-toggle';
import { MatButtonToggleModule } from '@angular/material/button-toggle';
import { MatIconModule } from '@angular/material/icon';
import { MatDialogModule } from '@angular/material/dialog';
import { MatSliderModule } from '@angular/material/slider';
import { MatTooltipModule } from '@angular/material/tooltip';

import { LoginComponent } from '../components/login/login.component';
import { AppqosService } from '../services/appqos.service';
import { LocalService } from '../services/local.service';
import { PermissionsGuard } from '../services/permissions.guard';
import { DashboardPageComponent } from '../components/dashboard-page/dashboard-page.component';
import { SystemCapsComponent } from '../components/system-caps/system-caps.component';
import { ToolbarComponent } from '../components/toolbar/toolbar.component';
import { OverviewComponent } from '../components/overview/overview.component';
import { RapConfigComponent } from '../components/rap-config/rap-config.component';
import { L3catComponent } from '../components/system-caps/l3cat/l3cat.component';
import { RdtIfaceComponent } from '../components/system-caps/rdt-iface/rdt-iface.component';
import { MbaComponent } from '../components/system-caps/mba/mba.component';
import { SstbfComponent } from '../components/system-caps/sstbf/sstbf.component';
import { L2catComponent } from '../components/system-caps/l2cat/l2cat.component';
import { SstcpComponent } from '../components/system-caps/sstcp/sstcp.component';
import { L3CacheAllocationComponent } from '../components/overview/l3-cache-allocation/l3-cache-allocation.component';
import { EditDialogComponent } from '../components/overview/edit-dialog/edit-dialog.component';
import { MbaAllocationComponent } from '../components/overview/mba-allocation/mba-allocation.component';

@NgModule({
  declarations: [
    LoginComponent,
    DashboardPageComponent,
    SystemCapsComponent,
    ToolbarComponent,
    OverviewComponent,
    RapConfigComponent,
    L3catComponent,
    RdtIfaceComponent,
    MbaComponent,
    SstbfComponent,
    SstcpComponent,
    L2catComponent,
    L3CacheAllocationComponent,
    EditDialogComponent,
    MbaAllocationComponent,
  ],
  imports: [
    MatInputModule,
    MatButtonModule,
    ReactiveFormsModule,
    CommonModule,
    HttpClientModule,
    MatProgressSpinnerModule,
    MatCardModule,
    MatGridListModule,
    MatListModule,
    MatDividerModule,
    MatSnackBarModule,
    MatToolbarModule,
    MatIconModule,
    MatSlideToggleModule,
    MatButtonToggleModule,
    MatDialogModule,
    MatSliderModule,
    MatTooltipModule,
  ],
  exports: [LoginComponent, HttpClientModule, DashboardPageComponent],
  providers: [AppqosService, LocalService, PermissionsGuard],
})
export class SharedModule {}
