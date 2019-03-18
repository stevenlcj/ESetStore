//
//  KeyValueEngine.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__KeyValueEngine__
#define __ECMeta__KeyValueEngine__

#include <stdio.h>
#include <pthread.h>
#include <stdint.h>

#include "KeyValueSlab.h"
#include "KeyValueWorker.h"
#include "ECJob.h"
#include "BlockManagement.h"

typedef struct keyValueItem{
    char *key;
    
    size_t keySize;
    size_t valueSize;
    
    struct keyValueItem *next;
    struct keyValueItem *pre;
    
    ECBlockGroup_t *blockGroupsHeader;
    ECBlockGroup_t *blockGroupsTail;
    size_t blockGroupsNum;
    size_t fileSize;
    
    size_t slabId;
}keyValueItem_t;

typedef struct {
    keyValueItem_t **keyValueChain;
    size_t keyChainSize;
    pthread_mutex_t chainLock;
    
    keyValueSlab_t *keyValueSlab;
    
    KeyValueWorker_t *workers;

    int efd;

    size_t workerNum;
    
    ECBlockGroupManagement_t *blockMgr;
    
    void *metaEnginePtr;
    
}KeyValueEngine_t;

KeyValueEngine_t *createKeyValueEngine(void *metaEnginePtr);
void submitJobToWorker(KeyValueEngine_t *keyValueEnginePtr, KeyValueJob_t *keyValueJob, size_t threadIdx);

keyValueItem_t *locateKeyValueItem(KeyValueEngine_t *keyValueEnginePtr, char *key, size_t keySize, uint32_t hashValue);

void itemIncreFileSize(KeyValueEngine_t *keyValueEnginePtr, keyValueItem_t *item, size_t addedSize);
void revokeItem(KeyValueWorker_t *worker, keyValueItem_t *item);
keyValueItem_t *insertKeyItem(KeyValueWorker_t *worker, KeyValueEngine_t *keyValueEnginePtr, char *key, size_t keySize, uint32_t hashValue);
keyValueItem_t *deleteKeyValueItem(KeyValueEngine_t *keyValueEnginePtr, char *key, size_t keySize, uint32_t hashValue);

#endif /* defined(__ECMeta__KeyValueEngine__) */
