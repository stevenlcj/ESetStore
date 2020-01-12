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
    printf("Idx:%d, Sending task:%s\n",taskRunnerInstance->idx, taskRunnerInstance->writeMsgBuf->buf);
    taskRunnerInstance->writeMsgBuf->wOffset = strlen(taskRunnerInstance->writeMsgBuf->buf);
    taskRunnerInstance->sendedTask  = 0;
}

int canParseRunnerInMsg(TaskRunnerInstance_t *taskRunnerInstance){
    char suffixStr[] = "\r\n";
    int idx = 0;
    size_t strLen = strlen(suffixStr);
    ECMessageBuffer_t *curMsgBuf = taskRunnerInstance->readMsgBuf;
    
    while (curMsgBuf->wOffset >= (idx + strLen)) {
        if (strncmp(curMsgBuf->buf + idx, suffixStr, strLen) == 0) {
            curMsgBuf->buf[curMsgBuf->wOffset] = '\0';
            return 1;
        }
        ++idx;
    }
    
    return 0;
}

//"StartSec:...StartUSec:...EndSec:...\rEndUSec:...\rFileSize:...\r\n"
void parseRunnerInMsg(TaskRunnerInstance_t *taskRunnerInstance){
    char suffixStr[] = "\r\n";
    char startSecStr[] = "StartSec:";
    char startUSecStr[] = "StartUSec:";
    char endSecStr[] = "EndSec:";
    char endUSecStr[] = "EndUSec:";
    char fileSizeStr[] = "FileSize:";
    
    gettimeofday(&taskRunnerInstance->curTask->endTime, NULL);

    ECMessageBuffer_t *curMsgBuf = taskRunnerInstance->readMsgBuf;
    printf("Get:%s\n",curMsgBuf->buf);
    
    taskRunnerInstance->curTask->runnerStartTime.tv_sec = getLongValueBetweenStrs(startSecStr, startUSecStr,curMsgBuf->buf);
    taskRunnerInstance->curTask->runnerStartTime.tv_usec = getLongValueBetweenStrs(startUSecStr, endSecStr,curMsgBuf->buf);
    taskRunnerInstance->curTask->runnerEndTime.tv_sec = getLongValueBetweenStrs(endSecStr, endUSecStr,curMsgBuf->buf);
    taskRunnerInstance->curTask->runnerEndTime.tv_usec = getLongValueBetweenStrs(endUSecStr, fileSizeStr,curMsgBuf->buf);
    taskRunnerInstance->curTask->taskSize = getLongValueBetweenStrs(fileSizeStr, suffixStr,curMsgBuf->buf);
    
    curMsgBuf->wOffset = 0;
}

