//
//  BlockServer.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/7/26.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__BlockServer__
#define __ECMeta__BlockServer__

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>

#include "MessageBuffer.h"
#include "ecCommon.h"
#include "ECJob.h"

typedef enum SERVER_SOCK_EVENT{
    SERVER_SOCK_EVENT_UNREGISTERED = 0,
    SERVER_SOCK_EVENT_READ = 1,
    SERVER_SOCK_EVENT_WRITE = 2,
    SERVER_SOCK_EVENT_READ_WRITE = 3,
    SERVER_SOCK_EVENT_READ_DELETE = 4
}SERVER_SOCK_EVENT_t;

typedef struct BlockServer{
//    SERVER_SOCK_EVENT_t currentState;
    
    int rackId;
    int serverId;
    
    SERVER_SOCK_EVENT_t sockState;
    int sockFd;
    
    int heartBeatRecvd;
    struct BlockServer *next;
    
    struct timeval lastActiveTime;
    struct timeval lastHeartBeatTime;
    
    struct timeval failedTime;
    
    ECMessageBuffer_t *readMsgBuf;
    ECMessageBuffer_t *writeMsgBuf;


}BlockServer_t;

typedef struct BlockServerManager{
    BlockServer_t *newAddedServers;
    int newAddedServersNum;
    
    BlockServer_t *activeServers;
    int activeServersNum;
    
//    BlockServer_t *unconnectedServers;
//    int unconnectedServersNum;
//    
//    BlockServer_t *inRecoveryServers;
//    int inRecoveryServersNum;
    
    BlockServer_t *metaBlockServer;
    
    pthread_mutex_t waitMutex;
    pthread_cond_t waitCond;

    KeyValueJobQueue_t *pendingJobQueue;
    KeyValueJobQueue_t *handlingJobQueue;
    KeyValueJobQueue_t *inProcessingJobQueue;
    
    int efd;
    int waitForConnections;
    
    int blockServerHeartBeatTimeInterval;
    int serverMarkFailedInterval;
    
    int writingTasks;
    
    void *metaEnginePtr;

}BlockServerManager_t;

BlockServerManager_t *createBlockServerManager(void *metaEnginePtr);

BlockServer_t *createBlockServer(int sockFd);

BlockServer_t *getActiveBlockServer(BlockServerManager_t *blockServerMgr, int rackId, int serverId);
void addNewBlockServer(BlockServerManager_t *blockServerMgr, BlockServer_t *blockServer);
void addActiveBlockServers(BlockServerManager_t *blockServerMgr);

size_t addWriteMsgToBlockserver(BlockServer_t *blockServer, char *msgBuf, size_t bufMsgSize);
void deallocBlockServer(BlockServer_t *blockServer);

void reportBlockDeletion(BlockServerManager_t *blockServerMgr, uint64_t blockId, int OKFlag);
void submitJobToBlockMgr(BlockServerManager_t *blockServerMgr, KeyValueJob_t *jobPtr);
void getPendingHandleJob(BlockServerManager_t *blockServerMgr);

void performServerJobs(BlockServerManager_t *blockServerMgr);
#endif /* defined(__ECMeta__BlockServer__) */
