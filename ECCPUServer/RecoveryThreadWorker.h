//
//  RecoveryThreadWorker.h
//  ECStoreCPUServer
//
//  Created by Liu Chengjian on 18/1/9.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ECStoreCPUServer__RecoveryThreadWorker__
#define __ECStoreCPUServer__RecoveryThreadWorker__

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include <semaphore.h>

#include "DiskIOWorker.h"

#include "MessageBuffer.h"

typedef enum SERVER_SOCK_EVENT{
    SOCK_EVENT_UNREGISTERED = 0,
    SOCK_EVENT_READ = 1,
    SOCK_EVENT_WRITE = 2,
    SOCK_EVENT_READ_WRITE = 3,
    SOCK_EVENT_READ_DELETE = 4
}SOCK_EVENT_STATE_t;


typedef enum {
    BLOCK_NET_STATE_UINIT = 0,
    BLOCK_NET_STATE_CONNECTED, //Transit from BLOCK_NET_STATE_CONNECTING or BLOCK_NET_STATE_START_READOVER
    BLOCK_NET_STATE_START_REQUEST_READ, // Sending cmd blockRead and waiting for receving OK or FAILED
    BLOCK_NET_STATE_START_READ, // Recvd OK for request read
    BLOCK_NET_STATE_READING, // Sending or sended cmd ReadSize:
    BLOCK_NET_STATE_READOVER // Sending Cmd ReadOver
    
}BLOCK_NET_STATE_t;

typedef struct {
    int index; // Used to mark its index in the blockInfoPtrs
    int idx; // Used to mark its idx in relevant blockgroup
    uint64_t blockId;
    char IPAddr[255];
    int IPSize;
    int port;
    int selfFlag;
    
    int bufIdx; // Cur buffer idx for performing relevant task
    
    int fd;
    
    int totalSize;
    int totalPendingHandleSize;
    int totalHandledSize;
    
    int pendingHandleSize; //For mark request size
    int handledSize; //For mark responded size
    
    BLOCK_NET_STATE_t curNetState;
    SOCK_EVENT_STATE_t curSockState;
    
    ECMessageBuffer_t *readMsgBuf;
    ECMessageBuffer_t *writeMsgBuf;

    int blockFd;
}blockInfo_t;

typedef struct recoveryBuf{
    int bufIdx;
    int inputBufNum;
    int outputBufNum;
    int eachBufSize;
    int eachBufWriteSize;
    
    int taskSize;
    
    int *bufWritedSize;
    int *bufToWriteSize;
    char *buf;
    char **inputBufsPtrs;
    char **outputBufsPtrs;
}recoveryBuf_t;

typedef struct RecoveryThreadManager{
    pthread_t pid;
    
    pthread_t diskReadPid;
    pthread_t ioReadPid;
    pthread_t codingPid;
    pthread_t ioWritePid;
    
    int eSetIdx;
    int stripeK;
    int stripeM;
    int stripeW;
    int failedNodesNum;
    int *matrix;
    int *decodingMatrix;
    
    int sourceNodesSize;
    int targetNodesSize;
    
    blockInfo_t *blockInfoPtrs;
    int *targetNodesIdx;
    
    int *dm_idx;
    int *erasures;
    
    int curBlockDone;
    int recoveryOverFlag;
    
    int recoveryBufNum;
    int recoveryBufSize;
    int recoveryBufIdx;
    int *recoveryBufsInUseFlag;
    recoveryBuf_t *recoveryBufs;
    
    void *enginePtr;
    
    sem_t codingSem;
    sem_t ioReadSem;
    sem_t diskIOReadSem;
    sem_t ioWriteSem;
    
    sem_t waitingJobSem;
    sem_t *writingFinishedSems;
    
    int mgrPipes[2];
    
    int recoveryEfd;
    
    uint64_t blockGroupIdx;
    
    int localDiskIOReadNums;
    int netIOReadNums;
    
//    ECBlockReader_t *blockReaders;
//    int blockReadersNum;
    
}RecoveryThreadManager_t;

RecoveryThreadManager_t *createRecoveryThreadMgr(char *initStr, void *enginePtr);
void deallocRecoveryThreadMgr(RecoveryThreadManager_t *recoveryThMgr);

int initRecoveryTask(RecoveryThreadManager_t *recoveryThMgr,  char *buf);

int initRecoveryBuf(recoveryBuf_t *recoveryBufPtr, int bufIdx, int inputBufNum, int outputBufNum, int eachBufSize);
void deallocRecoveryBuf(recoveryBuf_t *recoveryBufPtr);

#endif /* defined(__ECStoreCPUServer__RecoveryThreadWorker__) */
