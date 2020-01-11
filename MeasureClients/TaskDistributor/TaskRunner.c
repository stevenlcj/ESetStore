//
//  TaskRunner.c
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/16.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TaskRunner.h"
#include "TaskDistributorCommon.h"


TaskRunnerManager_t *initTaskRunnerManager(int startIdx, int endIdx, int clientNums, int portToListen, int taskType){
    TaskRunnerManager_t *taskRunnerMgr = talloc(TaskRunnerManager_t, 1);
    taskRunnerMgr->taskMgr = generateTasks(startIdx, endIdx, taskType);
    taskRunnerMgr->taskListener = createTaskListener(clientNums, portToListen);
    
    taskRunnerMgr->taskRunnerInstances = talloc(TaskRunnerInstance_t, clientNums);
    memset(taskRunnerMgr->taskRunnerInstances, 0, clientNums * sizeof(TaskRunnerInstance_t));
    
    taskRunnerMgr->clientNums = clientNums;
    taskRunnerMgr->port = portToListen;
    
    
    
    /*Task_t *theTask = NULL;
    while ((theTask = dequeueTask(taskRunnerMgr->taskMgr)) != NULL) {
        printf("Task:%s\n",theTask->taskStr);
    }*/
    
    return taskRunnerMgr;
}

void runTasks(int startIdx, int endIdx, int clientNums, int portToListen, int taskType){
    
    TaskRunnerManager_t *taskRunnerMgr = initTaskRunnerManager(startIdx, endIdx, clientNums, portToListen, taskType);
    acceptRunnerInstances(taskRunnerMgr->taskListener, taskRunnerMgr->taskRunnerInstances);
    startDistributingTasks(taskRunnerMgr->taskListener, taskRunnerMgr->taskRunnerInstances, taskRunnerMgr->taskMgr);
}
