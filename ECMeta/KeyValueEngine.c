//
//  KeyValueEngine.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "KeyValueEngine.h"
#include "ecCommon.h"
#include "KeyValueSlab.h"
#include "ECMetaEngine.h"
#include "ecNetHandle.h"
#include "ecUtils.h"

#include <unistd.h>
#include <string.h>

void printKey(char *key, size_t keySize){
    printf("Key size:%lu key:", keySize);
    size_t keyPrintSize = keySize;
    while (keyPrintSize != 0) {
        printf("%c", *(key + keySize - keyPrintSize));
        --keyPrintSize;
    }
    printf("\n");
}

keyValueItem_t *locateKeyValueItem(KeyValueEngine_t *keyValueEnginePtr, char *key, size_t keySize, uint32_t hashValue){
    size_t keyIdx = hashValue % keyValueEnginePtr->keyChainSize;
    keyValueItem_t *curItemPtr = keyValueEnginePtr->keyValueChain[keyIdx];
    
#ifdef EC_DEBUG_MODE
//    printf("locate key at idx:%lu\n",keyIdx);
//    printf("locateKeyValueItem hashvalue:%u\n", hashValue);
//    printKey(key, keySize);
#endif

    while (curItemPtr != NULL) {
        
        if (curItemPtr->keySize == keySize && (strncmp(curItemPtr->key, key, keySize) == 0)) {
            return curItemPtr;
        }
        
        curItemPtr = curItemPtr->next;
    }
    
    return NULL;
}

keyValueItem_t *insertKeyItem(KeyValueWorker_t *worker, KeyValueEngine_t *keyValueEnginePtr, char *key, size_t keySize, uint32_t hashValue){
    
    if (locateKeyValueItem(keyValueEnginePtr, key, keySize, hashValue) != NULL) {
        return NULL;
    }
    
    size_t itemSize = sizeof(keyValueItem_t) + keySize;
    size_t slabId = getSlabID(itemSize);
    void *itemMem = getSlabMemByID(worker->thKeyValueSlab, slabId);
    keyValueItem_t *curItemPtr = (keyValueItem_t *)itemMem;
    curItemPtr->key= (void*) (curItemPtr + 1);
    curItemPtr->keySize = keySize;
    memcpy(curItemPtr->key, key, keySize);

#ifdef EC_DEBUG_MODE
//    printf("insertKeyItem");
//    printKey(key, keySize);
#endif
    curItemPtr->blockGroupsHeader = requestBlockGroup(keyValueEnginePtr->blockMgr, worker->workerIdx);
    curItemPtr->blockGroupsTail = curItemPtr->blockGroupsHeader;
    curItemPtr->blockGroupsNum = 1;
    curItemPtr->fileSize = 0;
    
    ECMetaServer_t *ecMetaServer = (ECMetaServer_t *)keyValueEnginePtr->metaEnginePtr;
    ESets_t *eSets = requestPlacement(ecMetaServer->placementMgr, curItemPtr->blockGroupsTail);
    assignESetsToGroup(curItemPtr->blockGroupsTail, eSets);
    
    size_t keyIdx = hashValue % keyValueEnginePtr->keyChainSize;

//    printf("insert key at idx:%lu\n", keyIdx);
    
    curItemPtr->next = keyValueEnginePtr->keyValueChain[keyIdx];
    
    if (keyValueEnginePtr->keyValueChain[keyIdx] != NULL) {
        keyValueEnginePtr->keyValueChain[keyIdx]->pre = curItemPtr;
    }
    
    keyValueEnginePtr->keyValueChain[keyIdx] = curItemPtr;
    
    return curItemPtr;
}

//complex function
void itemIncreFileSize(KeyValueEngine_t *keyValueEnginePtr,keyValueItem_t *item, size_t addedSize){
    int idx = 0;

    item->fileSize = item->fileSize + addedSize;
    ECBlockGroup_t *blockGroupPtr = item->blockGroupsTail;
    ECMetaServer_t *metaEnginePtr = keyValueEnginePtr->metaEnginePtr;
    
    size_t streamingSizeInBytes = metaEnginePtr->streamingSize * 1024;
    //cal each parity block incred size
    size_t parityIncreSize = 0;
    size_t blockGroupAlignedStreamingSize = streamingSizeInBytes * metaEnginePtr->stripeK;
    
    size_t dataAlignedSize = blockGroupAlignedStreamingSize - (blockGroupPtr->blockGroupSize % blockGroupAlignedStreamingSize);
    
    if (dataAlignedSize == blockGroupAlignedStreamingSize ) {
        dataAlignedSize = 0;
    }
    
    if (addedSize < dataAlignedSize) {
        dataAlignedSize = addedSize;
    }
    
    parityIncreSize = streamingSizeInBytes * ((addedSize - dataAlignedSize) / blockGroupAlignedStreamingSize);
    
    if (((addedSize - dataAlignedSize) % blockGroupAlignedStreamingSize) != 0) {
        parityIncreSize = parityIncreSize + streamingSizeInBytes;
    }
    
    for (idx = 0; idx < metaEnginePtr->stripeM; ++idx) {
        ECBlock_t *blockPtr = blockGroupPtr->blocks + idx + metaEnginePtr->stripeK;
        blockPtr->blockSize = blockPtr->blockSize + parityIncreSize;
        
//        printf("Parity block idx:%lu, cur size:%lu\n", blockPtr->blockId, blockPtr->blockSize);
    }
    
    //Alignment for k*streamingSizeInBytes for data blocks
    if (dataAlignedSize != 0) {
        size_t sizeToAlign = dataAlignedSize;
        for (idx = 0; idx < metaEnginePtr->stripeK; ++idx) {
            ECBlock_t *blockPtr = blockGroupPtr->blocks + idx;
            size_t blockAlignSize = blockPtr->blockSize % streamingSizeInBytes;
            blockAlignSize = streamingSizeInBytes - blockAlignSize;
            
            if (sizeToAlign <= blockAlignSize) {
                blockPtr->blockSize = blockPtr->blockSize + sizeToAlign;
                break;
            }else{
                blockPtr->blockSize = blockPtr->blockSize + blockAlignSize;
                sizeToAlign = sizeToAlign - blockAlignSize;
            }
        }
    }
    
    if (addedSize > dataAlignedSize) {
        size_t newAlignedSize = (addedSize-dataAlignedSize) % blockGroupAlignedStreamingSize;
        size_t eachDataBlockAddedSize = ((addedSize-dataAlignedSize)/blockGroupAlignedStreamingSize) *streamingSizeInBytes;
        for (idx = 0; idx < metaEnginePtr->stripeK; ++idx) {
            ECBlock_t *blockPtr = blockGroupPtr->blocks + idx;
        
            if (newAlignedSize < streamingSizeInBytes) {
                blockPtr->blockSize = blockPtr->blockSize + newAlignedSize;
                newAlignedSize = 0;
            }else{
                blockPtr->blockSize = blockPtr->blockSize + streamingSizeInBytes;
                newAlignedSize = newAlignedSize - streamingSizeInBytes;
            }
            
            blockPtr->blockSize  =blockPtr->blockSize + eachDataBlockAddedSize;
//            printf("block idx:%lu, cur size:%lu\n", blockPtr->blockId, blockPtr->blockSize);
        }
        
    }
}

keyValueItem_t *deleteKeyValueItem(KeyValueEngine_t *keyValueEnginePtr, char *key, size_t keySize, uint32_t hashValue){
    keyValueItem_t *curItemPtr = locateKeyValueItem(keyValueEnginePtr, key, keySize, hashValue);
    
    if (curItemPtr == NULL)
    {
        return NULL;
    }

    size_t keyIdx = hashValue % keyValueEnginePtr->keyChainSize;

    if (keyValueEnginePtr->keyValueChain[keyIdx] == curItemPtr) {
        keyValueEnginePtr->keyValueChain[keyIdx] = keyValueEnginePtr->keyValueChain[keyIdx]->next;
        return curItemPtr;
    }
    
    keyValueItem_t *preItemPtr = curItemPtr->pre;
    keyValueItem_t *nextItemPtr = curItemPtr->next;
    
    preItemPtr->next = nextItemPtr;
    if (nextItemPtr != NULL) {
        nextItemPtr->pre = preItemPtr;
    }
    
    return curItemPtr;
}

void launchKeyValueWorker(KeyValueEngine_t *keyValueEnginePtr);

void submitJobToWorker(KeyValueEngine_t *keyValueEnginePtr, KeyValueJob_t *keyValueJob, size_t threadIdx){
    if (keyValueJob == NULL || threadIdx >= keyValueEnginePtr->workerNum) {
        return;
    }
    
    KeyValueWorker_t *workerPtr = keyValueEnginePtr->workers + threadIdx;
    
    pthread_mutex_lock(&workerPtr->threadLock);
    putJob(workerPtr->pendingJobQueue, keyValueJob);
    
    char wakeUpCh = 'w';
    write(workerPtr->workerPipes[1], &wakeUpCh, 1);

    pthread_mutex_unlock(&workerPtr->threadLock);
    
}

KeyValueEngine_t *createKeyValueEngine(void *metaEnginePtr){
    KeyValueEngine_t *keyValueEnginePtr = talloc(KeyValueEngine_t, 1);
    keyValueEnginePtr->keyValueChain = talloc(keyValueItem_t *, KEY_CHAIN_SIZE);
    memset(keyValueEnginePtr->keyValueChain, 0, sizeof(keyValueItem_t *) * KEY_CHAIN_SIZE);

    keyValueEnginePtr->keyChainSize = KEY_CHAIN_SIZE;
    
    keyValueEnginePtr->keyValueSlab = createKeyValueSlab();
    
    keyValueEnginePtr->metaEnginePtr = metaEnginePtr;
    keyValueEnginePtr->workers = createKeyValueWorkers(metaEnginePtr);
    keyValueEnginePtr->workerNum = DEFAULT_KEY_WORKER_THREAD_NUM;
    
    ECMetaServer_t *ecMetaServerPtr = (ECMetaServer_t *)metaEnginePtr;
    
    keyValueEnginePtr->blockMgr = createBlockGroupManager(ecMetaServerPtr->stripeK, ecMetaServerPtr->stripeM, keyValueEnginePtr->workerNum);
    
    int idx;
    
    for (idx = 0; idx < keyValueEnginePtr->workerNum; ++idx) {
        
        KeyValueWorker_t *workerPtr = (keyValueEnginePtr->workers + idx);
        workerPtr->thKeyValueSlab = createThreadKeyValueSlab(keyValueEnginePtr->keyValueSlab);
    }

    keyValueEnginePtr->efd = create_epollfd();
    
    launchKeyValueWorker(keyValueEnginePtr);
    
    return keyValueEnginePtr;
}

void launchKeyValueWorker(KeyValueEngine_t *keyValueEnginePtr){
    pthread_t pid;
    int idx;
    
    pthread_create(&pid, NULL, workerMonitor, (void *)keyValueEnginePtr);
    
    for (idx = 0; idx < DEFAULT_KEY_WORKER_THREAD_NUM; ++idx) {
        KeyValueWorker_t *workerPtr = (keyValueEnginePtr->workers + idx);
        pthread_create(&pid, NULL, startWorker, (void *)workerPtr);
    }
}

void revokeItem(KeyValueWorker_t *worker, keyValueItem_t *item){
       ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)worker->metaServerPtr;
    KeyValueEngine_t *keyValueEnginePtr = (KeyValueEngine_t *)metaServerPtr->keyValueEnginePtr;

    ECBlockGroup_t *blockGroupPtr = item->blockGroupsHeader;
    int tailFlag = 0;
    do{
       ECBlockGroup_t *blockGroupToRevoke = blockGroupPtr;

       if (blockGroupToRevoke == item->blockGroupsTail)
       {
            tailFlag = 1;
       }  

       blockGroupPtr = blockGroupPtr->next;
       revokeBlockGroup(keyValueEnginePtr->blockMgr, blockGroupToRevoke, worker->workerIdx);
    }while(tailFlag != 1);

    revokeSlabMemByID(worker->thKeyValueSlab, item->slabId, item);

}


