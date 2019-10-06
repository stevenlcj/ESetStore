#include "ThreadWorker.h"
#include <stdlib.h>
#include <string.h>
#include "MeanVarCal.h"

#define talloc(type,num) (type *)malloc(sizeof(type)*num)
#define kValue 3
pthread_mutex_t pMutex;
pthread_cond_t pCond;

ThreadJobQueue_t *threadJobQueue;
ThreadJobQueue_t *finishedJobQueue;

ThreadJobQueue_t *initThreadJobQueue(){
    ThreadJobQueue_t *tJobQueue=talloc(ThreadJobQueue_t, 1);
    pthread_mutex_init(&(tJobQueue->qMutex), NULL);
    tJobQueue->curJobNum = 0;
    tJobQueue->jobQueueHeader = NULL;
    tJobQueue->jobQueueTail = NULL;
    return tJobQueue;
}

int enqueueThreadJob(ThreadJobQueue_t *tJobQueue, ThreadJob_t *tJob){
    int curJobSize = 0;
    pthread_mutex_lock(&tJobQueue->qMutex);
    if (tJobQueue->curJobNum == 0) {
        tJobQueue->jobQueueHeader = tJob;
        tJobQueue->jobQueueTail = tJob;
    }else{
        tJobQueue->jobQueueTail->next = tJob;
        tJobQueue->jobQueueTail = tJob;
    }
    ++tJobQueue->curJobNum;
    
    curJobSize = tJobQueue->curJobNum;
    pthread_mutex_unlock(&tJobQueue->qMutex);
    
    return curJobSize;
}

ThreadJob_t *dequeueThreadJob(ThreadJobQueue_t *tJobQueue){
    ThreadJob_t *tJob = NULL;
    pthread_mutex_lock(&tJobQueue->qMutex);
    if (tJobQueue->curJobNum == 0) {
        pthread_mutex_unlock(&tJobQueue->qMutex);
        return NULL;
    }
    
    tJob = tJobQueue->jobQueueHeader;
    tJobQueue->jobQueueHeader = tJobQueue->jobQueueHeader->next;
    --tJobQueue->curJobNum;

    if (tJobQueue->curJobNum == 0 ) {
        tJobQueue->jobQueueHeader = NULL;
        tJobQueue->jobQueueTail = NULL;
    }
    pthread_mutex_unlock(&tJobQueue->qMutex);
    return tJob;
}

ThreadJob_t *createThreadJob(char *jobStr){
    ThreadJob_t *tJob = talloc(ThreadJob_t, 1);
    tJob->jobStr = jobStr;
    tJob->next = NULL;
    return tJob;
}

size_t numToStr(char *str, int num){
    char tempStr[1024];
    size_t cmdSize =0;
    int idx;
    while (num > 0) {
        tempStr[cmdSize] = '0' + num%10;
        num = num / 10;
        ++cmdSize;
    }
    
    for (idx=0; idx < cmdSize; ++idx) {
        *(str + idx ) = tempStr[cmdSize - idx - 1];
    }
    
    return  cmdSize;
}

void calThroughput(int workerIdx, int jobSize, ThreadJobQueue_t *theJobQueue){
    double *throughPutArray = timeConsume(jobSize);
    
    int idx = 0;
    struct timeval startTime, endTime;
    ThreadJob_t *tJob= theJobQueue->jobQueueHeader;
    startTime.tv_sec = tJob->startTime.tv_sec;
    startTime.tv_usec = tJob->startTime.tv_usec;
    endTime.tv_sec = tJob->endTime.tv_sec;
    endTime.tv_usec = tJob->endTime.tv_usec;

    while (idx != jobSize) {
        throughPutArray[idx] = (kValue*4.0 * 1000.0/getTimeIntervalInMS(tJob->startTime,tJob->endTime));
        endTime.tv_sec = tJob->endTime.tv_sec;
        endTime.tv_usec = tJob->endTime.tv_usec;
        tJob = tJob->next;
        ++idx;
    }
    
    double totalTime = getTimeIntervalInMS(startTime, endTime);
    double totalThroughput = (double)jobSize * kValue*4.0 * 1000.0 / totalTime;
    double meanValue = calMeanValue(throughPutArray, jobSize);
    double varValue = calVarValue(throughPutArray, jobSize);
    
    printf("Worker:%d,JobSize:%d,TotalTime:%fms,TotalThrougput:%fMB/s, Mean value:%fMB/s Var value:%fMB/s\n",workerIdx, jobSize,totalTime,totalThroughput, meanValue,varValue);
}

void *threadFun(void *arg){
    ThreadWorker_t *curWorker = (ThreadWorker_t *)arg;
    int workerJobNum =0;
    pthread_mutex_lock(&pMutex);
    pthread_cond_wait(&pCond, &pMutex);
    pthread_mutex_unlock(&pMutex);
    int curJobSize = 0;
    while(1){
        ThreadJob_t *tJob = dequeueThreadJob(threadJobQueue);
        if (tJob == NULL) {
            break;
        }
        ++workerJobNum;
        gettimeofday(&tJob->startTime, NULL);
        printf("Execute:%s\n",tJob->jobStr);
        system(tJob->jobStr);
        gettimeofday(&tJob->endTime, NULL);
        
        curJobSize = enqueueThreadJob(finishedJobQueue, tJob);
    }
    
    calThroughput(curWorker->workerIdx, curJobSize, finishedJobQueue);
    
    return curWorker;
}

ThreadMgr_t *createThreadMgr(int threadNum){
    int idx;
    ThreadMgr_t *threadMgr = talloc(ThreadMgr_t, 1);
    threadMgr->threadWorkerNum = threadNum;
    threadMgr->threadWorkers = talloc(ThreadWorker_t, threadNum);
    for (idx = 0; idx < threadNum; ++idx) {
        ThreadWorker_t *curWorker = threadMgr->threadWorkers + idx;
        curWorker->workerIdx = idx;
        pthread_create(&curWorker->workerPid, NULL, threadFun, (void *)curWorker);
    }
    
    return threadMgr;
}

void waitJobFinish(ThreadMgr_t *threadMgr){
    int idx;
    for (idx = 0; idx < threadMgr->threadWorkerNum; ++idx) {
        ThreadWorker_t *curWorker = threadMgr->threadWorkers + idx;
        pthread_join(curWorker->workerPid, NULL);
    }
}

void startThreadMgr(int startIdx, int endIdx, int threadNum){
    int idx;
    char cmdStr[]="ECClientFS -get file\0";
    size_t cmdStrLen = strlen(cmdStr);
    size_t cmdLen = cmdStrLen + 5;
    
    threadJobQueue = initThreadJobQueue();
    finishedJobQueue = initThreadJobQueue();
    
    pthread_mutex_init(&pMutex, NULL);
    pthread_cond_init(&pCond, NULL);
    ThreadMgr_t *threadMgr = createThreadMgr(threadNum);
    
    for (idx = startIdx; idx <= endIdx; ++idx) {
        size_t curCmdLen = 0;
        char *curCmd = talloc(char, cmdLen);
        memset(curCmd, 0, cmdLen);
        memcpy(curCmd, cmdStr, cmdStrLen);
        curCmdLen = cmdStrLen;
        curCmdLen = curCmdLen + numToStr(curCmd + curCmdLen , idx);
        *(curCmd + curCmdLen) = '\0';
        ThreadJob_t *tJob = createThreadJob(curCmd);
        enqueueThreadJob(threadJobQueue, tJob);
    }
    
    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);
    pthread_cond_broadcast(&pCond);
    
    waitJobFinish(threadMgr);
    gettimeofday(&endTime, NULL);

    double *throughPutArray = timeConsume(endIdx - startIdx + 1);
    
    for (idx = startIdx; idx <= endIdx; ++idx){
        ThreadJob_t *tJob = dequeueThreadJob(finishedJobQueue);
        if (tJob == NULL) {
            printf("Job only reach:%d\n",idx);
            break;
        }
        throughPutArray[idx - startIdx] = (kValue*4.0 * 1000.0/getTimeIntervalInMS(tJob->startTime,tJob->endTime));
        //printf("TimeInterval:%fms, Throughput:%fMB/s\n",getTimeIntervalInMS(tJob->startTime,tJob->endTime), throughPutArray[idx - startIdx]);
    }
    
    double totalTime = getTimeIntervalInMS(startTime, endTime);
    double meanValue = calMeanValue(throughPutArray, endIdx - startIdx + 1);
    double varValue = calVarValue(throughPutArray, endIdx - startIdx + 1);
    
    printf("TotalTime:%fms, Mean value:%fMB/s Var value:%fMB/s\n",totalTime, meanValue,varValue);
}






