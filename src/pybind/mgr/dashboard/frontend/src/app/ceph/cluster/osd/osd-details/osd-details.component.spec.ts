import { HttpClientTestingModule } from '@angular/common/http/testing';
import { ComponentFixture, TestBed } from '@angular/core/testing';

import { NgbNavModule } from '@ng-bootstrap/ng-bootstrap';

import { configureTestBed } from '../../../../../testing/unit-test-helper';
import { SharedModule } from '../../../../shared/shared.module';
import { TablePerformanceCounterComponent } from '../../../performance-counter/table-performance-counter/table-performance-counter.component';
import { CephSharedModule } from '../../../shared/ceph-shared.module';
import { OsdDetailsComponent } from './osd-details.component';

describe('OsdDetailsComponent', () => {
  let component: OsdDetailsComponent;
  let fixture: ComponentFixture<OsdDetailsComponent>;

  configureTestBed({
    imports: [HttpClientTestingModule, NgbNavModule, SharedModule, CephSharedModule],
    declarations: [OsdDetailsComponent, TablePerformanceCounterComponent]
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(OsdDetailsComponent);
    component = fixture.componentInstance;
    component.selection = undefined;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
