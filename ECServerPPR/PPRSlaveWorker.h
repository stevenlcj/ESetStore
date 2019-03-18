//
//  PPRSlaveWorker.h
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/7.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ESetStorePPR__PPRSlaveWorker__
#define __ESetStorePPR__PPRSlaveWorker__

#include <stdio.h>
#include <pthread.h>

#include <semaphore.h>
#include <stdint.h>

#include "MessageBuffer.h"

typedef enum {
    PPRWORKER_SLAVE_STATE_UNINIT = 0x01,
    PPRWORKER_SLAVE_STATE_WAITING,
    PPRWORKER_SLAVE_STATE_HANDLING_BLOCK,
    PPRWORKER_SLAVE_STATE_DONE
}PPRWORKER_SLAVE_STATE_t;

typedef struct {
    char *localIOBuf;
    char *netIOBuf;
    char *resultBuf;
    size_t bufSize;
    int taskSize;
    size_t localBufToWriteSize;
    size_t netBufToFillSize;
    size_t resutBufToWriteSize;
    size_t localBufFilledSize;
    size_t netBufFilledSize;
    size_t resultFilledSize;
}PPRWorkerBuf_t;

typedef struct {
    pthread_t pid;
    
    pthread_t diskReadPid;
    pthread_t netReadPid;
    pthread_t codingPid;
    pthread_t netWritePid;
    
    int outSockFd;
    int inSockFd;
    int bufNum;
    size_t bufSize;
    
    void *serverEnginePtr;
        
    PPRWORKER_SLAVE_STATE_t pprSlaveState;
    PPRWorkerBuf_t *pprWorkerBufs;
    int *pprWorkerBufsInUseFlag;
    
    int recoveryOverFlag;
    int curBlockDone;
    
    int sizeValue;
    int idx;
    
    int *localMat;
    
    int idxinBlock; // Used to mark its idx in relevant blockgroup
    uint64_t blockId;
    uint64_t blockGroupIdx;
    
    size_t blockSize;
    size_t pendingBlockSize;
    
    sem_t diskReadSem;
    sem_t diskReadDoneSem;
    sem_t netReadSem;
    sem_t netReadDoneSem;
    sem_t codingDoneSem;
    sem_t *writingFinishedSems;

    ECMessageBuffer_t *readMsgBuf;
}PPRSlaveWorker_t;

PPRSlaveWorker_t *createPPRSlaveWorker(int outSockFd, int bufNum, size_t bufSize, void *serverEnginePtr);
void *startPPRSlaveWorker(void *arg);
void deallocPPRSlaveWorker(PPRSlaveWorker_t *pprSlaveWorker);

#endif /* defined(__ESetStorePPR__PPRSlaveWorker__) */
