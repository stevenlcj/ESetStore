//
//  NetIOWorker.c
//  ECStoreCPUServer
//
//  Created by Liu Chengjian on 18/1/9.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "NetIOWorker.h"
#include "DiskIOHandler.h"
#include "ecNetHandle.h"

#include "BlockServerEngine.h"
#include "ECClientResponse.h"

#include "ecCommon.h"
#include "ecUtils.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void flushReadOverCmd(RecoveryThreadManager_t *recoveryThMgr, struct epoll_event* events, int maxEvents);

void getBlockFd(blockInfo_t *blockInfoPtr){
    char fdStr[] = "BlockFd:\0";
    char suffix[] = "\r\n\0";
    blockInfoPtr->blockFd = getIntValueBetweenStrings(fdStr, suffix, blockInfoPtr->readMsgBuf->buf);

    printf("get blockFd:%d\n", blockInfoPtr->blockFd);
}

int readBlockData(RecoveryThreadManager_t *recoveryThMgr, blockInfo_t *blockInfoPtr){
    recoveryBuf_t *curBuf = recoveryThMgr->recoveryBufs + blockInfoPtr->bufIdx;
    
    int curRecvdSize = 0, recvdSize = 0, recvingSize = *(curBuf->bufToWriteSize + blockInfoPtr->index) - *(curBuf->bufWritedSize + blockInfoPtr->index);
    char *recvingBuf = *(curBuf->inputBufsPtrs + blockInfoPtr->index) + *(curBuf->bufWritedSize + blockInfoPtr->index);
    while ((curRecvdSize = (int)recv(blockInfoPtr->fd, recvingBuf + recvdSize, recvingSize, 0)) > 0) {
        *(curBuf->bufWritedSize + blockInfoPtr->index) = *(curBuf->bufWritedSize + blockInfoPtr->index) + curRecvdSize;
        recvingSize = recvingSize - curRecvdSize;
        recvdSize = recvdSize + curRecvdSize;
        
        if (recvingSize == 0) {
            blockInfoPtr->curNetState = BLOCK_NET_STATE_START_READ;
            break;
        }
    }
    
    return recvdSize;
}

int parseOpenReqRecvdReply(RecoveryThreadManager_t *recoveryThMgr, blockInfo_t *blockInfoPtr){
    char blockOpenOK[] = "BlockOpenOK\0";
    char blockOpenFailed[] = "BlockOpenFailed\0";

    if (strncmp(blockInfoPtr->readMsgBuf->buf, blockOpenOK, strlen(blockOpenOK)) == 0)
    {
        getBlockFd(blockInfoPtr);
    }else if(strncmp(blockInfoPtr->readMsgBuf->buf, blockOpenFailed, strlen(blockOpenFailed)) == 0){
        printf("BlockOpenFailed for block:%lu\n", blockInfoPtr->blockId);
    }else{
        printf("Unknow str for Worker_STATE_WRITE_REQUESTING\n");
        printf("Recvd:%s, wOffset:%lu\n",blockInfoPtr->readMsgBuf->buf, blockInfoPtr->readMsgBuf->wOffset);
        return -1;
    }
    
    return 0;
}

int addNetWriteEvent(RecoveryThreadManager_t *recoveryThMgr, blockInfo_t *blockInfoPtr){
    if (blockInfoPtr->curSockState == SOCK_EVENT_READ_WRITE || blockInfoPtr->curSockState == SOCK_EVENT_WRITE) {
        return 0;
    }
    
    if (blockInfoPtr->curSockState == SOCK_EVENT_UNREGISTERED || blockInfoPtr->curSockState ==SOCK_EVENT_READ_DELETE ) {
        printf("curSockState: SOCK_EVENT_UNREGISTERED || SOCK_EVENT_READ_DELETE\n");
        return -1;
    }
    
    int status = update_event(recoveryThMgr->recoveryEfd, EPOLLIN | EPOLLOUT, blockInfoPtr->fd, NULL);
    
    if (status == -1) {
        printf("Unable to update event for blockserver sock with idx:%d\n", blockInfoPtr->idx);
        return -1;
    }
    
    if (blockInfoPtr->curSockState == SOCK_EVENT_READ) {
        blockInfoPtr->curSockState = SOCK_EVENT_READ_WRITE ;
    }else{
        blockInfoPtr->curSockState = SOCK_EVENT_WRITE;
    }
    
    return 1;
}

int readReplyFromBlockServer(RecoveryThreadManager_t *recoveryThMgr, blockInfo_t *blockInfoPtr){
    int recvSize = 1, recvdSize, cmdDoneFlag = 0;
    while ((recvdSize = (int) recv(blockInfoPtr->fd, (blockInfoPtr->readMsgBuf->buf+blockInfoPtr->readMsgBuf->wOffset), recvSize, 0)) > 0) {
        if (*(blockInfoPtr->readMsgBuf->buf + blockInfoPtr->readMsgBuf->wOffset) == '\0') {
            cmdDoneFlag = 1;
        }
        
        blockInfoPtr->readMsgBuf->wOffset = blockInfoPtr->readMsgBuf->wOffset + recvdSize;
        
        if (cmdDoneFlag == 1) {
            break;
        }
    }
    
    printf("Recvd size:%lu\n", blockInfoPtr->readMsgBuf->wOffset);
    if (blockInfoPtr->readMsgBuf->wOffset == 0) {
        sleep(500);
    }
    return cmdDoneFlag;
}

int writeRequestMsgToBlockServer(RecoveryThreadManager_t *recoveryThMgr, blockInfo_t *blockInfoPtr){
    int totalWriteSize = 0;
    
    while (blockInfoPtr->writeMsgBuf->rOffset != blockInfoPtr->writeMsgBuf->wOffset ) {
        ECMessageBuffer_t *writeMsgBuf = blockInfoPtr->writeMsgBuf;
        
        //        printf("start write to rack:%d server:%d wOffset:%lu, rOffset:%lu\n", blockServerPtr->rackId, blockServerPtr->serverId, blockServerPtr->writeMsgBuf->wOffset, blockServerPtr->writeMsgBuf->rOffset);
        int wSize = (int) send(blockInfoPtr->fd,  writeMsgBuf->buf+ writeMsgBuf->rOffset, (writeMsgBuf->wOffset-writeMsgBuf->rOffset), 0);
        //        printf("write:%ld to rack:%d server:%d\n",wSize,blockServerPtr->rackId, blockServerPtr->serverId);
        
        
        //        printf("send size:%ld to server\n", wSize);
        
        if (wSize <= 0) {
            //Add it to ...
            addNetWriteEvent(recoveryThMgr, blockInfoPtr);
            
            return totalWriteSize;
        }
        
        totalWriteSize = totalWriteSize + wSize;
        
        writeMsgBuf->rOffset = writeMsgBuf->rOffset + wSize;
                
        if (writeMsgBuf->rOffset == writeMsgBuf->wOffset) {
            writeMsgBuf->rOffset = 0;
            writeMsgBuf->wOffset = 0;
        }
    }
    
    return totalWriteSize;
}

int addWMsgToBlockServer(blockInfo_t *blockInfoPtr, char *msgBuf, size_t msgSize){
    ECMessageBuffer_t *wMsgBuf = blockInfoPtr->writeMsgBuf;
    if ((wMsgBuf->bufSize - wMsgBuf->wOffset) < msgSize ) {
        printf("(wMsgBuf->bufSize - wMsgBuf->wOffset) < msgSize\n");
        return -1;
    }
    
    memcpy((wMsgBuf->buf + wMsgBuf->wOffset), msgBuf, msgSize);
    wMsgBuf->wOffset = wMsgBuf->wOffset  + msgSize;
    
    return 0;
}

int connectBlockServer(blockInfo_t *blockInfoPtr){
    
    blockInfoPtr->fd = create_TCP_sockFd();
    
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(blockInfoPtr->IPAddr);
    
    if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
        printf("Error on convert server IP \n");
        printf("server IP:%s\n", blockInfoPtr->IPAddr);
    }
    
    serv_addr.sin_port = htons(blockInfoPtr->port);
    
    if (connect(blockInfoPtr->fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Connect error to block server");
        return -1;
    }
    
    make_non_blocking_sock(blockInfoPtr->fd);
    
    return 0;
}

int startBlockRead(RecoveryThreadManager_t *recoveryThMgr, blockInfo_t *blockInfoPtr){
    if (blockInfoPtr->curNetState != BLOCK_NET_STATE_READING) {
        printf("Block reading");
        return -1;
    }
    
    return 0;
}

void sendReadOverCmd(RecoveryThreadManager_t *recoveryThMgr, blockInfo_t *blockInfoPtr)
{
    writeRequestMsgToBlockServer(recoveryThMgr, blockInfoPtr);
    if (blockInfoPtr->writeMsgBuf->wOffset == 0) {
        blockInfoPtr->curNetState = BLOCK_NET_STATE_CONNECTED;
    }
}

//BLOCK_NET_STATE_UINIT->BLOCK_NET_STATE_CONNECTED-> form request read -> BLOCK_NET_STATE_START_REQUEST_READ send read request ->
//BLOCK_NET_STATE_START_READ -> BLOCK_NET_STATE_READOVER close -> send read request
int requestBlockRead(RecoveryThreadManager_t *recoveryThMgr, blockInfo_t *blockInfoPtr){
    switch (blockInfoPtr->curNetState) {
        case BLOCK_NET_STATE_UINIT:{
            printf("BLOCK_NET_STATE_UINIT for block with Idx:%lu\n",blockInfoPtr->blockId);
            
            if (connectBlockServer(blockInfoPtr) == -1) {
                printf("Unable to connect server with IP:%s, port:%d\n", blockInfoPtr->IPAddr, blockInfoPtr->port);
                return -1;
            }
            blockInfoPtr->curNetState = BLOCK_NET_STATE_CONNECTED;
            int status = add_event(recoveryThMgr->recoveryEfd, EPOLLIN , blockInfoPtr->fd, NULL);
            
            if (status  == -1) {
                perror("epoll_ctl add_event blockInfoPtr->fd");
                return -1;
            }
            
            blockInfoPtr->curSockState = SOCK_EVENT_READ;
            
            requestBlockRead(recoveryThMgr, blockInfoPtr);
        }
        break;
        case BLOCK_NET_STATE_CONNECTED:{
            
            if (blockInfoPtr->writeMsgBuf->wOffset != blockInfoPtr->writeMsgBuf->rOffset) {
                //Send cmd;
                if(writeRequestMsgToBlockServer(recoveryThMgr, blockInfoPtr) > 0){
                    requestBlockRead(recoveryThMgr, blockInfoPtr);
                    return 0;
                }
            }
            char blockOpenCmd[1024];
            size_t cmdLen = constructOpenBlockCmd(blockOpenCmd, blockInfoPtr->blockId);
            addWMsgToBlockServer(blockInfoPtr, blockOpenCmd, cmdLen);
            blockInfoPtr->curNetState = BLOCK_NET_STATE_START_REQUEST_READ;
            requestBlockRead(recoveryThMgr, blockInfoPtr);
            return 0;
        }
        break;
            
        case BLOCK_NET_STATE_START_REQUEST_READ:{
            if (blockInfoPtr->writeMsgBuf->wOffset != blockInfoPtr->writeMsgBuf->rOffset) {
                //Send cmd;
                if(writeRequestMsgToBlockServer(recoveryThMgr, blockInfoPtr) > 0){
                    requestBlockRead(recoveryThMgr, blockInfoPtr);
                    return 0;
                }
            }else{
                //recv reply;
                if (readReplyFromBlockServer(recoveryThMgr, blockInfoPtr) == 0) {
                    //Reply didn't complete, wait for new msg coming
                    return 0;
                }
                
                if (parseOpenReqRecvdReply(recoveryThMgr, blockInfoPtr) == -1) {
                    printf("Error: request read failed for blockserver with idx:%d\n", blockInfoPtr->idx);
                    blockInfoPtr->readMsgBuf->rOffset =  blockInfoPtr->readMsgBuf->wOffset;
                    return -1;
                }
                
                blockInfoPtr->readMsgBuf->rOffset = 0;
                blockInfoPtr->readMsgBuf->wOffset = 0;
                
                blockInfoPtr->curNetState = BLOCK_NET_STATE_START_READ;
                requestBlockRead(recoveryThMgr, blockInfoPtr);
                
            }
        }
        break;
            
        case BLOCK_NET_STATE_READING:{
            if (blockInfoPtr->writeMsgBuf->wOffset != 0) {
                writeRequestMsgToBlockServer(recoveryThMgr, blockInfoPtr);
                return 0;
            }else{
                readBlockData(recoveryThMgr, blockInfoPtr);
            }
        }
        break;
            
        case BLOCK_NET_STATE_START_READ:{
            
            recoveryBuf_t *curBuf = recoveryThMgr->recoveryBufs + blockInfoPtr->bufIdx;
            size_t readSize = (size_t) *(curBuf->bufToWriteSize + blockInfoPtr->index);
            char readCmdBuf[1024];
            size_t cmdSize = constructBlockReadCmd(readCmdBuf, blockInfoPtr->blockId, blockInfoPtr->blockFd, readSize); 
            
            addWMsgToBlockServer(blockInfoPtr, readCmdBuf, cmdSize);
            blockInfoPtr->curNetState = BLOCK_NET_STATE_READING;
            
            writeRequestMsgToBlockServer(recoveryThMgr, blockInfoPtr);
        }
        break;
            
        default:
            printf("Unknow state, please check\n");
            break;
    }
    
    return 0;
}

blockInfo_t *getBlockInfoPtrWithFd(RecoveryThreadManager_t *recoveryThMgr, int fd){
    int idx;
    for (idx = 0; idx < recoveryThMgr->sourceNodesSize; ++idx) {
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + idx;
        
        if (blockInfoPtr->fd == fd) {
            return blockInfoPtr;
        }
    }
    
    return NULL;
}

void recvingCurNetData(RecoveryThreadManager_t *recoveryThMgr , int bufIdx, struct epoll_event * events, int maxEvents, int *jobDoneFlag, int index){
    int idx,eventsNum, eventsIdx;
    
    eventsNum = epoll_wait(recoveryThMgr->recoveryEfd , events, maxEvents,DEFAULT_TIMEOUT_IN_Millisecond);
    
    if (eventsNum == 0) {
        //Time out
        return;
    }
    
    for (eventsIdx = 0; eventsIdx < eventsNum; ++eventsIdx) {
        int curFd = events[eventsIdx].data.fd;
        
        if((events[eventsIdx].events & EPOLLERR)||
           (events[eventsIdx].events & EPOLLHUP)||
           (!(events[eventsIdx].events & EPOLLIN) && !(events[eventsIdx].events & EPOLLOUT))){
            
            printf("Error value EPOLLERR:%d  EPOLLHUP: %d EPOLLIN: %d EPOLLOUT:%d\n",EPOLLERR,EPOLLHUP,EPOLLIN,EPOLLOUT);
            //                fprintf(stderr,"epoll error\n");
            if (curFd == recoveryThMgr->mgrPipes[0]) {
                printf("Error for mrgPipes\n");
            }else{
                blockInfo_t *curBlockPtr = getBlockInfoPtrWithFd(recoveryThMgr, curFd);
                printf("Error for block with idx:%lu\n",curBlockPtr->blockId);
            }
        }else{
            if (curFd == recoveryThMgr->mgrPipes[0]) {
                dry_pipe_msg(recoveryThMgr->mgrPipes[0]);
            }else{
                blockInfo_t *curBlockPtr = getBlockInfoPtrWithFd(recoveryThMgr, curFd);
                
                if (curBlockPtr->index != index) {
                    printf("error:%d\n", curBlockPtr->index);
                }
                
                if (curBlockPtr->curNetState == BLOCK_NET_STATE_READING) {
                    readBlockData(recoveryThMgr, curBlockPtr);
                }else{
                    requestBlockRead(recoveryThMgr, curBlockPtr);
                }
            }
        }
    }
    
    recoveryBuf_t *curRecoveryBuf =recoveryThMgr->recoveryBufs + bufIdx;
    
    int toWriteSize = *(curRecoveryBuf->bufToWriteSize + index), writedSize = *(curRecoveryBuf->bufWritedSize + index);
    
    if (toWriteSize != writedSize) {
        return;
    }

    *jobDoneFlag = 1;
    return;
}

void recevingDiskData(RecoveryThreadManager_t *recoveryThMgr , int bufIdx, struct epoll_event * events, int maxEvents, int *jobDoneFlag){
    int eventsNum, eventsIdx;
    eventsNum = epoll_wait(recoveryThMgr->recoveryEfd , events, maxEvents,DEFAULT_TIMEOUT_IN_Millisecond);
    
    if (eventsNum == 0) {
        //Time out
        return;
    }
    
    for (eventsIdx = 0; eventsIdx < eventsNum; ++eventsIdx) {
        int curFd = events[eventsIdx].data.fd;
        
        if((events[eventsIdx].events & EPOLLERR)||
           (events[eventsIdx].events & EPOLLHUP)||
           (!(events[eventsIdx].events & EPOLLIN) && !(events[eventsIdx].events & EPOLLOUT))){
            
            printf("Error value EPOLLERR:%d  EPOLLHUP: %d EPOLLIN: %d EPOLLOUT:%d\n",EPOLLERR,EPOLLHUP,EPOLLIN,EPOLLOUT);
            //                fprintf(stderr,"epoll error\n");
            if (curFd == recoveryThMgr->mgrPipes[0]) {
                printf("Error for mrgPipes\n");
            }else{
                blockInfo_t *curBlockPtr = getBlockInfoPtrWithFd(recoveryThMgr, curFd);
                printf("Error for block with idx:%lu\n",curBlockPtr->blockId);
            }
        }else{
            if (curFd == recoveryThMgr->mgrPipes[0]) {
                dry_pipe_msg(recoveryThMgr->mgrPipes[0]);
            }else{
                printf("could not be here, please check\n");
                blockInfo_t *curBlockPtr = getBlockInfoPtrWithFd(recoveryThMgr, curFd);
                
                if (curBlockPtr->curNetState == BLOCK_NET_STATE_READING) {
                    readBlockData(recoveryThMgr, curBlockPtr);
                }else{
                    requestBlockRead(recoveryThMgr, curBlockPtr);
                }
            }
        }
    }
}

void waitDiskData(RecoveryThreadManager_t *recoveryThMgr , int bufIdx, struct epoll_event * events, int maxEvents, int *jobDoneFlag){
    int idx;
    
    recoveryBuf_t *curRecoveryBuf =recoveryThMgr->recoveryBufs + bufIdx;

    for (idx = 0; idx < recoveryThMgr->sourceNodesSize; ++idx) {
        int toWriteSize = *(curRecoveryBuf->bufToWriteSize + idx), writedSize = *(curRecoveryBuf->bufWritedSize + idx);
        
        //        printf("idx:%d toWriteSize:%d writedSize:%d\n",idx, toWriteSize, writedSize);
        
        if (toWriteSize != writedSize) {
            recevingDiskData(recoveryThMgr, bufIdx, events, maxEvents, jobDoneFlag);
            return;
        }
    }
    
    *jobDoneFlag = 1;

}

void recvingNetDiskData(RecoveryThreadManager_t *recoveryThMgr , int bufIdx, struct epoll_event * events, int maxEvents, int *jobDoneFlag){
    int idx,eventsNum, eventsIdx;
    
    eventsNum = epoll_wait(recoveryThMgr->recoveryEfd , events, maxEvents,DEFAULT_TIMEOUT_IN_Millisecond);
    
    if (eventsNum == 0) {
        //Time out
        return;
    }
    
    for (eventsIdx = 0; eventsIdx < eventsNum; ++eventsIdx) {
        int curFd = events[eventsIdx].data.fd;
        
        if((events[eventsIdx].events & EPOLLERR)||
           (events[eventsIdx].events & EPOLLHUP)||
           (!(events[eventsIdx].events & EPOLLIN) && !(events[eventsIdx].events & EPOLLOUT))){
            
            printf("Error value EPOLLERR:%d  EPOLLHUP: %d EPOLLIN: %d EPOLLOUT:%d\n",EPOLLERR,EPOLLHUP,EPOLLIN,EPOLLOUT);
            //                fprintf(stderr,"epoll error\n");
            if (curFd == recoveryThMgr->mgrPipes[0]) {
                printf("Error for mrgPipes\n");
            }else{
                blockInfo_t *curBlockPtr = getBlockInfoPtrWithFd(recoveryThMgr, curFd);
                printf("Error for block with idx:%lu\n",curBlockPtr->blockId);
            }
        }else{
            if (curFd == recoveryThMgr->mgrPipes[0]) {
                dry_pipe_msg(recoveryThMgr->mgrPipes[0]);
            }else{
                blockInfo_t *curBlockPtr = getBlockInfoPtrWithFd(recoveryThMgr, curFd);
                
                if (curBlockPtr->curNetState == BLOCK_NET_STATE_READING) {
                    readBlockData(recoveryThMgr, curBlockPtr);
                }else{
                    requestBlockRead(recoveryThMgr, curBlockPtr);
                }
            }
        }
    }
    
    recoveryBuf_t *curRecoveryBuf =recoveryThMgr->recoveryBufs + bufIdx;
    
    for (idx = 0; idx < recoveryThMgr->sourceNodesSize; ++idx) {
        int toWriteSize = *(curRecoveryBuf->bufToWriteSize + idx), writedSize = *(curRecoveryBuf->bufWritedSize + idx);
        
//        printf("idx:%d toWriteSize:%d writedSize:%d\n",idx, toWriteSize, writedSize);
        
        if (toWriteSize != writedSize) {
            return;
        }
    }
    
    *jobDoneFlag = 1;
    return;
}

void monitoringReadOverSending(RecoveryThreadManager_t *recoveryThMgr, struct epoll_event* events, int maxEvents){
    int eventsNum, eventsIdx;
    
    eventsNum = epoll_wait(recoveryThMgr->recoveryEfd , events, maxEvents,DEFAULT_TIMEOUT_IN_Millisecond);

    for (eventsIdx = 0; eventsIdx < eventsNum; ++eventsIdx) {
        int curFd = events[eventsIdx].data.fd;
        
        if((events[eventsIdx].events & EPOLLERR)||
           (events[eventsIdx].events & EPOLLHUP)||
           (!(events[eventsIdx].events & EPOLLIN) && !(events[eventsIdx].events & EPOLLOUT))){
            
            printf("Error value EPOLLERR:%d  EPOLLHUP: %d EPOLLIN: %d EPOLLOUT:%d\n",EPOLLERR,EPOLLHUP,EPOLLIN,EPOLLOUT);
            //                fprintf(stderr,"epoll error\n");
            if (curFd == recoveryThMgr->mgrPipes[0]) {
                printf("Error for mrgPipes\n");
            }else{
                blockInfo_t *curBlockPtr = getBlockInfoPtrWithFd(recoveryThMgr, curFd);
                printf("Error for block with idx:%lu\n",curBlockPtr->blockId);
            }
        }else{
            blockInfo_t *curBlockPtr = getBlockInfoPtrWithFd(recoveryThMgr, curFd);
            
            if (curBlockPtr == NULL) {
                printf("wroing fd\n");
                continue;
            }
            
            if (curBlockPtr->curNetState == BLOCK_NET_STATE_READOVER) {
                sendReadOverCmd(recoveryThMgr, curBlockPtr);
            }else{
                printf("error: wrong monitoring\n");
            }
        }
    }
    
    flushReadOverCmd(recoveryThMgr, events, maxEvents);
}

void flushReadOverCmd(RecoveryThreadManager_t *recoveryThMgr, struct epoll_event* events, int maxEvents){
    int idx, unfinished = 0;
    for (idx = 0; idx < recoveryThMgr->sourceNodesSize; ++idx) {
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + idx;
        
        if (blockInfoPtr->selfFlag == 1) {
            continue;
        }
        
        if (blockInfoPtr->curNetState == BLOCK_NET_STATE_READOVER) {
            unfinished = 1;
        }
    }
    
    if (unfinished == 1) {
        monitoringReadOverSending(recoveryThMgr, events, MAX_EVENTS);
    }
}

void closeRemoteBlocksFd(RecoveryThreadManager_t *recoveryThMgr, struct epoll_event* events, int maxEvents){
    int idx;
    for (idx = 0; idx < recoveryThMgr->sourceNodesSize; ++idx) {
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + idx;
        
        if (blockInfoPtr->selfFlag == 1) {
            continue;
        }
        
        if (blockInfoPtr->curNetState == BLOCK_NET_STATE_START_READ) {
            char readOverCmd[1024];
            size_t cmdSize = constructBlockCloseCmd(readOverCmd, blockInfoPtr->blockId, blockInfoPtr->blockFd);
            blockInfoPtr->curNetState = BLOCK_NET_STATE_READOVER;
            addWMsgToBlockServer(blockInfoPtr, readOverCmd, cmdSize);            
            sendReadOverCmd(recoveryThMgr, blockInfoPtr);
        }
    }
    
    flushReadOverCmd(recoveryThMgr, events, maxEvents);
}


/**
 * a. Post Disk IO sem if there is a job for it
 * b. Doing job
 * c. Post sem for coding when all jobs finished
 */

void performNetDiskIOJob(RecoveryThreadManager_t *recoveryThMgr , int bufIdx, struct epoll_event* events, int maxEvents){
    //Job distributing
    recoveryBuf_t *curRecoveryBuf = recoveryThMgr->recoveryBufs + bufIdx;
    
    int bIdx;
    for (bIdx = 0; bIdx < recoveryThMgr->sourceNodesSize; ++bIdx) {
        if (*(curRecoveryBuf->bufToWriteSize + bIdx) == *(curRecoveryBuf->bufWritedSize + bIdx)) {
            //No data to write for this idx buf
            continue;
        }
        
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + bIdx;
        
        blockInfoPtr->bufIdx = bufIdx;
        
        blockInfoPtr->pendingHandleSize = *(curRecoveryBuf->bufToWriteSize + bIdx);
        blockInfoPtr->handledSize = 0;
        
        if (blockInfoPtr->selfFlag != 1 ) {
            requestBlockRead(recoveryThMgr, blockInfoPtr);
            
            int curTaskDoneFlag = 0;
            while (curTaskDoneFlag == 0) {
                recvingCurNetData(recoveryThMgr, bufIdx, events, maxEvents, &curTaskDoneFlag, blockInfoPtr->index);
            }
        }
    }
    
    int diskJobDone = 0;
    
    while (diskJobDone == 0) {
        waitDiskData(recoveryThMgr, bufIdx, events, maxEvents, &diskJobDone);
    }
    
    sem_post(&recoveryThMgr->codingSem);
}

void *netDiskIOWorker(void *arg){
    RecoveryThreadManager_t *recoveryThMgr = (RecoveryThreadManager_t *)arg;
    int bufIdx = 0;
    struct epoll_event* events;
    
    events = talloc(struct epoll_event, MAX_CONN_EVENTS);
    
    if (events == NULL) {
        perror("unable to alloc memory for events in netDiskIOWorker");
        return arg;
    }

    int status = add_event(recoveryThMgr->recoveryEfd, EPOLLIN , recoveryThMgr->mgrPipes[0], NULL);
    
    if (status == -1) {
        printf("Unable to add event for recovery pipe\n");
    }

    while (recoveryThMgr->recoveryOverFlag == 0) {
        sem_wait(&recoveryThMgr->ioReadSem);
        
        if (recoveryThMgr->recoveryOverFlag == 1) {
            break;
        }
        
        if (recoveryThMgr->localDiskIOReadNums != 0) {
            sem_post(&recoveryThMgr->diskIOReadSem);
        }
        
        if (recoveryThMgr->curBlockDone == 1) {
            closeRemoteBlocksFd(recoveryThMgr, events, MAX_EVENTS);
            continue;
        }
        
        performNetDiskIOJob(recoveryThMgr, bufIdx, events, MAX_EVENTS);
        
        if (recoveryThMgr->recoveryBufNum > 1) {
            bufIdx = (bufIdx + 1) % recoveryThMgr->recoveryBufNum;
        }
    }
    
    return arg;
}

