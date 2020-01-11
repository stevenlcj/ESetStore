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

void addWriteEvent(TaskListener_t *taskListener, TaskRunnerInstance_t *runnerInstance){
    if (runnerInstance->connState == CLIENT_CONN_STATE_READWRITE)
    {
        return;
    }
    
    runnerInstance->connState = CLIENT_CONN_STATE_READWRITE;
    update_event(taskListener->eFd, EPOLLIN | EPOLLOUT , runnerInstance->sockFd, taskListener);
}

void removeWriteEvent(TaskListener_t *taskListener, TaskRunnerInstance_t *runnerInstance){
    if (runnerInstance->connState != CLIENT_CONN_STATE_READWRITE)
    {
        return;
    }
    
    update_event(taskListener->eFd, EPOLLIN , runnerInstance->sockFd, taskListener);
    runnerInstance->connState = CLIENT_CONN_STATE_READ;
}


int writeDataToRunner(TaskListener_t *taskListener, TaskRunnerInstance_t *runnerInstance){
    ECMessageBuffer_t *writeMsgBuf = runnerInstance->writeMsgBuf;
    
    while (runnerInstance->writeMsgBuf->rOffset != runnerInstance->writeMsgBuf->wOffset ) {
        
        ssize_t wSize = send(runnerInstance->sockFd,  writeMsgBuf->buf+ writeMsgBuf->rOffset, (writeMsgBuf->wOffset-writeMsgBuf->rOffset), 0);
        
        //printf("wOffset:%lu, rOffset:%lu send size:%ld to client\n",ecClientPtr->writeMsgBuf->wOffset, ecClientPtr->writeMsgBuf->rOffset, wSize);
        
        if (wSize <= 0) {
            addWriteEvent(taskListener, runnerInstance);
            return 1;
        }
        
        //*writeSize = *writeSize + wSize;
        
        writeMsgBuf->rOffset = writeMsgBuf->rOffset + wSize;
        
        if (writeMsgBuf->rOffset == writeMsgBuf->wOffset) {
            writeMsgBuf->rOffset = 0;
            writeMsgBuf->wOffset = 0;
            runnerInstance->sendedTask = 1;
        }
    }
    
    if (runnerInstance->connState == CLIENT_CONN_STATE_READWRITE) {
        removeWriteEvent(taskListener, runnerInstance);
    }
    return 0;
}

int recvDataFromRunner(TaskListener_t *taskListener, TaskRunnerInstance_t *runnerInstance,int *readSize){
    ECMessageBuffer_t *rMsgBuf = runnerInstance->readMsgBuf;
    ssize_t recvdSize = recv(runnerInstance->sockFd, (void *)(rMsgBuf->buf + rMsgBuf->wOffset), (rMsgBuf->bufSize-rMsgBuf->wOffset), 0);
    
    //printf("Recvd %ld bytes from client\n", recvdSize);
    
    if (recvdSize < 0)
    {
        return (int ) recvdSize;
    }
    
    if (recvdSize >= 0)
    {
        *readSize = *readSize + (size_t)recvdSize;
        rMsgBuf->wOffset = rMsgBuf->wOffset + (size_t)recvdSize;
    }
    
    if (rMsgBuf->wOffset == rMsgBuf->bufSize) {
        return 1;
    }
    
    return 0;
}


TaskListener_t *createTaskListener(int clientNum, int portToListen){
    TaskListener_t *taskListener = talloc(TaskListener_t, 1);
    taskListener->eFd = create_epollfd();
    taskListener->portToListen = portToListen;
    taskListener->clientNum = clientNum;
    return taskListener;
}

void closeListenSock(TaskListener_t *taskListener){
    delete_event(taskListener->eFd, taskListener->listenFd);
    close(taskListener->listenFd);
    taskListener->listenFd = -1;
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
                taskRunnerInstance->idx = acceptedClientNum;
                
                struct sockaddr_in remoteAddr;
                socklen_t nAddrlen = sizeof(remoteAddr);
                taskRunnerInstance->sockFd = accept(taskListener->listenFd, (struct sockaddr *)&remoteAddr, &nAddrlen);
                taskRunnerInstance->IPAddr = inet_ntoa(remoteAddr.sin_addr);
                printf("Get Connection From IP:%s\n", taskRunnerInstance->IPAddr);
                
                taskRunnerInstance->connState = CLIENT_CONN_STATE_UNREGISTERED;
                
                taskRunnerInstance->readMsgBuf = createMessageBuf(DEFAULT_MESSAGE_BUFFER_SIZE);
                taskRunnerInstance->writeMsgBuf = createMessageBuf(DEFAULT_MESSAGE_BUFFER_SIZE);
                
                make_non_blocking_sock(taskRunnerInstance->sockFd);
                
                ++acceptedClientNum;
            }
        }
    }
    
    closeListenSock(taskListener);
    free(events);
    
    return acceptedClientNum;
}


int processingRunnerInMsg(TaskListener_t *taskListener, TaskRunnerInstance_t *taskRunnerInstance,TaskManager_t *taskMgr){
    int readSize=0;
    recvDataFromRunner(taskListener, taskRunnerInstance, &readSize);
    
    if (readSize == 0) {
        printf("Sock closed from:%s\n",taskRunnerInstance->IPAddr);
        delete_event(taskListener->eFd, taskRunnerInstance->sockFd);
        close(taskRunnerInstance->sockFd);
        taskRunnerInstance->connState = CLIENT_CONN_STATE_CLOSE;
        return 0;
    }else{
        if (canParseRunnerInMsg(taskRunnerInstance)) {
            parseRunnerInMsg(taskRunnerInstance);
            enqueueDoneTask(taskMgr, taskRunnerInstance->curTask);
            
            taskRunnerInstance->curTask = dequeueTask(taskMgr);
            if (taskRunnerInstance->curTask != NULL) {
                prepareSendingTask(taskRunnerInstance);
                writeDataToRunner(taskListener, taskRunnerInstance);
            }
            
            return 1;
        }
    }
    
    return 0;
}

void startDistributingTasks(TaskListener_t *taskListener, TaskRunnerInstance_t *runnerInstances, TaskManager_t *taskMgr){
    
    int idx;
    int totalTasks = taskMgr->taskNum;
    struct epoll_event* events;
    
    events = talloc(struct epoll_event, MAX_CONN_EVENTS);
    
    if (events == NULL) {
        perror("unable to alloc memory for events");
        return ;
    }
    
    for (idx = 0; idx < taskListener->clientNum; ++idx) {
        TaskRunnerInstance_t *taskRunnerInstance = runnerInstances + idx;
        taskRunnerInstance->curTask = NULL;
        add_event(taskListener->eFd, EPOLLIN, taskRunnerInstance->sockFd, (void *)taskRunnerInstance);
        taskRunnerInstance->connState = CLIENT_CONN_STATE_READ;
    }
    
    printf("press enter to start running\n");
    getchar();
    
    for (idx = 0; idx < taskListener->clientNum; ++idx){
        TaskRunnerInstance_t *taskRunnerInstance = runnerInstances + idx;
        taskRunnerInstance->curTask = dequeueTask(taskMgr);
        
        if (taskRunnerInstance->curTask == NULL) {
            break;
        }
    
        prepareSendingTask(taskRunnerInstance);
        writeDataToRunner(taskListener, taskRunnerInstance);
    }

do{
    int eventsNum =epoll_wait(taskListener->eFd, events, MAX_CONN_EVENTS,DEFAULT_ACCEPT_TIME_OUT_IN_MLSEC);

    for (idx = 0 ; idx < eventsNum; ++idx) {
        if((events[idx].events & EPOLLERR)||
           (events[idx].events & EPOLLHUP)||
           (!(events[idx].events & EPOLLIN)) && (!(events[idx].events & EPOLLOUT)))
        {
            printf("EPOLLERR %d EPOLLHUP:%d EPOLLIN:%d EPOLLOUT:%d\n ",EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
            fprintf(stderr,"epoll error event id:%d\n", events[idx].events);
        }else{
            TaskRunnerInstance_t *taskRunnerInstance = (TaskRunnerInstance_t *)events[idx].data.ptr;
            if (events[idx].events & EPOLLOUT) {
                writeDataToRunner(taskListener, taskRunnerInstance);
            }else{
                processingRunnerInMsg(taskListener, taskRunnerInstance,taskMgr);
            }
        }
    }

}while(taskMgr->doneTasksNum != totalTasks);
    
    
    calThroughPut(taskMgr);
}
