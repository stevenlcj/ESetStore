//
//  ECMetaServerHandle.c
//  ECServer
//
//  Created by Liu Chengjian on 17/8/10.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECMetaServerHandle.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>

#include "ecNetHandle.h"
#include "ecUtils.h"
#include "metaBlockServerCommand.h"

const char HeartBeatReply[] = "HeartBeatOK\r\n\0";

#define DEFAULT_META_TIME_OUT_IN_MS 500

int canParseMetaCmd(char *buf, size_t bufSize){
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

void addMetaWriteEvent(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    if (ecBlockServerEnginePtr->metaSockState == SERVER_SOCK_STATE_READ_WRITE) {
        return;
    }
    
    update_event(ecBlockServerEnginePtr->eMetafd, EPOLLIN | EPOLLOUT | EPOLLET , ecBlockServerEnginePtr->metaFd, NULL);
    ecBlockServerEnginePtr->metaSockState = SERVER_SOCK_STATE_READ_WRITE;
}

void removeMetaWriteEvent(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    update_event(ecBlockServerEnginePtr->eMetafd, EPOLLIN | EPOLLET , ecBlockServerEnginePtr->metaFd, NULL);
    ecBlockServerEnginePtr->metaSockState = SERVER_SOCK_STATE_READ;
}

ssize_t processMetaOutMsg(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    ssize_t totalWriteSize = 0;
//    printf("start write to meta server wOffset:%lu, rOffset:%lu\n", ecBlockServerEnginePtr->writeMsgBuf->wOffset, ecBlockServerEnginePtr->writeMsgBuf->rOffset);
    
    while (ecBlockServerEnginePtr->writeMsgBuf->rOffset != ecBlockServerEnginePtr->writeMsgBuf->wOffset ) {
        ECMessageBuffer_t *writeMsgBuf = ecBlockServerEnginePtr->writeMsgBuf;
        
        //printf("wOffset:%lu, rOffset:%lu before sending\n", ecBlockServerEnginePtr->writeMsgBuf->wOffset, 
        //                                                                ecBlockServerEnginePtr->writeMsgBuf->rOffset);

        ssize_t wSize = send(ecBlockServerEnginePtr->metaFd,  
                                writeMsgBuf->buf+ writeMsgBuf->rOffset, 
                                (writeMsgBuf->wOffset-writeMsgBuf->rOffset), 0);
        
        printf("wOffset:%lu, rOffset:%lu, sended size:%ld to server\n", ecBlockServerEnginePtr->writeMsgBuf->wOffset, 
                                                                        ecBlockServerEnginePtr->writeMsgBuf->rOffset, wSize);
        
        if (wSize <= 0) {
            //Add it to ...
            addMetaWriteEvent(ecBlockServerEnginePtr);
            
            return totalWriteSize;
        }
        
        totalWriteSize = totalWriteSize + wSize;
        
        writeMsgBuf->rOffset = writeMsgBuf->rOffset + wSize;
        
        if (writeMsgBuf->rOffset == writeMsgBuf->wOffset) {
            if (writeMsgBuf->next != NULL) {
                ecBlockServerEnginePtr->writeMsgBuf = ecBlockServerEnginePtr->writeMsgBuf->next;
                freeMessageBuf(writeMsgBuf);
            }else{
                writeMsgBuf->rOffset = 0;
                writeMsgBuf->wOffset = 0;
            }
        }
    }
    
    if (ecBlockServerEnginePtr->metaSockState == SERVER_SOCK_STATE_READ_WRITE) {
        removeMetaWriteEvent(ecBlockServerEnginePtr);
    }
    
    return totalWriteSize;
}

void processRecoveryNodes(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    printf("recvd recovery nodes cmd\n");
    //            printf("cmd:%s", ecBlockServerEnginePtr->readMsgBuf->buf);
    
    if (ecBlockServerEnginePtr->pprMasterWorkerPtr == NULL) {
        ecBlockServerEnginePtr->pprMasterWorkerPtr = createPPRMasterWorker(ecBlockServerEnginePtr->readMsgBuf->buf, (void*) ecBlockServerEnginePtr);
        writeRecoveryNodesOKReplyCmd(ecBlockServerEnginePtr->pprMasterWorkerPtr->eSetIdx , (void *)ecBlockServerEnginePtr);
    }else{
        char str1[]="ESetIdx:", str2[]="\r\n\0";
        int setIdx = getIntValueBetweenStrings(str1, str2, ecBlockServerEnginePtr->readMsgBuf->buf);
        writeRecoveryNodesFailedReplyCmd(setIdx, ecBlockServerEnginePtr);
        printf("Error ecBlockServerEnginePtr->ecRecoveryHandlePtr != NULL");
    }
    
    processMetaOutMsg(ecBlockServerEnginePtr);
}

void processRecoveryNodesOver(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    char str1[]="ESetIdx:", str2[]="\r\n\0";
    int setIdx = getIntValueBetweenStrings(str1, str2, ecBlockServerEnginePtr->readMsgBuf->buf);
    
    if (ecBlockServerEnginePtr->pprMasterWorkerPtr == NULL) {
        printf("Error:We don't have ecRecoveryHandlePtr\n");
        return;
    }
    
    if (ecBlockServerEnginePtr->pprMasterWorkerPtr->eSetIdx != setIdx) {
        printf("Error: ecBlockServerEnginePtr->ecRecoveryHandlePtr->eSetIdx:%d != setIdx:%d\n",ecBlockServerEnginePtr->pprMasterWorkerPtr->eSetIdx, setIdx);
        return;
    }
    
    deallocPPRMasterWorker(ecBlockServerEnginePtr->pprMasterWorkerPtr);
    
    ecBlockServerEnginePtr->pprMasterWorkerPtr = NULL;
    return;
}

void processDeleteFile(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    char str1[]="BlockId:\0", str2[]="\r\n\0";
    uint64_t blockId = getUInt64ValueBetweenStrs(str1, str2, 
                        ecBlockServerEnginePtr->readMsgBuf->buf ,ecBlockServerEnginePtr->readMsgBuf->wOffset);
    int ret = deleteFile(blockId, ecBlockServerEnginePtr->diskIOMgr);

    if (ret == 0)
    {
        writeDeleteOKCmd(blockId, ecBlockServerEnginePtr);
    }else{
        writeDeleteFailedCmd(blockId, ecBlockServerEnginePtr);
    }

    processMetaOutMsg(ecBlockServerEnginePtr);
}

void processMetaCmd(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    if ((canParseMetaCmd(ecBlockServerEnginePtr->readMsgBuf->buf,
                         ecBlockServerEnginePtr->readMsgBuf->wOffset) == 0)) {
        return;
    }
    
    MetaServerCommand_t revCmd = parseMetaCommand(ecBlockServerEnginePtr->readMsgBuf->buf,
                                                        ecBlockServerEnginePtr->readMsgBuf->wOffset);
    
    switch (revCmd) {
        case MetaServerCommand_HeartBeat:
//            printf("Recvd heartbeat\n");
            addWriteMsgToMetaServer(ecBlockServerEnginePtr, (char *)HeartBeatReply, strlen(HeartBeatReply) + 1);
            processMetaOutMsg(ecBlockServerEnginePtr);
        break;
        case MetaServerCommand_RecoverNodes:
            processRecoveryNodes(ecBlockServerEnginePtr);
        break;
        case MetaServerCommand_RecoverNodesOver:{
            printf("Recvd recover over\n");
            processRecoveryNodesOver(ecBlockServerEnginePtr);
        }
        break;
        case MetaServerCommand_RecoverBlock:{
            if (ecBlockServerEnginePtr->pprMasterWorkerPtr == NULL) {
                printf("Error: ecBlockServerEnginePtr->pprMasterWorkerPtr = NULL\n");
            }
            
            if(initPPRTask(ecBlockServerEnginePtr->pprMasterWorkerPtr, ecBlockServerEnginePtr->readMsgBuf->buf) != 0){
                printf("failed to init recovery task\n");
            }
        }
        break;
        case MetaServerCommand_DeleteBlock:{
            processDeleteFile(ecBlockServerEnginePtr);
        }
        break;
        default:
            break;
    }
    
    dumpMetaMsgWithSuffix(ecBlockServerEnginePtr->readMsgBuf, '\0');
    
    if (canParseMetaCmd(ecBlockServerEnginePtr->readMsgBuf->buf,
                        ecBlockServerEnginePtr->readMsgBuf->wOffset ) == 1) {
        processMetaCmd(ecBlockServerEnginePtr);
    }
}

ssize_t processMetaInMsg(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    ssize_t recvdSize = 0, bufFullFlag = 0;
    recvdSize = recv(ecBlockServerEnginePtr->metaFd, 
                    ecBlockServerEnginePtr->readMsgBuf->buf + ecBlockServerEnginePtr->readMsgBuf->wOffset, 
                    (ecBlockServerEnginePtr->readMsgBuf->bufSize - ecBlockServerEnginePtr->readMsgBuf->wOffset), 0);
    
    if (recvdSize <= 0) {
        return 0;
    }
    
    ecBlockServerEnginePtr->readMsgBuf->wOffset = ecBlockServerEnginePtr->readMsgBuf->wOffset + recvdSize;
    
    if (ecBlockServerEnginePtr->readMsgBuf->wOffset == ecBlockServerEnginePtr->readMsgBuf->bufSize) {
        bufFullFlag = 1;
    }
    
    if (canParseMetaCmd(ecBlockServerEnginePtr->readMsgBuf->buf, ecBlockServerEnginePtr->readMsgBuf->wOffset) == 1) {
        processMetaCmd(ecBlockServerEnginePtr);
    }
    
    if (bufFullFlag == 1) {
        return recvdSize + processMetaInMsg(ecBlockServerEnginePtr);
    }
    
    return recvdSize;
}


void *metaWorkingThread(void *arg){
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)arg;
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ecBlockServerEnginePtr->metaServerIP);
    
    if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
        printf("Error on convert meta IP addr\n");
        printf("meta server IP:%s\n", ecBlockServerEnginePtr->metaServerIP);
    }
    
    serv_addr.sin_port = htons(ecBlockServerEnginePtr->metaServerPort);

    ecBlockServerEnginePtr->metaSockState = SERVER_SOCK_STATE_UNREGISTERED;
    
    if (connect(ecBlockServerEnginePtr->metaFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Connect error to meta server");
        return NULL;
    }
    
    printf("Connected to meta server\n");
    
    ecBlockServerEnginePtr->eMetafd = create_epollfd();

    struct epoll_event* events;
    
    int status = add_event(ecBlockServerEnginePtr->eMetafd, EPOLLIN | EPOLLET, ecBlockServerEnginePtr->metaFd, NULL);
    
    ecBlockServerEnginePtr->metaSockState = SERVER_SOCK_STATE_READ;
    
    if(status == -1)
    {
        perror("epoll_ctl metaEvent");
        return NULL;
    }
    
    status = add_event(ecBlockServerEnginePtr->eMetafd, EPOLLIN | EPOLLET, ecBlockServerEnginePtr->serverRecoveryPipes[0], NULL);
    
    if(status == -1)
    {
        perror("epoll_ctl serverRecovery Event");
        return NULL;
    }

    int idx, eventsNum;

    events = talloc(struct epoll_event, MAX_CONN_EVENTS);

    while (ecBlockServerEnginePtr->exitFlag == 0) {
        eventsNum = epoll_wait(ecBlockServerEnginePtr->eMetafd, events, MAX_EVENTS,DEFAULT_META_TIME_OUT_IN_MS);
        
        if (eventsNum  <= 0 ) {
            //            printf("Time out\n");
            continue;
        }
        
        for (idx = 0 ; idx < eventsNum; ++idx) {
            if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)||
               (!(events[idx].events & EPOLLIN) && !(events[idx].events & EPOLLOUT)))
            {
                printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
                fprintf(stderr,"epoll error：%d\n",events[idx].events);
                if (ecBlockServerEnginePtr->metaFd == events[idx].data.fd) {
                    delete_event(ecBlockServerEnginePtr->eMetafd, events[idx].data.fd);
                    perror("error on the meta fd, have to close\n");
                }
                
                close(events[idx].data.fd);
                break;
                
            }else if(ecBlockServerEnginePtr->metaFd == events[idx].data.fd){
                /*Notification on the client*/
                if (events[idx].events & EPOLLIN) {
                    processMetaInMsg(ecBlockServerEnginePtr);
                }else if(events[idx].events & EPOLLOUT){
                    processMetaOutMsg(ecBlockServerEnginePtr);
                }else{
                    printf("Detected event not EPOLLIN not EPOLLOUT\n");
                }
                
            }else if (ecBlockServerEnginePtr->serverRecoveryPipes[0] == events[idx].data.fd){
                dry_pipe_msg(ecBlockServerEnginePtr->serverRecoveryPipes[0]);
                if (ecBlockServerEnginePtr->pprMasterWorkerPtr->blockGroupIdx != 0 && ecBlockServerEnginePtr->pprMasterWorkerPtr->curBlockDone == 1) {
                    writeRecoveryBlockOverCmd(ecBlockServerEnginePtr->pprMasterWorkerPtr->eSetIdx, ecBlockServerEnginePtr->pprMasterWorkerPtr->blockGroupIdx, (void *)ecBlockServerEnginePtr);
                    processMetaOutMsg(ecBlockServerEnginePtr);
                    
                    ecBlockServerEnginePtr->pprMasterWorkerPtr->blockGroupIdx = 0;
                }

            }
            else {
                printf("Detected fd for communicating with metaserver\n");
            }
            
        }
    }
    
    
    return (void *)ecBlockServerEnginePtr;
}

void startMetaHandle(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    pthread_t pid;
    
    pthread_create(&pid, NULL, metaWorkingThread, (void *)ecBlockServerEnginePtr);
}