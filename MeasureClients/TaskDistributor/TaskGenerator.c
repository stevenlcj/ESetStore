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
#include "MeanVarCal.h"

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

void enqueueDoneTask(TaskManager_t *mgr, Task_t *theTask){
    if (mgr->doneTasksNum == 0) {
        mgr->taskDoneHeader = theTask;
        mgr->taskDoneTail = theTask;
    }else{
        mgr->taskDoneTail->next = theTask;
        mgr->taskDoneTail = theTask;
    }
    
    ++mgr->doneTasksNum;
}

Task_t *dequeueDoneTask(TaskManager_t *mgr){
    Task_t *theTask = NULL;
    
    if (mgr->doneTasksNum == 0) {
        return NULL;
    }
    
    theTask = mgr->taskDoneHeader;
    mgr->taskDoneHeader = mgr->taskDoneHeader->next;
    --mgr->doneTasksNum;
    
    if (mgr->doneTasksNum == 0 ) {
        mgr->taskDoneHeader = NULL;
        mgr->taskDoneTail = NULL;
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
    
    mgr->doneTasksNum = 0;
    mgr->taskDoneHeader = NULL;
    mgr->taskDoneTail = NULL;
    
    return mgr;
}

TaskManager_t *generateTasks(int startIdx, int endIdx, int taskType){
    
    if (taskType == 0) {
        return generateESetStoreTasks(startIdx, endIdx);
    }
    
    return generateCephTasks(startIdx, endIdx);
}

void calThroughPut(TaskManager_t *mgr){
    mgr->totalFileSize = 0;
    mgr->totalTimeConsume = getTimeIntervalInMS(mgr->taskDoneHeader->startTime, mgr->taskDoneTail->endTime);
    mgr->tasksFileSize = talloc(int, mgr->doneTasksNum);
    mgr->tasksRunTimeConsumes = talloc(double, mgr->doneTasksNum);
    mgr->tasksRunTimeThroughput = talloc(double, mgr->doneTasksNum);
    mgr->tasksDoneTimeConsumes = talloc(double, mgr->doneTasksNum);
    mgr->tasksDoneTimeThroughput = talloc(double, mgr->doneTasksNum);
    
    Task_t *taskPtr = mgr->taskDoneHeader;
    int idx = 0;
    for (idx = 0; idx < mgr->doneTasksNum; ++idx) {
        *(mgr->tasksFileSize+idx)  = taskPtr->taskSize;
        *(mgr->tasksRunTimeConsumes + idx) = getTimeIntervalInMS(taskPtr->runnerStartTime, taskPtr->runnerEndTime);
        *(mgr->tasksRunTimeThroughput + idx) = (double) taskPtr->taskSize * 1000.0 / *(mgr->tasksRunTimeConsumes + idx)/1024.0/1024.0;
        *(mgr->tasksDoneTimeConsumes + idx) = getTimeIntervalInMS(taskPtr->startTime, taskPtr->endTime);
        *(mgr->tasksDoneTimeThroughput + idx) = (double) taskPtr->taskSize * 1000.0 / *(mgr->tasksDoneTimeConsumes + idx)/1024.0/1024.0;
        
        mgr->totalFileSize = mgr->totalFileSize + (uint64_t)taskPtr->taskSize;
        taskPtr = taskPtr->next;
    }
    
    mgr->totalThroughput = (double)mgr->totalFileSize* 1000.0/mgr->totalTimeConsume/1024.0/1024.0;
    
    mgr->runTimeMeanThroughput = calMeanValue(mgr->tasksRunTimeThroughput, mgr->doneTasksNum);
    mgr->runTimethroughputVariance = calVarValue(mgr->tasksRunTimeThroughput, mgr->doneTasksNum);
    
    mgr->doneTimeMeanThroughput = calMeanValue(mgr->tasksDoneTimeThroughput, mgr->doneTasksNum);
    mgr->doneTimethroughputVariance = calVarValue(mgr->tasksDoneTimeThroughput, mgr->doneTasksNum);

    printf("Total Time Consume:%fms, Total File Size:%fMB, Total Throughput:%fMB/s\n",mgr->totalTimeConsume, (double)mgr->totalFileSize/1024.0/1024.0, mgr->totalThroughput);
    printf("Run Time throughput:%fMB/s, Run Throughput variance:%f\n",mgr->runTimeMeanThroughput, mgr->runTimethroughputVariance);
    printf("Done Time throughput:%fMB/s, Done Throughput variance:%f\n",mgr->doneTimeMeanThroughput, mgr->doneTimethroughputVariance);
    
}

