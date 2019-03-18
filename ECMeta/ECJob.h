//
//  ECJob.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef ECMeta_ECJob_h
#define ECMeta_ECJob_h

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

typedef enum {
    KEYVALUEJOB_CREATE = 0x01,
    KEYVALUEJOB_OPEN,
    KEYVALUEJOB_READ,
    KEYVALUEJOB_APPEND,
    KEYVALUEJOB_WRITE,
    KEYVALUEJOB_WRITESIZE,
    KEYVALUEJOB_DELETE,
    KEYVALUEJOB_DELETE_DONE
}KEYVALUEJOB_t;

typedef struct KeyValueJob{
    char *key;
    size_t keySize;
    
    char *feedbackContent;
    size_t feedbackBufSize;
    size_t feedbackWriteOffset;
    
    int workerIdx;

    int clientThreadIdx;
    int clientConnIdx;
    
    uint32_t hashValue;
    
    KEYVALUEJOB_t jobType;
    struct KeyValueJob *next;
    
    size_t writeSize;

    int inProcessingNum; //Marks the number of blocks to be deleted by servers

    void *itemPtr;

    struct timeval clientInTime;
    struct timeval workerInTime;
    struct timeval serverInTime;
    struct timeval serverOutTime;
    struct timeval workerOutTime;
}KeyValueJob_t;

typedef struct {
    KeyValueJob_t *jobPtr;
    
    size_t jobSize;
}KeyValueJobQueue_t;

KeyValueJobQueue_t *createKeyValueJobQueue();
int putJob(KeyValueJobQueue_t *jobQueue, KeyValueJob_t *keyValueJob);
KeyValueJob_t *getKeyValueJob(KeyValueJobQueue_t *jobQueue);
void removeTheJobFromQueue(KeyValueJobQueue_t *jobQueue, KeyValueJob_t *keyValueJob);

KeyValueJob_t *createECJob(char *key, size_t keySize, uint32_t hashValue,
                           int threadIdx, int connIdx, KEYVALUEJOB_t jobType, size_t writeSize);

void deallocECJob(KeyValueJob_t *keyValueJob);
#endif
