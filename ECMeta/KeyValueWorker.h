//
//  KeyValueWorker.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__KeyValueWorker__
#define __ECMeta__KeyValueWorker__

#include "ECJob.h"
#include "ecCommon.h"
#include "KeyValueSlab.h"

#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

typedef enum{
    WORKER_PIPE_WRITEABLE = 0x01,
    WORKER_PIPE_UNABLETOWRITE
}WORKER_PIPE_WRITE_STATE_t;

typedef struct {
    int workerPipes[2];
    
    size_t workerIdx;
//    int workerLogFd;
    int pipeIdx;
    size_t hashTableSize;

    WORKER_PIPE_WRITE_STATE_t curPipeState;
    
    size_t hashStartIdx;
    size_t hashEndIdx;
    
    pthread_mutex_t threadLock;
    
    void *metaServerPtr;
    
    KeyValueJobQueue_t *pendingJobQueue;
    
    KeyValueJobQueue_t *handlingJobQueue;

    threadKeyValueSlab *thKeyValueSlab;

    sem_t waitJobSem;
}KeyValueWorker_t;

KeyValueWorker_t *createKeyValueWorkers(void *metaServerPtr);

int createWorkerLogFd(int workerIdx);
void *startWorker(void *arg);
void *workerMonitor(void *arg);
void monitoringWorkerPipeWritable(KeyValueWorker_t *workerPtr);
#endif /* defined(__ECMeta__KeyValueWorker__) */
