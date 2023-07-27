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
import { FormsModule, ReactiveFormsModule } from '@angular/forms';
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
import { MatSelectModule } from '@angular/material/select';
import { MatTableModule } from '@angular/material/table';
import { MatMenuModule } from '@angular/material/menu';
import { MatTabsModule } from '@angular/material/tabs';
import {MatExpansionModule} from '@angular/material/expansion';

import { LoginComponent } from '../components/login/login.component';
import { AppqosService } from '../services/appqos.service';
import { LocalService } from '../services/local.service';
import { PermissionsGuard } from '../services/permissions.guard';
import { DashboardPageComponent } from '../components/dashboard-page/dashboard-page.component';
import { SystemCapsComponent } from '../components/system-caps/system-caps.component';
import { ToolbarComponent } from '../components/toolbar/toolbar.component';
import { OverviewComponent } from '../components/overview/overview.component';
import { RapConfigComponent } from '../components/rap-config/rap-config.component';
import { RdtIfaceComponent } from '../components/system-caps/rdt-iface/rdt-iface.component';
import { MbaComponent } from '../components/system-caps/mba/mba.component';
import { SstbfComponent } from '../components/system-caps/sstbf/sstbf.component';
import { SstcpComponent } from '../components/system-caps/sstcp/sstcp.component';
import { L3CacheAllocationComponent } from '../components/overview/l3-cache-allocation/l3-cache-allocation.component';
import { EditDialogComponent } from '../components/overview/edit-dialog/edit-dialog.component';
import { MbaAllocationComponent } from '../components/overview/mba-allocation/mba-allocation.component';
import { L2CacheAllocationComponent } from '../components/overview/l2-cache-allocation/l2-cache-allocation.component';
import { PoolConfigComponent } from '../components/rap-config/pool-config/pool-config.component';
import { PoolAddDialogComponent } from '../components/rap-config/pool-config/pool-add-dialog/pool-add-dialog.component';
import { AppsConfigComponent } from '../components/rap-config/apps-config/apps-config.component';
import { CoresEditDialogComponent } from '../components/rap-config/pool-config/cores-edit-dialog/cores-edit-dialog.component';
import { AppsAddDialogComponent } from '../components/rap-config/apps-config/apps-add-dialog/apps-add-dialog.component';
import { AppsEditDialogComponent } from '../components/rap-config/apps-config/apps-edit-dialog/apps-edit-dialog.component';
import { CatComponent } from '../components/system-caps/cat/cat.component';
import { SystemTopologyComponent } from '../components/system-topology/system-topology.component';
import { NodeComponent } from '../components/system-topology/node/node.component';
import { CoreComponent } from '../components/system-topology/core/core.component';
import { PowerProfilesComponent } from '../components/power-profiles/power-profiles.component';
import { PowerProfileDialogComponent } from '../components/power-profiles/power-profiles-dialog/power-profiles-dialog.component';
import { DisplayConfigComponent } from '../components/display-config/display-config.component';
import { SstBfComponent } from '../components/overview/sst-bf/sst-bf.component';

@NgModule({
  declarations: [
    LoginComponent,
    DashboardPageComponent,
    SystemCapsComponent,
    ToolbarComponent,
    OverviewComponent,
    RapConfigComponent,
    RdtIfaceComponent,
    MbaComponent,
    SstbfComponent,
    SstcpComponent,
    L3CacheAllocationComponent,
    L2CacheAllocationComponent,
    EditDialogComponent,
    MbaAllocationComponent,
    PoolConfigComponent,
    CoresEditDialogComponent,
    PoolAddDialogComponent,
    AppsConfigComponent,
    AppsAddDialogComponent,
    AppsEditDialogComponent,
    CatComponent,
    SystemTopologyComponent,
    NodeComponent,
    CoreComponent,
    PowerProfilesComponent,
    PowerProfileDialogComponent,
    DisplayConfigComponent,
    SstBfComponent,
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
    MatSelectModule,
    FormsModule,
    MatTableModule,
    MatMenuModule,
    MatTabsModule,
    MatExpansionModule,
  ],
  exports: [LoginComponent, HttpClientModule, DashboardPageComponent],
  providers: [AppqosService, LocalService, PermissionsGuard],
})
export class SharedModule { }
