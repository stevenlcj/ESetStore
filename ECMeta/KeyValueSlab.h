//
//  KeyValueSlab.h
//  ECStoreMeta
//
//  Created by Liu Chengjian on 17/7/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECStoreMeta__KeyValueSlab__
#define __ECStoreMeta__KeyValueSlab__

#include <stdio.h>
#include <pthread.h>

typedef struct {
    void *mem_base;
    void *mem_current;
    size_t availableNum;
    size_t totalNum;
    
    /**
     * Access to the slab allocator is protected by this lock
     */
    pthread_mutex_t lock;
}keyValueSlab_t;

typedef struct slab_item{
    struct slab_item *next;
}slab_item_t;

typedef struct {
    void *mem_base;
    void *mem_cur;
    size_t availableMem;
    size_t maxId;
    slab_item_t **slab_items;
    
    keyValueSlab_t *keyValueSlab;
}threadKeyValueSlab;

keyValueSlab_t *createKeyValueSlab();
void *requestKeyValueSlabMem(keyValueSlab_t *keyValueSlab, size_t reqSize);
threadKeyValueSlab *createThreadKeyValueSlab(keyValueSlab_t *keyValueSlab);

size_t getSlabID(size_t itemSize);
size_t getSlabSize(size_t slabID);
void *getSlabMemByID(threadKeyValueSlab *thKeyValueSlab, size_t slabID);
void revokeSlabMemByID(threadKeyValueSlab *thKeyValueSlab, size_t slabID, void *memToRevoke);
#endif /* defined(__ECStoreMeta__KeyValueSlab__) */
