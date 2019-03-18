//
//  RecoveryThreadWorker.c
//  ECStoreCPUServer
//
//  Created by Liu Chengjian on 18/1/9.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "RecoveryThreadWorker.h"
#include "BlockServerEngine.h"
#include "GCRSMatrix.h"
#include "ecUtils.h"
#include "ecNetHandle.h"
#include "ecCommon.h"
#include "NetIOWorker.h"
#include "CodingWorker.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

void createRecoveryThreads(RecoveryThreadManager_t* recoveryThMgr);
void startRecoveryThreadMgr(RecoveryThreadManager_t* recoveryThMgr);

void getESetValue(const char *Buf, int *eSetIdx){
    char str1[]="ESetIdx:", str2[]="\r\n\0";
    int value = getIntValueBetweenStrings(str1, str2, Buf);
    
    *eSetIdx = value;
    return ;
}

void getStripeK(const char *Buf, int *stripeK){
    char str1[]="stripeK:\0", str2[]="\r\n\0";
    int value = getIntValueBetweenStrings(str1, str2, Buf);
    
    *stripeK = value;
}

void getStripeM(const char *Buf, int *stripeM){
    char str1[]="stripeM:\0", str2[]="\r\n\0";
    int value = getIntValueBetweenStrings(str1, str2, Buf);
    
    *stripeM = value;
}

void getStripeW(const char *Buf, int *stripeW){
    char str1[]="stripeW:\0", str2[]="\r\n\0";
    int value = getIntValueBetweenStrings(str1, str2, Buf);
    
    *stripeW = value;
}

void getMatrix(const char *buf, size_t stripeK, size_t stripeM, int *matrix){
    char str1[]="matrix:\0", str2[] = "\r\n\0";
    size_t matSize = stripeK * stripeM;
    
    getMatrixValues(str1, str2, buf, matrix, matSize);
    printMatrix(matrix, (int)stripeM, (int)stripeK);
    
}

void getBlocksInfo(RecoveryThreadManager_t *recoveryThMgr, blockInfo_t *blockInfo, int blocksNum, char *initStr){
    ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)recoveryThMgr->enginePtr;
    
    int curOffset = 0, startOffset, endOffset;
    char SourceNodeIdxHeader[] = "SourceNodeIdx:\0";
    char IPStr[] = "IP:\0";
    char PortStr[] = "Port:\0";
    char sufixStr[] = "\r\n\0";
    
    while (strncmp(SourceNodeIdxHeader, (initStr + curOffset), strlen(SourceNodeIdxHeader)) != 0) {
        ++curOffset;
    }
    
    int idx;
    startOffset = curOffset;
    
    for (idx = 0; idx < blocksNum; ++idx) {
        blockInfo_t *curBlockPtr = blockInfo + idx;
        //        printf("cur str:%s\n", initStr+startOffset);
        curBlockPtr->index = idx;
        curBlockPtr->idx = getIntValueBetweenStrings(SourceNodeIdxHeader, sufixStr, initStr + startOffset);
        curBlockPtr->fd = -1;
        curOffset = startOffset + (int) strlen(SourceNodeIdxHeader) + (int) strlen(sufixStr);
        
        while (strncmp(IPStr, initStr+curOffset, strlen(IPStr)) != 0) {
            ++curOffset;
        }
        
        startOffset = curOffset + (int)strlen(IPStr);
        endOffset = startOffset;
        
        while (strncmp(sufixStr, initStr+endOffset, strlen(sufixStr)) != 0) {
            ++endOffset;
        }
        
        curBlockPtr->IPSize = endOffset - startOffset;
        
        memcpy(curBlockPtr->IPAddr, initStr+startOffset, curBlockPtr->IPSize);
        curBlockPtr->IPAddr[curBlockPtr->IPSize] = '\0';
        
        curBlockPtr->port = getIntValueBetweenStrings(PortStr, sufixStr, initStr + endOffset);
        
        
        if (curBlockPtr->IPSize == blockServerEnginePtr->chunkServerIPSize &&
            (strncmp(curBlockPtr->IPAddr, blockServerEnginePtr->chunkServerIP, (size_t)curBlockPtr->IPSize) == 0) &&
            curBlockPtr->port == blockServerEnginePtr->chunkServerPort) {
            curBlockPtr->selfFlag = 1;
            curBlockPtr->curNetState = BLOCK_NET_STATE_UINIT;
            curBlockPtr->readMsgBuf = NULL;
            curBlockPtr->writeMsgBuf = NULL;
            printf("self idx:%d IP:%s Port:%d\n", curBlockPtr->idx, curBlockPtr->IPAddr, curBlockPtr->port);
            
        }else{
            curBlockPtr->selfFlag = 0;
            curBlockPtr->curNetState = BLOCK_NET_STATE_UINIT;
        
            curBlockPtr->readMsgBuf = createMessageBuf(0);
            curBlockPtr->writeMsgBuf = createMessageBuf(0);
        }
        
        *(recoveryThMgr->dm_idx+idx) = curBlockPtr->idx;
    }
}

void getTargeNodesIdx(RecoveryThreadManager_t *recoveryThMgr, int *idxPtr, int targetNodesNum, char *initStr){
    int idx, curOffset = 0, startOffset;
    char TargetNodeIdxHeader[] = "TargetNodeIdx:\0";
    char sufixStr[] = "\r\n\0";
    
    while (strncmp(TargetNodeIdxHeader, (initStr + curOffset), strlen(TargetNodeIdxHeader)) != 0) {
        ++curOffset;
    }
    
    printf("curOffset:%d\n", curOffset);
    startOffset = curOffset;
    
    for (idx = 0; idx < targetNodesNum; ++idx) {
        *(idxPtr + idx) = getIntValueBetweenStrings(TargetNodeIdxHeader, sufixStr, initStr + startOffset);
        *(recoveryThMgr->erasures+idx) = *(idxPtr + idx);
        startOffset = startOffset + (int)strlen(TargetNodeIdxHeader) + (int)strlen(sufixStr);
        printf("idx:%d *(idxPtr + idx):%d\n",idx, *(idxPtr + idx));
    }
    
    *(recoveryThMgr->erasures+idx) = -1;
}

void initSourceTargetNodes(RecoveryThreadManager_t* recoveryThMgr, char *initStr){
    char sourceNodeSizeHeader[] = "SourceNodesSize:\0";
    //    char sourceNodeIdxHeader[] = "SourceNodeIdx:\0";
    char TargetNodeSizeHeader[] = "TargetNodesSize:\0";
    //    char TargetNodeIdxHeader[] = "TargetNodeIdx:\0";
    //    char IPStr[] = "IP:\0";
    //    char PortStr[] = "Port:\0";
    char sufixStr[] = "\r\n\0";
    
    recoveryThMgr->sourceNodesSize = getIntValueBetweenStrings(sourceNodeSizeHeader, sufixStr, initStr);
    recoveryThMgr->targetNodesSize = getIntValueBetweenStrings(TargetNodeSizeHeader, sufixStr, initStr);

    recoveryThMgr->blockInfoPtrs = talloc(blockInfo_t, (recoveryThMgr->sourceNodesSize  + recoveryThMgr->targetNodesSize));
    recoveryThMgr->targetNodesIdx = talloc(int, recoveryThMgr->targetNodesSize + 1);
    *(recoveryThMgr->targetNodesIdx + recoveryThMgr->targetNodesSize) = -1;
    printf("recoveryThMgr->sourceNodesSize:%d\n",recoveryThMgr->sourceNodesSize);
    
    getBlocksInfo(recoveryThMgr, recoveryThMgr->blockInfoPtrs, recoveryThMgr->sourceNodesSize, initStr);
    getTargeNodesIdx(recoveryThMgr, recoveryThMgr->targetNodesIdx, recoveryThMgr->targetNodesSize, initStr);
    
    int idx;
    for (idx = recoveryThMgr->sourceNodesSize; idx < (recoveryThMgr->sourceNodesSize + recoveryThMgr->targetNodesSize); ++idx) {
        blockInfo_t *curBlockInfoPtr = recoveryThMgr->blockInfoPtrs + idx;
        curBlockInfoPtr->index = idx;
        curBlockInfoPtr->idx = *(recoveryThMgr->targetNodesIdx + idx - recoveryThMgr->sourceNodesSize);
        curBlockInfoPtr->fd = -1;
        curBlockInfoPtr->curNetState = BLOCK_NET_STATE_UINIT;
    }
}

void createRecoveryBufs(RecoveryThreadManager_t *recoveryThMgr){
    int idx;

    recoveryThMgr->recoveryBufs = talloc(recoveryBuf_t, recoveryThMgr->recoveryBufNum);
    recoveryThMgr->recoveryBufIdx = 0;
    
    memset(recoveryThMgr->recoveryBufs, 0, sizeof(recoveryBuf_t) * recoveryThMgr->recoveryBufNum);
    
    recoveryThMgr->writingFinishedSems = talloc(sem_t, recoveryThMgr->recoveryBufNum);
    recoveryThMgr->recoveryBufsInUseFlag = talloc(int, recoveryThMgr->recoveryBufNum);

    for (idx = 0; idx < recoveryThMgr->recoveryBufNum; ++idx) {
        recoveryBuf_t *curRecoveryBuf = recoveryThMgr->recoveryBufs + idx;
        
        if (initRecoveryBuf(curRecoveryBuf, idx, recoveryThMgr->stripeK, recoveryThMgr->targetNodesSize, recoveryThMgr->recoveryBufSize) == -1) {
            printf("init buf:%d failed\n", idx);
        }
        
        if(sem_init(&(recoveryThMgr->writingFinishedSems[idx]), 0, 0) == -1){
            perror("sem_init failed:\n");
        }

        recoveryThMgr->recoveryBufsInUseFlag[idx] = 0;

    }
    
}
/**
 Cmd:
 RecoveryNodes\r\n
 ESetIdx:__\r\n
 StripeK:__\r\n
 StripeM:__\r\n
 StripeW:__\r\n
 matrix:....\r\n
 SourceNodeSize:__\r\n
 SourceNodeIdx:__\r\n
 IP:__\r\n
 Port:__\r\n
 ...
 TargetNodeSize:__\r\n
 TargetNodeIdx:__\r\n
 ...
 TargetNodeIdx:__\r\n\0
 */

RecoveryThreadManager_t *createRecoveryThreadMgr(char *initStr, void *enginePtr){
    
    printf("Str for init recovery:\n%s\n",initStr);
    
    RecoveryThreadManager_t *recoveryThMgr = talloc(RecoveryThreadManager_t, 1);
    
    memset(recoveryThMgr, 0, sizeof(RecoveryThreadManager_t));
    
    recoveryThMgr->enginePtr = enginePtr;
    
    if (sem_init(&recoveryThMgr->waitingJobSem, 0, 0) == -1) {
        perror("waitingJobSem sem_init error:");
    }
    
    recoveryThMgr->recoveryOverFlag = 0;
    
    getESetValue(initStr, &recoveryThMgr->eSetIdx);
    getStripeK(initStr, &recoveryThMgr->stripeK);
    getStripeM(initStr, &recoveryThMgr->stripeM);
    getStripeW(initStr, &recoveryThMgr->stripeW);
    
    size_t matSize = (recoveryThMgr->stripeK  * recoveryThMgr->stripeM);
    recoveryThMgr->matrix = talloc(int, matSize);
    getMatrix(initStr, recoveryThMgr->stripeK, recoveryThMgr->stripeM , recoveryThMgr->matrix);
    
    recoveryThMgr->dm_idx = talloc(int, recoveryThMgr->stripeK);
    recoveryThMgr->erasures = talloc(int, recoveryThMgr->stripeK + recoveryThMgr->stripeM + 1);
    initSourceTargetNodes(recoveryThMgr, initStr);
    
    recoveryThMgr->decodingMatrix = grs_create_decoding_matrix(recoveryThMgr->stripeK, recoveryThMgr->stripeM, recoveryThMgr->stripeW, recoveryThMgr->matrix, recoveryThMgr->targetNodesIdx, recoveryThMgr->erasures, recoveryThMgr->dm_idx);
    
    printf("\n*******matrix********\n");
    printMatrix(recoveryThMgr->matrix, recoveryThMgr->stripeK, recoveryThMgr->stripeM);
    
    printf("\n*******decodingMatrix********\n");
    printMatrix(recoveryThMgr->decodingMatrix, recoveryThMgr->stripeK , recoveryThMgr->targetNodesSize );
    
    ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)recoveryThMgr->enginePtr;
    
    recoveryThMgr->recoveryBufSize = blockServerEnginePtr->recoveryBufSize;
    recoveryThMgr->recoveryBufNum = blockServerEnginePtr->recoveryBufNum;
    
    recoveryThMgr->localDiskIOReadNums = 0;
    recoveryThMgr->netIOReadNums = 0;
    
    if(pipe(recoveryThMgr->mgrPipes) != 0){
        perror("unable to create mgrPipes");
    }
    
    make_non_blocking_sock(recoveryThMgr->mgrPipes[0]);
    
    recoveryThMgr->recoveryEfd = create_epollfd();
    
    recoveryThMgr->blockGroupIdx = 0;
    
    createRecoveryBufs(recoveryThMgr);
    
    if(sem_init(&(recoveryThMgr->ioReadSem), 0, 0) == -1){
        perror("sem_init ioReadSem failed:\n");
    }
    
    if(sem_init(&(recoveryThMgr->diskIOReadSem), 0, 0) == -1){
        perror("sem_init diskIOReadSem failed:\n");
    }

    if(sem_init(&(recoveryThMgr->codingSem), 0, 0) == -1){
        perror("sem_init codingSem failed:\n");
    }

    if(sem_init(&(recoveryThMgr->ioWriteSem), 0, 0) == -1){
        perror("sem_init  ioWriteSem failed:\n");
    }
    
    createRecoveryThreads(recoveryThMgr);
    
    return recoveryThMgr;
}

int initRecoveryBuf(recoveryBuf_t *recoveryBufPtr, int bufIdx, int inputBufNum, int outputBufNum, int eachBufSize){
    recoveryBufPtr->bufIdx = bufIdx;
    recoveryBufPtr->inputBufNum = inputBufNum;
    recoveryBufPtr->outputBufNum = outputBufNum;
    recoveryBufPtr->eachBufSize = eachBufSize;
    
    recoveryBufPtr->buf = talloc(char, (recoveryBufPtr->eachBufSize * (inputBufNum + outputBufNum)));
    
    if (recoveryBufPtr->buf == NULL) {
        printf("Unable to alloc memory for buf:%d\n", recoveryBufPtr->bufIdx);
        return -1;
    }
    
    recoveryBufPtr->bufWritedSize = talloc(int, inputBufNum + outputBufNum);
    recoveryBufPtr->bufToWriteSize = talloc(int, inputBufNum + outputBufNum);
    
    memset(recoveryBufPtr->bufWritedSize, 0 , sizeof(int) * (inputBufNum + outputBufNum));
    memset(recoveryBufPtr->bufToWriteSize, 0 , sizeof(int) * (inputBufNum + outputBufNum));

    recoveryBufPtr->inputBufsPtrs = talloc(char *, recoveryBufPtr->inputBufNum);
    recoveryBufPtr->outputBufsPtrs = talloc(char *, recoveryBufPtr->outputBufNum);
    
    if (recoveryBufPtr->inputBufsPtrs == NULL || recoveryBufPtr->outputBufsPtrs == NULL) {
        deallocRecoveryBuf(recoveryBufPtr);
        printf("Unable to alloc memory ptrs for buf:%d\n", recoveryBufPtr->bufIdx);
        return -1;
    }
    
    int idx;
    
//    printf("recoveryBufPtr->buf address:%p , each buf size:%d\n",recoveryBufPtr->buf, recoveryBufPtr->eachBufSize);
    
    for (idx = 0; idx < recoveryBufPtr->inputBufNum; ++idx) {
        *(recoveryBufPtr->inputBufsPtrs + idx) = recoveryBufPtr->buf + recoveryBufPtr->eachBufSize * idx;
//        printf("input buf idx:%d address:%p\n",idx, *(recoveryBufPtr->inputBufsPtrs + idx));

    }
    
    for (idx = 0; idx < recoveryBufPtr->outputBufNum; ++idx) {
        *(recoveryBufPtr->outputBufsPtrs + idx) = recoveryBufPtr->buf + recoveryBufPtr->eachBufSize * (idx + recoveryBufPtr->inputBufNum);
//        printf("output buf idx:%d address:%p\n",idx, *(recoveryBufPtr->outputBufsPtrs + idx));

    }
    
    return 0;
}

void deallocRecoveryBuf(recoveryBuf_t *recoveryBufPtr){
    if (recoveryBufPtr->inputBufsPtrs != NULL) {
        free(recoveryBufPtr->inputBufsPtrs);
    }
    
    if (recoveryBufPtr->outputBufsPtrs != NULL) {
        free(recoveryBufPtr->outputBufsPtrs);
    }
    
    if (recoveryBufPtr->buf != NULL) {
        free(recoveryBufPtr->buf);
    }
    
    return;
}

void createRecoveryThreads(RecoveryThreadManager_t* recoveryThMgr){
    int idx;
    
    for (idx = 0; idx < recoveryThMgr->sourceNodesSize; ++idx) {
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + idx;
        
        if (blockInfoPtr->selfFlag == 1) {
            ++recoveryThMgr->localDiskIOReadNums;
        }else{
            ++recoveryThMgr->netIOReadNums;
        }
    }

    startRecoveryThreadMgr(recoveryThMgr);

}

int initRecoveryTask(RecoveryThreadManager_t *recoveryThMgr,  char *buf){
    char str1[]="ESetIdx:", str2[]="\r\n\0", recoveryBlockGroupIdHeader[] = "BlockGroupId:\0", recoveryBlockSizeHeader[] = "BlockSize:\0";;
    
    //int setIdx = getIntValueBetweenStrings(str1, str2, buf);

    size_t bufSize = strlen(buf);
    uint64_t blockGroupIdx = getUInt64ValueBetweenStrs(recoveryBlockGroupIdHeader, str2, buf, bufSize);
    
//    printf("buf:%s\n",buf);
//    printf("Task eESet:%d, blockGroupIdx:%lu\n", setIdx, blockGroupIdx);
    
    if (recoveryThMgr->blockGroupIdx != 0) {
        printf("Error: our recovery doesn't finish yet while new task coming\n");
        return -1;
    }
    
    recoveryThMgr->blockGroupIdx = blockGroupIdx;
    
    int idx;
    char blockStr[DEFAUT_MESSGAE_READ_WRITE_BUF];
    for (idx = 0; idx < (recoveryThMgr->sourceNodesSize + recoveryThMgr->targetNodesSize); ++idx) {
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + idx;
        blockInfoPtr->blockId = recoveryThMgr->blockGroupIdx + blockInfoPtr->idx;
        
        uint64_to_str(blockInfoPtr->blockId, blockStr, DEFAUT_MESSGAE_READ_WRITE_BUF);
        size_t blockStrSize = strlen(blockStr);
//        printf("blockStrSize:%lu blockStr:%s\n",blockStrSize, blockStr);

        memcpy((blockStr+ blockStrSize), str2, strlen(str2));
        blockStrSize = blockStrSize + strlen(str2);
//        printf("blockStrSize:%lu\n",blockStrSize);
        memcpy((blockStr+ blockStrSize), recoveryBlockSizeHeader, strlen(recoveryBlockSizeHeader));
        blockStrSize = blockStrSize + strlen(recoveryBlockSizeHeader);
        blockStr[blockStrSize] = '\0';
        
//        printf("blockStrSize:%lu blockStr:%s\n",blockStrSize, blockStr);
        
        blockInfoPtr->totalSize = (int )getUInt64ValueBetweenStrs(blockStr, str2, buf, bufSize);
        blockInfoPtr->totalPendingHandleSize = blockInfoPtr->totalSize;
//        printf("idx:%d totalPendingHandleSize:%d",idx, blockInfoPtr->totalPendingHandleSize);
        blockInfoPtr->totalHandledSize = 0;
        blockInfoPtr->pendingHandleSize = 0;
        blockInfoPtr->handledSize = 0;
    }
    
    recoveryThMgr->curBlockDone = 0;
    sem_post(&recoveryThMgr->waitingJobSem);
    return 0;
}

int assignTask(RecoveryThreadManager_t* recoveryThMgr, recoveryBuf_t *curBufPtr){
    int blocksIdx, taskSize = 0;
    
    for (blocksIdx = recoveryThMgr->sourceNodesSize; blocksIdx < (recoveryThMgr->sourceNodesSize + recoveryThMgr->targetNodesSize); ++blocksIdx) {
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + blocksIdx;
        
        if (blockInfoPtr->totalPendingHandleSize != 0) {
            int thTaskSize = recoveryThMgr->recoveryBufSize > blockInfoPtr->totalPendingHandleSize ? blockInfoPtr->totalPendingHandleSize : recoveryThMgr->recoveryBufSize;
            
            if (taskSize < thTaskSize) {
                taskSize = thTaskSize;
            }
        }
    }
    
    if (taskSize == 0) {
        return taskSize;
    }
    
    curBufPtr->taskSize = taskSize;
    
    // To do: Alignment for taskSize taskSize = ...
    
    for (blocksIdx = 0; blocksIdx < (recoveryThMgr->sourceNodesSize + recoveryThMgr->targetNodesSize); ++blocksIdx) {
        
        blockInfo_t *blockInfoPtr = recoveryThMgr->blockInfoPtrs + blocksIdx;
        int blockHandleSize = 0;
        
        if (blockInfoPtr->totalPendingHandleSize >= curBufPtr->taskSize) {
            blockHandleSize = curBufPtr->taskSize;
        }else{
            blockHandleSize = blockInfoPtr->totalPendingHandleSize;
        }
        
        curBufPtr->bufWritedSize[blocksIdx] = 0;
        curBufPtr->bufToWriteSize[blocksIdx] = blockHandleSize;
        
        blockInfoPtr->totalPendingHandleSize = blockInfoPtr->totalPendingHandleSize - blockHandleSize;
        blockInfoPtr->totalHandledSize = blockInfoPtr->totalHandledSize + blockHandleSize;
        
    }

    return taskSize;
}

void resetRecoveryBufs(RecoveryThreadManager_t* recoveryThMgr){
    int bufIdx;
    
    for (bufIdx = 0; bufIdx < recoveryThMgr->recoveryBufNum; ++bufIdx) {
        recoveryBuf_t *recoveryBufPtr = recoveryThMgr->recoveryBufs + bufIdx;
        
        recoveryBufPtr->taskSize = 0;
        memset(recoveryBufPtr->bufToWriteSize, 0, sizeof(int)* (recoveryBufPtr->inputBufNum + recoveryBufPtr->outputBufNum));
        memset(recoveryBufPtr->bufWritedSize, 0, sizeof(int)* (recoveryBufPtr->inputBufNum + recoveryBufPtr->outputBufNum));
        
    }
}

void *recoveryMgrThread(void *arg){
    RecoveryThreadManager_t* recoveryThMgr = (RecoveryThreadManager_t*)arg;
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)recoveryThMgr->enginePtr;
    /**
     a. Create netIO thread
     b. Create diskReadIO thread if necessary
     c. Create coding thread
     d. Create diskWriteIO thread
     */
    
    int idx;
    
    if (recoveryThMgr->localDiskIOReadNums != 0) {
        // To do: launch thread to ..
        pthread_create(&recoveryThMgr->diskReadPid, NULL, readDiskIOWorker, (void *) recoveryThMgr);
    }
    
    if (recoveryThMgr->netIOReadNums != 0 || recoveryThMgr->localDiskIOReadNums != 0) {
        // To do: launch thread to ..
        pthread_create(&recoveryThMgr->ioReadPid, NULL, netDiskIOWorker, (void *) recoveryThMgr);
    }
    
    pthread_create(&recoveryThMgr->codingPid, NULL, thCodingThread, (void *) recoveryThMgr);
    
    pthread_create(&recoveryThMgr->ioWritePid, NULL, writeDiskIOWorker, (void *) recoveryThMgr);
    
    
    while (recoveryThMgr->recoveryOverFlag == 0) {
        sem_wait(&recoveryThMgr->waitingJobSem);
        
        if (recoveryThMgr->recoveryOverFlag == 1) {
            break;
        }
        
        
        int recoveryFlag = 1;
        
        while (recoveryFlag == 1) {
            for (idx = 0; idx < recoveryThMgr->recoveryBufNum; ++idx) {
                
                if (recoveryThMgr->recoveryBufsInUseFlag[idx] == 1) {
                    sem_wait(&recoveryThMgr->writingFinishedSems[idx]);
                    recoveryThMgr->recoveryBufsInUseFlag[idx] = 0;
                }
                
                recoveryBuf_t *curBufPtr = recoveryThMgr->recoveryBufs + idx;
                
                int taskSize = assignTask(recoveryThMgr, curBufPtr);
                
                if (taskSize != 0) {
                    recoveryThMgr->recoveryBufsInUseFlag[idx] = 1;
                    sem_post(&recoveryThMgr->ioReadSem);
                }else{
//                    printf("*********taskSize = 0 task done ***********\n");
                    recoveryFlag = 0;
                    // No more task.
                    break;
                }
                
            }
        }
        
        for (idx = 0; idx < recoveryThMgr->recoveryBufNum; ++idx) {
            //Wait all task finished
            if (recoveryThMgr->recoveryBufsInUseFlag[idx] == 1) {
                sem_wait(&recoveryThMgr->writingFinishedSems[idx]);
                recoveryThMgr->recoveryBufsInUseFlag[idx] = 0;
            }
        }
        
        
        recoveryThMgr->curBlockDone = 1;
        
        
        resetRecoveryBufs(recoveryThMgr);
        
        //Recovery done for current blocks, notify pertinent threads to close relvevant blocks or fds
        sem_post(&recoveryThMgr->ioReadSem);
        //Notify the writing thread to close its fd
        sem_post(&recoveryThMgr->ioWriteSem);

        printf("curBlockDone:%d\n", recoveryThMgr->curBlockDone);
        
        char ch = 'D';

        write(ecBlockServerEnginePtr->serverRecoveryPipes[1], &ch, 1);

    }
    
    return (void *)recoveryThMgr;
}

void startRecoveryThreadMgr(RecoveryThreadManager_t* recoveryThMgr){
    pthread_create(&recoveryThMgr->pid, NULL, recoveryMgrThread, (void *)recoveryThMgr);
}

void revokeThreads(RecoveryThreadManager_t *recoveryThMgr){
    sem_post(&recoveryThMgr->ioReadSem);
    sem_post(&recoveryThMgr->diskIOReadSem);
    sem_post(&recoveryThMgr->codingSem);
    sem_post(&recoveryThMgr->ioWriteSem);
    
    pthread_join(recoveryThMgr->ioReadPid, NULL);
    printf("ioRead Thread exit\n");
    
    pthread_join(recoveryThMgr->diskReadPid, NULL);
    printf("diskRead Thread exit\n");
    
    pthread_join(recoveryThMgr->codingPid, NULL);
    printf("coding Thread exit\n");
    
    pthread_join(recoveryThMgr->ioWritePid, NULL);
    printf("diskWrite Thread exit\n");
}

void deallocRecoveryThreadMgr(RecoveryThreadManager_t *recoveryThMgr){
    recoveryThMgr->recoveryOverFlag = 1;

    revokeThreads(recoveryThMgr);
    
}





