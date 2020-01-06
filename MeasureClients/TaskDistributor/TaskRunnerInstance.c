//
//  TaskRunnerInstance.c
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/20.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#include "TaskRunnerInstance.h"
#include <string.h>
#include "strUtils.h"

void prepareSendingTask(TaskRunnerInstance_t *taskRunnerInstance){
    gettimeofday(&taskRunnerInstance->curTask->startTime, NULL);
    sprintf(taskRunnerInstance->writeMsgBuf->buf, "%s\r\n",taskRunnerInstance->curTask->taskStr);
    taskRunnerInstance->writeMsgBuf->wOffset = strlen(taskRunnerInstance->writeMsgBuf->buf);
    taskRunnerInstance->sendedTask  = 0;
}

int canParseRunnerInMsg(TaskRunnerInstance_t *taskRunnerInstance){
    char suffixStr[] = "\r\n";
    int idx = 0;
    size_t strLen = strlen(suffixStr);
    ECMessageBuffer_t *curMsgBuf = taskRunnerInstance->readMsgBuf;
    
    while (curMsgBuf->wOffset > (idx + strLen)) {
        if (strncmp(curMsgBuf->buf + idx, suffixStr, strLen) == 0) {
            curMsgBuf->buf[curMsgBuf->wOffset] = '\0';
            return 1;
        }
        ++idx;
    }
    
    return 0;
}

//"StartSec:...\rStartUSec:...EndSec:...\rEndUSec:...\rFileSize:...\r\n"
void parseRunnerInMsg(TaskRunnerInstance_t *taskRunnerInstance){
    char suffixStr[] = "\r";
    char startSecStr[] = "StartSec:";
    char startUSecStr[] = "StartUSec:";
    char endSecStr[] = "EndSec:";
    char endUSecStr[] = "EndUSec:";
    char fileSizeStr[] = "FileSize:";
    
    gettimeofday(&taskRunnerInstance->curTask->endTime, NULL);

    ECMessageBuffer_t *curMsgBuf = taskRunnerInstance->readMsgBuf;
    printf("Get:%s\n",curMsgBuf->buf);

    taskRunnerInstance->curTask->runnerStartTime.tv_sec = getLongValueBetweenStrs(curMsgBuf->buf, startSecStr, suffixStr);
    taskRunnerInstance->curTask->runnerStartTime.tv_usec = getLongValueBetweenStrs(curMsgBuf->buf, startUSecStr, suffixStr);
    taskRunnerInstance->curTask->runnerEndTime.tv_sec = getLongValueBetweenStrs(curMsgBuf->buf, endSecStr, suffixStr);
    taskRunnerInstance->curTask->runnerEndTime.tv_usec = getLongValueBetweenStrs(curMsgBuf->buf, endUSecStr, suffixStr);
    taskRunnerInstance->curTask->taskSize = getLongValueBetweenStrs(curMsgBuf->buf, fileSizeStr, suffixStr);
}

