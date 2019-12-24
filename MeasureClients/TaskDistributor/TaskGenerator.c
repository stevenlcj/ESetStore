//
//  TaskGenerator.c
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/16.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#include "TaskGenerator.h"
#include "TaskDistributorCommon.h"
#include "TaskDistributorUtilities.h"

#include <stdlib.h>
#include <string.h>

void enqueueTask(TaskManager_t *mgr, Task_t *theTask){
    if (mgr->taskNum == 0) {
        mgr->taskHeader = theTask;
        mgr->taskTail = theTask;
    }else{
        mgr->taskTail->next = theTask;
        mgr->taskTail = theTask;
    }
    
    ++mgr->taskNum;
}

Task_t *dequeueTask(TaskManager_t *mgr){
    Task_t *theTask = NULL;
    
    if (mgr->taskNum == 0) {
        return NULL;
    }
    
    theTask = mgr->taskHeader;
    mgr->taskHeader = mgr->taskHeader->next;
    --mgr->taskNum;
    
    if (mgr->taskNum == 0 ) {
        mgr->taskHeader = NULL;
        mgr->taskTail = NULL;

    }
    
    return theTask;
}

Task_t *createTask(char *taskStr){
    Task_t *tTask = talloc(Task_t, 1);
    tTask->taskStr = taskStr;
    tTask->next = NULL;
    return tTask;
}

TaskManager_t *generateESetStoreTasks(int startIdx, int endIdx){
    TaskManager_t *mgr = talloc(TaskManager_t, 1);
    mgr->taskNum = 0;
    mgr->taskHeader = NULL;
    mgr->taskTail = NULL;
    
    char cmdStr[]="ECClientFS -get file\0";
    size_t cmdStrLen = strlen(cmdStr);
    size_t cmdLen = cmdStrLen + 5;
    
    int idx;
    for (idx = startIdx; idx <= endIdx; ++idx) {
        size_t curCmdLen = 0;
        char *curCmd = talloc(char, cmdLen);
        memset(curCmd, 0, cmdLen);
        memcpy(curCmd, cmdStr, cmdStrLen);
        curCmdLen = cmdStrLen;
        curCmdLen = curCmdLen + numToStr(curCmd + curCmdLen , idx);
        *(curCmd + curCmdLen) = '\0';
        Task_t *tTask = createTask(curCmd);
        enqueueTask(mgr, tTask);
    }
    
    return mgr;
}

TaskManager_t *generateCephTasks(int startIdx, int endIdx){
    TaskManager_t *mgr = talloc(TaskManager_t, 1);
    mgr->taskNum = 0;
    mgr->taskHeader = NULL;
    mgr->taskTail = NULL;
    
    return mgr;
}

TaskManager_t *generateTasks(int startIdx, int endIdx, int taskType){
    
    if (taskType == 0) {
        return generateESetStoreTasks(startIdx, endIdx);
    }
    
    return generateCephTasks(startIdx, endIdx);
}


