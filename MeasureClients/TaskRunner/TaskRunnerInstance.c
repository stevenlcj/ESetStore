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
#include "strUtils.h"


void addWriteEvent(TaskRunnerManager_t *taskRunnerMgr){
    if (taskRunnerMgr->connState == CLIENT_CONN_STATE_READWRITE)
    {
        return;
    }
    
    taskRunnerMgr->connState = CLIENT_CONN_STATE_READWRITE;
    update_event(taskRunnerMgr->efd, EPOLLIN | EPOLLOUT , taskRunnerMgr->sockFd, taskRunnerMgr);
}

void removeWriteEvent(TaskRunnerManager_t *taskRunnerMgr){
    if (taskRunnerMgr->connState != CLIENT_CONN_STATE_READWRITE)
    {
        return;
    }
    
    update_event(taskRunnerMgr->efd, EPOLLIN , taskRunnerMgr->sockFd, taskRunnerMgr);
    taskRunnerMgr->connState = CLIENT_CONN_STATE_READ;
}


int writeDataToDistributor(TaskRunnerManager_t *taskRunnerMgr){
    ECMessageBuffer_t *writeMsgBuf = taskRunnerMgr->writeMsgBuf;
    
    while (taskRunnerMgr->writeMsgBuf->rOffset != taskRunnerMgr->writeMsgBuf->wOffset ) {
        
        ssize_t wSize = send(taskRunnerMgr->sockFd,  writeMsgBuf->buf+ writeMsgBuf->rOffset, (writeMsgBuf->wOffset-writeMsgBuf->rOffset), 0);
        
        //printf("wOffset:%lu, rOffset:%lu send size:%ld to client\n",ecClientPtr->writeMsgBuf->wOffset, ecClientPtr->writeMsgBuf->rOffset, wSize);
        
        if (wSize <= 0) {
            addWriteEvent(taskRunnerMgr);
            return 1;
        }
        
        //*writeSize = *writeSize + wSize;
        
        writeMsgBuf->rOffset = writeMsgBuf->rOffset + wSize;
        
        if (writeMsgBuf->rOffset == writeMsgBuf->wOffset) {
            writeMsgBuf->rOffset = 0;
            writeMsgBuf->wOffset = 0;
        }
    }
    
    if (taskRunnerMgr->connState == CLIENT_CONN_STATE_READWRITE) {
        removeWriteEvent(taskRunnerMgr);
    }
    return 0;
}

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

void constructTaskDoneCmd(TaskRunnerManager_t *taskRunnerMgr, TaskRunnerTask_t *theTask){
    char startSecStr[] = "StartSec:";
    char sufCh = '\r';
    char startUSecStr[] = "StartUSec:";
    char endSecStr[] = "EndSec:";
    char endUSecStr[] = "EndUSec:";
    char fileSizeStr[] = "FileSize:";
    char suffixStr[] = "\r\n";
    
    char tempStr[1024];
    
    ECMessageBuffer_t *writeMsgBuffer= taskRunnerMgr->writeMsgBuffer;
    
    size_t writeSize;
    writeToBuf(writeMsgBuffer->buf, startSecStr, &writeSize, &writeMsgBuffer->wOffset);
    uint64_to_str((uint64_t)theTask->startTime.tv_sec, tempStr, 1024);
    writeToBuf(writeMsgBuffer->buf+writeMsgBuffer->wOffset, tempStr, &writeSize, &writeMsgBuffer->wOffset);
    writeMsgBuffer->buf[writeMsgBuffer->wOffset] = sufCh;
    writeMsgBuffer->wOffset = writeMsgBuffer->wOffset + 1;
    
    writeToBuf(writeMsgBuffer->buf, startUSecStr, &writeSize, &writeMsgBuffer->wOffset);
    uint64_to_str((uint64_t)theTask->startTime.tv_usec, tempStr, 1024);
    writeToBuf(writeMsgBuffer->buf+writeMsgBuffer->wOffset, tempStr, &writeSize, &writeMsgBuffer->wOffset);
    writeMsgBuffer->buf[writeMsgBuffer->wOffset] = sufCh;
    writeMsgBuffer->wOffset = writeMsgBuffer->wOffset + 1;

    writeToBuf(writeMsgBuffer->buf, endSecStr, &writeSize, &writeMsgBuffer->wOffset);
    uint64_to_str((uint64_t)theTask->endTime.tv_sec, tempStr, 1024);
    writeToBuf(writeMsgBuffer->buf+writeMsgBuffer->wOffset, tempStr, &writeSize, &writeMsgBuffer->wOffset);
    writeMsgBuffer->buf[writeMsgBuffer->wOffset] = sufCh;
    writeMsgBuffer->wOffset = writeMsgBuffer->wOffset + 1;

    writeToBuf(writeMsgBuffer->buf, endUSecStr, &writeSize, &writeMsgBuffer->wOffset);
    uint64_to_str((uint64_t)theTask->endTime.tv_usec, tempStr, 1024);
    writeToBuf(writeMsgBuffer->buf+writeMsgBuffer->wOffset, tempStr, &writeSize, &writeMsgBuffer->wOffset);
    writeMsgBuffer->buf[writeMsgBuffer->wOffset] = sufCh;
    writeMsgBuffer->wOffset = writeMsgBuffer->wOffset + 1;
    
    writeToBuf(writeMsgBuffer->buf, fileSizeStr, &writeSize, &writeMsgBuffer->wOffset);
    uint64_to_str((uint64_t)taskRunnerMgr->fileSize, tempStr, 1024);
    writeToBuf(writeMsgBuffer->buf+writeMsgBuffer->wOffset, tempStr, &writeSize, &writeMsgBuffer->wOffset);
    writeToBuf(writeMsgBuffer->buf+writeMsgBuffer->wOffset, suffixStr, &writeSize, &writeMsgBuffer->wOffset);
    writeMsgBuffer->buf[writeMsgBuffer->wOffset] = '\0';
    printf("Msg to write:%s\n",writeMsgBuffer->buf);
    
}

//Send cmd:"StartSec:...\rStartUSec:...\rEndSec:...\rEndUSec:...\rFileSize:\r\n"
void reportTask(TaskRunnerManager_t *taskRunnerMgr, TaskRunnerTask_t *theTask){
    constructTaskDoneCmd(taskRunnerMgr, theTask);
    writeDataToDistributor(taskRunnerMgr);
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
    reportTask(taskRunnerMgr, aTask);
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
        return ;
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
                        printf("Revd 0, exit\n");
                        taskRunnerMgr->exitFlag = 1;
                        break;
                    }
                    parseTask(taskRunnerMgr);
                }else{
                    
                }
            }
        }
    }
    
    delete_event(taskRunnerMgr->efd, taskRunnerMgr->sockFd);
    close(taskRunnerMgr->sockFd);
}

void runTask(char *IPAddr, int port, int taskType, int fileSize){
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
    taskRunnerMgr->fileSize = fileSize
    
    taskRunnerMgr->runnerInstance = talloc(TaskRunnerInstance_t, 1);
    memset(taskRunnerMgr->runnerInstance, 0, sizeof(TaskRunnerInstance_t));
    
    taskRunnerMgr->writeMsgBuffer = createMessageBuf(DEFAULT_MESSAGE_BUFFER_SIZE);
    taskRunnerMgr->readMsgBuffer = createMessageBuf(DEFAULT_MESSAGE_BUFFER_SIZE);
    
    waitTask(taskRunnerMgr);
    deallocTaskRunner(taskRunnerMgr);
}
