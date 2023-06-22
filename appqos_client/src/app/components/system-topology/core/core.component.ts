import { Component, Input } from '@angular/core';
import { CoreInfo } from '../../system-caps/system-caps.model';

interface CoreColor {
  bgColor: string;
  fontColor: string;
}

@Component({
  selector: 'app-core',
  templateUrl: './core.component.html',
  styleUrls: ['./core.component.scss'],
})
export class CoreComponent {
  @Input() L2ID!: number;
  @Input() cores!: CoreInfo[];
  @Input() detailedView!: boolean;
  // @todo extend colors to allow setting color for each pool
  colors: CoreColor[] = [{ bgColor: '#50e1ff', fontColor: 'black' }];
}
