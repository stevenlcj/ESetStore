//
//  ESetPlacement.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/8/1.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ESetPlacement.h"
#include "ecCommon.h"

BlockGroupMapping_t *newBlockGroupMapping(uint64_t groupBlockId){
    BlockGroupMapping_t *mappingPtr = talloc(BlockGroupMapping_t, 1);
    mappingPtr->blockId = groupBlockId;
    mappingPtr->next = NULL;
    mappingPtr->pre = NULL;
    
    mappingPtr->recoveredFlag = 0;
    return mappingPtr;
}

void deallocBlockGroupMapping(BlockGroupMapping_t *mappingPtr){
    free(mappingPtr);
}

void printESets(PlacementManager_t *placementMgr){
    int setIdx;
    
    for (setIdx = 0; setIdx < placementMgr->eSetsSize; ++setIdx) {
        ESets_t *curESetPtr = placementMgr->eSets + setIdx;
        int nodeIdx;
        printf("Set Idx:%d, GroupIdx:%d\n", curESetPtr->esetId, curESetPtr->groupId);
        
        for (nodeIdx = 0; nodeIdx < curESetPtr->size; ++nodeIdx) {
            ESetNode_t *curESetNodePtr = (curESetPtr->setNodes+nodeIdx);
            printf("node Id:%d  row:%d col:%d serverId:%d serverRack:%d\n",
                   nodeIdx, curESetNodePtr->row, curESetNodePtr->col,curESetNodePtr->serverIdInRack, curESetNodePtr->rackId);
        }
        
        printf("***********\n");
    }
}

void createGroupESets(PlacementGroup_t *placementGroup, int setStartIdx, ESets_t *eSets){
    int setIdx = 0, rIdx, cIdx;
    
    for (setIdx = 0; setIdx < (placementGroup->colSize * placementGroup->colSize); ++setIdx) {
        ESets_t *curESet = eSets+ setIdx;
        
        curESet->recoveringFlag = 0;
        curESet->redoRecover = 0;
        curESet->failedServerNum = 0;
        
        curESet->esetId = setStartIdx + setIdx;
        curESet->size = placementGroup->rowSize;
        curESet->setNodes = talloc(ESetNode_t, curESet->size);
        for (rIdx = 0 ; rIdx < placementGroup->rowSize; ++rIdx) {
            if (rIdx == 0) {
                cIdx = setIdx / placementGroup->colSize;
                
            }else{
                cIdx = ((setIdx / placementGroup->colSize)*(rIdx-1)+setIdx)%(placementGroup->colSize);
            }
            
            (curESet->setNodes+rIdx)->col = cIdx;
            (curESet->setNodes+rIdx)->row = rIdx;
            (curESet->setNodes+rIdx)->rackId = (placementGroup->groupNodes[rIdx] + cIdx)->rackId;
            (curESet->setNodes+rIdx)->serverIdInRack = (placementGroup->groupNodes[rIdx] + cIdx)->serverIdInRack;
            (curESet->setNodes+rIdx)->failedFlag = 0;
            (curESet->setNodes+rIdx)->recoveredFlag = 0;
        }
    }
}

void createESets(PlacementGroup_t *placementGroup, int groupSize, ESets_t *eSets){
    int groupIdx, setStartIdx;
    
    for (groupIdx = 0; groupIdx < groupSize; ++groupIdx) {
        PlacementGroup_t *curGroup = placementGroup + groupIdx;
        setStartIdx = placementGroup->colSize * placementGroup->colSize * groupIdx;
        ESets_t *curESetPtr = eSets + setStartIdx;
        createGroupESets(curGroup, setStartIdx, curESetPtr);
    }
}

void printPlacementGroup(PlacementGroup_t *placementGroup){
    int rIdx, cIdx;
    
    for (rIdx = 0; rIdx < placementGroup->rowSize; ++rIdx) {
        for (cIdx = 0; cIdx < placementGroup->colSize; ++cIdx) {
            GroupNode_t *groupNode = placementGroup->groupNodes[rIdx] + cIdx;
            printf("rackId:%d serverId:%d ", groupNode->rackId, groupNode->serverIdInRack);
        }
        printf("\n");
    }
}

void printfPlacementGroups(PlacementManager_t *placementMgr){
    int idx;
    for (idx = 0; idx < placementMgr->groupSize; ++idx) {
        PlacementGroup_t *placementGroup = placementMgr->placementGroups+idx;
        printf("Group id:%d\n", idx);
        printPlacementGroup(placementGroup);
        
    }
}

int checkRackInGroup(PlacementGroup_t *placementGroup, int rowIdx, int rackIdx){
    int idx;

    for (idx = 0; idx < rowIdx; ++idx) {
        int cIdx;
        
        for (cIdx = 0; cIdx < placementGroup->colSize; ++cIdx) {
            int serverRackId = (placementGroup->groupNodes[rowIdx]+cIdx)->rackId;
            if (serverRackId == rackIdx) {
                return 1;
            }
        }
    }
    
    return 0;
}

void createPlacementGroup(PlacementGroup_t *placementGroup, int *serverIdxInRack, int serverNumEachRack, int rackNum){
    int rowIdx;
    
    placementGroup->groupNodes = talloc(GroupNode_t *, placementGroup->rowSize);
    
    for (rowIdx = 0; rowIdx < placementGroup->rowSize; ++rowIdx) {
        int maxServersRackId, rackId, choosedNodesForCol = 0;
        
        placementGroup->groupNodes[rowIdx] = talloc(GroupNode_t, placementGroup->colSize);
        while (choosedNodesForCol != placementGroup->colSize) {
            maxServersRackId = 0;
            
            for (rackId = 0; rackId < rackNum; ++rackId) {
                //Check the if servers of the rack already in the group of the other row
                if (checkRackInGroup(placementGroup, rowIdx, rackId) == 1) {
                    continue;
                }

                if ((serverNumEachRack - *(serverIdxInRack + rackId)) > (serverNumEachRack - *(serverIdxInRack + maxServersRackId))) {
                    maxServersRackId = rackId;
                }
            }
            
            int idxForCol;
            for (idxForCol = choosedNodesForCol; idxForCol < placementGroup->colSize; ++idxForCol) {
                (placementGroup->groupNodes[rowIdx] + idxForCol)->rackId = maxServersRackId;
                (placementGroup->groupNodes[rowIdx] + idxForCol)->serverIdInRack = *(serverIdxInRack + maxServersRackId);
                
//                printf("r:%d c:%d rack:%d, serverId:%d serverNumEachRack:%d\n", rowIdx, choosedNodesForRow, maxServersRackId, *(serverIdxInRack + maxServersRackId), serverNumEachRack);

                *(serverIdxInRack + maxServersRackId) = *(serverIdxInRack + maxServersRackId) + 1;
                ++choosedNodesForCol;

                if (*(serverIdxInRack + maxServersRackId) == serverNumEachRack) {
                    break;
                }

            }
            
        }
    }
}

void createPlacementGroups(PlacementManager_t * placementMgr){
    int idx, *serverIdxInRack;
    
    serverIdxInRack = talloc(int, placementMgr->rMap->rackNum);
    
    for (idx = 0; idx < placementMgr->rMap->rackNum; ++idx) {
        *(serverIdxInRack + idx) = 0;
    }
    
    for (idx = 0; idx < placementMgr->groupSize; ++idx) {
        PlacementGroup_t *placementGroup = placementMgr->placementGroups+idx;
        placementGroup->colSize = placementMgr->overlappingFactor;
        placementGroup->rowSize = placementMgr->nSize;
        createPlacementGroup(placementGroup, serverIdxInRack, placementMgr->rMap->racks->serverNum, placementMgr->rMap->rackNum);
    }
    
}

void setServersPlacementGroup(PlacementManager_t *placementMgr, rackMap_t *rMap){
    int gIdx, rIdx, cIdx;
    
    for (gIdx = 0; gIdx < placementMgr->groupSize; ++gIdx) {
        PlacementGroup_t *placementGpPtr= placementMgr->placementGroups + gIdx;
        
        for (rIdx = 0; rIdx < placementGpPtr->rowSize; ++rIdx) {
            for (cIdx = 0; cIdx <  placementGpPtr->colSize; ++cIdx) {
                GroupNode_t *gNodePtr = placementGpPtr->groupNodes[rIdx] + cIdx;
                Rack_t *rackPtr = rMap->racks + gNodePtr->rackId;
                Server_t *serverPtr = rackPtr->servers + gNodePtr->serverIdInRack;
                serverPtr->groupId = gIdx;
                serverPtr->groupRowId = rIdx;
                serverPtr->groupColId = cIdx;
            }
        }
    }
}

PlacementManager_t *createPlacementMgr(rackMap_t *rMap, int nSize, int oFactor){
    PlacementManager_t *placementMgr = talloc(PlacementManager_t, 1);
    
    if (placementMgr == NULL) {
        return NULL;
    }
    
    placementMgr->nSize = nSize;
    placementMgr->overlappingFactor = oFactor;
    placementMgr->rackNum = rMap->rackNum;
    placementMgr->totalServerNum = rMap->racks->serverNum * placementMgr->rackNum;
    
    placementMgr->groupSize = (placementMgr->totalServerNum / (placementMgr->nSize *  placementMgr->overlappingFactor));
    
    placementMgr->placementGroups = talloc(PlacementGroup_t, placementMgr->groupSize);
    
    placementMgr->rMap = rMap;
    
    createPlacementGroups(placementMgr);
    setServersPlacementGroup(placementMgr, rMap);
    
    printfPlacementGroups(placementMgr);
    
    placementMgr->eSetsSize = placementMgr->groupSize * oFactor * oFactor;
    
    placementMgr->eSets = talloc(ESets_t, placementMgr->eSetsSize);
    
    createESets(placementMgr->placementGroups, placementMgr->groupSize, placementMgr->eSets);
    
    placementMgr->placeIdx = 0;
    pthread_mutex_init(&placementMgr->placeLock, NULL);
    
    placementMgr->blockGroupMap = talloc(BlockGroupMapping_t *, placementMgr->eSetsSize);
    
    int idx;
    
    for (idx = 0; idx < placementMgr->eSetsSize; ++idx) {
        placementMgr->blockGroupMap[idx] = NULL;
    }
    
    printESets(placementMgr);
    
    return placementMgr;
    
}

ESets_t *requestPlacement(PlacementManager_t *placementMgr, ECBlockGroup_t *blockGroupPtr){
    pthread_mutex_lock(&placementMgr->placeLock);
//    printf("placeIdx:%lu eSetsSize:%d\n",placementMgr->placeIdx,  placementMgr->eSetsSize);
    ESets_t *sets = placementMgr->eSets+placementMgr->placeIdx;
    size_t placeIdx = placementMgr->placeIdx;
    while (sets->failedServerNum > 0) {
        //Find an ESet that has no failed storage server
        printf("set idx: failed server num:%d\n",sets->failedServerNum);
        placementMgr->placeIdx = (placementMgr->placeIdx + 1) % placementMgr->eSetsSize;
        sets = placementMgr->eSets+placementMgr->placeIdx;
        
        if (placeIdx == placementMgr->placeIdx) {
            //No ESet has unfailed storage server
            break;
        }
    }
    
    BlockGroupMapping_t *blockGroupMappingPtr = newBlockGroupMapping(blockGroupPtr->blockGroupId);
    blockGroupMappingPtr->blockGroupPtr = blockGroupPtr;
    
    if (sets->failedServerNum > 0) {
        blockGroupMappingPtr->recoveredFlag = 1;
    }
    
    blockGroupMappingPtr->next = placementMgr->blockGroupMap[placementMgr->placeIdx];
    placementMgr->blockGroupMap[placementMgr->placeIdx] = blockGroupMappingPtr;
    
    pthread_mutex_unlock(&placementMgr->placeLock);
    return sets;
}

void removeBlockGroupMapping(PlacementManager_t *placementMgr, size_t eSetIdx, uint64_t blockId){
    if (eSetIdx >= placementMgr->eSetsSize) {
        return;
    }
    
    BlockGroupMapping_t *blockGroupMappingPtr = placementMgr->blockGroupMap[eSetIdx];
    
    if (blockGroupMappingPtr->blockId == blockId) {
        placementMgr->blockGroupMap[eSetIdx] = blockGroupMappingPtr->next;
        deallocBlockGroupMapping(blockGroupMappingPtr);
        return;
    }
    
    BlockGroupMapping_t *preBlockGroupPtr = blockGroupMappingPtr->pre;
    preBlockGroupPtr->next = blockGroupMappingPtr->next;
    
    
    if (blockGroupMappingPtr->next != NULL) {
        blockGroupMappingPtr->next->pre = preBlockGroupPtr;
    }
    
    deallocBlockGroupMapping(blockGroupMappingPtr);
    return;
}

void assignESetsToGroup(ECBlockGroup_t *blockGroup, ESets_t *eSets){
    size_t idx;
    
    if (blockGroup->blocksNum != eSets->size) {
        printf("blockGroup->blocksNum != eSets->size\n");
    }
    
    blockGroup->setIdx = eSets->esetId;
    
    for (idx = 0; idx < blockGroup->blocksNum; ++idx) {
        ESetNode_t *setNode = eSets->setNodes + idx;
        ECBlock_t *curBlock = blockGroup->blocks + idx;
        curBlock->rackId = setNode->rackId;
        curBlock->serverId = setNode->serverIdInRack;
    }
    
}

