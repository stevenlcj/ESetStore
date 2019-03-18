//
//  PPRMasterWorkerIOHandle.h
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/10.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ESetStorePPR__PPRMasterWorkerDiskHandle__
#define __ESetStorePPR__PPRMasterWorkerDiskHandle__

#include <stdio.h>

void *diskReadWorker(void *arg);

void *diskWriteWorker(void *arg);

void *netReadWorker(void *arg);

void *codingWorker(void *arg);

#endif /* defined(__ESetStorePPR__PPRMasterWorkerDiskHandle__) */
