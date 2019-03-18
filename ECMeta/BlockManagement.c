//
//  BlockManagement.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/6.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "BlockManagement.h"

#include "ecCommon.h"
#include "ESetPlacement.h"
#include "ecUtils.h"
#include "KeyValueWorker.h"

#include <string.h>
#include <unistd.h>

ECBlockGroupManagement_t *createBlockGroupManager(size_t kSize, size_t mSize, size_t workerThNum){
    ECBlockGroupManagement_t *blockMgr = talloc(ECBlockGroupManagement_t, 1);
    
    blockMgr->kSize = kSize;
    blockMgr->mSize = mSize;
    blockMgr->workerThNum = workerThNum;

    blockMgr->blockIdxForTh = talloc(uint64_t, workerThNum);
    blockMgr->blockGroups = talloc(ECBlockGroup_t *, workerThNum);
    
    blockMgr->blockSlab = createBlockSlab(workerThNum, (kSize + mSize));
    
    uint64_t idx;
    uint64_t blockBaseIdx = 1 << EC_BLOCKGROUP_START_OFFSET;
    for (idx = 0; idx < (uint64_t)workerThNum; ++idx) {
        *(blockMgr->blockIdxForTh+idx) =  blockBaseIdx + (idx << EC_BLOCKGROUP_START_OFFSET);
        blockMgr->blockGroups[idx] = NULL;
    }
        
    return blockMgr;
}

ECBlockGroup_t *requestBlockGroup(ECBlockGroupManagement_t * blockGroupMgr, size_t thIdx){
    
    ECBlockGroup_t *blockGroupPtr;
    
    if (blockGroupMgr->blockGroups[thIdx] != NULL) {

        blockGroupPtr = blockGroupMgr->blockGroups[thIdx];
        
        blockGroupMgr->blockGroups[thIdx] = blockGroupMgr->blockGroups[thIdx]->next;

        return blockGroupPtr;
    }

    //Req block ID, alloc new block group
    blockGroupPtr = (ECBlockGroup_t *) reqABlockGroupMem(blockGroupMgr->blockSlab, thIdx);
    
    blockGroupPtr->blocks = (ECBlock_t *)(blockGroupPtr+1);

    blockGroupPtr->blocksNum = blockGroupMgr->kSize + blockGroupMgr->mSize;
    
    blockGroupPtr->blockGroupId = *(blockGroupMgr->blockIdxForTh+thIdx);

    *(blockGroupMgr->blockIdxForTh+thIdx) = *(blockGroupMgr->blockIdxForTh+thIdx) + (((uint64_t)blockGroupMgr->workerThNum) << EC_BLOCKGROUP_START_OFFSET);

    blockGroupPtr->blockGroupSize = 0;
    
    size_t idx;
    for (idx = 0; idx < blockGroupPtr->blocksNum; ++idx) {

        ECBlock_t *block = blockGroupPtr->blocks + idx;
        block->blockId = blockGroupPtr->blockGroupId + (int64_t)idx;
        block->recoveredFlag = 0;
        block->missingFlag = 0;
        block->blockSize = 0;
    }
    
    return blockGroupPtr;
}

void revokeBlockGroup(ECBlockGroupManagement_t * blockGroupMgr, ECBlockGroup_t *blockGroup, size_t thIdx){
    
    blockGroup->next = blockGroupMgr->blockGroups[thIdx];
    blockGroupMgr->blockGroups[thIdx] = blockGroup;
    
    return;
}

uint64_t getBlockGroupId(uint64_t blockId){
    uint64_t blockGroupId = (blockId >> EC_BLOCKGROUP_START_OFFSET);
    blockGroupId = (blockGroupId << EC_BLOCKGROUP_START_OFFSET);

    printf("blockId:%lu, blockGroupId:%lu\n",blockId, blockGroupId);
    return blockGroupId;
}