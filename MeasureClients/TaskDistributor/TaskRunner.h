//
//  TaskRunner.h
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/16.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#ifndef TaskRunner_h
#define TaskRunner_h

#include <stdio.h>

#include "TaskListener.h"
#include "TaskGenerator.h"
#include "TaskRunnerInstance.h"

typedef struct TaskRunnerManager{
    TaskManager_t *taskMgr;
    TaskListener_t *taskListener;
    TaskRunnerInstance_t *taskRunnerInstances; //The number of instances is equal to clientNums;
    int clientNums;
    int port;
}TaskRunnerManager_t;

//taskType: 0 is ESetStore, 1 is Ceph
void runTasks(int startIdx, int endIdx, int clientNum, int portToListen, int taskType);

#endif /* TaskRunner_h */
