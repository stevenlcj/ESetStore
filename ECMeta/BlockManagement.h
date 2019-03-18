//
//  BlockManagement.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/6.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__BlockManagement__
#define __ECMeta__BlockManagement__

#include <stdio.h>
#include <stdint.h>

#include "BlockSlab.h"

typedef struct {
    uint64_t blockId;
    int serverId;
    int rackId;
    
    int missingFlag;
    int recoveredFlag;
    
    size_t blockSize;
}ECBlock_t;

typedef struct ECBlockGroup{
    uint64_t blockGroupId;
    size_t blocksNum;
    size_t blockGroupSize;
    ECBlock_t *blocks;
    struct ECBlockGroup *next;
    
    size_t setIdx;
}ECBlockGroup_t;

typedef struct {
    size_t kSize;
    size_t mSize;
    
    size_t workerThNum;
    uint64_t *blockIdxForTh;
    
    ECBlockGroup_t **blockGroups;
    
    BlockSlab_t *blockSlab;
}ECBlockGroupManagement_t;

ECBlockGroupManagement_t *createBlockGroupManager(size_t kSize, size_t mSize, size_t workerThNum);
ECBlockGroup_t *requestBlockGroup(ECBlockGroupManagement_t * blockGroupMgr, size_t thIdxs);
void revokeBlockGroup(ECBlockGroupManagement_t * blockGroupMgr, ECBlockGroup_t *blockGroup, size_t thIdx);

uint64_t getBlockGroupId(uint64_t blockId);
#endif /* defined(__ECMeta__BlockManagement__) */
