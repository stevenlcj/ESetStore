//
//  DiskIOWorker.c
//  ECStoreCPUServer
//
//  Created by Liu Chengjian on 18/1/9.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "DiskIOWorker.h"
#include "ecCommon.h"

#include "DiskIOHandler.h"
#include "RecoveryThreadWorker.h"
#include "BlockServerEngine.h"

#include <stdio.h>
#include <unistd.h>

int readLocalFile(int fd, char *buf, int sizeToRead, DiskIOManager_t *diskIOMgr){
    int readedSize = 0;
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)diskIOMgr->serverEngine;

    
    do {
        ssize_t curReadedSize = readFile(fd, buf + readedSize, (sizeToRead - readedSize), diskIOMgr,ecBlockServerEnginePtr->metaFd);
        if (curReadedSize <= 0) {
            perror("Unable to read\n");
        }
        
        readedSize = readedSize + (int) curReadedSize;
    }while (readedSize != sizeToRead);
    
    return readedSize;
    
}

int performDiskReadJob(RecoveryThreadManager_t *recoveryThMgr, recoveryBuf_t *curRecoveryBuf){
    int bIdx;

    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)recoveryThMgr->enginePtr;

    for (bIdx = 0; bIdx < recoveryThMgr->sourceNodesSize; ++bIdx) {
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + bIdx;
        
        if (blockInfoPtr->selfFlag == 0) {
            continue;
        }
        
        if (blockInfoPtr->fd == -1) {
            blockInfoPtr->fd = startReadFile(blockInfoPtr->blockId, ecBlockServerEnginePtr->diskIOMgr, ecBlockServerEnginePtr->metaFd);
        }
        
        if (blockInfoPtr->fd < 0) {
            printf("Unable to start read for blockIdx:%llu\n", blockInfoPtr->blockId);
            return -1;
        }
        
        char *buf = curRecoveryBuf->inputBufsPtrs[bIdx] + *(curRecoveryBuf->bufWritedSize + bIdx);
        int readSize = *(curRecoveryBuf->bufToWriteSize + bIdx) - *(curRecoveryBuf->bufWritedSize + bIdx);
//        printf("readSize:%d bIdx:%d\n",readSize, bIdx);
        
        int readedSize = readLocalFile(blockInfoPtr->fd, buf, readSize, ecBlockServerEnginePtr->diskIOMgr);
        
        if (readedSize <= 0 || readedSize != readSize) {
            printf("readLocalFile error readedSize:%d for blockIdx:%llu\n",readedSize, blockInfoPtr->blockId);
            break;
        }
        
        *(curRecoveryBuf->bufWritedSize + bIdx) = *(curRecoveryBuf->bufWritedSize + bIdx) + readedSize;
    }
    
    return 0;
}

void closeReadedBlocksFds(RecoveryThreadManager_t *recoveryThMgr){
    int idx;
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)recoveryThMgr->enginePtr;

    for (idx = 0; idx < recoveryThMgr->sourceNodesSize; ++idx) {
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + idx;
        
        if (blockInfoPtr->selfFlag == 0) {
            continue;
        }

        closeFile(blockInfoPtr->fd, ecBlockServerEnginePtr->diskIOMgr);
        blockInfoPtr->fd = -1;

    }
}

void closeWritedBlocksFds(RecoveryThreadManager_t *recoveryThMgr){
    int idx , totalSize = recoveryThMgr->sourceNodesSize + recoveryThMgr->targetNodesSize;
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)recoveryThMgr->enginePtr;
    
    for (idx = recoveryThMgr->sourceNodesSize; idx < totalSize; ++idx) {
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + idx;
        closeFile(blockInfoPtr->fd, ecBlockServerEnginePtr->diskIOMgr);
        blockInfoPtr->fd = -1;
    }
}

void performWriteDiskJob(RecoveryThreadManager_t *recoveryThMgr, int bufIdx){
    int idx , totalSize = recoveryThMgr->sourceNodesSize + recoveryThMgr->targetNodesSize;
    recoveryBuf_t *curBuf = recoveryThMgr->recoveryBufs + bufIdx;
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)recoveryThMgr->enginePtr;

    for (idx = recoveryThMgr->sourceNodesSize; idx < totalSize; ++idx) {
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + idx;
        
        if (blockInfoPtr->fd == -1) {
            printf("startWriteFile\n");
            blockInfoPtr->fd = startWriteFile(blockInfoPtr->blockId, ecBlockServerEnginePtr->diskIOMgr, ecBlockServerEnginePtr->metaFd);
        }
        
        if (blockInfoPtr->fd < 0) {
            printf("Unable to write for blockIdx:%lu as fd < 0\n", blockInfoPtr->blockId);
            return ;
        }
    
        *(curBuf->bufWritedSize + idx) = (int)writeFile(blockInfoPtr->fd, curBuf->outputBufsPtrs[idx - recoveryThMgr->sourceNodesSize], *(curBuf->bufToWriteSize + idx), ecBlockServerEnginePtr->diskIOMgr,ecBlockServerEnginePtr->metaFd);
        
        if(*(curBuf->bufWritedSize + idx) != *(curBuf->bufToWriteSize + idx)){
            printf("Size to write:%d, Writed size:%d for blockIdx:%lu\n",*(curBuf->bufToWriteSize + idx), *(curBuf->bufWritedSize + idx), blockInfoPtr->blockId);
            return;
        }
    }
    
    return;
}


void *readDiskIOWorker(void *arg){
    RecoveryThreadManager_t *recoveryThMgr = (RecoveryThreadManager_t *)arg;
    int bufIdx = 0;
    
    char ch = 'D';
    while (recoveryThMgr->recoveryOverFlag == 0) {
        sem_wait(&recoveryThMgr->diskIOReadSem);
        
        if (recoveryThMgr->recoveryOverFlag == 1) {
            break;
        }
        
        if (recoveryThMgr->curBlockDone == 1) {
            closeReadedBlocksFds(recoveryThMgr);
            continue;
        }
        
        recoveryBuf_t *curRecoveryBuf = recoveryThMgr->recoveryBufs + bufIdx;
        
        performDiskReadJob(recoveryThMgr, curRecoveryBuf);
        
        write(recoveryThMgr->mgrPipes[1], (void *)(&ch), 1);
        
        if (recoveryThMgr->recoveryBufNum > 1) {
            bufIdx = (bufIdx + 1) % recoveryThMgr->recoveryBufNum;
        }
    }

    return arg;
}

void *writeDiskIOWorker(void *arg){
    RecoveryThreadManager_t *recoveryThMgr = (RecoveryThreadManager_t *)arg;
    int bufIdx = 0;
    
    while (recoveryThMgr->recoveryOverFlag == 0){
        sem_wait(&recoveryThMgr->ioWriteSem);
            
        if (recoveryThMgr->recoveryOverFlag == 1) {
            break;
        }
        
        if (recoveryThMgr->curBlockDone == 1) {
            closeWritedBlocksFds(recoveryThMgr);
            continue;
        }
            
        performWriteDiskJob(recoveryThMgr, bufIdx);
        sem_post(&(recoveryThMgr->writingFinishedSems[bufIdx]));
        
        if (recoveryThMgr->recoveryBufNum > 1) {
            bufIdx = (bufIdx + 1) % recoveryThMgr->recoveryBufNum;
        }
    }
    
    return arg;
}

