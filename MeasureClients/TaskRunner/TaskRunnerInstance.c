//
//  TaskRunnerInstance.c
//  TaskRunner
//
//  Created by Lau SZ on 2019/12/26.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

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

#include "TaskRunnerInstance.h"
#include "TaskRunnerCommon.h"
#include "TaskNetUtilities.h"


//Wait Task->readTheData->parseTask ->performTask || -> reportTask

int isCmdRecvd(char *buf, size_t bufSize){
    int idx = 0;
    char suffixBuf[] = "\r\n";
    size_t suffixLen = strlen(suffixBuf);
    
    while ((idx+suffixLen) >= bufSize) {
        if(strncmp(suffixBuf, buf + idx, suffixLen) == 0 ){
            return 1;
        }
        ++idx;
    }

    return 0;
}

ssize_t readTheData(TaskRunnerManager_t *taskRunnerMgr){
    ECMessageBuffer_t *rMsgBuf = taskRunnerMgr->readMsgBuffer;
    ssize_t recvdDataSize = readSockData(taskRunnerMgr->sockFd,
                                         rMsgBuf->buf + rMsgBuf->wOffset,
                                         rMsgBuf->bufSize - rMsgBuf->wOffset);
    if (recvdDataSize > 0 ) {
        rMsgBuf->wOffset = rMsgBuf->wOffset + recvdDataSize;
    }
    
    return recvdDataSize;
}

void deallocTaskRunner(TaskRunnerManager_t *taskRunnerMgr){
    
}

void reportTask(TaskRunnerInstance_t *taskRunnerInstance){
    
}

void addDoneTask(TaskRunnerInstance_t *taskRunnerInstance, TaskRunnerTask_t *aTask){
    if (taskRunnerInstance->taskNum == 0) {
        taskRunnerInstance->tasksHeader = aTask;
        taskRunnerInstance->tasksTail = aTask;
    }else{
        taskRunnerInstance->tasksTail->next = aTask;
        taskRunnerInstance->tasksTail = aTask;
    }
    ++taskRunnerInstance->taskNum;
}

void sendTaskDoneCmd(TaskRunnerManager_t *taskRunnerMgr){
    char taskDoneBuf[] = "TaskDone\r\n";

}

void performTask(TaskRunnerManager_t *taskRunnerMgr){
    TaskRunnerTask_t *aTask = talloc(TaskRunnerTask_t, 1);
    memcpy(aTask->taskDescription, taskRunnerMgr->readMsgBuffer->buf, taskRunnerMgr->readMsgBuffer->wOffset);
    aTask->taskDescription[taskRunnerMgr->readMsgBuffer->wOffset - 2] = '\0';
    taskRunnerMgr->readMsgBuffer->wOffset = 0;
    printf("Get task:%s\n",aTask->taskDescription);
    aTask->next = NULL;
    gettimeofday(&aTask->startTime, NULL);
    system(aTask->taskDescription);
    gettimeofday(&aTask->endTime, NULL);
    addDoneTask(taskRunnerMgr->runnerInstance, aTask);
}

void parseTask(TaskRunnerManager_t *taskRunnerMgr){
    if(isCmdRecvd(taskRunnerMgr->readMsgBuffer->buf, taskRunnerMgr->readMsgBuffer->wOffset) == 1){
        //Is Task Done
        char taskReportBuf[] = "TaskReport\r\n";
        if(taskRunnerMgr->taskReportFlag == 0){
            if (strncmp(taskReportBuf, taskRunnerMgr->readMsgBuffer->buf, strlen(taskReportBuf)) == 0) {
                //Task is done
                taskRunnerMgr->taskReportFlag = 1;
                return;
            }else{
                performTask(taskRunnerMgr);
            }
        }
        
    }
}

void waitTask(TaskRunnerManager_t *taskRunnerMgr){
    
    struct epoll_event* events;
    
    events = talloc(struct epoll_event, MAX_CONN_EVENTS);
    
    if (events == NULL) {
        perror("unable to alloc memory for events");
        return -1;
    }
    
    add_event(taskRunnerMgr->efd, EPOLLIN, taskRunnerMgr->sockFd, (void *)taskRunnerMgr);
    
    while (taskRunnerMgr->exitFlag == 0) {
        int eventsNum =epoll_wait(taskRunnerMgr->efd, events, MAX_CONN_EVENTS,DEFAULT_ACCEPT_TIME_OUT_IN_MLSEC);
        int idx;
        for (idx = 0 ; idx < eventsNum; ++idx) {
            if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)||
               (!(events[idx].events & EPOLLIN)) && (!(events[idx].events & EPOLLOUT)))
            {
                printf("EPOLLERR %d EPOLLHUP:%d EPOLLIN:%d EPOLLOUT:%d\n ",EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
                fprintf(stderr,"epoll error event id:%d\n", events[idx].events);
            }else{
                if (events[idx].events & EPOLLIN) {
                    ssize_t readedDataSize = readTheData(taskRunnerMgr);
                    if (readedDataSize == 0) {
                        taskRunnerMgr->exitFlag = 1;
                        break;
                    }
                    parseTask(taskRunnerMgr);
                }else{
                    
                }
            }
        }
    }
    
}

void runTask(char *IPAddr, int port, int taskType){
    TaskRunnerManager_t *taskRunnerMgr = talloc(TaskRunnerManager_t, 1);
    taskRunnerMgr->efd = create_epollfd();
    taskRunnerMgr->sockFd = connectToIPPort(IPAddr, port);
    taskRunnerMgr->taskReportFlag = 0;
    taskRunnerMgr->exitFlag = 0;
    if (taskRunnerMgr->sockFd == -1) {
        printf("Cannot connet to IP:%s, port:%d\n",IPAddr, port);
        close(taskRunnerMgr->efd);
        return;
    }
    
    taskRunnerMgr->taskType = taskType;
    
    taskRunnerMgr->runnerInstance = talloc(TaskRunnerInstance_t, 1);
    memset(taskRunnerMgr->runnerInstance, 0, sizeof(TaskRunnerInstance_t));
    
    taskRunnerMgr->writeMsgBuffer = createMessageBuf(DEFAULT_MESSAGE_BUFFER_SIZE);
    taskRunnerMgr->readMsgBuffer = createMessageBuf(DEFAULT_MESSAGE_BUFFER_SIZE);
    
    waitTask(taskRunnerMgr);
    deallocTaskRunner(taskRunnerMgr);
}
