//
//  DiskJobWorker.h
//  ECCPUServer
//
//  Created by Liu Chengjian on 18/5/2.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ECCPUServer__DiskJobWorker__
#define __ECCPUServer__DiskJobWorker__

#include <stdio.h>
#include <pthread.h>

#include "DiskJobQueue.h"

typedef struct DiskJobWorker{
    
    int efd;
    
    int exitFlag;
    
    int workerPipes[2];
    int count;
    void *diskIOMgrPtr;
    
    pthread_t pid;
    
    pthread_mutex_t workerLock;
    
    DiskJobManager_t *diskJobMgr;
    
    DiskJobQueue_t *handlingJobQueue;
    DiskJobQueue_t *comingJobQueue;
}DiskJobWorker_t;

DiskJobWorker_t *createDiskJobWorker(void *diskIOMgrPtr);
DiskJob_t *requestDiskJob(DiskJobWorker_t *diskJobWorker);
void revokeDiskJob(DiskJobWorker_t *diskJobWorker, DiskJob_t *diskJobPtr);

void putDiskIOJob(DiskJobWorker_t *diskJobWorker, DiskJob_t *diskJobPtr);
void *startDiskJobWorker(void *diskJobWorker);
#endif /* defined(__ECCPUServer__DiskJobWorker__) */
