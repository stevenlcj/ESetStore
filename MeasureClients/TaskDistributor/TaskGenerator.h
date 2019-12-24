//
//  TaskGenerator.h
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/16.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#ifndef TaskGenerator_h
#define TaskGenerator_h

#include <stdio.h>
#include <pthread.h>

typedef struct theTask{
    char *taskStr;
    int taskSize;
    struct theTask *next;
}Task_t;

typedef struct theTaskManager{
    int taskNum;
    Task_t *taskHeader;
    Task_t *taskTail;
}TaskManager_t;

TaskManager_t *generateTasks(int startIdx, int endIdx, int taskType);
void enqueueTask(TaskManager_t *mgr, Task_t *theTask);
Task_t *dequeueTask(TaskManager_t *mgr);
#endif /* TaskGenerator_h */
