//
//  KeyValueSlab.c
//  ECStoreMeta
//
//  Created by Liu Chengjian on 17/7/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "KeyValueSlab.h"
#include "ecCommon.h"

#include <pthread.h>

size_t calMaxSlabId(size_t baseSize, float powerFactor, size_t maxSize){
    
    if (maxSize < baseSize) {
        return 0;
    }
    
    float minSize = (float)baseSize, maxSizeInFloat = (float)maxSize;
    size_t id = 0;
    
    do {
        minSize = minSize * powerFactor;
        ++id;
        
    }while (minSize < maxSizeInFloat);
    
    return id;
}

size_t calSlabSize(size_t baseSize, float powerFactor, size_t slabID){
    if (slabID <= 0) {
        return 0;
    }
    
    float slabSize = (float)baseSize;
    while ((--slabID) != 0) {
        slabSize = slabSize * powerFactor;
    }
    
    return (size_t)slabSize;
}

size_t calSlabID(size_t baseSize, float powerFactor, size_t maxSize, size_t slabSize){
    
    if (slabSize > maxSize) {
        return 0;
    }
    
    size_t slabID = 1;
    float curSlabSize = baseSize , slabSizeInFloat = (float) slabSize;
    while (curSlabSize < slabSizeInFloat) {
        curSlabSize = curSlabSize * powerFactor;
        ++slabID;
    }
    
    return slabID;
}

keyValueSlab_t *createKeyValueSlab(){
    keyValueSlab_t *keyValueSlab = talloc(keyValueSlab_t, 1);
    
    keyValueSlab->mem_base = talloc(char , ECSTORE_ITEM_SIZE_INCRE_SIZE);
    keyValueSlab->mem_current = keyValueSlab->mem_base;
    keyValueSlab->availableNum = ECSTORE_ITEM_SIZE_INCRE_SIZE;
    keyValueSlab->totalNum = ECSTORE_ITEM_SIZE_INCRE_SIZE;
    
    pthread_mutex_init(&(keyValueSlab->lock), NULL);
    
    return keyValueSlab;
}

void allocThreadSlabMem(threadKeyValueSlab *thKeyValueSlab){
    thKeyValueSlab->mem_base = requestKeyValueSlabMem(thKeyValueSlab->keyValueSlab, ECSTORE_ITEM_SIZE_REQ_SIZE);
    thKeyValueSlab->mem_cur = thKeyValueSlab->mem_base;
    thKeyValueSlab->availableMem = ECSTORE_ITEM_SIZE_REQ_SIZE;
}

void distributeThreadSlab(threadKeyValueSlab *thKeyValueSlab, size_t slabID, size_t size){
    size_t slabSize = calSlabSize(ITEM_SLAB_BASE_SIZE, ITEM_SLAB_POWER, slabID);
    size_t reqSize = slabSize * size;
//    printf("slabId:%lu distributeThreadSlab req size:%lu size:%lu \n",slabID, reqSize, size);
    size_t idx = 0;
    if (thKeyValueSlab->availableMem < reqSize) {
        allocThreadSlabMem(thKeyValueSlab);
    }
    
    char *memPtr = thKeyValueSlab->mem_cur;
    thKeyValueSlab->mem_cur = (void *) (memPtr + reqSize);
    
    while (idx < size) {
        slab_item_t *slabItem = (slab_item_t *)memPtr;
        slabItem->next = thKeyValueSlab->slab_items[slabID];
        thKeyValueSlab->slab_items[slabID] = slabItem;
        memPtr = memPtr + slabSize;
        ++idx;
    }
    
    return;
}

void *requestKeyValueSlabMem(keyValueSlab_t *keyValueSlab, size_t reqSize){
    char *reqMem = NULL;
    if (reqSize > ECSTORE_ITEM_SIZE_INCRE_SIZE) {
        return reqMem;
    }
    
    pthread_mutex_lock(&keyValueSlab->lock);
    if (reqSize > keyValueSlab->availableNum) {
        keyValueSlab->mem_base = realloc(keyValueSlab->mem_base, (keyValueSlab->totalNum + ECSTORE_ITEM_SIZE_INCRE_SIZE));
        keyValueSlab->availableNum =  keyValueSlab->availableNum + ECSTORE_ITEM_SIZE_INCRE_SIZE;
        keyValueSlab->totalNum = keyValueSlab->totalNum + ECSTORE_ITEM_SIZE_INCRE_SIZE;
    }
    reqMem = keyValueSlab->mem_current;
    keyValueSlab->mem_current = (void *) (reqMem + reqSize);
    pthread_mutex_unlock(&keyValueSlab->lock);
    
    return (void *)reqMem;
}

threadKeyValueSlab *createThreadKeyValueSlab(keyValueSlab_t *keyValueSlab){
    
    threadKeyValueSlab *thKeyValueSlab = talloc(threadKeyValueSlab, 1);
    thKeyValueSlab->keyValueSlab = keyValueSlab;
    thKeyValueSlab->maxId = calMaxSlabId(ITEM_SLAB_BASE_SIZE, ITEM_SLAB_POWER, ITEM_SLAB_MAX_SIZE);
    
    allocThreadSlabMem(thKeyValueSlab);
    
    thKeyValueSlab->slab_items = talloc(slab_item_t *, thKeyValueSlab->maxId);
    size_t idx;
    
    for (idx = 0; idx < thKeyValueSlab->maxId ; ++idx) {
        thKeyValueSlab->slab_items[idx] = NULL;
        distributeThreadSlab(thKeyValueSlab, idx, ITEM_ALLOC_SIZE);
    }
    
    return thKeyValueSlab;
}

size_t getSlabID(size_t itemSize){
    return calSlabID(ITEM_SLAB_BASE_SIZE, ITEM_SLAB_POWER, ITEM_SLAB_MAX_SIZE, itemSize);
}

size_t getSlabSize(size_t slabID){
    return calSlabSize(ITEM_SLAB_BASE_SIZE, ITEM_SLAB_POWER, slabID);
}

void *getSlabMemByID(threadKeyValueSlab *thKeyValueSlab, size_t slabID){
    if (thKeyValueSlab->slab_items[slabID] == NULL){
        distributeThreadSlab(thKeyValueSlab, slabID, ITEM_ALLOC_SIZE);
    }
    
    slab_item_t *slabItem = thKeyValueSlab->slab_items[slabID];
    thKeyValueSlab->slab_items[slabID] = slabItem->next;
    return (void *)slabItem;
}

void revokeSlabMemByID(threadKeyValueSlab *thKeyValueSlab, size_t slabID, void *memToRevoke){
    slab_item_t *slabItem = (slab_item_t *)memToRevoke;
    slabItem->next = thKeyValueSlab->slab_items[slabID];
    thKeyValueSlab->slab_items[slabID] = slabItem;
}