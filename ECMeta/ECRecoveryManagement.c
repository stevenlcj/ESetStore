//
//  ECRecoveryManagement.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/11/7.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECRecoveryManagement.h"
#include "ECMetaEngine.h"
#include "ecUtils.h"
#include "ECBlockServerCmd.h"
#include "BlockServersManagement.h"
#include <string.h>

FailedESetsList_t *createFailedESetsList(ECMetaServer_t *metaServerPtr, FailedServerList_t *serverList, ESets_t *eSetsPtr){
    FailedESetsList_t *eSetsList = talloc(FailedESetsList_t, 1);
    memset(eSetsList, 0, sizeof(FailedESetsList_t));
    eSetsList->failedESets = eSetsPtr;
    eSetsList->failedIdx = talloc(int, metaServerPtr->stripeM);//Support at most stripeM failures
    memset(eSetsList->failedIdx, 0, sizeof(int) * metaServerPtr->stripeM);
    *eSetsList->failedIdx = serverList->failedServer->groupRowId;
    eSetsList->failedNodesNum = 1;
    
    gettimeofday(&eSetsList->failedTime, NULL);

    return eSetsList;
}

FailedServerList_t *createFailedServerList(ECRecoveryManager_t *ecRecoveryMgrPtr, Server_t *server){
    FailedServerList_t *failedServerList = (FailedServerList_t *)malloc(sizeof(FailedServerList_t));  //talloc(FailedServerList_t, 1);
    
    if (failedServerList == NULL) {
        printf("unable to alloc FailedServerList_t\n");
        return NULL;
    }
    
    memset(failedServerList, 0, sizeof(FailedServerList_t));
    
    server->recoveredSets = 0;
    failedServerList->failedServer = server;
    gettimeofday(&server->failedTime, NULL);
    
    pthread_mutex_lock(&ecRecoveryMgrPtr->lock);
    
    if (ecRecoveryMgrPtr->pendingServerListsNum == 0) {
        ecRecoveryMgrPtr->pendingRecoverServerLists = failedServerList;
    }else{
        failedServerList->next = ecRecoveryMgrPtr->pendingRecoverServerLists;
        ecRecoveryMgrPtr->pendingRecoverServerLists = failedServerList;
    }
    
    ++ecRecoveryMgrPtr->pendingServerListsNum;
    
    pthread_mutex_unlock(&ecRecoveryMgrPtr->lock);

    return failedServerList;
}


BlockServer_t *getHeadNode(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedESetsList_t *eSetListPtr){
    
    ECMetaServer_t *enginePtr = (ECMetaServer_t *)ecRecoveryMgrPtr->ecMetaServerPtr;
    BlockServerManager_t *serverMgrPtr = enginePtr->blockServerMgr;
    BlockServer_t *blockServer = getActiveBlockServer(serverMgrPtr, eSetListPtr->eSetNodes->rackId, eSetListPtr->eSetNodes->serverIdInRack);
    return blockServer;
}

//void writeCmdToHeadNode(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedESetsList_t *eSetListPtr, char *cmd){
//    ECMetaServer_t *enginePtr = (ECMetaServer_t *)ecRecoveryMgrPtr->ecMetaServerPtr;
//    BlockServerManager_t *serverMgrPtr = enginePtr->blockServerMgr;
//    BlockServer_t *blockServer = getActiveBlockServer(serverMgrPtr, eSetListPtr->eSetNodes->rackId, eSetListPtr->eSetNodes->serverIdInRack);
//    
//    addWriteMsgToBlockserver(blockServer, cmd, strlen(cmd) + 1);
//    writeToConn(serverMgrPtr, blockServer);

//    printf("writeCmdToHeadNodeb before free ptr address:%p\n", cmd);
//    free(cmd);
//    printf("writeCmdToHeadNodeb after free\n");
//}

void deallocFailedESetsList(FailedESetsList_t *eSetsListPtr){
    if (eSetsListPtr == NULL) {
        return;
    }
    
    
    if (eSetsListPtr->blockGPMappingPtr != NULL) {
        free(eSetsListPtr->blockGPMappingPtr);
    }
    
    
    if (eSetsListPtr->eSetNodesNum != 0) {
        free(eSetsListPtr->eSetNodes);
        eSetsListPtr->eSetNodesNum = 0;
    }
    
    free(eSetsListPtr);
}

ECRecoveryManager_t *createECRecoveryManager(void *ecMetaSeverPtr){
    ECRecoveryManager_t *ecRecoveryMgr = talloc(ECRecoveryManager_t, 1);
    
    pthread_mutex_init(&ecRecoveryMgr->lock, NULL);
    
    ecRecoveryMgr->pendingRecoverServerLists = NULL;
    ecRecoveryMgr->pendingServerListsNum = 0;
    
    ecRecoveryMgr->pendingRecoverESetsLists = NULL;
    ecRecoveryMgr->pendingRecoverESetsListsNum = 0;
    
    ecRecoveryMgr->recoveringServerLists = NULL;
    ecRecoveryMgr->recoveringServerListsNum = 0;
    
    ecRecoveryMgr->recoveringESetsLists = NULL;
    ecRecoveryMgr->recoveringESetsListsNum = 0;
    
    ecRecoveryMgr->ecMetaServerPtr = ecMetaSeverPtr;
    
    ecRecoveryMgr->recoverInterval = ((ECMetaServer_t *)ecMetaSeverPtr)->recoverInterval;
    
    printf("ecRecoveryMgr->recoverInterval:%d\n",ecRecoveryMgr->recoverInterval);
    
    ecRecoveryMgr->recoverTimeConsume = 0.0;
    ecRecoveryMgr->totalRecoveredSize = 0;
    return ecRecoveryMgr;
}

void markFailedBlocks(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedServerList_t *serverList){
    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)ecRecoveryMgrPtr->ecMetaServerPtr;
    PlacementManager_t *placementMgrPtr = metaServerPtr->placementMgr;

    FailedESetsList_t *eSetListsPtr = serverList->pendingRecoverESetsLists;

    pthread_mutex_lock(&placementMgrPtr->placeLock); // lock to avoid deletion from client at the same time

    while  (eSetListsPtr != NULL) {
        ESets_t *eSetPtr = eSetListsPtr->failedESets;
        printf("Set idx:%d\n", eSetPtr->esetId);
        BlockGroupMapping_t *blockGpMappingPtr = placementMgrPtr->blockGroupMap[eSetPtr->esetId];
        
        while (blockGpMappingPtr != NULL) {
            ECBlock_t *blockPtr =  blockGpMappingPtr->blockGroupPtr->blocks+ serverList->failedServer->groupRowId;
            blockPtr->missingFlag = 1;
            blockPtr->recoveredFlag = 0;
            
            blockGpMappingPtr = blockGpMappingPtr->next;
        }
        
        eSetListsPtr = eSetListsPtr->next;
        
    }
    
    pthread_mutex_unlock(&placementMgrPtr->placeLock);

}

void addFailedESets(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedServerList_t *serverList){
//    printf("addFailedESets\n");

    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)ecRecoveryMgrPtr->ecMetaServerPtr;
    PlacementManager_t *placementMgrPtr = metaServerPtr->placementMgr;
    
//    printf("setsStartIdx serverList->failedServer:%p placementMgrPtr->overlappingFactor:%d\n",serverList->failedServer, placementMgrPtr->overlappingFactor);

    int setsStartIdx = serverList->failedServer->groupId * (placementMgrPtr->overlappingFactor * placementMgrPtr->overlappingFactor);
    
    int row = serverList->failedServer->groupRowId, col = serverList->failedServer->groupColId;
    int idx;

    if (row == 0) {
        printf("row = 0\n");

        for (idx = setsStartIdx + col * placementMgrPtr->overlappingFactor; serverList->eSetsNum != placementMgrPtr->overlappingFactor; ++idx) {
            
            ESets_t *eSetsPtr = placementMgrPtr->eSets + idx;
            eSetsPtr->recoveringFlag = 1;
            eSetsPtr->setNodes->failedFlag = 1;
            
            printf("Add eset:%d addr:%p\n",eSetsPtr->esetId, eSetsPtr);
            
            FailedESetsList_t *eSetsList = createFailedESetsList(metaServerPtr, serverList, eSetsPtr);
//            FailedESetsList_t *eSetsList = talloc(FailedESetsList_t, 1);
//            memset(eSetsList, 0, sizeof(FailedESetsList_t));
//            eSetsList->failedESets = eSetsPtr;
//            eSetsList->failedIdx = talloc(int, metaServerPtr->stripeM);//Support at most stripeM failures
//            memset(eSetsList->failedIdx, 0, sizeof(int) * metaServerPtr->stripeM);
//            *eSetsList->failedIdx = serverList->failedServer->groupRowId;
//            eSetsList->failedNodesNum = 1;
//            
//            gettimeofday(&eSetsList->failedTime, NULL);
            
            if (serverList->eSetsNum == 0) {
                serverList->pendingRecoverESetsLists = eSetsList;
            }else{
                eSetsList->next = serverList->pendingRecoverESetsLists;
                serverList->pendingRecoverESetsLists->pre = eSetsList;
                serverList->pendingRecoverESetsLists = eSetsList;
            }
            
            printf("row = 0 serverList->eSetsNum:%lu, idx:%d eSetsList:%p, eSetsList->failedESets:%p\n", serverList->eSetsNum,idx, eSetsList, eSetsList->failedESets);
            ++serverList->eSetsNum;
        }
    }else{
        printf("row != 0\n");
        for (idx = 0; idx < (placementMgrPtr->overlappingFactor*placementMgrPtr->overlappingFactor); ++idx) {
            int modValue = (idx/placementMgrPtr->overlappingFactor)*(row -1) + idx;
            modValue = modValue % placementMgrPtr->overlappingFactor;
            
            printf("modValue:%d\n",modValue);
            if (col == modValue) {
                ESets_t *eSetsPtr = placementMgrPtr->eSets + idx + setsStartIdx;
                eSetsPtr->recoveringFlag = 1;
                (eSetsPtr->setNodes+row)->failedFlag = 1;
                
                printf("Add eset:%d addr:%p\n",eSetsPtr->esetId, eSetsPtr);
//                printf("eSetsPtr->setNodes+ row r:%d, s:%d\n",(eSetsPtr->setNodes+row)->rackId, (eSetsPtr->setNodes+row)->serverIdInRack);
                FailedESetsList_t *eSetsList = createFailedESetsList(metaServerPtr, serverList, eSetsPtr);

//                FailedESetsList_t *eSetsList = talloc(FailedESetsList_t, 1);
//                memset(eSetsList, 0, sizeof(FailedESetsList_t));
//                eSetsList->failedESets = eSetsPtr;
//                eSetsList->failedIdx = talloc(int, metaServerPtr->stripeM);//Support at most stripeM failures
//                memset(eSetsList->failedIdx, 0, sizeof(int) * metaServerPtr->stripeM);
//                *eSetsList->failedIdx = serverList->failedServer->groupRowId;
//                eSetsList->failedNodesNum = 1;

                if (serverList->eSetsNum == 0) {
                    serverList->pendingRecoverESetsLists = eSetsList;
                }else{
                    eSetsList->next = serverList->pendingRecoverESetsLists;
                    serverList->pendingRecoverESetsLists->pre = eSetsList;
                    serverList->pendingRecoverESetsLists = eSetsList;
                }
                
                printf("row = %d serverList->eSetsNum:%lu, idx:%d\n",row,  serverList->eSetsNum,idx);
                ++serverList->eSetsNum;
            }
        }
    }
    
    printf("call markFailedBlocks\n");
    markFailedBlocks(ecRecoveryMgrPtr, serverList);
    return;

}

int checkInSets(FailedESetsList_t *eSetsList, ESetNode_t *eSetNodePtr, int idxSize){
    int idx;
    
    for (idx = 0; idx < idxSize; ++idx) {
        ESetNode_t *curNodePtr = eSetsList->eSetNodes + idx;
        
        if (eSetNodePtr->col == curNodePtr->col && eSetNodePtr->row == curNodePtr->row) {
            return 1;
        }
    }
    
    return 0;
}

int checkIOCollision(FailedESetsList_t *eSetsList, ESetNode_t *eSetNodePtr){
    int setIdx;
    
    if (eSetNodePtr->row == 0) {
        return 0;
    }
    
    for (setIdx = 0; setIdx < eSetsList->failedESets->size; ++setIdx) {
        if (setIdx == eSetNodePtr->row) {
            continue;
        }
        
        ESetNode_t *curNodePtr = eSetsList->failedESets->setNodes + setIdx;
        
        if (curNodePtr->failedFlag == 1 && curNodePtr->recoveredFlag == 0) {
            int minusValue =0 ;
            
            if (curNodePtr->row > eSetNodePtr->row) {
                minusValue = curNodePtr->row - eSetNodePtr->row;
            }else{
                minusValue = eSetNodePtr->row - curNodePtr->row;
            }
            
            if (minusValue % eSetsList->failedESets->size == 0) {
                return 1;
            }
        }
        
    }
    
    return 0;
}

void sortSelectedNodes(FailedESetsList_t *eSetsList){
    int minIdx, iterIdx, selectedIdx;
    
    //A simple sorting implementation. Efficiency doesn't matter here
    for (minIdx = 0; minIdx < (eSetsList->eSetNodesNum - 1); ++minIdx) {
        selectedIdx = minIdx;
        
        for (iterIdx = minIdx + 1; iterIdx < eSetsList->eSetNodesNum; ++iterIdx) {
            ESetNode_t *selectedNodePtr = eSetsList->eSetNodes+selectedIdx;
            ESetNode_t *curNodePtr = eSetsList->eSetNodes+iterIdx;
            if (curNodePtr->row < selectedNodePtr->row) {
                selectedIdx = iterIdx;
            }
        }
        
        if (selectedIdx != minIdx) {
            ESetNode_t swapNode;
            ESetNode_t *selectedNodePtr = eSetsList->eSetNodes+selectedIdx;
            ESetNode_t *curNodePtr = eSetsList->eSetNodes+minIdx;
            
            memcpy((void *)&swapNode, (void *)curNodePtr, sizeof(ESetNode_t));
            memcpy((void *)curNodePtr, (void *)selectedNodePtr, sizeof(ESetNode_t));
            memcpy((void *)selectedNodePtr, (void *)&swapNode, sizeof(ESetNode_t));
        }
    }
}

void selectRecoveryNodes(FailedESetsList_t *eSetsList, ECMetaServer_t *enginePtr){
    int idx = 0, setIdx = 0;
    
    for (setIdx = 0; setIdx < eSetsList->failedESets->size; ++setIdx) {
        ESetNode_t *eSetNodePtr = eSetsList->failedESets->setNodes + setIdx;
        
        if (eSetNodePtr->failedFlag == 1 && eSetNodePtr->recoveredFlag != 1) {
            continue;
        }
        
        if (checkIOCollision(eSetsList, eSetNodePtr) == 0) {
            ESetNode_t *selectedESetNodePtr = eSetsList->eSetNodes + idx;
            
            memcpy((void *)selectedESetNodePtr, (void *)eSetNodePtr, sizeof(ESetNode_t));
            printf("Selected rack :%d, server:%d\n", selectedESetNodePtr->rackId, selectedESetNodePtr->serverIdInRack);

            ++idx;
        }
    }
    
    
    if (idx != eSetsList->eSetNodesNum) {
        for (setIdx = 0; setIdx < eSetsList->failedESets->size; ++setIdx) {
            ESetNode_t *eSetNodePtr = eSetsList->failedESets->setNodes + setIdx;
            
            if (eSetNodePtr->failedFlag == 1 && eSetNodePtr->recoveredFlag != 1) {
                continue;
            }
            
            if (checkInSets(eSetsList,eSetNodePtr, idx) == 1) {
                continue;
            }
            
            ESetNode_t *selectedESetNodePtr = eSetsList->eSetNodes + idx;
            
            memcpy((void *)selectedESetNodePtr, (void *)eSetNodePtr, sizeof(ESetNode_t));
            ++idx;
            
            if (idx == eSetsList->eSetNodesNum) {
                break;
            }
        }
    
    	if(idx == eSetsList->eSetNodesNum )
    	{
            break;
    	}
    }

    if (idx != eSetsList->eSetNodesNum) {
        printf("Unable to recover this eset:%d, only %d nodes available\n",eSetsList->failedESets->esetId, idx);
        return;
    }
    
    sortSelectedNodes(eSetsList);
    
    BlockServer_t *headBlockServer = getHeadNode(enginePtr->ecRecoveryMgr, eSetsList);
    writeRecoveryCmd((void *)eSetsList, (void *)enginePtr, (void *)headBlockServer);
    writeToConn(enginePtr->blockServerMgr, headBlockServer);
    
//    printf("Ptr address:%p\n",recoverCmd);
//    BlockServerManager_t *serverMgrPtr = enginePtr->blockServerMgr;
//    BlockServer_t *blockServer = getActiveBlockServer(serverMgrPtr, eSetsList->eSetNodes->rackId, eSetsList->eSetNodes->serverIdInRack);
//    
//    addWriteMsgToBlockserver(blockServer, recoverCmd, strlen(recoverCmd) + 1);
//    writeToConn(serverMgrPtr, blockServer);
//    
//    free(recoverCmd);
//    printf("write recover cmd to rack:%d server:%d\n",blockServer->rackId, blockServer->serverId);
}

void performRecoveringESet(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedESetsList_t *eSetsList){
    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)ecRecoveryMgrPtr->ecMetaServerPtr;

    printf("start performRecoveringESet :%d %p %p\n", eSetsList->failedESets->esetId,eSetsList ,eSetsList->failedESets);
    
    if (eSetsList->eSetNodesNum == 0) {
        eSetsList->eSetNodes = talloc(ESetNode_t, metaServerPtr->stripeK);
        eSetsList->eSetNodesNum = metaServerPtr->stripeK;
    }
    
    selectRecoveryNodes(eSetsList,metaServerPtr);
    
    gettimeofday(&eSetsList->startRecoveryTime, NULL);
}

void addRecoveringESets(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedESetsList_t *failedESetsList){
    if (ecRecoveryMgrPtr->pendingRecoverESetsLists == failedESetsList) {
        ecRecoveryMgrPtr->pendingRecoverESetsLists = failedESetsList->next;
        
        if (ecRecoveryMgrPtr->pendingRecoverESetsLists != NULL) {
            ecRecoveryMgrPtr->pendingRecoverESetsLists->pre = NULL;
        }
    }else{
        if (failedESetsList->pre != NULL) {
            failedESetsList->pre->next = failedESetsList->next;
        }
        
        if (failedESetsList->next != NULL) {
            failedESetsList->next->pre = failedESetsList->pre;
        }
    }
    
    failedESetsList->next = ecRecoveryMgrPtr->recoveringESetsLists;
    
    if (ecRecoveryMgrPtr->recoveringESetsLists != NULL) {
        ecRecoveryMgrPtr->recoveringESetsLists->pre = failedESetsList;
    }
    
    failedESetsList->pre = NULL;
    ecRecoveryMgrPtr->recoveringESetsLists = failedESetsList;
    
    performRecoveringESet(ecRecoveryMgrPtr, failedESetsList);
    
    FailedESetsList_t *inRecoveringESet = ecRecoveryMgrPtr->recoveringESetsLists;
    
}

//Add esetsList from pendingrecoverylist to recoveringlist
void startRecoveryESets(ECRecoveryManager_t *ecRecoveryMgrPtr){
    if (ecRecoveryMgrPtr->pendingRecoverESetsLists == NULL) {
        return;
    }
    
    FailedESetsList_t *failedESetsList = ecRecoveryMgrPtr->pendingRecoverESetsLists;
    
    struct timeval now;
    gettimeofday(&now, NULL);
    
    while (failedESetsList != NULL) {
        double timeInterval = timeIntervalInMS(failedESetsList->failedTime, now);
        if ((int)(timeInterval/1000.0) >= ecRecoveryMgrPtr->recoverInterval) {
            FailedESetsList_t *recoveringESets = failedESetsList;
            failedESetsList = failedESetsList->next;
            
            printf("add recovering ESet:%d\n", recoveringESets->failedESets->esetId);
            
            addRecoveringESets(ecRecoveryMgrPtr, recoveringESets);
        }else{
//            printf("ESets:%d time interval:%fms, ecRecoveryMgrPtr->recoverInterval:%d\n",failedESetsList->failedESets->esetId, timeInterval, ecRecoveryMgrPtr->recoverInterval);
            
            failedESetsList = failedESetsList->next;
        }
    }
}

void deleteESetsListInPendingList(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedESetsList_t *eSetsListPtr){
    FailedESetsList_t *pendingESetsListPtr = ecRecoveryMgrPtr->pendingRecoverESetsLists;
    
    if (pendingESetsListPtr->failedESets->esetId == eSetsListPtr->failedESets->esetId) {
        ecRecoveryMgrPtr->pendingRecoverESetsLists = pendingESetsListPtr->next;
        
        deallocFailedESetsList(pendingESetsListPtr);
        return;
    }
}

FailedESetsList_t *getESetsInPendingList(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedESetsList_t *eSetsListPtr){
    
    FailedESetsList_t *pendingESetsListPtr = ecRecoveryMgrPtr->pendingRecoverESetsLists;
    
    while (pendingESetsListPtr != NULL) {
        if (pendingESetsListPtr->failedESets->esetId == eSetsListPtr->failedESets->esetId) {
            return pendingESetsListPtr;
        }
        
        pendingESetsListPtr = pendingESetsListPtr->next;
    }
    
    return NULL;
}

FailedESetsList_t *getESetsInRecoveryingList(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedESetsList_t *eSetsListPtr){
    
    FailedESetsList_t *recoveringESetsListPtr = ecRecoveryMgrPtr->recoveringESetsLists;
    
    while (recoveringESetsListPtr != NULL) {
        
        if (recoveringESetsListPtr->failedESets->esetId == recoveringESetsListPtr->failedESets->esetId) {
            return recoveringESetsListPtr;
        }
        
        recoveringESetsListPtr = recoveringESetsListPtr->next;
    }
    
    return NULL;
}

void addPendingRecoveryESets(ECRecoveryManager_t *ecRecoveryMgrPtr){
    FailedServerList_t *serverListPtr = ecRecoveryMgrPtr->pendingRecoverServerLists;
    
    while (serverListPtr != NULL) {
        FailedESetsList_t *eSetsListPtr ;
        
        while ((eSetsListPtr = serverListPtr->pendingRecoverESetsLists) != NULL) {
            
//            printf("start to add esets:%d to pending list\n",eSetsListPtr->failedESets->esetId);
            
            serverListPtr->pendingRecoverESetsLists = eSetsListPtr->next;
            
            if (serverListPtr->pendingRecoverESetsLists != NULL) {
                serverListPtr->pendingRecoverESetsLists->pre = NULL;
            }
            
//            printf("getESetsInPendingList\n");
            if (getESetsInPendingList(ecRecoveryMgrPtr, eSetsListPtr) != NULL) {
                deleteESetsListInPendingList(ecRecoveryMgrPtr, eSetsListPtr);
            }
            
//            printf("getESetsInRecoveryingList\n");
            FailedESetsList_t *inRecoveringESetsListPtr = getESetsInRecoveryingList(ecRecoveryMgrPtr, eSetsListPtr);
            
//            printf("after getESetsInRecoveryingList\n");

            if (inRecoveringESetsListPtr != NULL) {
                inRecoveringESetsListPtr->failedESets->redoRecover = 1;
            }
            
//            printf("inRecoveringESetsListPtr != NULL\n");
            eSetsListPtr->next = ecRecoveryMgrPtr->pendingRecoverESetsLists;
            
            if (ecRecoveryMgrPtr->pendingRecoverESetsLists != NULL) {
                ecRecoveryMgrPtr->pendingRecoverESetsLists->pre = eSetsListPtr;
            }

//            printf("ecRecoveryMgrPtr->pendingRecoverESetsLists != NULL\n");
            eSetsListPtr->pre = NULL;
            
            ecRecoveryMgrPtr->pendingRecoverESetsLists = eSetsListPtr;
        }
        
        serverListPtr = serverListPtr->next;
    }
    
//    printf("end addPendingRecoveryESets\n");
}

/*
 Init failedSeverList
 
 Entry point for recovering server started at addFailedServer
 */
void addFailedServer(ECRecoveryManager_t *ecRecoveryMgrPtr, Server_t *server){
    
    if (server->groupId < 0) {
        printf("No data stored on the failed server\n");
        return;
    }
    
    printf("addFailedServer server gid:%d grid:%d gcid:%d %p\n", server->groupId, server->groupRowId, server->groupColId ,server);
    
    FailedServerList_t *serverList = createFailedServerList(ecRecoveryMgrPtr, server);
    
    addFailedESets(ecRecoveryMgrPtr, serverList);
    addPendingRecoveryESets(ecRecoveryMgrPtr);
}

int recoveryingABlock(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedESetsList_t *eSetListPtr){
    ECMetaServer_t *ecMetaServerPtr = (ECMetaServer_t *)ecRecoveryMgrPtr->ecMetaServerPtr;
    PlacementManager_t *placementMgr = ecMetaServerPtr->placementMgr;
    
    pthread_mutex_lock(&placementMgr->placeLock);
    BlockGroupMapping_t *setBlockGpMpPtr = placementMgr->blockGroupMap[eSetListPtr->failedESets->esetId];
    
    if (setBlockGpMpPtr == NULL) {
        printf("setBlockGpMpPtr == NULL\n");
        pthread_mutex_unlock(&placementMgr->placeLock);

        return 0;
    }
    
    while (setBlockGpMpPtr != NULL){
        
        if (setBlockGpMpPtr->recoveredFlag == 1) {
            setBlockGpMpPtr = setBlockGpMpPtr->next;
            continue;
        }
        
        if (eSetListPtr->blockGPMappingPtr == NULL) {
            eSetListPtr->blockGPMappingPtr = talloc(BlockGroupMapping_t, 1);
        }
        
        memcpy(eSetListPtr->blockGPMappingPtr, setBlockGpMpPtr, sizeof(BlockGroupMapping_t));
        
        pthread_mutex_unlock(&placementMgr->placeLock);
        return 1;
    }
    
    pthread_mutex_unlock(&placementMgr->placeLock);
//    printf("recover done recoveryingABlock\n");
    return 0;
}

void recoveredBlock(ECRecoveryManager_t *ecRecoveryMgrPtr, FailedESetsList_t *eSetListPtr, uint64_t blockId){
    if (eSetListPtr->blockGPMappingPtr == NULL) {
        printf("error eSetListPtr->blockGPMappingPtr = NULL");
        return;
    }
    
    if (eSetListPtr->blockGPMappingPtr->blockId != blockId) {
        printf("error eSetListPtr->blockGPMappingPtr->blockId:%lu != blockId:%lu\n",eSetListPtr->blockGPMappingPtr->blockId, blockId);
        return;
    }
    
    ECMetaServer_t *ecMetaServerPtr = (ECMetaServer_t *)ecRecoveryMgrPtr->ecMetaServerPtr;
    PlacementManager_t *placementMgr = ecMetaServerPtr->placementMgr;

    pthread_mutex_lock(&placementMgr->placeLock);
    BlockGroupMapping_t *setBlockGpMpPtr = placementMgr->blockGroupMap[eSetListPtr->failedESets->esetId];
    while (setBlockGpMpPtr != NULL) {
//        printf("setBlockGpMpPtr blockId:%lu\n",setBlockGpMpPtr->blockId);
        
        if (setBlockGpMpPtr->blockId == blockId) {
            setBlockGpMpPtr->recoveredFlag = 1;
            ECBlockGroup_t *ecBlockGpPtr = setBlockGpMpPtr->blockGroupPtr;
            int idx = 0;
            for (idx = 0; idx < eSetListPtr->failedNodesNum; ++idx) {
                ECBlock_t *block = ecBlockGpPtr->blocks + *(eSetListPtr->failedIdx + idx);
                if (block->missingFlag == 1 && block->recoveredFlag == 0) {
                    block->recoveredFlag = 1;
                    block->serverId = eSetListPtr->eSetNodes->serverIdInRack;
                    block->rackId = eSetListPtr->eSetNodes->rackId;
                    ecRecoveryMgrPtr->totalRecoveredSize =  ecRecoveryMgrPtr->totalRecoveredSize + block->blockSize;
//                    printf("block:%lu size:%lu, recovered by rackId:%d serverId:%d\n",block->blockId, block->blockSize, block->rackId, block->serverId);
                }else{
                    printf("Error: block not in failed state\n");
                }
            }
            break;
        }
        
        setBlockGpMpPtr = setBlockGpMpPtr->next;
    }
    
    pthread_mutex_unlock(&placementMgr->placeLock);
    
//    printf("done recoveredBlock:%lu\n",blockId);
}

void recoverESet(ECRecoveryManager_t *ecRecoveryMgrPtr, int eSetIdx, uint64_t blockId){
    FailedESetsList_t *eSetListPtr = ecRecoveryMgrPtr->recoveringESetsLists;
    
    while (eSetListPtr != NULL) {
        if (eSetListPtr->failedESets->esetId == eSetIdx) {
            break;
        }
        eSetListPtr = eSetListPtr->next;
    }
    
    if (eSetListPtr == NULL) {
        printf("error:eSetListPtr == NULL for recoverESet\n");
        return;
    }
    
    if (eSetListPtr->failedESets->esetId != eSetIdx) {
        printf("unable to find eSetIdx:%d to recoverESet\n", eSetIdx);
        return;
    }
    
    if (blockId != 0) {
//        printf("recoveredBlock:%lu\n",blockId);
        recoveredBlock(ecRecoveryMgrPtr, eSetListPtr, blockId);
    }
    
    if (recoveryingABlock(ecRecoveryMgrPtr, eSetListPtr) == 0) {
//        printf("The ESet:%d is recovered, pls finish the remaining code\n", eSetListPtr->failedESets->esetId);
        ECMetaServer_t *enginePtr = (ECMetaServer_t *)ecRecoveryMgrPtr->ecMetaServerPtr;
        BlockServer_t *headBlockServer = getHeadNode(ecRecoveryMgrPtr, eSetListPtr);
        writeRecoveryOverCmd((void *)eSetListPtr, (void *)headBlockServer);
        writeToConn(enginePtr->blockServerMgr, headBlockServer);

        //To do: report recovered ESets
        struct timeval now;
        gettimeofday(&now, NULL);
        
        ecRecoveryMgrPtr->recoverTimeConsume = timeIntervalInMS(eSetListPtr->startRecoveryTime, now);
        
        double recoveryThroughput = (((double) ecRecoveryMgrPtr->totalRecoveredSize)/1024.0/1024.0) / (ecRecoveryMgrPtr->recoverTimeConsume/1000.0);
        printf("recovered set:%d recoveredSize:%luMB, timeConsume:%fms throughput:%fMB/s\n",eSetListPtr->failedESets->esetId, (( ecRecoveryMgrPtr->totalRecoveredSize)/1024/1024),ecRecoveryMgrPtr->recoverTimeConsume, recoveryThroughput);
        
        return;
    }else{
//        printf("Start to recover block:%lu\n",eSetListPtr->blockGPMappingPtr->blockId);
        
        ECMetaServer_t *enginePtr = (ECMetaServer_t *)ecRecoveryMgrPtr->ecMetaServerPtr;
        BlockServer_t *headBlockServer = getHeadNode(ecRecoveryMgrPtr, eSetListPtr);
        writeRecoveryBlockGroupCmd((void *)eSetListPtr, (void *) eSetListPtr->blockGPMappingPtr->blockGroupPtr, (void *)headBlockServer);
        writeToConn(enginePtr->blockServerMgr, headBlockServer);
    }
    
    return;
}
