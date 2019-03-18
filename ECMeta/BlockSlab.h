//
//  BlockSlab.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/6.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__BlockSlab__
#define __ECMeta__BlockSlab__

#include <stdio.h>
#include <pthread.h>

typedef struct {
    void *memBase;
    void *memCur;
    size_t totalMemSize;
    size_t remainMemSize;
    size_t blockSize;
    size_t eachGroupMemSize;
}ThreadBlockSlab_t;

typedef struct {
    void *memBase;
    void *memCur;
    size_t totalMemSize;
    size_t remainMemSize;
    size_t blockSize;
    size_t threadNum;
    
    ThreadBlockSlab_t *threadBlockSlab;
    pthread_mutex_t slabLock;
}BlockSlab_t;

BlockSlab_t *createBlockSlab(size_t threadNum, size_t blockNumEachGroup);
void *reqABlockGroupMem(BlockSlab_t *blockSlab, size_t threadIdx);
#endif /* defined(__ECMeta__BlockSlab__) */
