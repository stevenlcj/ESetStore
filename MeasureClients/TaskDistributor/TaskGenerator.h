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
#include <sys/time.h>
#include <stdint.h>

typedef struct theTask{
    char *taskStr;
    int taskSize;
    struct theTask *next;
    
    struct timeval startTime;
    struct timeval endTime;
    
    struct timeval runnerStartTime;
    struct timeval runnerEndTime;
    
}Task_t;

typedef struct theTaskManager{
    int taskNum;
    Task_t *taskHeader;
    Task_t *taskTail;
    
    int doneTasksNum;
    Task_t *taskDoneHeader;
    Task_t *taskDoneTail;
    
    
    uint64_t totalFileSize;
    double totalTimeConsume;
    double totalThroughput;
    
    int *tasksFileSize;
    double *tasksRunTimeConsumes; //Time between runnerStartTime and runnerEndTime
    double *tasksRunTimeThroughput;
    double *tasksDoneTimeConsumes; //Time between startTime and endTime
    double *tasksDoneTimeThroughput;
    
    double runTimeMeanThroughput;
    double runTimethroughputVariance;
    
    double doneTimeMeanThroughput;
    double doneTimethroughputVariance;

}TaskManager_t;

TaskManager_t *generateTasks(int startIdx, int endIdx, int taskType);
void enqueueTask(TaskManager_t *mgr, Task_t *theTask);
Task_t *dequeueTask(TaskManager_t *mgr);

void enqueueDoneTask(TaskManager_t *mgr, Task_t *theTask);
Task_t *dequeueDoneTask(TaskManager_t *mgr);


void calThroughPut(TaskManager_t *mgr);
#endif /* TaskGenerator_h */
