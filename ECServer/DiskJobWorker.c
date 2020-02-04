//
//  DiskJobWorker.c
//  ECCPUServer
//
//  Created by Liu Chengjian on 18/5/2.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "DiskJobWorker.h"
#include "ecCommon.h"
#include "DiskIOHandler.h"

#include "ecNetHandle.h"

#include "ecUtils.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define WORKER_EPOLL_TIMEOUT_IN_SEC 100000

DiskJobWorker_t *createDiskJobWorker(void *diskIOMgrPtr){
    DiskJobWorker_t *diskJobWorker = talloc(DiskJobWorker_t, 1);
    
    if (diskJobWorker == NULL) {
        printf("alloc diskJobWorker failed\n");
        return diskJobWorker;
    }
    
    diskJobWorker->diskIOMgrPtr = diskIOMgrPtr;
    diskJobWorker->efd = create_epollfd();
    diskJobWorker->exitFlag = 0;
    if (pipe(diskJobWorker->workerPipes) != 0){
        fprintf (stderr, "Create serverRecoveryPipes failed.\n");
    }
    
    make_non_blocking_sock(diskJobWorker->workerPipes[0]);
    
    diskJobWorker->diskJobMgr = createDiskJobMgr(DEFAULT_DISK_IO_JOB_STEP,DEFAULT_DISK_IO_JOB_BUF_SIZE);
    diskJobWorker->count = 0;
    
    diskJobWorker->handlingJobQueue = createDiskJobQueue();
    diskJobWorker->comingJobQueue = createDiskJobQueue();
    
    pthread_mutex_init(&diskJobWorker->workerLock, NULL);
    
    return diskJobWorker;
}

DiskJob_t *requestDiskJob(DiskJobWorker_t *diskJobWorker){
    DiskJob_t *diskJobPtr = NULL;
    pthread_mutex_lock(&diskJobWorker->workerLock);
    diskJobPtr = getIdleDiskJob(diskJobWorker->diskJobMgr);
    pthread_mutex_unlock(&diskJobWorker->workerLock);
    
    gettimeofday(&diskJobPtr->startTime, NULL);

    return diskJobPtr;
}

void revokeDiskJob(DiskJobWorker_t *diskJobWorker, DiskJob_t *diskJobPtr){
    gettimeofday(&diskJobPtr->endTime, NULL);
    size_t reqSize = diskJobPtr->bufReqSize;
    double timeInterval = timeIntervalInMS(diskJobPtr->startTime, diskJobPtr->endTime);
    
    double startRInter = timeIntervalInMS(diskJobPtr->startTime, diskJobPtr->startRead);
    double rInter = timeIntervalInMS(diskJobPtr->startRead, diskJobPtr->endRead);
    double cInter = timeIntervalInMS(diskJobPtr->endRead, diskJobPtr->startClient);
    double wToCInter = timeIntervalInMS(diskJobPtr->startClient, diskJobPtr->endTime);
    pthread_mutex_lock(&diskJobWorker->workerLock);
    
    putIdleJob(diskJobWorker->diskJobMgr, diskJobPtr);
    pthread_mutex_unlock(&diskJobWorker->workerLock);
    
    double throughput = ((double)reqSize/1024.0/1024.0)/(timeInterval/1000.0);
    
    if (throughput < 100.0) {
        ++diskJobWorker->count;
        //printf("count:%d, wToCInter:%fms ,Req size:%lu, timeInterval:%fms, throughput:%fMB/s,startRInter:%fms,rInter:%fms,cInter:%fms\n",diskJobWorker->count, wToCInter, reqSize, timeInterval, throughput, startRInter,rInter, cInter);
    }
    return;
}

void submitReadDoneJob(DiskJobWorker_t *diskJobWorkerPtr, DiskJob_t *diskJobPtr){
    DiskIOManager_t *diskIOPtr = (DiskIOManager_t *)diskJobWorkerPtr->diskIOMgrPtr;
    ECBlockServerEngine_t *metaServerPtr = (ECBlockServerEngine_t *)diskIOPtr->serverEngine;
    ECClientManager_t *ecClientMgr = metaServerPtr->ecClientMgr;
    
    pthread_mutex_lock(&ecClientMgr->lock);
   // printf("submit readdone job with req size:%lu\n", diskJobPtr->bufReqSize);
    enqueueDiskJob(ecClientMgr->comingJobQueue, diskJobPtr);
    
    ECClient_t *ecClientPtr = (ECClient_t *)diskJobPtr->sourcePtr;

    char ch = 'C';
    write(ecClientMgr->clientMgrPipes[1], (void *)&ch, 1);
    
    pthread_mutex_unlock(&ecClientMgr->lock);
}

void putDiskIOJob(DiskJobWorker_t *diskJobWorker, DiskJob_t *diskJobPtr){
    pthread_mutex_lock(&diskJobWorker->workerLock);
    enqueueDiskJob(diskJobWorker->comingJobQueue, diskJobPtr);
    
    char ch = 'C';
    write(diskJobWorker->workerPipes[1], (void *)&ch, 1);

    pthread_mutex_unlock(&diskJobWorker->workerLock);
}

void handleReadJob(DiskJobWorker_t *diskJobWorkerPtr, DiskJob_t *diskJobPtr){
    DiskIOManager_t *diskIOPtr = (DiskIOManager_t *)diskJobWorkerPtr->diskIOMgrPtr;
    gettimeofday(&diskJobPtr->startRead, NULL);
    do{
        ssize_t readSize = readFile(diskJobPtr->diskFd, diskJobPtr->buf + diskJobPtr->bufHandledSize, (diskJobPtr->bufReqSize - diskJobPtr->bufHandledSize), diskIOPtr);
        if (readSize > 0)
        {
            diskJobPtr->bufHandledSize = diskJobPtr->bufHandledSize + (size_t)readSize;
        }else{
            perror("Unable to read data from disk");
//            break;
        }
    }while(diskJobPtr->bufHandledSize != diskJobPtr->bufReqSize);
    
    diskJobPtr->jobType = DISK_IO_JOB_READDONE;
    
//    ECClient_t *ecClientPtr = (ECClient_t *)diskJobPtr->sourcePtr;
//    printf("Func:handleReadJob sockFd:%d, blockId:%llu, disk reqSize:%lu, handledSize:%lu diskJobPtr:%p\n",ecClientPtr->sockFd, ecClientPtr->blockId,diskJobPtr->bufReqSize, diskJobPtr->bufHandledSize,diskJobPtr);

    gettimeofday(&diskJobPtr->endRead, NULL);
    
    submitReadDoneJob(diskJobWorkerPtr, diskJobPtr);
}

void handleWriteJob(DiskJobWorker_t *diskJobWorkerPtr, DiskJob_t *diskJobPtr){
    DiskIOManager_t *diskIOPtr = (DiskIOManager_t *)diskJobWorkerPtr->diskIOMgrPtr;
    do{
        ssize_t writeSize = writeFile(diskJobPtr->diskFd, diskJobPtr->buf + diskJobPtr->bufHandledSize, (diskJobPtr->bufReqSize - diskJobPtr->bufHandledSize), diskIOPtr);
        if (writeSize > 0)
        {
            diskJobPtr->bufHandledSize = diskJobPtr->bufHandledSize + (size_t)writeSize;
        }else{
            perror("Unable to write data to disk");
//            break;
        }
    }while(diskJobPtr->bufHandledSize != diskJobPtr->bufReqSize);

}

void handleDiskIOJob(DiskJobWorker_t *diskJobWorkerPtr, DiskJob_t *diskJobPtr){
    //printf("handle diskIOJOB with type:%d\n", diskJobPtr->jobType);
    switch (diskJobPtr->jobType) {
        case DISK_IO_JOB_READ:
            handleReadJob(diskJobWorkerPtr, diskJobPtr);
            break;
        case DISK_IO_JOB_WRITE:
            handleWriteJob(diskJobWorkerPtr, diskJobPtr);
            break;
        default:
            break;
    }
}

void executeDiskIOJobs(DiskJobWorker_t *diskJobWorkerPtr){
    DiskJob_t *diskJobPtr = NULL;
    
    while ((diskJobPtr = dequeueDiskJob(diskJobWorkerPtr->handlingJobQueue))!= NULL){
        handleDiskIOJob(diskJobWorkerPtr, diskJobPtr);
    }
}

void performComingDiskIOJobs(DiskJobWorker_t *diskJobWorkerPtr){
    pthread_mutex_lock(&diskJobWorkerPtr->workerLock);
    DiskJob_t *diskJobPtr = NULL;
    while ((diskJobPtr = dequeueDiskJob(diskJobWorkerPtr->comingJobQueue))!= NULL) {
        enqueueDiskJob(diskJobWorkerPtr->handlingJobQueue, diskJobPtr);
    }
    pthread_mutex_unlock(&diskJobWorkerPtr->workerLock);
    
    executeDiskIOJobs(diskJobWorkerPtr);
}

void performDiskIOWork(DiskJobWorker_t *diskJobWorkerPtr){
    int idx, eventsNum;
    struct epoll_event* events;
    
    events = talloc(struct epoll_event, MAX_CONN_EVENTS);
    
    if (events == NULL) {
        perror("unable to alloc memory for events");
        return;
    }
    
    add_event(diskJobWorkerPtr->efd, EPOLLIN, diskJobWorkerPtr->workerPipes[0], NULL);

    //printf("start recv disk io job\n");
     do{
         eventsNum =epoll_wait(diskJobWorkerPtr->efd, events, MAX_EVENTS,WORKER_EPOLL_TIMEOUT_IN_SEC);
         if (eventsNum  <= 0) {
             continue;
         }
         
         for (idx = 0; idx < eventsNum; ++idx) {
             if(events[idx].events & EPOLLIN)
             {
                 //printf("performComingDiskIOJobs\n");
                 performComingDiskIOJobs(diskJobWorkerPtr);
             }else{
                 printf("unkown event for performDiskIOWork\n");
             }
         }

     }while (diskJobWorkerPtr->exitFlag == 0);
    
}

void *startDiskJobWorker(void *diskJobWorker){
    DiskJobWorker_t *diskJobWorkerPtr = (DiskJobWorker_t *)diskJobWorker;
    performDiskIOWork(diskJobWorkerPtr);
    
    return diskJobWorker;
}
















