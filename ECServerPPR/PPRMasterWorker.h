//
//  PPRMasterWorker.h
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/7.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ESetStorePPR__PPRMasterWorker__
#define __ESetStorePPR__PPRMasterWorker__

#include <stdio.h>
#include <semaphore.h>

#include "MessageBuffer.h"
#include "PPRSlaveWorker.h"

typedef struct {
    int index; // Used to mark its index in the blockInfoPtrs
    int idx; // Used to mark its idx in relevant blockgroup
    uint64_t blockId;
    char IPAddr[255];
    int IPSize;
    int port;
    int selfFlag;
    
    int blockFd;
    
    int totalSize;
    int totalPendingHandleSize;
    int totalHandledSize;

    int pendingHandleSize; //For mark request size
    int handledSize; //For mark responded size

}blockInfo_t;

typedef struct {
    pthread_t pid;
    pthread_t diskReadPid;
    pthread_t netReadPid;
    pthread_t codingPid;
    pthread_t diskWritePid;
    
    int eSetIdx;
    int stripeK;
    int stripeM;
    int stripeW;
    int failedNodesNum;
    
    int *matrix;
    int *decodingVector;
    
    int sourceNodesSize;
    int targetNodesSize;
    
    blockInfo_t *blockInfoPtrs;
    int *targetNodesIdx;

    int *dm_idx;
    int *erasures;
    
    int curBlockDone;
    uint64_t blockGroupIdx;
    
    int recoveryOverFlag;
    
    void *enginePtr;
    
    int recoveryBufNum;
    int recoveryBufSize;
    int recoveryBufIdx;
    int *recoveryBufsInUseFlag;
    
    int *localMat;
    PPRWorkerBuf_t *recoveryBufs;
    
    int firstSlaveFd;
    
    sem_t waitingJobSem;
    sem_t diskReadSem;
    sem_t diskReadDoneSem;
    sem_t netReadSem;
    sem_t netReadDoneSem;
    sem_t codingDoneSem;
    sem_t *writingFinishedSems;

}PPRMasterWorker_t;

PPRMasterWorker_t *createPPRMasterWorker(char *initStr, void *enginePtr);

int initPPRTask(PPRMasterWorker_t *pprMasterWorkerPtr,  char *buf);

void initRecoveryTask(PPRMasterWorker_t *pprMasterWorkerPtr, char *buf);
void deallocPPRMasterWorker(PPRMasterWorker_t *pprMasterWorkerPtr);
#endif /* defined(__ESetStorePPR__PPRMasterWorker__) */
