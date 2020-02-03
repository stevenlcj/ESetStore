//
//  ClientHandle.c
//  ECServer
//
//  Created by Liu Chengjian on 17/10/3.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ClientHandle.h"
#include "ECClientHandle.h"
#include "ecCommon.h"
#include "ecNetHandle.h"
#include "BlockServerEngine.h"
#include "ClientBlockServerCmd.h"
#include "ecUtils.h"
#include "MessageBuffer.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>

#define DEFAULT_ACCEPT_TIME_OUT_IN_SEC 100000

void *startManagingClients(void *arg);

ECClient_t *newECClient(int sockFd){
    ECClient_t *ecClientPtr = talloc(ECClient_t, 1);
    
    ecClientPtr->sockFd = sockFd;
    ecClientPtr->pre = NULL;
    ecClientPtr->next = NULL;
    ecClientPtr->clientInState = ECCLIENT_IN_STATE_WAIT;
    ecClientPtr->connState = CLIENT_CONN_STATE_UNREGISTERED;
    ecClientPtr->readMsgBuf = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);
    ecClientPtr->writeMsgBuf = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);

    return ecClientPtr;
}

void deallocECClient(ECClient_t *ecClientPtr){
    while (ecClientPtr->readMsgBuf != NULL) {
        ECMessageBuffer_t *curBuf = ecClientPtr->readMsgBuf;
        ecClientPtr->readMsgBuf = ecClientPtr->readMsgBuf->next;
        freeMessageBuf(curBuf);
    }
    
    while (ecClientPtr->writeMsgBuf != NULL) {
        ECMessageBuffer_t *curBuf = ecClientPtr->writeMsgBuf;
        ecClientPtr->writeMsgBuf = ecClientPtr->writeMsgBuf->next;
        freeMessageBuf(curBuf);
    }
    
    free(ecClientPtr);
}

ECClientManager_t *createClientManager(void *serverEnginePtr){
    ECClientManager_t *ecClientMgr = talloc(ECClientManager_t, 1);
    
    ecClientMgr->efd = create_epollfd();
    
    pthread_mutex_init(&ecClientMgr->lock, NULL);
    
    ecClientMgr->comingClients = NULL;
    ecClientMgr->monitoringClients = NULL;
    
    ecClientMgr->serverEnginePtr = serverEnginePtr;
    ecClientMgr->waitingClientsFlag = 0;
    
    if(pipe(ecClientMgr->clientMgrPipes) != 0){
        perror("pipe(ecClientMgr->clientMgrPipes) failed\n");
    }

    make_non_blocking_sock(ecClientMgr->clientMgrPipes[0]);
    
    ecClientMgr->clientPipe = talloc(ECClient_t, 1);
    ecClientMgr->clientPipe->sockFd = ecClientMgr->clientMgrPipes[0];
    ecClientMgr->clientPipe->next = NULL;
    ecClientMgr->clientPipe->clientInState = ECCLIENT_IN_STATE_WAIT;
    
    ecClientMgr->monitoringClientsNum = 0;

    ecClientMgr->totalRecv = 0;
    ecClientMgr->totalDump = 0;
    ecClientMgr->totalSend = 0;
    
    ecClientMgr->comingJobQueue = createDiskJobQueue();
    ecClientMgr->handlingJobQueue = createDiskJobQueue();
    
    return ecClientMgr;
}

void addComingECClient(ECClientManager_t *ecClientMgr, ECClient_t * ecClientPtr){
    pthread_mutex_lock(&ecClientMgr->lock);
    
    ecClientPtr->next = ecClientMgr->comingClients;
    ecClientMgr->comingClients = ecClientPtr;
    
    char ch = 'C';
    write(ecClientMgr->clientMgrPipes[1], (void *)&ch, 1);
    
    pthread_mutex_unlock(&ecClientMgr->lock);
}


void startECClientManager(ECClientManager_t *ecClientMgr){
    pthread_t tid;
    
    pthread_create(&tid, NULL, startManagingClients, (void *) ecClientMgr);
}

void printRemainingClients(ECClientManager_t *ecClientMgr){
        ECClient_t *ecClientPtr = ecClientMgr->monitoringClients;
            while (ecClientPtr != NULL) {
                        printf("Client blockId:%llu reqSize:%lu, handledSize:%lu\n", ecClientPtr->blockId
                                               ,ecClientPtr->reqSize, ecClientPtr->handledSize);
                                ecClientPtr = ecClientPtr->next;
        }
}

void removeClient(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
    printf("Close client sock:%d blockId:%llu\n", ecClientPtr->sockFd,ecClientPtr->blockId);
    printRemainingClients(ecClientMgr);
    
    if (ecClientPtr->connState == CLIENT_CONN_STATE_READ ||
        ecClientPtr->connState ==  CLIENT_CONN_STATE_WRITE ||
        ecClientPtr->connState == CLIENT_CONN_STATE_READWRITE) {
        delete_event(ecClientMgr->efd, ecClientPtr->sockFd);
    }
    
    close(ecClientPtr->sockFd);

    if (ecClientMgr->monitoringClients == ecClientPtr) {
        
        if (ecClientMgr->monitoringClients->next != NULL) {
            ecClientMgr->monitoringClients->next->pre = NULL;
        }
        
        ecClientMgr->monitoringClients = ecClientMgr->monitoringClients->next;
    }else{
        ecClientPtr->pre->next = ecClientPtr->next;
        if (ecClientPtr->next != NULL) {
            ecClientPtr->next->pre = ecClientPtr->pre;
        }
    }

    deallocECClient(ecClientPtr);
}

void monitoringNewClient(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
    ecClientPtr->pre= NULL;
    ecClientPtr->next= NULL;
    
    if (ecClientMgr->monitoringClients != NULL) {
        ecClientPtr->next = ecClientMgr->monitoringClients;
        ecClientMgr->monitoringClients->pre = ecClientPtr;
    }
    
    ecClientMgr->monitoringClients = ecClientPtr;
    
    printf("Add new client sock:%d\n", ecClientPtr->sockFd);
    
    add_event(ecClientMgr->efd, EPOLLIN , ecClientPtr->sockFd, (void *) ecClientPtr);
    
    ecClientPtr->clientInState = ECCLIENT_IN_STATE_WAIT;
    ecClientPtr->connState = CLIENT_CONN_STATE_READ;
    return;
}

void addNewClients(ECClientManager_t *ecClientMgr){
    while (ecClientMgr->comingClients != NULL) {
        ECClient_t *ecClientPtr = ecClientMgr->comingClients;
        ecClientMgr->comingClients = ecClientMgr->comingClients->next;
        
        monitoringNewClient(ecClientMgr, ecClientPtr);
    }
}

void handleDiskIOResponse(ECClientManager_t *ecClientMgr){
    DiskJob_t *diskJobPtr = NULL;
    
    while ((diskJobPtr = dequeueDiskJob(ecClientMgr->comingJobQueue)) != NULL) {
        //printf("Recv disk io response\n");
        enqueueDiskJob(ecClientMgr->handlingJobQueue, diskJobPtr);
    }
}

void startMonitoringClients(ECClientManager_t *ecClientMgr){
    
    ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)ecClientMgr->serverEnginePtr;
    
    int idx, eventsNum;
    struct epoll_event* events;
    
    events = talloc(struct epoll_event, MAX_CONN_EVENTS);
    
    if (events == NULL) {
        perror("unable to alloc memory for events");
        return;
    }
    
    add_event(ecClientMgr->efd, EPOLLIN, ecClientMgr->clientPipe->sockFd, (void *)ecClientMgr->clientPipe);
    
    while (blockServerEnginePtr->exitFlag == 0) {
        eventsNum =epoll_wait(ecClientMgr->efd, events, MAX_EVENTS,DEFAULT_ACCEPT_TIME_OUT_IN_SEC);
        if (eventsNum  <= 0) {
            //            printf("Time out\n");
            continue;
        }

        for (idx = 0 ; idx < eventsNum; ++idx) {
            if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)||
               (!(events[idx].events & EPOLLIN)) && (!(events[idx].events & EPOLLOUT)))
            {
//                printf("EPOLLERR %d EPOLLHUP:%d EPOLLIN:%d EPOLLOUT:%d\n ",EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
//                fprintf(stderr,"epoll error event id:%d\n", events[idx].events);
                ECClient_t *curClient = (ECClient_t *) events[idx].data.ptr;

                perror("error on the client fd, have to close\n");
                removeClient(ecClientMgr, curClient);
                continue;
                
            }else {
                /*Notification on the client*/
                ECClient_t *curClient = (ECClient_t *) events[idx].data.ptr;
                
                if (curClient->connState == CLIENT_CONN_STATE_PENDINGCLOSE) {
                    removeClient(ecClientMgr, curClient);
                    continue;
                }
                
                if (curClient == ecClientMgr->clientPipe) {
                    //Msg from pipe, thread has work to do
                    pthread_mutex_lock(&ecClientMgr->lock);
                    
                    dry_pipe_msg(ecClientMgr->clientPipe->sockFd);
                    addNewClients(ecClientMgr);
                    handleDiskIOResponse(ecClientMgr);
                    pthread_mutex_unlock(&ecClientMgr->lock);
                    
                    writeClientReadContent(ecClientMgr);
                    
                }else{
                    if ((events[idx].events & EPOLLIN)) {
//                        printf("EPOLLIN from client\n");
                        size_t readSize = 0;
                        int ret = 0;
                        do{
                            ret = processClientInMsg(ecClientMgr, curClient, &readSize);
                            processClientInCmd(ecClientMgr, curClient);
                        }while(ret == 1);
                        
                        if (readSize == 0 || curClient->clientInState == ECCLIENT_IN_STATE_CLOSE) {
                            removeClient(ecClientMgr, curClient);
                        }
                    }else{
//                        printf("EPOLLOUT from client\n");
                        size_t writeSize = 0;
                        processClientOutMsg(ecClientMgr, curClient, &writeSize);
                    }
                }
            }
        }
    }
}

void *startManagingClients(void *arg){
    if (arg == NULL) {
        printf("startManagingClients arg is NULL\n");
        return NULL;
    }
    
    ECClientManager_t *ecClientMgr = (ECClientManager_t *) arg;
    
    startMonitoringClients(ecClientMgr);
    
    
    return arg;
}












