//
//  BlockSlab.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/6.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "BlockSlab.h"
#include "BlockManagement.h"
#include "ecCommon.h"

#include <stdlib.h>

void allockThreadBlockSlabMem(BlockSlab_t *blockSlab, ThreadBlockSlab_t *thBlockSlab){
    pthread_mutex_lock(&blockSlab->slabLock);
    size_t reqSize = thBlockSlab->eachGroupMemSize * EC_THREADBLOCKSLAB_ALLOC_SIZE;
//    printf("eachGroupMemSize:%lu reqSize:%lu\n",thBlockSlab->eachGroupMemSize, reqSize);
    
    if (blockSlab->remainMemSize < reqSize) {
        size_t newBlockSlabSize = blockSlab->totalMemSize + thBlockSlab->eachGroupMemSize*EC_BLOCKSLAB_INCRE_SIZE;
        blockSlab->memBase = realloc(blockSlab->memBase, newBlockSlabSize);
        
        if(blockSlab->memBase == NULL){
            printf("Realloc mem failed allockThreadBlockSlabMem\n");
            exit(0);
        }
        
        blockSlab->remainMemSize = blockSlab->remainMemSize + thBlockSlab->eachGroupMemSize*EC_BLOCKSLAB_INCRE_SIZE;
    }
    
    thBlockSlab->memBase = blockSlab->memCur;
    thBlockSlab->memCur = thBlockSlab->memBase;
    thBlockSlab->totalMemSize = reqSize;
    thBlockSlab->remainMemSize = thBlockSlab->totalMemSize;
    
    blockSlab->memCur = (void *)(((char*)blockSlab->memCur) + reqSize);
    
    blockSlab->remainMemSize = blockSlab->remainMemSize - reqSize;
    
    pthread_mutex_unlock(&blockSlab->slabLock);
}

BlockSlab_t *createBlockSlab(size_t threadNum, size_t blockNumEachGroup){
    BlockSlab_t *blockSlab = talloc(BlockSlab_t, 1);
    size_t totalSizeEachGroup = sizeof(ECBlockGroup_t) + blockNumEachGroup * sizeof(ECBlock_t);
    blockSlab->memBase = talloc(char, totalSizeEachGroup * EC_BLOCKSLAB_INCRE_SIZE);
    blockSlab->memCur = blockSlab->memBase;
    
    blockSlab->blockSize = blockNumEachGroup;
    
    blockSlab->totalMemSize = totalSizeEachGroup * EC_BLOCKSLAB_INCRE_SIZE;
    blockSlab->remainMemSize = totalSizeEachGroup * EC_BLOCKSLAB_INCRE_SIZE;
    blockSlab->threadNum = threadNum;
    
    blockSlab->threadBlockSlab = talloc(ThreadBlockSlab_t, threadNum);
    
    pthread_mutex_init(&blockSlab->slabLock, NULL);
    
    int idx;
    for (idx = 0; idx < threadNum; ++idx) {
        ThreadBlockSlab_t *thBlockSlab = blockSlab->threadBlockSlab + idx;
        thBlockSlab->blockSize = blockNumEachGroup;
        thBlockSlab->eachGroupMemSize = totalSizeEachGroup;
        allockThreadBlockSlabMem(blockSlab, thBlockSlab);
    }
    
    return blockSlab;
}

void *reqABlockGroupMem(BlockSlab_t *blockSlab, size_t threadIdx){
    ThreadBlockSlab_t *thBlockSlab = blockSlab->threadBlockSlab + threadIdx;
    
    if (thBlockSlab->remainMemSize < thBlockSlab->eachGroupMemSize) {
        allockThreadBlockSlabMem(blockSlab, thBlockSlab);
    }
    
    void *memPtr = thBlockSlab->memCur;
    thBlockSlab->memCur = (void *)((char *)thBlockSlab->memCur + thBlockSlab->eachGroupMemSize);
    thBlockSlab->remainMemSize = thBlockSlab->remainMemSize - thBlockSlab->eachGroupMemSize;
    
    return memPtr;
}