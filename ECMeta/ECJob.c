//
//  ECJob.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/6.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include <stdio.h>
#include <string.h>

#include "ECJob.h"
#include "ecCommon.h"

KeyValueJobQueue_t *createKeyValueJobQueue(){
    
    KeyValueJobQueue_t *jobQueue = talloc(KeyValueJobQueue_t, 1);
    jobQueue->jobPtr = NULL;
    jobQueue->jobSize = 0;
    
    return jobQueue;
}

int putJob(KeyValueJobQueue_t *jobQueue, KeyValueJob_t *keyValueJob){
    if (keyValueJob == NULL) {
        return -1;
    }
    
    if (jobQueue->jobPtr != NULL) {
        keyValueJob->next = jobQueue->jobPtr;
    }
    
    jobQueue->jobPtr = keyValueJob;
    ++jobQueue->jobSize;

    return 0;
}

KeyValueJob_t *getKeyValueJob(KeyValueJobQueue_t *jobQueue){
    if (jobQueue->jobPtr == NULL) {
        return NULL;
    }
    
    KeyValueJob_t *keyValueJob = jobQueue->jobPtr;
    jobQueue->jobPtr = jobQueue->jobPtr->next;
    --jobQueue->jobSize;
    
    if (jobQueue->jobSize == 0) {
        jobQueue->jobPtr = NULL;
    }
    
    return keyValueJob;
}

void removeTheJobFromQueue(KeyValueJobQueue_t *jobQueue, KeyValueJob_t *keyValueJob){
    size_t idx = 0;
    KeyValueJob_t *jobPtr = jobQueue->jobPtr;

    printf("the key:%s\n",keyValueJob->key);
    printf("key for comp:%s\n",keyValueJob->key);
    if (keyValueJob->keySize == jobPtr->keySize &&
        strncmp(keyValueJob->key, jobPtr->key, keyValueJob->keySize) == 0)
    {
        jobQueue->jobPtr = jobQueue->jobPtr->next;
        --jobQueue->jobSize;
        return;
    }

    for (idx = 1; idx < jobQueue->jobSize; ++idx)
    {
        if (keyValueJob->keySize == jobPtr->next->keySize &&
            strncmp(keyValueJob->key, jobPtr->next->key, keyValueJob->keySize) == 0)
        {
            jobPtr->next = keyValueJob->next;
            --jobQueue->jobSize;
            return;
        }

        jobPtr = jobPtr->next;
    }

    printf("Error:job not found in the queue\n");
}

KeyValueJob_t *createECJob(char *key, size_t keySize, uint32_t hashValue,
                           int threadIdx, int connIdx, KEYVALUEJOB_t jobType, size_t writeSize){
    if (key == NULL || keySize == 0) {
        return NULL;
    }
    
    KeyValueJob_t *keyValueJob = talloc(KeyValueJob_t, 1);
    keyValueJob->key = talloc(char, keySize + 1);
    
    memcpy(keyValueJob->key, key, keySize);
    *(keyValueJob->key + keySize) = '\0';
    keyValueJob->hashValue = hashValue;
    
    keyValueJob->feedbackContent = NULL;
    keyValueJob->feedbackBufSize = 0;
    keyValueJob->feedbackWriteOffset = 0;
    
    keyValueJob->keySize = keySize;
    keyValueJob->clientThreadIdx = threadIdx;
    keyValueJob->clientConnIdx = connIdx;
    
    keyValueJob->jobType = jobType;
    
    keyValueJob->next = NULL;
    
    keyValueJob->writeSize = writeSize;
    
    return keyValueJob;
}

void deallocECJob(KeyValueJob_t *keyValueJob){
    if (keyValueJob->key != NULL) {
        free(keyValueJob->key);
    }
    
    if (keyValueJob->feedbackContent != NULL) {
        free(keyValueJob->feedbackContent);
    }

    free(keyValueJob);
}
