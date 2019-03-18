//
//  DiskIOWorker.h
//  ECStoreCPUServer
//
//  Created by Liu Chengjian on 18/1/9.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ECStoreCPUServer__DiskIOWorker__
#define __ECStoreCPUServer__DiskIOWorker__

#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

void *readDiskIOWorker(void *arg);

void *writeDiskIOWorker(void *arg);

#endif /* defined(__ECStoreCPUServer__DiskIOWorker__) */
