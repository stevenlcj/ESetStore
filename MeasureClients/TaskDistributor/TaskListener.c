//
//  TaskListener.c
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/20.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#include "TaskListener.h"
#include "TaskDistributorCommon.h"
#include "TaskNetUtilities.h"
#include "MessageBuffer.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


TaskListener_t *createTaskListener(int clientNum, int portToListen){
    TaskListener_t *taskListener = talloc(TaskListener_t, 1);
    taskListener->eFd = create_epollfd();
    taskListener->portToListen = portToListen;
    taskListener->clientNum = clientNum;
    return taskListener;
}


int acceptRunnerInstances(TaskListener_t *taskListener, TaskRunnerInstance_t *runnerInstances){
    
    taskListener->listenFd = create_bind_listen(taskListener->portToListen);
    struct epoll_event* events;
    
    events = talloc(struct epoll_event, MAX_CONN_EVENTS);

    if (events == NULL) {
        perror("unable to alloc memory for events");
        return -1;
    }
    
    add_event(taskListener->eFd, EPOLLIN, taskListener->listenFd, (void *)taskListener);

    
    int acceptedClientNum = 0;
    while (acceptedClientNum != taskListener->clientNum) {
        int eventsNum =epoll_wait(taskListener->eFd, events, MAX_CONN_EVENTS,DEFAULT_ACCEPT_TIME_OUT_IN_MLSEC);
        if (eventsNum  <= 0) {
            //            printf("Time out\n");
            continue;
        }
        
        int idx;
        for (idx = 0 ; idx < eventsNum; ++idx) {
            if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)||
               (!(events[idx].events & EPOLLIN)) && (!(events[idx].events & EPOLLOUT)))
            {
                    printf("EPOLLERR %d EPOLLHUP:%d EPOLLIN:%d EPOLLOUT:%d\n ",EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
                    fprintf(stderr,"epoll error event id:%d\n", events[idx].events);
            }else{
                TaskRunnerInstance_t *taskRunnerInstance = runnerInstances + acceptedClientNum;
                struct sockaddr_in remoteAddr;
                socklen_t nAddrlen = sizeof(remoteAddr);
                taskRunnerInstance->sockFd = accept(taskListener->listenFd, (struct sockaddr *)&remoteAddr, &nAddrlen);
                taskRunnerInstance->IPAddr = inet_ntoa(remoteAddr.sin_addr.s_addr);
                printf("Get Connection From IP:%s\n", taskRunnerInstance->IPAddr);
                
                taskRunnerInstance->connState = CLIENT_CONN_STATE_UNREGISTERED;
                
                taskRunnerInstance->readMsgBuf = createMessageBuf(DEFAULT_MESSAGE_BUFFER_SIZE);
                taskRunnerInstance->writeMsgBuf = createMessageBuf(DEFAULT_MESSAGE_BUFFER_SIZE);
                
                ++acceptedClientNum;
            }
        }

    }
    
}
