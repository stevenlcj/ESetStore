//
//  TaskRunnerInstance.h
//  TaskRunner
//
//  Created by Lau SZ on 2019/12/26.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#ifndef TaskRunnerInstance_h
#define TaskRunnerInstance_h

#include <stdio.h>
#include <sys/time.h>

#include "MessageBuffer.h"

typedef struct TaskRunnerTask{
    char taskDescription[1024];
    struct timeval startTime;
    struct timeval endTime;
    struct TaskRunnerTask *next;
}TaskRunnerTask_t;

typedef struct TaskRunnerInstance{
    TaskRunnerTask_t *tasksHeader;
    TaskRunnerTask_t *tasksTail;
    int taskNum;
}TaskRunnerInstance_t;

typedef enum {
    CLIENT_CONN_STATE_UNREGISTERED = 0x01,
    CLIENT_CONN_STATE_READ,
    CLIENT_CONN_STATE_WRITE,
    CLIENT_CONN_STATE_READWRITE,
    CLIENT_CONN_STATE_PENDINGCLOSE
}CLIENT_CONN_STATE;


typedef struct TaskRunnerManager{
    int efd;
    int sockFd;
    int taskType;
    int taskReportFlag;
    int exitFlag;
    
    CLIENT_CONN_STATE connState;
    
    TaskRunnerInstance_t *runnerInstance;
    
    ECMessageBuffer_t *writeMsgBuffer;
    ECMessageBuffer_t *readMsgBuffer;
}TaskRunnerManager_t;

void runTask(char *IPAddr, int port, int taskType);

#endif /* TaskRunnerInstance_h */
