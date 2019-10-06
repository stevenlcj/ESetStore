#ifndef ThreadWorker_h
#define ThreadWorker_h

#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>

typedef struct ThreadJob{
        struct timeval startTime;
            struct timeval endTime;
                char *jobStr;
                    struct ThreadJob *next;
}ThreadJob_t;

typedef struct {
        int curJobNum;
            pthread_mutex_t qMutex;
                ThreadJob_t *jobQueueHeader;
                    ThreadJob_t *jobQueueTail;
}ThreadJobQueue_t;

typedef struct{
        pthread_t workerPid;
            int workerIdx;
}ThreadWorker_t;

typedef struct{
        int threadWorkerNum;
            ThreadWorker_t *threadWorkers;
}ThreadMgr_t;

void startThreadMgr(int startIdx, int endIdx, int threadNum);
#endif /* ThreadWorker_h */
