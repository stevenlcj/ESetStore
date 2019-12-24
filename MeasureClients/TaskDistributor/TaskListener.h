//
//  TaskListener.h
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/20.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#ifndef TaskListener_h
#define TaskListener_h

#include <stdio.h>
#include "TaskRunnerInstance.h"

typedef struct TaskListener{
    int eFd;
    int listenFd;
    
    int clientNum;
    int portToListen;
}TaskListener_t;

TaskListener_t *createTaskListener(int clientNum, int portToListen);

int acceptRunnerInstances(TaskListener_t *taskListener, TaskRunnerInstance_t *runnerInstances);
#endif /* TaskListener_h */
