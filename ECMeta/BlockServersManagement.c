//
//  BlockServersManagement.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/8/17.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "BlockServersManagement.h"
#include "ECMetaEngine.h"
#include "BlockServer.h"
#include "rackMap.h"
#include "ESetPlacement.h"
#include "ecCommon.h"
#include "ecNetHandle.h"
#include "ecUtils.h"
#include "metaServerBlockServerProtocol.h"

#include <sys/epoll.h>
#include <sys/socket.h>
#include <string.h>

void addWriteEvent(BlockServerManager_t *blockServerMgr, BlockServer_t *blockServerPtr){
    if (blockServerPtr->sockState == SERVER_SOCK_EVENT_READ_WRITE)
    {
        return;
    }

    update_event(blockServerMgr->efd, EPOLLIN | EPOLLOUT | EPOLLET , blockServerPtr->sockFd, blockServerPtr);
    blockServerPtr->sockState = SERVER_SOCK_EVENT_READ_WRITE;
}

void removeWriteEvent(BlockServerManager_t *blockServerMgr, BlockServer_t *blockServerPtr){
    update_event(blockServerMgr->efd, EPOLLIN | EPOLLET , blockServerPtr->sockFd, blockServerPtr);
    blockServerPtr->sockState = SERVER_SOCK_EVENT_READ;
}

ssize_t writeToConn(BlockServerManager_t *blockServerMgr, BlockServer_t *blockServerPtr){
    ssize_t totalWriteSize = 0;
    
    while (blockServerPtr->writeMsgBuf->rOffset != blockServerPtr->writeMsgBuf->wOffset ) {
        ECMessageBuffer_t *writeMsgBuf = blockServerPtr->writeMsgBuf;
        
//        printf("start write to rack:%d server:%d wOffset:%lu, rOffset:%lu\n", blockServerPtr->rackId, blockServerPtr->serverId, blockServerPtr->writeMsgBuf->wOffset, blockServerPtr->writeMsgBuf->rOffset);
        ssize_t wSize = send(blockServerPtr->sockFd,  writeMsgBuf->buf+ writeMsgBuf->rOffset, (writeMsgBuf->wOffset-writeMsgBuf->rOffset), 0);
//        printf("write:%ld to rack:%d server:%d\n",wSize,blockServerPtr->rackId, blockServerPtr->serverId);
        
        gettimeofday(&blockServerPtr->lastActiveTime, NULL);

//        printf("send size:%ld to server\n", wSize);
        
        if (wSize <= 0) {
            //Add it to ...
            addWriteEvent(blockServerMgr, blockServerPtr);
            
            return totalWriteSize;
        }
        
        //As msg sended, it countted as one heartbeat
        gettimeofday(&blockServerPtr->lastHeartBeatTime, NULL);
        
//        printf("wSize:%ld to server with rackid:%d, serverid:%d\n", wSize, blockServerPtr->rackId, blockServerPtr->serverId);
        
        totalWriteSize = totalWriteSize + wSize;
        
        writeMsgBuf->rOffset = writeMsgBuf->rOffset + wSize;
        
        if (writeMsgBuf->rOffset == writeMsgBuf->wOffset) {
            if (writeMsgBuf->next != NULL) {
                blockServerPtr->writeMsgBuf = blockServerPtr->writeMsgBuf->next;
                freeMessageBuf(writeMsgBuf);
            }else{
                writeMsgBuf->rOffset = 0;
                writeMsgBuf->wOffset = 0;
            }
        }
    }
    
    if (blockServerPtr->sockState == SERVER_SOCK_EVENT_READ_WRITE) {
        removeWriteEvent(blockServerMgr, blockServerPtr);
    }
    
    return totalWriteSize;
}

int canParseServerCmd(char *buf, size_t bufSize){
    int idx;
    
    if (bufSize == 0) {
        return 0;
    }
    
    for (idx = 0; idx < bufSize; ++idx) {
        if (*(buf + idx) == '\0') {
            return 1;
        }
    }
    
    return 0;
}

void processServerCmd(BlockServerManager_t *blockServerMgr, BlockServer_t *curServerPtr){
    int cmdLen = 0;
    
    if (canParseServerCmd(curServerPtr->readMsgBuf->buf, curServerPtr->readMsgBuf->wOffset) == 0) {
        return;
    }

    MetaServerCommand_t serverCmd = parseBlockServerReplyCommand(curServerPtr->readMsgBuf->buf+curServerPtr->readMsgBuf->rOffset, &cmdLen);
    switch (serverCmd) {
        case MetaServerCommand_HeartBeatReply:
        {
            gettimeofday(&curServerPtr->lastActiveTime, NULL);
            curServerPtr->heartBeatRecvd = 1;
//            printf("Heartbeat recvd rack:%d server:%d\n", curServerPtr->rackId, curServerPtr->serverId);
        }
            break;
        case MetaServerCommand_RecoveryNodesOKReply:{
            const char header[] = "RecoveryNodesOK:\0";
            const char suffix[] = "\r\n\0";
            int eSetIdx = getIntValueBetweenStrings(header, suffix, curServerPtr->readMsgBuf->buf);
            ECMetaServer_t *ecMetaServerPtr = (ECMetaServer_t *)blockServerMgr->metaEnginePtr;
//            printf("Recvd MetaServerCommand_RecoveryNodesOKReply eSetIdx:%d\n",eSetIdx);
            recoverESet(ecMetaServerPtr->ecRecoveryMgr, eSetIdx, 0);
        }
        break;
        case MetaServerCommand_RecoveryNodesFAILEDReply:{
            printf("Recvd MetaServerCommand_RecoveryNodesFailed\n");
        }
        break;
        case MetaServerCommand_RecoveryBlockOKReply:{
            char eSetIdxHeader[] = "ESetIdx:\0";
            char recoveryBlockIdHeader[] = "BlockId:\0";
            char suffix[] = "\r\n\0";

            int eSetIdx = getIntValueBetweenStrings(eSetIdxHeader, suffix, curServerPtr->readMsgBuf->buf);
            uint64_t blockIdx = getUInt64ValueBetweenStrs(recoveryBlockIdHeader, suffix, curServerPtr->readMsgBuf->buf, strlen(curServerPtr->readMsgBuf->buf));
            
//            printf("Recovery OK for ESet:%d blockGroupId:%lu\n",eSetIdx, blockIdx);
            ECMetaServer_t *ecMetaServerPtr = (ECMetaServer_t *)blockServerMgr->metaEnginePtr;
            recoverESet(ecMetaServerPtr->ecRecoveryMgr, eSetIdx, blockIdx);
            
        }
            break;
        case MetaServerCommand_DeleteBlockOKReply:{
            char theBlockIdStr[] = "BlockId:\0";
            char suffix[] = "\r\n\0";
            uint64_t  blockIdx = getUInt64ValueBetweenStrs(theBlockIdStr, suffix, curServerPtr->readMsgBuf->buf, strlen(curServerPtr->readMsgBuf->buf));
            printf("delete OK for blockId:%lu\n",blockIdx);
            reportBlockDeletion(blockServerMgr, blockIdx, 1);
        }
        break;
        case MetaServerCommand_DeleteBlockFailedReply:{
            char theBlockIdStr[] = "BlockId:\0";
            char suffix[] = "\r\n\0";
            uint64_t blockIdx = getUInt64ValueBetweenStrs(theBlockIdStr, suffix, curServerPtr->readMsgBuf->buf, strlen(curServerPtr->readMsgBuf->buf));
            printf("delete Failed for blockId:%lu\n",blockIdx);
            reportBlockDeletion(blockServerMgr, blockIdx, 0);
        }
        break;
        default:
            printf("recvd unknow msg %s\n", curServerPtr->readMsgBuf->buf);
            break;
    }
    
    if (dumpMsgWithCh(curServerPtr->readMsgBuf, '\0') == 0){
        printf("Unable to dump msg\n");
    }
    
    if (canParseServerCmd(curServerPtr->readMsgBuf->buf, curServerPtr->readMsgBuf->wOffset) == 1) {
        return processServerCmd(blockServerMgr, curServerPtr);
    }
    
    return ;
}

ssize_t recvBlockServerData(BlockServerManager_t *blockServerMgr, BlockServer_t *curServerPtr){
    ssize_t recvSize = 0;
    int recvFlag = 0; // keep recving if buf is full after processing msg
    
//    printf("rack:%d server:%d start recv\n",curServerPtr->rackId, curServerPtr->serverId);
    recvSize = recv(curServerPtr->sockFd, (void *)(curServerPtr->readMsgBuf->buf + curServerPtr->readMsgBuf->wOffset), (curServerPtr->readMsgBuf->bufSize - curServerPtr->readMsgBuf->wOffset), 0);
    
//    printf("rack:%d server:%d recvd:%ld\n",curServerPtr->rackId, curServerPtr->serverId, recvSize);

    if (recvSize <= 0) {
        return 0;
    }
    
    gettimeofday(&curServerPtr->lastActiveTime, NULL);
    
    curServerPtr->readMsgBuf->wOffset = curServerPtr->readMsgBuf->wOffset+recvSize;
    
    if (canParseServerCmd(curServerPtr->readMsgBuf->buf, curServerPtr->readMsgBuf->wOffset) == 0) {
        return recvSize;
    }
    
    if (curServerPtr->readMsgBuf->wOffset == curServerPtr->readMsgBuf->bufSize) {
        recvFlag = 1;
    }
    
    processServerCmd(blockServerMgr, curServerPtr);
    
    if (recvFlag == 1) {
        recvBlockServerData(blockServerMgr, curServerPtr);
    }
    
    return recvSize;
}

void sendHeartBeat(BlockServer_t *curBlockServer){
    char HeartBeatStr[255] = "HeartBeat\r\n\0";
    
    size_t sendMsgLen = strlen(HeartBeatStr);
    sendMsgLen = sendMsgLen + 1;
    
    size_t writeLen = addWriteMsgToBlockserver(curBlockServer, HeartBeatStr,  sendMsgLen);
    
    if (writeLen != sendMsgLen) {
        printf("writeLen:%lu != sendMsgLen:%lu\n", writeLen, sendMsgLen);
        return;
    }
    
    curBlockServer->heartBeatRecvd = -1;
}

/**
 a. Remove the server from activeServers
 b. stop monitoring the sock and close sock
 c. Set server stat to SERVER_DOWN
 d. add failed server to ecRecoveryMgr
 e. dealloc the blockServer
*/
void reportFailedBlockServer(BlockServerManager_t *blockServerMgr, BlockServer_t *blockServerPtr){
    
    printf("reportFailedBlockServer rack:%d server:%d\n", blockServerPtr->rackId, blockServerPtr->serverId);
    
    if (blockServerMgr->activeServers == blockServerPtr) {
        blockServerMgr->activeServers = blockServerPtr->next;
    }else{
        BlockServer_t *activeServersPtr = blockServerMgr->activeServers;
        
        while (activeServersPtr->next != NULL && activeServersPtr->next != blockServerPtr ) {
            activeServersPtr = activeServersPtr->next;
        }
        
        if(activeServersPtr->next == NULL ){
            printf("error activeServersPtr->next == NULL\n");
        }
        
        activeServersPtr->next = activeServersPtr->next->next;
    }
    
    int status = delete_event(blockServerMgr->efd, blockServerPtr->sockFd);
    printf("delete sever:%p fd:%d status:%d\n", blockServerPtr, blockServerPtr->sockFd, status);
    close(blockServerPtr->sockFd);
    
    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)blockServerMgr->metaEnginePtr;
    Rack_t *rackPtr = metaServerPtr->rMap->racks + blockServerPtr->rackId;
    Server_t *serverPtr = rackPtr->servers + blockServerPtr->serverId;
    
    printf("add Failed Server:%d in Rack:%d\n", serverPtr->serverId, serverPtr->rackId);
    serverPtr->curState = SERVER_DOWN;
    addFailedServer(metaServerPtr->ecRecoveryMgr, serverPtr);
    
    --blockServerMgr->activeServersNum;
    deallocBlockServer(blockServerPtr);
}

void checkHeartBeat(BlockServerManager_t *blockServerMgr){
    BlockServer_t *curServerPtr = blockServerMgr->activeServers;
    while (curServerPtr != NULL) {
        
        if (curServerPtr->sockState == SERVER_SOCK_EVENT_UNREGISTERED) {
            curServerPtr = curServerPtr->next;
            continue;
        }
        
        struct timeval now;
        gettimeofday(&now, NULL);
        double timeInterval = timeIntervalInMS(curServerPtr->lastHeartBeatTime, now);
        
        double activeInterval = timeIntervalInMS(curServerPtr->lastActiveTime, now);
        if (curServerPtr->heartBeatRecvd == -1) {
            //Heartbeat sended, but not recvd
            if ((int)(activeInterval/1000.0) >= blockServerMgr->serverMarkFailedInterval) {
                printf("activeInterval:%fms rackId:%d serverId:%d\n",activeInterval, curServerPtr->rackId, curServerPtr->serverId);
                reportFailedBlockServer(blockServerMgr, curServerPtr);
            }
            
            curServerPtr = curServerPtr->next;
            continue;
        }
        
        if (curServerPtr->heartBeatRecvd == 0 || ((int) timeInterval) >= blockServerMgr->blockServerHeartBeatTimeInterval) {
            //Sending heartbeat to ...
//            printf("Send heartbeat to:%d %d, timeInterval:%d\n", curServerPtr->rackId, curServerPtr->serverId, ((int) timeInterval));
            sendHeartBeat(curServerPtr);
            writeToConn(blockServerMgr, curServerPtr);
            curServerPtr->lastHeartBeatTime.tv_sec = now.tv_sec;
            curServerPtr->lastHeartBeatTime.tv_usec = now.tv_usec;
        }
        
        curServerPtr = curServerPtr->next;
    }
}

void addEventsToBlockServerSock(BlockServerManager_t *blockServerMgr){
    
    if (blockServerMgr->metaBlockServer->sockState == SERVER_SOCK_EVENT_UNREGISTERED) {
        int status = add_event(blockServerMgr->efd, EPOLLIN | EPOLLET, blockServerMgr->metaBlockServer->sockFd, blockServerMgr->metaBlockServer);
        if(status ==-1)
        {
            perror("epoll_ctl add_event blockServer");
            return;
        }
        
        blockServerMgr->metaBlockServer->sockState = SERVER_SOCK_EVENT_READ;
    }
    
    
    BlockServer_t *curServerPtr = blockServerMgr->activeServers;

    while (curServerPtr != NULL) {
        
        if (curServerPtr->sockState == SERVER_SOCK_EVENT_UNREGISTERED) {
            int status = add_event(blockServerMgr->efd, EPOLLIN | EPOLLET, curServerPtr->sockFd, curServerPtr);
            printf("r:%d s:%d with addr:%p add EPOLLIN | EPOLLET\n", curServerPtr->rackId, curServerPtr->serverId, curServerPtr);
            if(status ==-1)
            {
                perror("epoll_ctl add_event blockServer");
                return;
            }
            
            curServerPtr->sockState = SERVER_SOCK_EVENT_READ;
            
            gettimeofday(&curServerPtr->lastActiveTime, NULL);
        }
        
        curServerPtr = curServerPtr->next;
    }
}

void startManagingBlockServers(ECMetaServer_t *ecMetaServer, BlockServerManager_t *blockServerMgr){
    
//    printf("startManagingBlockServers\n");
    
    int idx, eventsNum;
    
    //    struct epoll_event clientEvent, serverEvent;
    struct epoll_event* events;
    
    events = talloc(struct epoll_event, MAX_CONN_EVENTS);

    if (events == NULL) {
        perror("unable to alloc memory for events");
        return;
    }

    if (blockServerMgr->newAddedServersNum != 0) {
        addActiveBlockServers(blockServerMgr);
    }

    while (ecMetaServer->exitFlag == 0) {
        
        if (blockServerMgr->newAddedServersNum != 0) {
            addActiveBlockServers(blockServerMgr);
        }

        addEventsToBlockServerSock(blockServerMgr);
        
        eventsNum = epoll_wait(blockServerMgr->efd, events, MAX_CONN_EVENTS,DEFAULT_TIMEOUT_IN_MillisecondForBlockServers);
//        printf("eventsNum:%d\n",eventsNum);
        
        for (idx = 0 ; idx < eventsNum; ++idx) {
            if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)||
               (!(events[idx].events & EPOLLIN) && !(events[idx].events & EPOLLOUT))){
                BlockServer_t *curServerPtr = (BlockServer_t *) events[idx].data.ptr;
                printf("Error value EPOLLERR:%d  EPOLLHUP: %d EPOLLIN: %d EPOLLOUT:%d\n",EPOLLERR,EPOLLHUP,EPOLLIN,EPOLLOUT);
                printf("error on sock server rack:%d, id:%d error:%d\n", curServerPtr->rackId, curServerPtr->serverId, events[idx].events);
//                fprintf(stderr,"epoll error\n");
                if (curServerPtr != blockServerMgr->metaBlockServer) {
                    reportFailedBlockServer(blockServerMgr, curServerPtr);
                }else{
                    printf("blockServerMgr->metaBlockServer recvd event:%d\n",events[idx].events);
                }
            }else{
                BlockServer_t *curServerPtr = (BlockServer_t *) events[idx].data.ptr;
                
                if (curServerPtr == blockServerMgr->metaBlockServer) {
                    //We have ... to do
                    dry_pipe_msg(blockServerMgr->metaBlockServer->sockFd);
                    getPendingHandleJob(blockServerMgr);

                    if (blockServerMgr->handlingJobQueue->jobSize > 0)
                    {
                        performServerJobs(blockServerMgr);
                    }
                }else{
                    if ((events[idx].events & EPOLLIN)) {
//                        printf("EPOLLIN rack:%d, server:%d\n", curServerPtr->rackId, curServerPtr->serverId);
                        recvBlockServerData(blockServerMgr, curServerPtr);
                        
                    }else{
                        //We have something to write
                        writeToConn(blockServerMgr, curServerPtr);
                    }
                }
                
            }
        }
        
        checkHeartBeat(blockServerMgr);
        
        ECMetaServer_t *ecMetaServer = (ECMetaServer_t *)blockServerMgr->metaEnginePtr;
        ECRecoveryManager_t *ecRecoveryMgrPtr = ecMetaServer->ecRecoveryMgr;
        startRecoveryESets(ecRecoveryMgrPtr);
    }
}

void *startBlockServersManagement(void *arg){
    printf("startBlockServersManagement\n");
    
    ECMetaServer_t *ecMetaServer = (ECMetaServer_t *)arg;
    BlockServerManager_t *blockServerMgr = ecMetaServer->blockServerMgr;
    
    blockServerMgr->blockServerHeartBeatTimeInterval = ecMetaServer->blockServerHeartBeatTimeInterval;
    blockServerMgr->serverMarkFailedInterval = ecMetaServer->serverMarkFailedInterval;
    
    blockServerMgr->efd = create_epollfd();
    blockServerMgr->metaBlockServer = talloc(BlockServer_t, 1);
    blockServerMgr->metaBlockServer->sockFd = ecMetaServer->serverPipes[0];
    make_non_blocking_sock(ecMetaServer->serverPipes[0]);
    blockServerMgr->metaBlockServer->sockState = SERVER_SOCK_EVENT_UNREGISTERED;
    
    while (ecMetaServer->exitFlag == 0) {
        if (blockServerMgr->newAddedServersNum== 0 && blockServerMgr->activeServersNum == 0) {
            //Wait for servers
            pthread_mutex_lock(&blockServerMgr->waitMutex);
            blockServerMgr->waitForConnections = 1;
            
            printf("Wait connection\n");
            printf("newAddedServersNum:%d, activeServersNum:%d\n",blockServerMgr->newAddedServersNum, blockServerMgr->activeServersNum);
            pthread_cond_wait(&blockServerMgr->waitCond, &blockServerMgr->waitMutex);
            
            blockServerMgr->waitForConnections = 0;
            printf("Wake up, connection come\n");
            pthread_mutex_unlock(&blockServerMgr->waitMutex);
        }else{
            startManagingBlockServers(ecMetaServer, blockServerMgr);
        }
    }
    
    return ecMetaServer;
}