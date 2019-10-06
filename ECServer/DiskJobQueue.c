//
//  DiskJobQueue.c
//  ECCPUServer
//
//  Created by Liu Chengjian on 18/5/2.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "DiskJobQueue.h"
#include "ecCommon.h"

void initDiskIOJob(DiskJob_t *diskJobPtr){
    diskJobPtr->bufReqSize = 0;
    diskJobPtr->bufHandledSize = 0;
    diskJobPtr->sourcePtr = NULL;
    diskJobPtr->next = NULL;
    diskJobPtr->pre = NULL;
    diskJobPtr->jobType = DISK_IO_JOB_NONE;
}

int allocDiskIOJob(DiskJobManager_t *diskJobMgr, int allocNum){
    int idx;
    for (idx = 0; idx < allocNum; ++idx) {
        DiskJob_t *diskJobPtr = (DiskJob_t *)diskJobMgr->curMemPtr;
        diskJobMgr->curMemPtr = diskJobMgr->curMemPtr + diskJobMgr->diskJobMemSize;
        ++diskJobMgr->diskJobAllocNum;
        diskJobPtr->buf = (char *)diskJobPtr + sizeof(DiskJob_t);
        diskJobPtr->bufSize = diskJobMgr->bufSize;
        
        initDiskIOJob(diskJobPtr);
        enqueueDiskJob(diskJobMgr->idleDiskJobQueue, diskJobPtr);
    }
    
    return diskJobMgr->idleDiskJobQueue->jobNum;
}

DiskJobManager_t *createDiskJobMgr(int jobStep, size_t bufSize){
    DiskJobManager_t *diskJobMgr = talloc(DiskJobManager_t, 1);
    
    if (diskJobMgr == NULL) {
        printf("Alloc diskJobMgr failed\n");
        return diskJobMgr;
    }
    
    diskJobMgr->diskJobMemSize = sizeof(DiskJob_t) + bufSize;
    diskJobMgr->memSize = diskJobMgr->diskJobMemSize * jobStep;
    diskJobMgr->memUsedSize = 0;
    diskJobMgr->baseMem = talloc(char, diskJobMgr->memSize);
    diskJobMgr->curMemPtr = diskJobMgr->baseMem;
    diskJobMgr->diskJobAllocNum = 0;
    diskJobMgr->diskJobAllocStep = jobStep;
    diskJobMgr->bufSize = bufSize;
    diskJobMgr->idleDiskJobQueue = createDiskJobQueue();
    
    allocDiskIOJob(diskJobMgr, jobStep);
    return diskJobMgr;
}

DiskJob_t *getIdleDiskJob(DiskJobManager_t *diskJobMgr){
    DiskJob_t *diskJobPtr = dequeueDiskJob(diskJobMgr->idleDiskJobQueue);
    
    if (diskJobPtr == NULL) {
        diskJobMgr->memSize = diskJobMgr->memSize + diskJobMgr->diskJobMemSize * diskJobMgr->diskJobAllocStep;
        diskJobMgr->baseMem = realloc(diskJobMgr->baseMem, sizeof(char) * diskJobMgr->memSize);
        diskJobMgr->curMemPtr = diskJobMgr->baseMem + diskJobMgr->diskJobAllocNum * diskJobMgr->diskJobMemSize;
        allocDiskIOJob(diskJobMgr, diskJobMgr->diskJobAllocStep);
        
        diskJobPtr = dequeueDiskJob(diskJobMgr->idleDiskJobQueue);
    }
    
    return diskJobPtr;
}

void putIdleJob(DiskJobManager_t *diskJobMgr, DiskJob_t *diskJobPtr){
    initDiskIOJob(diskJobPtr);
    enqueueDiskJob(diskJobMgr->idleDiskJobQueue, diskJobPtr);
}

DiskJobQueue_t *createDiskJobQueue(){
    DiskJobQueue_t *diskJobQueuePtr = talloc(DiskJobQueue_t, 1);
    
    if (diskJobQueuePtr == NULL) {
        printf("Alloc diskJobQueue failed\n");
        return diskJobQueuePtr;
    }
    
    diskJobQueuePtr->jobNum = 0;
    diskJobQueuePtr->diskJobHeader = NULL;
    diskJobQueuePtr->diskJobTail = NULL;
    
    return diskJobQueuePtr;
}

int enqueueDiskJob(DiskJobQueue_t *diskJobQueuePtr, DiskJob_t *diskJobPtr){
    //printf("JobNum:%d Disk job type:%d, req size:%lu\n",diskJobQueuePtr->jobNum, diskJobPtr->jobType, diskJobPtr->bufReqSize);
    if (diskJobQueuePtr->jobNum == 0) {
        diskJobPtr->pre = NULL;
        diskJobPtr->next = NULL;
        diskJobQueuePtr->diskJobHeader = diskJobPtr;
        diskJobQueuePtr->diskJobTail = diskJobPtr;
        ++diskJobQueuePtr->jobNum;
        return diskJobQueuePtr->jobNum;
    }
    
    diskJobQueuePtr->diskJobTail->next = diskJobPtr;
    diskJobPtr->pre = diskJobQueuePtr->diskJobTail;
    diskJobQueuePtr->diskJobTail = diskJobPtr;
    
    ++diskJobQueuePtr->jobNum;
    return diskJobQueuePtr->jobNum;
}

DiskJob_t *dequeueDiskJob(DiskJobQueue_t *diskJobQueuePtr){
    
    if (diskJobQueuePtr->jobNum == 0) {
        return NULL;
    }
    
    DiskJob_t *diskJobPtr = diskJobQueuePtr->diskJobHeader;

    if (diskJobQueuePtr->jobNum == 1) {

        diskJobQueuePtr->diskJobTail = NULL;
        diskJobQueuePtr->diskJobHeader = NULL;
        --diskJobQueuePtr->jobNum;
        return diskJobPtr;
    }
    
    diskJobQueuePtr->diskJobHeader = diskJobQueuePtr->diskJobHeader->next;

    --diskJobQueuePtr->jobNum;
    
    return diskJobPtr;
}

















