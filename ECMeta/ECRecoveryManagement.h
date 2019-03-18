//
//  ECRecoveryManagement.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/11/7.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__ECRecoveryManagement__
#define __ECMeta__ECRecoveryManagement__

#include <stdio.h>
#include <pthread.h>

#include <sys/time.h>

#include "BlockServer.h"
#include "rackMap.h"
#include "ESetPlacement.h"

typedef struct {
    int eSetIdx;
    int recoveredFlag;
}FailedServerESetsMark_t;

typedef struct FailedESetsList{
    ESets_t *failedESets;
    
    BlockGroupMapping_t *blockGPMappingPtr;
    
    struct FailedESetsList *next;
    struct FailedESetsList *pre;
    
    ESetNode_t *eSetNodes;
    int eSetNodesNum;
    
    int failedNodesNum;
    int *failedIdx;
    
    struct timeval failedTime;
    struct timeval startRecoveryTime;
}FailedESetsList_t;

typedef struct FailedServerList{
    Server_t *failedServer;
    
    FailedServerESetsMark_t *eSetsMark;
    int recoveredESetsNum;
    
    FailedESetsList_t *pendingRecoverESetsLists;
    size_t eSetsNum;
    
    struct FailedServerList *next;
    struct FailedServerList *pre;
}FailedServerList_t;

typedef struct {
    
    pthread_mutex_t lock;
    
    FailedServerList_t *pendingRecoverServerLists;
    size_t pendingServerListsNum;
    
    FailedESetsList_t *pendingRecoverESetsLists;
    size_t pendingRecoverESetsListsNum;
    
    FailedServerList_t *recoveringServerLists;
    size_t recoveringServerListsNum;
    
    FailedESetsList_t *recoveringESetsLists;
    size_t recoveringESetsListsNum;
    
    void *ecMetaServerPtr;
    
    int recoverInterval;
    
    double recoverTimeConsume;
    size_t totalRecoveredSize;
}ECRecoveryManager_t;

ECRecoveryManager_t *createECRecoveryManager(void *ecMetaSeverPtr);

void addFailedServer(ECRecoveryManager_t *ecRecoveryMgrPtr, Server_t *server);

void startRecoveryESets(ECRecoveryManager_t *ecRecoveryMgrPtr);

// blockIdx = 0 if no block recovered, else indicates the block that recovered
void recoverESet(ECRecoveryManager_t *ecRecoveryMgrPtr, int eSetIdx, uint64_t blockId);
#endif /* defined(__ECMeta__ECRecoveryManagement__) */
