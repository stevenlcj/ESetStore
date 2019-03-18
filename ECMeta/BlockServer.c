//
//  BlockServer.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/7/26.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "BlockServer.h"
#include "BlockServersManagement.h"

#include "KeyValueEngine.h"
#include "BlockManagement.h"

#include "ECMetaEngine.h"
#include "ECBlockServerCmd.h"

#include <string.h>
#include <unistd.h>

BlockServer_t *createBlockServer(int sockFd){
    BlockServer_t *blockServerPtr = talloc(BlockServer_t, 1);
    
    if (blockServerPtr == NULL) {
        printf("Unable to alloc BlockServer_t\n");
        return NULL;
    }
    
    blockServerPtr->sockFd = sockFd;
    blockServerPtr->sockState = SERVER_SOCK_EVENT_UNREGISTERED;
    
    blockServerPtr->heartBeatRecvd = 0;
    blockServerPtr->next = NULL;
    blockServerPtr->readMsgBuf = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);
    blockServerPtr->writeMsgBuf = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);
    
    return blockServerPtr;
}

BlockServer_t *getActiveBlockServer(BlockServerManager_t *blockServerMgr, int rackId, int serverId){
    BlockServer_t *activeBlockServerPtr = blockServerMgr->activeServers;
    
    while (activeBlockServerPtr != NULL) {
        if (activeBlockServerPtr->rackId == rackId && activeBlockServerPtr->serverId == serverId) {
            return activeBlockServerPtr;
        }
        activeBlockServerPtr = activeBlockServerPtr->next;
    }
    
    return NULL;
}

BlockServerManager_t *createBlockServerManager(void *metaEnginePtr){
    BlockServerManager_t * blockServerMgr = talloc(BlockServerManager_t, 1);
    
    blockServerMgr->newAddedServers = NULL;
    blockServerMgr->newAddedServersNum = 0;
    
    blockServerMgr->activeServers = NULL;
    blockServerMgr->activeServersNum = 0;
    
//    blockServerMgr->unconnectedServers = NULL;
//    blockServerMgr->unconnectedServersNum = 0;
//    
//    blockServerMgr->inRecoveryServers = NULL;
//    blockServerMgr->inRecoveryServersNum = 0;
    
    blockServerMgr->writingTasks = 0;
    
    blockServerMgr->metaEnginePtr = metaEnginePtr;
    pthread_mutex_init(&blockServerMgr->waitMutex, NULL);
    pthread_cond_init(&blockServerMgr->waitCond, NULL);
    

    blockServerMgr->pendingJobQueue = createKeyValueJobQueue();
    blockServerMgr->handlingJobQueue = createKeyValueJobQueue();
    blockServerMgr->inProcessingJobQueue = createKeyValueJobQueue();
    return blockServerMgr;
}

void addNewBlockServer(BlockServerManager_t *blockServerMgr, BlockServer_t *blockServer){
    if (blockServer == NULL) {
        return;
    }
    
    pthread_mutex_lock(&blockServerMgr->waitMutex);
    blockServer->next = blockServerMgr->newAddedServers;
    blockServerMgr->newAddedServers = blockServer;
    ++blockServerMgr->newAddedServersNum;
    blockServer->sockState = SERVER_SOCK_EVENT_UNREGISTERED;
    
    //gettimeofday(&blockServer->lastActiveTime, NULL);
    
    blockServer->lastHeartBeatTime.tv_sec = blockServer->lastActiveTime.tv_sec;
    blockServer->lastHeartBeatTime.tv_usec = blockServer->lastActiveTime.tv_usec;
    
    if (blockServerMgr->waitForConnections == 1) {
        pthread_cond_signal(&blockServerMgr->waitCond);
    }
    
    pthread_mutex_unlock(&blockServerMgr->waitMutex);
}

void addActiveBlockServers(BlockServerManager_t *blockServerMgr){
    
    if (blockServerMgr->newAddedServersNum == 0) {
        return;
    }
    
    int idx;
    
    pthread_mutex_lock(&blockServerMgr->waitMutex);
    
    for (idx = 0; idx < blockServerMgr->newAddedServersNum; ++idx) {
        BlockServer_t *newAddedBlockServer = blockServerMgr->newAddedServers;
        blockServerMgr->newAddedServers = blockServerMgr->newAddedServers->next;
        newAddedBlockServer->next = blockServerMgr->activeServers;
        blockServerMgr->activeServers = newAddedBlockServer;
        ++blockServerMgr->activeServersNum;
    }
    
    blockServerMgr->newAddedServersNum = 0;
    blockServerMgr->newAddedServers = NULL;
    
    pthread_mutex_unlock(&blockServerMgr->waitMutex);
    
    return;
}

size_t addWriteMsgToBlockserver(BlockServer_t *blockServer, char *msgBuf, size_t bufMsgSize){
    
    if (blockServer == NULL) {
        return 0;
    }
    
    ECMessageBuffer_t *writeMsgBufPtr = blockServer->writeMsgBuf;
    
    while (writeMsgBufPtr->next != NULL) {
        writeMsgBufPtr = writeMsgBufPtr->next;
    }
    
    size_t cpedSize = 0, cpSize = 0;
    
    while (cpedSize != bufMsgSize) {
        if (writeMsgBufPtr->wOffset == writeMsgBufPtr->bufSize) {
            writeMsgBufPtr->next = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);
            writeMsgBufPtr = writeMsgBufPtr->next;
        }
        
        cpSize = (writeMsgBufPtr->bufSize -  writeMsgBufPtr->wOffset) > (bufMsgSize - cpedSize) ? (bufMsgSize - cpedSize) : (writeMsgBufPtr->bufSize - writeMsgBufPtr->wOffset);
        
        memcpy((writeMsgBufPtr->buf + writeMsgBufPtr->wOffset), (msgBuf + cpedSize), cpSize);
        writeMsgBufPtr->wOffset = writeMsgBufPtr->wOffset + cpSize;
        cpedSize = cpedSize + cpSize;
    }
    
    return cpedSize;
}

void deallocBlockServer(BlockServer_t *blockServer){
    freeMessageBuf(blockServer->readMsgBuf);
    
    ECMessageBuffer_t *msgBufPtr = blockServer->writeMsgBuf;
    
    while (msgBufPtr != NULL) {
        ECMessageBuffer_t *curPtr = msgBufPtr;
        msgBufPtr = msgBufPtr->next;
        freeMessageBuf(curPtr);
    }
    
//    free(blockServer);
}

int jobHasTheBlock(KeyValueJob_t *jobPtr, uint64_t blockGroupIdx){
    keyValueItem_t *itemPtr = (keyValueItem_t *)jobPtr->itemPtr;
    ECBlockGroup_t *blockGroup = itemPtr->blockGroupsHeader;
    
    do{
        if (blockGroupIdx == blockGroup->blockGroupId )
        {
            return 1;
        }

        if (blockGroup == itemPtr->blockGroupsTail)
        {
            break;
        }

        blockGroup = blockGroup->next;
    }while(1);

    return 0;
}

void reportServersDeleteJobDone(BlockServerManager_t *blockServerMgr,  KeyValueJob_t *jobPtr){    
    
    jobPtr->jobType = KEYVALUEJOB_DELETE_DONE;
    
    ECMetaServer_t *metaServer = (ECMetaServer_t *)blockServerMgr->metaEnginePtr;
    KeyValueWorker_t *keyValueWorker = metaServer->keyValueEnginePtr->workers + jobPtr->workerIdx;

    pthread_mutex_lock(&keyValueWorker->threadLock);
    putJob(keyValueWorker->pendingJobQueue, jobPtr);
    char ch='D';
    write(keyValueWorker->workerPipes[1], &ch, 1);
    pthread_mutex_unlock(&keyValueWorker->threadLock);
}

void reportBlockDeletion(BlockServerManager_t *blockServerMgr, uint64_t blockId, int OKFlag){
    uint64_t blockGroupIdx = getBlockGroupId(blockId);
    printf("blockGroupIdx:%lu, blockId:%lu\n", blockGroupIdx, blockId);
    if (OKFlag == 0)
    {
        printf("Delete none exist block:%lu\n", blockId);
    }

    KeyValueJob_t *jobPtr = blockServerMgr->inProcessingJobQueue->jobPtr;

    size_t idx;

    for (idx = 0; idx < blockServerMgr->inProcessingJobQueue->jobSize; ++idx)
    {
        if (jobHasTheBlock(jobPtr, blockGroupIdx) == 1)
        {
            //gettimeofday(&jobPtr->serverOutTime, NULL);

            jobPtr->inProcessingNum = jobPtr->inProcessingNum - 1;
            break;      
        }

        jobPtr = jobPtr->next;
    }

    if (jobPtr->inProcessingNum == 0)
    {
        removeTheJobFromQueue(blockServerMgr->inProcessingJobQueue, jobPtr);
        reportServersDeleteJobDone(blockServerMgr, jobPtr);
    }
}

void submitJobToBlockMgr(BlockServerManager_t *blockServerMgr, KeyValueJob_t *jobPtr){
    ECMetaServer_t *metaServer = (ECMetaServer_t *)blockServerMgr->metaEnginePtr;

    pthread_mutex_lock(&blockServerMgr->waitMutex);
    putJob(blockServerMgr->pendingJobQueue, jobPtr);
    char ch='D';
    write(metaServer->serverPipes[1], &ch, 1);
    pthread_mutex_unlock(&blockServerMgr->waitMutex);
}

void getPendingHandleJob(BlockServerManager_t *blockServerMgr){
    pthread_mutex_lock(&blockServerMgr->waitMutex);

    while (blockServerMgr->pendingJobQueue->jobSize != 0) {
        putJob(blockServerMgr->handlingJobQueue, getKeyValueJob(blockServerMgr->pendingJobQueue));
    }

    pthread_mutex_unlock(&blockServerMgr->waitMutex);
}

size_t putDeleteCmd(BlockServer_t *blockServer, ECBlock_t *curBlockPtr){
    return  writeDeleteBlockCmd((void *)curBlockPtr, (void *)blockServer);
}

//DeleteBlock\r\nBlockID:__\r\n\0
void deleteBlockGroup(BlockServerManager_t *blockServerMgr, ECBlockGroup_t *blockGroup, KeyValueJob_t *jobPtr){
    size_t idx = 0;
    for (idx = 0; idx < blockGroup->blocksNum; ++idx)
    {
        ECBlock_t *curBlockPtr = blockGroup->blocks + idx;
        BlockServer_t *blockServer = getActiveBlockServer(blockServerMgr, curBlockPtr->rackId, curBlockPtr->serverId);
        //printf("blockgroup idx:%lu delete blockid:%lu blockSize:%lu\n",blockGroup->blockGroupId, curBlockPtr->blockId, curBlockPtr->blockSize);
        if (curBlockPtr->blockSize > 0)
        {
            putDeleteCmd(blockServer, curBlockPtr);
            writeToConn(blockServerMgr, blockServer);
            ++jobPtr->inProcessingNum;
        }
    }
}

void performDeleteBlocksJob(BlockServerManager_t *blockServerMgr, KeyValueJob_t *jobPtr){
    keyValueItem_t *itemPtr = (keyValueItem_t *)jobPtr->itemPtr;
    ECBlockGroup_t *blockGroup = itemPtr->blockGroupsHeader;

    //gettimeofday(&jobPtr->serverInTime, NULL);
    
    do{
        deleteBlockGroup(blockServerMgr, blockGroup, jobPtr);

        if (blockGroup == itemPtr->blockGroupsTail)
        {
            break;
        }

        blockGroup = blockGroup->next;
    }while(1);

    if (jobPtr->inProcessingNum > 0)
    {   
        putJob(blockServerMgr->inProcessingJobQueue, jobPtr);
    }else{
        //Return the job to keyvalue worker...
        reportServersDeleteJobDone(blockServerMgr, jobPtr);
    }
}

void performServerJobs(BlockServerManager_t *blockServerMgr){
    while (blockServerMgr->handlingJobQueue->jobSize != 0) {
        KeyValueJob_t *jobPtr = getKeyValueJob(blockServerMgr->handlingJobQueue);
        
        switch(jobPtr->jobType){
            case KEYVALUEJOB_DELETE:{
                jobPtr->inProcessingNum = 0;
                performDeleteBlocksJob(blockServerMgr, jobPtr);
            }
            break;
            default:
                printf("Unknow jobs for block servers\n");
            break;
        }
    } 
}
