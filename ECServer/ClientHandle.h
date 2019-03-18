//
//  ClientHandle.h
//  ECServer
//
//  Created by Liu Chengjian on 17/10/3.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECServer__ClientHandle__
#define __ECServer__ClientHandle__

#include "MessageBuffer.h"
#include "DiskIOHandler.h"

#include "DiskJobQueue.h"

#include <stdio.h>
#include <pthread.h>

#include <sys/time.h>

typedef enum {
    ECCLIENT_IN_STATE_WAIT = 0x01,
    ECCLIENT_IN_STATE_CREATE,
    ECCLIENT_IN_STATE_OPEN,
    ECCLIENT_IN_STATE_WRITING,
    ECCLIENT_IN_STATE_READING,
    ECCLIENT_IN_STATE_CLOSE,
    ECClient_IN_STATE_UNKNOW
}ECCLIENT_IN_STATE_t;

typedef enum{
    ECCLIENT_OUT_STATE_WAIT = 0x01,
    ECCLIENT_OUT_STATE_SENDING_CMD = 0x02,
    ECCLIENT_OUT_STATE_READING = 0x03
}ECCLIENT_OUT_STATE_t;

typedef enum {
    CLIENT_CONN_STATE_UNREGISTERED = 0x01,
    CLIENT_CONN_STATE_READ,
    CLIENT_CONN_STATE_WRITE,
    CLIENT_CONN_STATE_READWRITE,
    CLIENT_CONN_STATE_PENDINGCLOSE
}CLIENT_CONN_STATE;

typedef struct ECClient{
    int sockFd;
    
    int fileFd;
    uint64_t blockId;
    size_t reqSize;
    size_t handledSize;
    
    struct ECClient *next;
    struct ECClient *pre;
    
    ECCLIENT_IN_STATE_t clientInState;
    ECCLIENT_OUT_STATE_t clientOutState;
    CLIENT_CONN_STATE connState;

    ECMessageBuffer_t *readMsgBuf;
    ECMessageBuffer_t *writeMsgBuf;
    
    struct timeval startTime;
    struct timeval endTime;
    
}ECClient_t;

typedef struct{
    int efd;
    int waitingClientsFlag;

    int clientMgrPipes[2];
    
    pthread_mutex_t lock;
    
    ECClient_t *clientPipe;
    ECClient_t *comingClients;
    ECClient_t *monitoringClients;

    size_t totalRecv;
    size_t totalDump;
    size_t totalSend;
    
    size_t monitoringClientsNum;
        
    void *serverEnginePtr;
    
    DiskJobQueue_t *handlingJobQueue;
    DiskJobQueue_t *comingJobQueue;
}ECClientManager_t;

ECClientManager_t *createClientManager(void *serverEnginePtr);
void startECClientManager(ECClientManager_t *ecClientMgr);
void addComingECClient(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr);
ECClient_t *newECClient(int sockFd);

void deallocECClient(ECClient_t *ecClientPtr);
#endif /* defined(__ECServer__ClientHandle__) */
