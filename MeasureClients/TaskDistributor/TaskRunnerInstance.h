//
//  TaskRunnerInstance.h
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/20.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#ifndef TaskRunnerInstance_h
#define TaskRunnerInstance_h

#include <stdio.h>
#include "MessageBuffer.h"
#include "TaskGenerator.h"

typedef enum {
    CLIENT_CONN_STATE_UNREGISTERED = 0x01,
    CLIENT_CONN_STATE_READ,
    CLIENT_CONN_STATE_WRITE,
    CLIENT_CONN_STATE_READWRITE,
    CLIENT_CONN_STATE_PENDINGCLOSE,
    CLIENT_CONN_STATE_CLOSE
}CLIENT_CONN_STATE;

typedef struct TaskRunnerInstance{
    int sockFd;
    int sendedTask;
    char *IPAddr;
    CLIENT_CONN_STATE connState;

    ECMessageBuffer_t *readMsgBuf;
    ECMessageBuffer_t *writeMsgBuf;
    
    Task_t *curTask;

}TaskRunnerInstance_t;

void prepareSendingTask(TaskRunnerInstance_t *taskRunnerInstance);

int canParseRunnerInMsg(TaskRunnerInstance_t *taskRunnerInstance);
void parseRunnerInMsg(TaskRunnerInstance_t *taskRunnerInstance);
#endif /* TaskRunnerInstance_h */
