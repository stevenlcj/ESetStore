//
//  BlockServerEngine.h
//  ECServer
//
//  Created by Liu Chengjian on 17/8/9.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECServer__BlockServer__
#define __ECServer__BlockServer__

#include <stdio.h>

#include "configProperties.h"
#include "ClientHandle.h"
#include "DiskIOHandler.h"

#include "MessageBuffer.h"

#include "PPRMasterWorker.h"
#include "PPREngine.h"

typedef enum META_SOCK_EVENT{
    SERVER_SOCK_STATE_UNREGISTERED = 0,
    SERVER_SOCK_STATE_READ = 1,
    SERVER_SOCK_STATE_WRITE = 2,
    SERVER_SOCK_STATE_READ_WRITE = 3,
    SERVER_SOCK_STATE_READ_DELETE = 4
}META_SOCK_STATE_t;

typedef struct ECBlockServerEngine{
    struct configProperties *cfg;
    
    int eClientfd; //epoll fd for monitoring incoming connections for client fd
    int eMetafd; //monitoring sock for communicating with meta server
    
    char metaServerIP[255];
    int metaServerIPSize;
    int metaServerPort;
    
    char chunkServerIP[255];
    int chunkServerIPSize;
    int chunkServerPort;
    
    int pprPort;
    
    int metaFd;
    
    int recoveryBufSize;
    int recoveryBufNum;
    
    META_SOCK_STATE_t metaSockState;
    
    int clientFd;
    
    int exitFlag;
    
    ECClientManager_t *ecClientMgr;
    
    DiskIOManager_t *diskIOMgr;
    
    ECMessageBuffer_t *readMsgBuf;
    ECMessageBuffer_t *writeMsgBuf;

    PPRMasterWorker_t *pprMasterWorkerPtr;
    PPREngine_t *pprEnginePtr;
    int serverRecoveryPipes[2];
}ECBlockServerEngine_t;

void startBlockServer(const char *cfgFile);
size_t addWriteMsgToMetaServer(ECBlockServerEngine_t *blockServerEnginePtr, char *msgBuf, size_t bufMsgSize);
#endif /* defined(__ECServer__BlockServer__) */
