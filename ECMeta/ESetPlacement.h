//
//  ESetPlacement.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/8/1.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__ESetPlacement__
#define __ECMeta__ESetPlacement__

#include <stdio.h>

#include "rackMap.h"
#include "BlockManagement.h"

#include <pthread.h>
#include <sys/time.h>

typedef struct GroupNode{
    int rackId;
    int serverIdInRack;
}GroupNode_t;

typedef struct ESetNode{
    int rackId;
    int serverIdInRack;
    
    int row;
    int col;
    
    int failedFlag;
    int recoveredFlag;

}ESetNode_t;

typedef struct BlockGroupMapping{
    uint64_t blockId;
    ECBlockGroup_t *blockGroupPtr;
    
    int recoveredFlag;
    
    struct BlockGroupMapping *next;
    struct BlockGroupMapping *pre;
    
    struct timeval failedTime;
    struct timeval startRecoveryTime;
}BlockGroupMapping_t;

typedef struct PlacementGroup{
    GroupNode_t **groupNodes;
    int rowSize;
    int colSize;
}PlacementGroup_t;

typedef struct ESets{
    ESetNode_t *setNodes;
    int size;
    int esetId;
    int groupId;
    
    int failedServerNum;
    int recoveringFlag;
    int redoRecover;
    
}ESets_t;

typedef struct PlacementManager{
    int overlappingFactor;
    int nSize;
    int rackNum;
    int totalServerNum;
    
    int groupSize;    
    PlacementGroup_t *placementGroups;
    
    int eSetsSize;
    ESets_t *eSets;
    
    BlockGroupMapping_t **blockGroupMap;
    
    rackMap_t *rMap;
    
    size_t placeIdx;
    pthread_mutex_t placeLock;
}PlacementManager_t;

PlacementManager_t *createPlacementMgr(rackMap_t *rMap, int nSize, int oFactor);
void setServersPlacementGroup(PlacementManager_t *placementMgr, rackMap_t *rMap);
ESets_t *requestPlacement(PlacementManager_t *placementMgr, ECBlockGroup_t *blockGroupPtr);

void assignESetsToGroup(ECBlockGroup_t *blockGroup, ESets_t *eSets);

void removeBlockGroupMapping(PlacementManager_t *placementMgr, size_t eSetIdx, uint64_t blockId);
#endif /* defined(__ECMeta__ESetPlacement__) */
