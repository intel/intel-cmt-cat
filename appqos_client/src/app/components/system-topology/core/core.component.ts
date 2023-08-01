import { Component, Input, OnInit } from '@angular/core';
import { LocalService } from 'src/app/services/local.service';
import { CoreInfo, Node } from '../../system-caps/system-caps.model';

@Component({
  selector: 'app-core',
  templateUrl: './core.component.html',
  styleUrls: ['./core.component.scss'],
})
export class CoreComponent implements OnInit {
  @Input() L2ID!: number;
  @Input() node!: Node;
  @Input() detailedView!: boolean;
  cores: CoreInfo[] = [];

  constructor(public local: LocalService) { }

  ngOnInit(): void {
    this.cores = this.getL2IDCores(this.L2ID);
  }

  /*
   * get list of cores with common L2ID
   */
  getL2IDCores(id: number): CoreInfo[] {
    return this.node.cores!.filter((core) => core.L2ID === id);
  }
}
