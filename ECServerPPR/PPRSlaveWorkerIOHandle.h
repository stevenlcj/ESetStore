//
//  PPRSlaveWorkerIOHandle.h
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/12.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ESetStorePPR__PPRSlaveWorkerIOHandle__
#define __ESetStorePPR__PPRSlaveWorkerIOHandle__

#include <stdio.h>


void *slaveDiskReadWorker(void *arg);

void *slaveNetWriteWorker(void *arg);

void *slaveNetReadWorker(void *arg);

void *slaveCodingWorker(void *arg);

#endif /* defined(__ESetStorePPR__PPRSlaveWorkerIOHandle__) */
