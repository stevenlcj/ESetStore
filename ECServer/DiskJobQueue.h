//
//  DiskJobQueue.h
//  ECCPUServer
//
//  Created by Liu Chengjian on 18/5/2.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ECCPUServer__DiskJobQueue__
#define __ECCPUServer__DiskJobQueue__

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

typedef enum {
    DISK_IO_JOB_NONE = 0x01,
    DISK_IO_JOB_READ,
    DISK_IO_JOB_WRITE,
    DISK_IO_JOB_READDONE,
    DISK_IO_JOB_WRITEDONE
}DISK_IO_JOB_t;

typedef struct DiskJob{
    void *sourcePtr;
    int diskFd;
    
    DISK_IO_JOB_t jobType;
    
    char *buf;
    size_t bufSize;
    size_t bufReqSize;
    size_t bufHandledSize;
    
    struct DiskJob *next;
    struct DiskJob *pre;
    
    struct timeval startTime;
    struct timeval startRead;
    struct timeval endRead;
    struct timeval startClient;
    struct timeval endTime;
}DiskJob_t;

typedef struct DiskJobQueue{
    int jobNum;
    DiskJob_t *diskJobHeader;
    DiskJob_t *diskJobTail;
}DiskJobQueue_t;

typedef struct DiskJobManager{
    char *baseMem;
    char *curMemPtr;
    size_t memSize;
    size_t memUsedSize;
    int diskJobAllocNum;
    int diskJobAllocStep;
    size_t diskJobMemSize;
    size_t bufSize;
    
    DiskJobQueue_t *idleDiskJobQueue;
}DiskJobManager_t;

DiskJob_t *getIdleDiskJob(DiskJobManager_t *diskJobMgr);
void putIdleJob(DiskJobManager_t *diskJobMgr, DiskJob_t *diskJobPtr);

DiskJobManager_t *createDiskJobMgr(int jobStep, size_t bufSize);
DiskJobQueue_t *createDiskJobQueue();

int enqueueDiskJob(DiskJobQueue_t *diskJobQueuePtr, DiskJob_t *diskJobPtr);
DiskJob_t *dequeueDiskJob(DiskJobQueue_t *diskJobQueuePtr);
#endif /* defined(__ECCPUServer__DiskJobQueue__) */
