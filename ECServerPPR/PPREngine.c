//
//  PPREngine.c
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/7.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "PPREngine.h"
#include "ecCommon.h"
#include "ecNetHandle.h"
#include "PPRSlaveWorker.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <strings.h>

#define DEFAULT_ACCEPT_TIME_OUT_IN_SEC 100000

void accept_ppr_conn(PPREngine_t *pprEnginePtr){
    //    struct connection *conn = new_connection();
    struct sockaddr_in remoteAddr;
    socklen_t nAddrlen = sizeof(remoteAddr);
    
    int sockFd = accept(pprEnginePtr->listenFd, (struct sockaddr *)&remoteAddr, &nAddrlen);
    printf("Accept ppr conn fd:%d\n", sockFd);
    
    PPRSlaveWorker_t *pprSlaveWorker = createPPRSlaveWorker(sockFd, pprEnginePtr->bufNum, pprEnginePtr->bufSize, pprEnginePtr->serverEnginePtr);
    pthread_create(&pprSlaveWorker->pid, NULL, startPPRSlaveWorker, (void *)pprSlaveWorker);
    
}

PPREngine_t *pprLoop(PPREngine_t *pprEnginePtr){
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)pprEnginePtr->serverEnginePtr;
    
    int eventsNum, idx;
    struct epoll_event* events;
    
    int status = add_event(pprEnginePtr->efd, EPOLLIN | EPOLLET, pprEnginePtr->listenFd, NULL);
    if(status == -1)
    {
        perror("epoll_ctl pprEvent");
        return pprEnginePtr;
    }
    
    events = talloc(struct epoll_event, MAX_EVENTS);
    
    if (events == NULL) {
        perror("unable to alloc memory for events");
        return pprEnginePtr;
    }
    
    printf("start to wait events\n");
    
    while (ecBlockServerEnginePtr->exitFlag == 0) {
        eventsNum = epoll_wait(pprEnginePtr->efd, events, MAX_EVENTS,DEFAULT_ACCEPT_TIME_OUT_IN_SEC);
        
        if (eventsNum  <= 0) {
            //            printf("Time out\n");
            continue;
        }
        
        for (idx = 0 ; idx < eventsNum; ++idx) {
            if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)||
               (!(events[idx].events & EPOLLIN)))
            {
                fprintf(stderr,"ppr epoll error\n");
                break;
            }else{
                accept_ppr_conn(pprEnginePtr);
            }
            
        }
    }
    
    return pprEnginePtr;
}

void *PPRHandler(void *arg){
    PPREngine_t *pprEnginePtr = (PPREngine_t *)arg;
    
    pprEnginePtr->efd = create_epollfd();
    pprEnginePtr->listenFd = create_bind_listen(pprEnginePtr->PPRPort);
    
    pprLoop(pprEnginePtr);
    
    return arg;
}

PPREngine_t *createPPREngine(int PPRPort, int bufNum, size_t bufSize, void *serverEnginePtr){
    PPREngine_t *pprEnginePtr = talloc(PPREngine_t, 1);

    if (pprEnginePtr == NULL) {
        printf("Allock pprEnginePtr Failed\n");
        return NULL;
    }
    
    pprEnginePtr->PPRPort = PPRPort;
    pprEnginePtr->bufNum = bufNum;
    pprEnginePtr->bufSize = bufSize;
    pprEnginePtr->serverEnginePtr = serverEnginePtr;
    
    pthread_create(&pprEnginePtr->pid, NULL, PPRHandler, (void *)pprEnginePtr);
    
    return pprEnginePtr;
}