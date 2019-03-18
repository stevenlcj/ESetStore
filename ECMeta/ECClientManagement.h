//
//  ECClientManagement.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__ECClientManagement__
#define __ECMeta__ECClientManagement__

#include "ClientConnection.h"

#include <stdio.h>
#include <pthread.h>

#include "ECJob.h"

typedef enum{
    PIPE_WRITEABLE = 0x01,
    PIPE_UNABLETOWRITE
}CLIENT_PIPE_WRITE_STATE_t;

typedef struct ECClientThread{
    int pipes[2];
    
    CLIENT_PIPE_WRITE_STATE_t curPipeState;
    pthread_mutex_t threadLock;
    
    ClientConnection_t **clientConn;
    size_t clientConnSize;
    size_t clientConnNum;
    int *clientConnIndicator;
    void *ecMetaEnginePtr;
    size_t idx;
    int totalPipeRSize;
    
    ClientConnection_t *incomingConn;
    
    KeyValueJobQueue_t *incomingJobQueue;
    KeyValueJobQueue_t *writeJobQueue;

    int efd;
    
//    int clientLogFd;
}ECClientThread_t;

typedef struct {
    void *ecMetaEnginePtr;
    ECClientThread_t *clientThreads;
    size_t clientThreadsNum;
}ECClientThreadManager_t;

ECClientThreadManager_t *createClientThreadManager(void *ecMetaEnginePtr);

void monitoringPipeWriteble(ECClientThread_t *ecClientThread);
void initECClientThread(ECClientThread_t *ecClientThread);
void *startECClientThread(void *arg);

int createThreadLog(int idx);
void addConnection(ECClientThread_t *ecClientThread, ClientConnection_t *clientConn);
void removeConnection(ECClientThread_t *ecClientThread, ClientConnection_t *clientConn);
int dry_pipe_msg(int sockFd);
#endif /* defined(__ECMeta__ECClientManagement__) */
