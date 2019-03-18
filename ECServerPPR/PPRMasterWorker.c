//
//  PPRMasterWorker.c
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/7.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "PPRMasterWorker.h"

#include "BlockServerEngine.h"
#include "GCRSMatrix.h"
#include "ecUtils.h"
#include "ecNetHandle.h"
#include "ecCommon.h"
#include "PPRCommand.h"
#include "PPRMasterWorkerIOHandle.h"

#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>

void reorderBlocksAndVector(PPRMasterWorker_t *pprMasterWorkerPtr){
    int idx, selfIdx = -1, middleValue;
    
    for (idx = 0; idx < pprMasterWorkerPtr->sourceNodesSize; ++idx) {
        blockInfo_t *blockPtr = pprMasterWorkerPtr->blockInfoPtrs + idx;
        if (blockPtr->selfFlag == 1) {
            selfIdx = idx;
            break;
        }
    }
    
    if (selfIdx != 0) {
        middleValue = *(pprMasterWorkerPtr->decodingVector);
        *(pprMasterWorkerPtr->decodingVector) = *(pprMasterWorkerPtr->decodingVector + selfIdx);
        *(pprMasterWorkerPtr->decodingVector + selfIdx) = middleValue;
        
        
        blockInfo_t *selfBlockInfoPtr = pprMasterWorkerPtr->blockInfoPtrs+selfIdx;
        blockInfo_t middleBlockInfo;
        middleBlockInfo.IPSize = pprMasterWorkerPtr->blockInfoPtrs->IPSize;
        
        memcpy(middleBlockInfo.IPAddr, pprMasterWorkerPtr->blockInfoPtrs->IPAddr, middleBlockInfo.IPSize);
        middleBlockInfo.port = pprMasterWorkerPtr->blockInfoPtrs->port;
        middleBlockInfo.idx = pprMasterWorkerPtr->blockInfoPtrs->idx;
        
        pprMasterWorkerPtr->blockInfoPtrs->IPSize = selfBlockInfoPtr->IPSize;
        memcpy(pprMasterWorkerPtr->blockInfoPtrs->IPAddr, selfBlockInfoPtr->IPAddr, selfBlockInfoPtr->IPSize);
        pprMasterWorkerPtr->blockInfoPtrs->IPAddr[selfBlockInfoPtr->IPSize] = '\0';
        pprMasterWorkerPtr->blockInfoPtrs->port = selfBlockInfoPtr->port;
        pprMasterWorkerPtr->blockInfoPtrs->idx = selfBlockInfoPtr->idx;

        selfBlockInfoPtr->IPSize = middleBlockInfo.IPSize;
        memcpy(selfBlockInfoPtr->IPAddr, middleBlockInfo.IPAddr, middleBlockInfo.IPSize);
        selfBlockInfoPtr->IPAddr[selfBlockInfoPtr->IPSize] = '\0';
        selfBlockInfoPtr->port = middleBlockInfo.port;
        selfBlockInfoPtr->idx = middleBlockInfo.idx;
    }
    
    pprMasterWorkerPtr->localMat = talloc(int, 2);
    *(pprMasterWorkerPtr->localMat) = *(pprMasterWorkerPtr->decodingVector);
    *(pprMasterWorkerPtr->localMat + 1) = 1;
}

void connectFirstSlaveWorker(PPRMasterWorker_t *pprMasterWorkerPtr){
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)pprMasterWorkerPtr->enginePtr;

    pprMasterWorkerPtr->firstSlaveFd = create_TCP_sockFd();

    blockInfo_t *selectedBlockPtr = pprMasterWorkerPtr->blockInfoPtrs+1;
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(selectedBlockPtr->IPAddr);
    
    if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
        printf("Error on convert block server IP addr\n");
        printf("tryp connected server IP:%s\n", selectedBlockPtr->IPAddr);
    }
    
    serv_addr.sin_port = htons(ecBlockServerEnginePtr->pprPort);
    
    if (connect(pprMasterWorkerPtr->firstSlaveFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Connect error to ppr slave server");
        return ;
    }
    
    printf("Connect firstSlaveFd:%s port:%d succeed\n", selectedBlockPtr->IPAddr,ecBlockServerEnginePtr->pprPort );
}

void sendPPRSlaveStartCmd(PPRMasterWorker_t *pprMasterWorkerPtr){
    char cmdBuf[1024];
    size_t cmdSize = formPPRCmd(cmdBuf, 1024, (void *)pprMasterWorkerPtr);
    size_t sendedSize = 0;
    do{
        ssize_t sendingSize = send(pprMasterWorkerPtr->firstSlaveFd, (cmdBuf + sendedSize), (cmdSize - sendedSize), 0);
        
        if (sendingSize > 0) {
            sendedSize = sendedSize + (size_t)sendingSize;
        }
        
    }while (sendedSize != cmdSize);
}

void startPPRMasterWork(PPRMasterWorker_t *pprMasterWorkerPtr){
    connectFirstSlaveWorker(pprMasterWorkerPtr);
    sendPPRSlaveStartCmd(pprMasterWorkerPtr);
    
    pthread_create(&pprMasterWorkerPtr->diskReadPid, NULL, diskReadWorker, (void *)pprMasterWorkerPtr);
    pthread_create(&pprMasterWorkerPtr->netReadPid, NULL, netReadWorker, (void *)pprMasterWorkerPtr);
    pthread_create(&pprMasterWorkerPtr->codingPid, NULL, codingWorker, (void *)pprMasterWorkerPtr);
    pthread_create(&pprMasterWorkerPtr->diskWritePid, NULL, diskWriteWorker, (void *)pprMasterWorkerPtr);

}

void recoveryDone(PPRMasterWorker_t *pprMasterWorkerPtr){
}

int assignTask(PPRMasterWorker_t *pprMasterWorkerPtr, PPRWorkerBuf_t *curBufPtr){
    int blocksIdx, taskSize = 0;
    
    for (blocksIdx = pprMasterWorkerPtr->sourceNodesSize; blocksIdx < (pprMasterWorkerPtr->sourceNodesSize + pprMasterWorkerPtr->targetNodesSize); ++blocksIdx) {
        blockInfo_t *blockInfoPtr = pprMasterWorkerPtr->blockInfoPtrs + blocksIdx;
        
        if (blockInfoPtr->totalPendingHandleSize != 0) {
            int thTaskSize = pprMasterWorkerPtr->recoveryBufSize > blockInfoPtr->totalPendingHandleSize ? blockInfoPtr->totalPendingHandleSize : pprMasterWorkerPtr->recoveryBufSize;
            
            if (taskSize < thTaskSize) {
                taskSize = thTaskSize;
            }
        }
    }
    
    if (taskSize == 0) {
        return taskSize;
    }
    
    curBufPtr->taskSize = taskSize;
    
    curBufPtr->localBufToWriteSize = taskSize;
    curBufPtr->netBufToFillSize = taskSize;
    curBufPtr->resutBufToWriteSize = taskSize;
    
    curBufPtr->localBufFilledSize = 0;
    curBufPtr->netBufFilledSize = 0;
    curBufPtr->resultFilledSize = 0;

    
    for (blocksIdx = 0; blocksIdx < (pprMasterWorkerPtr->sourceNodesSize + pprMasterWorkerPtr->targetNodesSize); ++blocksIdx) {
        
        blockInfo_t *blockInfoPtr = pprMasterWorkerPtr->blockInfoPtrs + blocksIdx;
        int blockHandleSize = 0;
        
        if (blockInfoPtr->totalPendingHandleSize >= curBufPtr->taskSize) {
            blockHandleSize = curBufPtr->taskSize;
        }else{
            blockHandleSize = blockInfoPtr->totalPendingHandleSize;
        }
        
        blockInfoPtr->totalPendingHandleSize = blockInfoPtr->totalPendingHandleSize - blockHandleSize;
        blockInfoPtr->totalHandledSize = blockInfoPtr->totalHandledSize + blockHandleSize;
    }
    // To do: Alignment for taskSize taskSize = ...
    
    return taskSize;
}

void resetRecoveryBufs(PPRMasterWorker_t *pprMasterWorkerPtr){
    int bufIdx;
    
    for (bufIdx = 0; bufIdx < pprMasterWorkerPtr->recoveryBufNum; ++bufIdx) {
        PPRWorkerBuf_t *recoveryBufPtr = pprMasterWorkerPtr->recoveryBufs + bufIdx;
        
        recoveryBufPtr->taskSize = 0;
        
        recoveryBufPtr->localBufToWriteSize = 0;
        recoveryBufPtr->netBufToFillSize = 0;
        recoveryBufPtr->resutBufToWriteSize = 0;

        recoveryBufPtr->localBufFilledSize = 0;
        recoveryBufPtr->netBufFilledSize = 0;
        recoveryBufPtr->resultFilledSize = 0;
        
    }
}

void *startPPRMasterWorker(void *arg){
    PPRMasterWorker_t *pprMasterWorkerPtr = (PPRMasterWorker_t *)arg;
    int idx;
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)pprMasterWorkerPtr->enginePtr;

    reorderBlocksAndVector(pprMasterWorkerPtr);
    startPPRMasterWork(pprMasterWorkerPtr);
    
    while (pprMasterWorkerPtr->recoveryOverFlag == 0) {
        sem_wait(&pprMasterWorkerPtr->waitingJobSem);
        
        char  cmdBuf[1024];
        
        size_t cmdSize = formPPRRecoveryCmd(cmdBuf, 1024, pprMasterWorkerPtr->blockGroupIdx, (pprMasterWorkerPtr->blockInfoPtrs + pprMasterWorkerPtr->sourceNodesSize)->totalSize);
        size_t sendedSize = 0;
        
        do{
            ssize_t sendingSize = send(pprMasterWorkerPtr->firstSlaveFd, (cmdBuf + sendedSize), (cmdSize - sendedSize), 0);
            
            if (sendingSize > 0) {
                sendedSize = sendedSize + (size_t)sendingSize;
            }
            
        }while (sendedSize < cmdSize);

        if (pprMasterWorkerPtr->recoveryOverFlag == 1) {
            recoveryDone(pprMasterWorkerPtr);
            break;
        }
        
        int recoveryFlag = 1;
        
        while (recoveryFlag == 1) {
            for (idx = 0; idx < pprMasterWorkerPtr->recoveryBufNum; ++idx) {
                
                if (pprMasterWorkerPtr->recoveryBufsInUseFlag[idx] == 1) {
                    sem_wait(&pprMasterWorkerPtr->writingFinishedSems[idx]);
                    pprMasterWorkerPtr->recoveryBufsInUseFlag[idx] = 0;
                }
                
                PPRWorkerBuf_t *curBufPtr = pprMasterWorkerPtr->recoveryBufs + idx;
                
                int taskSize = assignTask(pprMasterWorkerPtr, curBufPtr);
                
                if (taskSize != 0) {
                    pprMasterWorkerPtr->recoveryBufsInUseFlag[idx] = 1;
                    sem_post(&pprMasterWorkerPtr->diskReadSem);
                    sem_post(&pprMasterWorkerPtr->netReadSem);
                }else{
                    recoveryFlag = 0;
                    break;
                }
                
            }
        }
        
        for (idx = 0; idx < pprMasterWorkerPtr->recoveryBufNum; ++idx) {
            //Wait all task finished
            if (pprMasterWorkerPtr->recoveryBufsInUseFlag[idx] == 1) {
                sem_wait(&pprMasterWorkerPtr->writingFinishedSems[idx]);
                pprMasterWorkerPtr->recoveryBufsInUseFlag[idx] = 0;
            }
        }
        
        
        pprMasterWorkerPtr->curBlockDone = 1;
        
        resetRecoveryBufs(pprMasterWorkerPtr);
        
        //Recovery done for current blocks, notify pertinent threads to close relvevant blocks or fds
        sem_post(&pprMasterWorkerPtr->diskReadSem);
        //Notify the writing thread to close its fd
        sem_post(&pprMasterWorkerPtr->netReadSem);
        sem_post(&pprMasterWorkerPtr->diskReadDoneSem);
        sem_post(&pprMasterWorkerPtr->netReadDoneSem);
        sem_post(&pprMasterWorkerPtr->codingDoneSem);
        
        printf("curBlockDone:%d\n", pprMasterWorkerPtr->curBlockDone);
        
        char ch = 'D';
        
        write(ecBlockServerEnginePtr->serverRecoveryPipes[1], &ch, 1);

    }
    
    return arg;
}

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

void getBlocksInfo(PPRMasterWorker_t *pprMasterWorkerPtr, blockInfo_t *blockInfo, int blocksNum, char *initStr){
    ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)pprMasterWorkerPtr->enginePtr;
    
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
        
        printf("index:%d, idx:%d\n", curBlockPtr->index, curBlockPtr->idx);
        
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
            printf("self idx:%d IP:%s Port:%d\n", curBlockPtr->idx, curBlockPtr->IPAddr, curBlockPtr->port);
            
        }
        
        *(pprMasterWorkerPtr->dm_idx+idx) = curBlockPtr->idx;
    }
}

void getTargeNodesIdx(PPRMasterWorker_t *pprMasterWorkerPtr, int *idxPtr, int targetNodesNum, char *initStr){
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
        *(pprMasterWorkerPtr->erasures+idx) = *(idxPtr + idx);
        startOffset = startOffset + (int)strlen(TargetNodeIdxHeader) + (int)strlen(sufixStr);
        printf("idx:%d *(idxPtr + idx):%d\n",idx, *(idxPtr + idx));
    }
    
    *(pprMasterWorkerPtr->erasures+idx) = -1;
}

void initSourceTargetNodes(PPRMasterWorker_t *pprMasterWorkerPtr, char *initStr){
    char sourceNodeSizeHeader[] = "SourceNodesSize:\0";
    //    char sourceNodeIdxHeader[] = "SourceNodeIdx:\0";
    char TargetNodeSizeHeader[] = "TargetNodesSize:\0";
    //    char TargetNodeIdxHeader[] = "TargetNodeIdx:\0";
    //    char IPStr[] = "IP:\0";
    //    char PortStr[] = "Port:\0";
    char sufixStr[] = "\r\n\0";
    
    pprMasterWorkerPtr->sourceNodesSize = getIntValueBetweenStrings(sourceNodeSizeHeader, sufixStr, initStr);
    pprMasterWorkerPtr->targetNodesSize = getIntValueBetweenStrings(TargetNodeSizeHeader, sufixStr, initStr);
    
    pprMasterWorkerPtr->blockInfoPtrs = talloc(blockInfo_t, (pprMasterWorkerPtr->sourceNodesSize  + pprMasterWorkerPtr->targetNodesSize));
    pprMasterWorkerPtr->targetNodesIdx = talloc(int, pprMasterWorkerPtr->targetNodesSize + 1);
    *(pprMasterWorkerPtr->targetNodesIdx + pprMasterWorkerPtr->targetNodesSize) = -1;
    printf("pprMasterWorkerPtr->sourceNodesSize:%d\n",pprMasterWorkerPtr->sourceNodesSize);
    printf("pprMasterWorkerPtr->targetNodesSize:%d\n",pprMasterWorkerPtr->targetNodesSize);

    getBlocksInfo(pprMasterWorkerPtr, pprMasterWorkerPtr->blockInfoPtrs, pprMasterWorkerPtr->sourceNodesSize, initStr);
    getTargeNodesIdx(pprMasterWorkerPtr, pprMasterWorkerPtr->targetNodesIdx, pprMasterWorkerPtr->targetNodesSize, initStr);
    
    int idx;
    for (idx = pprMasterWorkerPtr->sourceNodesSize; idx < (pprMasterWorkerPtr->sourceNodesSize + pprMasterWorkerPtr->targetNodesSize); ++idx) {
        blockInfo_t *curBlockInfoPtr = pprMasterWorkerPtr->blockInfoPtrs + idx;
        curBlockInfoPtr->index = idx;
        curBlockInfoPtr->idx = *(pprMasterWorkerPtr->targetNodesIdx + idx - pprMasterWorkerPtr->sourceNodesSize);
    }
}

void createPPRMasterBufs(PPRMasterWorker_t *pprMasterWorkerPtr){
    int idx;
    
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)pprMasterWorkerPtr->enginePtr;
    
    pprMasterWorkerPtr->recoveryBufNum = ecBlockServerEnginePtr->recoveryBufNum;
    pprMasterWorkerPtr->recoveryBufSize = ecBlockServerEnginePtr->recoveryBufSize;
    
    pprMasterWorkerPtr->recoveryBufs = talloc(PPRWorkerBuf_t, pprMasterWorkerPtr->recoveryBufNum);
    pprMasterWorkerPtr->recoveryBufIdx = 0;
    
    memset(pprMasterWorkerPtr->recoveryBufs, 0, sizeof(PPRWorkerBuf_t) * pprMasterWorkerPtr->recoveryBufNum);
    
    pprMasterWorkerPtr->writingFinishedSems = talloc(sem_t, pprMasterWorkerPtr->recoveryBufNum);
    pprMasterWorkerPtr->recoveryBufsInUseFlag = talloc(int, pprMasterWorkerPtr->recoveryBufNum);
    
    for (idx = 0; idx < pprMasterWorkerPtr->recoveryBufNum; ++idx) {
        
        PPRWorkerBuf_t *bufPtr = pprMasterWorkerPtr->recoveryBufs + idx;
        bufPtr->bufSize = pprMasterWorkerPtr->recoveryBufSize;
        char *buf = talloc(char, 3*bufPtr->bufSize);
        bufPtr->localIOBuf = buf;
        bufPtr->netIOBuf = bufPtr->localIOBuf + bufPtr->bufSize;
        bufPtr->resultBuf = bufPtr->netIOBuf + bufPtr->bufSize;
        
        bufPtr->localBufFilledSize = 0;
        bufPtr->netBufFilledSize = 0;
        
        bufPtr->localBufToWriteSize = 0;
        bufPtr->netBufToFillSize = 0;
        bufPtr->resutBufToWriteSize = 0;
        
        if(sem_init(&(pprMasterWorkerPtr->writingFinishedSems[idx]), 0, 0) == -1){
            perror("sem_init failed:\n");
        }
        
        pprMasterWorkerPtr->recoveryBufsInUseFlag[idx] = 0;
        
    }
    
}

PPRMasterWorker_t *createPPRMasterWorker(char *initStr, void *enginePtr){
    PPRMasterWorker_t *pprMasterWorkerPtr = talloc(PPRMasterWorker_t, 1);
    
    if (pprMasterWorkerPtr == NULL) {
        printf("unable to alloc pprMasterWorkerPtr\n");
        return NULL;
    }
    
    pprMasterWorkerPtr->enginePtr = enginePtr;
    
    pprMasterWorkerPtr->recoveryOverFlag = 0;
    
    getESetValue(initStr, &pprMasterWorkerPtr->eSetIdx);
    getStripeK(initStr, &pprMasterWorkerPtr->stripeK);
    getStripeM(initStr, &pprMasterWorkerPtr->stripeM);
    getStripeW(initStr, &pprMasterWorkerPtr->stripeW);
    
    size_t matSize = (pprMasterWorkerPtr->stripeK  * pprMasterWorkerPtr->stripeM);
    pprMasterWorkerPtr->matrix = talloc(int, matSize);
    getMatrix(initStr, pprMasterWorkerPtr->stripeK, pprMasterWorkerPtr->stripeM , pprMasterWorkerPtr->matrix);
    
    pprMasterWorkerPtr->dm_idx = talloc(int, pprMasterWorkerPtr->stripeK);
    pprMasterWorkerPtr->erasures = talloc(int, pprMasterWorkerPtr->stripeK + pprMasterWorkerPtr->stripeM + 1);
    initSourceTargetNodes(pprMasterWorkerPtr, initStr);
    
    pprMasterWorkerPtr->decodingVector = grs_create_decoding_matrix(pprMasterWorkerPtr->stripeK, pprMasterWorkerPtr->stripeM, pprMasterWorkerPtr->stripeW, pprMasterWorkerPtr->matrix, pprMasterWorkerPtr->targetNodesIdx, pprMasterWorkerPtr->erasures, pprMasterWorkerPtr->dm_idx);
    
    pprMasterWorkerPtr->blockGroupIdx = 0;
    pprMasterWorkerPtr->curBlockDone = 0;
    
    pprMasterWorkerPtr->recoveryOverFlag = 0;
    
    if(sem_init(&(pprMasterWorkerPtr->waitingJobSem), 0, 0) == -1){
        perror("sem_init taskStartSem failed:\n");
    }
    
//    if(sem_init(&(pprMasterWorkerPtr->taskEndSem), 0, 0) == -1){
//        perror("sem_init taskEndSem failed:\n");
//    }
    
    if(sem_init(&(pprMasterWorkerPtr->diskReadSem), 0, 0) == -1){
        perror("sem_init taskStartSem failed:\n");
    }
    
    if(sem_init(&(pprMasterWorkerPtr->diskReadDoneSem), 0, 0) == -1){
        perror("sem_init taskEndSem failed:\n");
    }
    
    if(sem_init(&(pprMasterWorkerPtr->netReadSem), 0, 0) == -1){
        perror("sem_init taskStartSem failed:\n");
    }
    
    if(sem_init(&(pprMasterWorkerPtr->netReadDoneSem), 0, 0) == -1){
        perror("sem_init taskEndSem failed:\n");
    }
    
    if(sem_init(&(pprMasterWorkerPtr->codingDoneSem), 0, 0) == -1){
        perror("sem_init taskStartSem failed:\n");
    }
    
    createPPRMasterBufs(pprMasterWorkerPtr);
    
    printf("The decodingVector*************\n");
    printMatrix(pprMasterWorkerPtr->decodingVector, pprMasterWorkerPtr->sourceNodesSize, pprMasterWorkerPtr->targetNodesSize);
    printf("The decodingVector*************\n");
    
    pthread_create(&pprMasterWorkerPtr->pid, NULL, startPPRMasterWorker, (void *)pprMasterWorkerPtr);
    

    return pprMasterWorkerPtr;
}

void deallocPPRMasterWorker(PPRMasterWorker_t *pprMasterWorkerPtr){

}

int initPPRTask(PPRMasterWorker_t *pprMasterWorkerPtr,  char *buf){
    char str2[]="\r\n\0", recoveryBlockGroupIdHeader[] = "BlockGroupId:\0", recoveryBlockSizeHeader[] = "BlockSize:\0";;
    
    //int setIdx = getIntValueBetweenStrings(str1, str2, buf);
    
    size_t bufSize = strlen(buf);
    uint64_t blockGroupIdx = getUInt64ValueBetweenStrs(recoveryBlockGroupIdHeader, str2, buf, bufSize);
    
    //    printf("buf:%s\n",buf);
    printf("Task eESet:%d, blockGroupIdx:%lu\n", pprMasterWorkerPtr->eSetIdx, blockGroupIdx);
    
    if (pprMasterWorkerPtr->blockGroupIdx != 0) {
        printf("Error: our recovery doesn't finish yet while new task coming\n");
        return -1;
    }
    
    pprMasterWorkerPtr->blockGroupIdx = blockGroupIdx;
    
    pprMasterWorkerPtr->blockInfoPtrs->blockId = pprMasterWorkerPtr->blockInfoPtrs->idx + pprMasterWorkerPtr->blockGroupIdx;
    printf("local blockIdx:%lu\n", pprMasterWorkerPtr->blockInfoPtrs->blockId);
    
    int idx;
    char blockStr[DEFAUT_MESSGAE_READ_WRITE_BUF];
    for (idx = 0; idx < (pprMasterWorkerPtr->sourceNodesSize + pprMasterWorkerPtr->targetNodesSize); ++idx) {
        blockInfo_t *blockInfoPtr = pprMasterWorkerPtr->blockInfoPtrs + idx;
        blockInfoPtr->blockId = pprMasterWorkerPtr->blockGroupIdx + blockInfoPtr->idx;
        
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
        blockInfoPtr->totalHandledSize = 0;
        blockInfoPtr->pendingHandleSize = 0;
        blockInfoPtr->handledSize = 0;
    }
    
    pprMasterWorkerPtr->curBlockDone = 0;
    
    sem_post(&pprMasterWorkerPtr->waitingJobSem);
    
    return 0;
}

















