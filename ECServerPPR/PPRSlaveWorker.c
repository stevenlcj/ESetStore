//
//  PPRSlaveWorker.c
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/7.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "PPRSlaveWorker.h"
#include "ecCommon.h"
#include "BlockServerEngine.h"
#include "ecUtils.h"
#include "ecNetHandle.h"
#include "PPRCommand.h"
#include "PPRSlaveWorkerIOHandle.h"

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

int canParseBuf(char *buf, size_t BufSize){
    char suffixStr[] = "\r\n\0";
    size_t suffixStrLen = strlen(suffixStr);
    
    size_t offset = 0;
    while ((offset + suffixStrLen + 1) <= BufSize) {
        if (strncmp(suffixStr, (buf + offset), suffixStrLen) == 0) {
            if (*(buf + offset + suffixStrLen) == '\0') {
                return 1;
            }
        }
        
        offset = offset + 1;
    }
    
    return 0;
}

void handleTaskOver(PPRSlaveWorker_t *pprSlaveWorker, char *buf, size_t bufSize){
    size_t sendedSize = 0;
    do{
        ssize_t sendingSize = send(pprSlaveWorker->inSockFd, buf + sendedSize, (bufSize - sendedSize), 0);
        
        if (sendingSize > 0) {
            sendedSize = sendedSize + (size_t)sendingSize;
        }
    }while (sendedSize < bufSize);
        
    return;
}


/*
 Size: \r\n
 IP: \r\n
 Idx: \r\n
 Value: \r\n
 IP: \r\n
 Idx: \r\n
 Value: \r\n
 ...
 */

size_t getSlaveValues(PPRSlaveWorker_t *pprSlaveWorker, char *buf, size_t bufSize){
    char sizeStr[] = "Size:\0", suffixStr[] = "\r\n\0";
    char IdxStr[] = "Idx:", valueStr[] = "Value:";
    
    size_t offset = 0;
    pprSlaveWorker->sizeValue = getIntValueBetweenStrings(sizeStr, suffixStr, buf);
    pprSlaveWorker->idxinBlock = getIntValueBetweenStrings(IdxStr, suffixStr, buf);
    
    int value = getIntValueBetweenStrings(valueStr, suffixStr, buf);
    if (pprSlaveWorker->sizeValue > 1) {
        pprSlaveWorker->localMat = talloc(int, 2);
        *(pprSlaveWorker->localMat+1) = 1;
    }else{
        pprSlaveWorker->localMat = talloc(int, 2);
    }
    
    *pprSlaveWorker->localMat = value;
    
    while (strncmp(valueStr, buf + offset, strlen(valueStr)) != 0) {
        offset = offset + 1;
    }
    
    offset = offset + strlen(valueStr);
    
    while (strncmp(suffixStr, buf + offset, strlen(suffixStr)) != 0) {
        offset = offset + 1;
    }
    
    offset = offset + strlen(suffixStr);
    
    return offset;
}


void initPPRSlaveTask(PPRSlaveWorker_t *pprSlaveWorker, char *buf, size_t bufSize){
    char taskOverStr[] = "TaskOver\r\n\0";
    
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)pprSlaveWorker->serverEnginePtr;
    
    if (bufSize > strlen(taskOverStr) &&
        strncmp(taskOverStr, buf, strlen(taskOverStr)) == 0) {
        pprSlaveWorker->pprSlaveState = PPRWORKER_SLAVE_STATE_DONE;
        
        handleTaskOver(pprSlaveWorker, buf, bufSize);
        return;
    }
    
    pprSlaveWorker->pprSlaveState = PPRWORKER_SLAVE_STATE_WAITING;
    size_t offset = getSlaveValues(pprSlaveWorker, buf, bufSize);
    
    if (pprSlaveWorker->sizeValue > 1) {
        size_t endOffset = offset;
        char IPStr[] = "IP:", suffixStr[] = "\r\n\0";
        char theIP[20];

        while (strncmp(buf + endOffset, suffixStr, strlen(suffixStr)) != 0) {
            endOffset = endOffset + 1;
        }
        size_t IPLen = endOffset - (offset + strlen(IPStr));
        memcpy(theIP, buf + offset + strlen(IPStr), IPLen) ;
        theIP[IPLen] = '\0';
        
        printf("Next IP:%s\n", theIP);
        
        pprSlaveWorker->inSockFd = create_TCP_sockFd();
        
        struct sockaddr_in serv_addr;
        bzero((char *) &serv_addr, sizeof(serv_addr));
        
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_addr.s_addr = inet_addr(theIP);
        
        if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
            printf("Error on convert block server IP addr\n");
            printf("tryp connected server IP:%s\n", theIP);
        }
        
        serv_addr.sin_port = htons(ecBlockServerEnginePtr->pprPort);
        
        if (connect(pprSlaveWorker->inSockFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
            perror("Connect error to ppr slave server");
            return ;
        }
        
        char cmdBuf[1024];
        
        size_t cmdSize = formNextPPRCmd(cmdBuf, buf + offset, bufSize - offset, pprSlaveWorker->sizeValue - 1);
        
        size_t sendedSize = 0;
        do{
            ssize_t sendingSize = send(pprSlaveWorker->inSockFd, cmdBuf + sendedSize, (cmdSize - sendedSize), 0);
            
            if (sendingSize > 0) {
                sendedSize = sendedSize + (size_t)sendingSize;
            }
        }while (sendedSize < cmdSize);
    }
}

/**
 BlockGroupIdx:__\r\n
 BlockSize:__\r\n
 */

void getPPRBlockTask(PPRSlaveWorker_t *pprSlaveWorker, char *buf, size_t bufSize){
    char taskOverStr[] = "TaskOver\r\n\0";
    char blockGroupIdxStr[] = "BlockGroupIdx:\0", blockSizeStr[] = "BlockSize:\0", suffixStr[] = "\r\n\0";
    
    if (bufSize > strlen(taskOverStr) &&
        strncmp(taskOverStr, buf, strlen(taskOverStr)) == 0) {
        pprSlaveWorker->pprSlaveState = PPRWORKER_SLAVE_STATE_DONE;
        handleTaskOver(pprSlaveWorker, buf, bufSize);
        return;
    }
    
    pprSlaveWorker->blockGroupIdx = getUInt64ValueBetweenStrs(blockGroupIdxStr, suffixStr, buf, bufSize);
    pprSlaveWorker->blockSize = (size_t)getUInt64ValueBetweenStrs(blockSizeStr, suffixStr, buf, bufSize);
    pprSlaveWorker->pendingBlockSize = pprSlaveWorker->blockSize;
    pprSlaveWorker->blockId = pprSlaveWorker->blockGroupIdx + (uint64_t)pprSlaveWorker->idxinBlock;
    pprSlaveWorker->pprSlaveState = PPRWORKER_SLAVE_STATE_HANDLING_BLOCK;
    pprSlaveWorker->curBlockDone = 0;
    
    //Pass the message to next slave
    if (pprSlaveWorker->sizeValue > 1) {
        size_t sendedSize = 0;
        do{
            ssize_t sendingSize = send(pprSlaveWorker->inSockFd, buf + sendedSize, (bufSize - sendedSize), 0);
            
            if (sendingSize > 0) {
                sendedSize = sendedSize + (size_t)sendingSize;
            }
        }while (sendedSize < bufSize);
    }

}

void waitPPRTask(PPRSlaveWorker_t *pprSlaveWorker){

    int cmdDoneFlag = 0;
    printf("recving task\n");

    while (cmdDoneFlag != 1) {
        if (pprSlaveWorker->readMsgBuf->wOffset != 0 &&
            canParseBuf(pprSlaveWorker->readMsgBuf->buf, pprSlaveWorker->readMsgBuf->wOffset) == 1) {
            cmdDoneFlag = 1;
            break;
        }
        ssize_t recvSize = recv(pprSlaveWorker->outSockFd, pprSlaveWorker->readMsgBuf->buf + pprSlaveWorker->readMsgBuf->wOffset, (pprSlaveWorker->readMsgBuf->bufSize - pprSlaveWorker->readMsgBuf->wOffset), 0);
        
        if (recvSize <= 0) {
            continue;
        }
        
        pprSlaveWorker->readMsgBuf->wOffset = pprSlaveWorker->readMsgBuf->wOffset + (size_t)recvSize;
        
        if (canParseBuf(pprSlaveWorker->readMsgBuf->buf, pprSlaveWorker->readMsgBuf->wOffset) == 1) {
            cmdDoneFlag = 1;
        }
    }
    
    printf("recvd PPR cmd:%s\n",pprSlaveWorker->readMsgBuf->buf);
    
    if (pprSlaveWorker->pprSlaveState == PPRWORKER_SLAVE_STATE_UNINIT) {
        initPPRSlaveTask(pprSlaveWorker, pprSlaveWorker->readMsgBuf->buf, strlen(pprSlaveWorker->readMsgBuf->buf) + 1);
        dumpMsgWithCh(pprSlaveWorker->readMsgBuf, '\0');
        return;
    }
    
    if (pprSlaveWorker->pprSlaveState == PPRWORKER_SLAVE_STATE_WAITING) {
        getPPRBlockTask(pprSlaveWorker, pprSlaveWorker->readMsgBuf->buf, strlen(pprSlaveWorker->readMsgBuf->buf) + 1);
        dumpMsgWithCh(pprSlaveWorker->readMsgBuf, '\0');
        return;
    }
    
}

/**
 State Transition:
 PPRWORKER_SLAVE_STATE_UNINIT->PPRWORKER_SLAVE_STATE_WAITING or PPRWORKER_SLAVE_STATE_DONE >PPRWORKER_SLAVE_STATE_HANDLING_BLOCK or PPRWORKER_SLAVE_STATE_DONE
     -> PPRWORKER_SLAVE_STATE_WAITING or PPRWORKER_SLAVE_STATE_DONE
 */

void performPPRTask(PPRSlaveWorker_t *pprSlaveWorker){
    if (pprSlaveWorker->pprSlaveState == PPRWORKER_SLAVE_STATE_DONE) {
        printf("Recovery over\n");
        return;
    }
    
    if (pprSlaveWorker->pprSlaveState != PPRWORKER_SLAVE_STATE_HANDLING_BLOCK ) {
        return;
    }
    
    printf("start perform ppr task\n");
    
    int recoveryFlag = 1, idx;
    
    while (recoveryFlag == 1) {
        for (idx = 0; idx < pprSlaveWorker->bufNum; ++idx) {
            
            if (pprSlaveWorker->pprWorkerBufsInUseFlag[idx] == 1) {
                sem_wait(&pprSlaveWorker->writingFinishedSems[idx]);
                pprSlaveWorker->pprWorkerBufsInUseFlag[idx] = 0;
            }
            
            PPRWorkerBuf_t *curBufPtr = pprSlaveWorker->pprWorkerBufs + idx;
            
            int taskSize = 0;
            
            if(pprSlaveWorker->pendingBlockSize > curBufPtr->bufSize){
                taskSize= (int) curBufPtr->bufSize;
            }else{
                taskSize= (int) pprSlaveWorker->pendingBlockSize;
            }
            
            pprSlaveWorker->pendingBlockSize = pprSlaveWorker->pendingBlockSize - taskSize;
            
            if (taskSize != 0) {
                pprSlaveWorker->pprWorkerBufsInUseFlag[idx] = 1;
                
                curBufPtr->taskSize = taskSize;
                curBufPtr->netBufToFillSize = curBufPtr->taskSize;
                curBufPtr->localBufToWriteSize = curBufPtr->taskSize;
                curBufPtr->resutBufToWriteSize = curBufPtr->taskSize;
                
                curBufPtr->netBufFilledSize = 0;
                curBufPtr->localBufFilledSize = 0;
                curBufPtr->resultFilledSize = 0;
                
                sem_post(&pprSlaveWorker->diskReadSem);
                if (pprSlaveWorker->sizeValue > 1) {
                    sem_post(&pprSlaveWorker->netReadSem);
                }
            }else{
                recoveryFlag = 0;
                break;
            }
            
        }
    }
    
    for (idx = 0; idx < pprSlaveWorker->bufNum; ++idx) {
        //Wait all task finished
        if (pprSlaveWorker->pprWorkerBufsInUseFlag[idx] == 1) {
            sem_wait(&pprSlaveWorker->writingFinishedSems[idx]);
            pprSlaveWorker->pprWorkerBufsInUseFlag[idx] = 0;
        }
    }
    
    pprSlaveWorker->curBlockDone = 1;
    pprSlaveWorker->pprSlaveState = PPRWORKER_SLAVE_STATE_WAITING;
    //Recovery done for current blocks, notify pertinent threads to close relvevant blocks or fds
    sem_post(&pprSlaveWorker->diskReadSem);
    if (pprSlaveWorker->sizeValue > 1) {
        sem_post(&pprSlaveWorker->netReadSem);
        sem_post(&pprSlaveWorker->netReadDoneSem);
    }
    
    sem_post(&pprSlaveWorker->codingDoneSem);
    
    printf("curBlockDone:%d\n", pprSlaveWorker->curBlockDone);

    
}

void *startPPRSlaveWorker(void *arg){
    PPRSlaveWorker_t *pprSlaveWorker = (PPRSlaveWorker_t *)arg;
    
    pthread_create(&pprSlaveWorker->diskReadPid, NULL, slaveDiskReadWorker, (void *)pprSlaveWorker);
    pthread_create(&pprSlaveWorker->netReadPid, NULL, slaveNetReadWorker, (void *)pprSlaveWorker);
    pthread_create(&pprSlaveWorker->netWritePid, NULL, slaveNetWriteWorker, (void *)pprSlaveWorker);
    pthread_create(&pprSlaveWorker->codingPid, NULL, slaveCodingWorker, (void *)pprSlaveWorker);

    while (pprSlaveWorker->recoveryOverFlag != 1) {
        waitPPRTask(pprSlaveWorker);
        performPPRTask(pprSlaveWorker);
        
        if (pprSlaveWorker->pprSlaveState == PPRWORKER_SLAVE_STATE_DONE) {
            pprSlaveWorker->recoveryOverFlag = 1;
            sem_post(&pprSlaveWorker->diskReadSem);
            sem_post(&pprSlaveWorker->netReadSem);
            sem_post(&pprSlaveWorker->codingDoneSem);
            sem_post(&pprSlaveWorker->netReadDoneSem);
            
            close(pprSlaveWorker->outSockFd);
            if (pprSlaveWorker->sizeValue > 1) {
                close(pprSlaveWorker->inSockFd);
            }
        }
    }
    
    deallocPPRSlaveWorker(pprSlaveWorker);
    return arg;
}

PPRSlaveWorker_t *createPPRSlaveWorker(int outSockFd, int bufNum, size_t bufSize, void *serverEnginePtr){
    PPRSlaveWorker_t *pprSlaveWorker = talloc(PPRSlaveWorker_t, 1);
    int idx;
    
    pprSlaveWorker->outSockFd = outSockFd;
    pprSlaveWorker->inSockFd = -1;
    pprSlaveWorker->bufNum = bufNum;
    pprSlaveWorker->bufSize = bufSize;
    
    pprSlaveWorker->pprWorkerBufs = talloc(PPRWorkerBuf_t, bufNum);
    pprSlaveWorker->recoveryOverFlag = 0;
    
    pprSlaveWorker->pprSlaveState = PPRWORKER_SLAVE_STATE_UNINIT;
    pprSlaveWorker->serverEnginePtr = serverEnginePtr;
    
    pprSlaveWorker->recoveryOverFlag = 0;
    
    pprSlaveWorker->readMsgBuf = createMessageBuf(1024);
    
    if(sem_init(&(pprSlaveWorker->diskReadSem), 0, 0) == -1){
        perror("sem_init diskReadSem failed:\n");
    }
    
    if(sem_init(&(pprSlaveWorker->diskReadDoneSem), 0, 0) == -1){
        perror("sem_init diskReadDoneSem failed:\n");
    }
    
    if(sem_init(&(pprSlaveWorker->netReadSem), 0, 0) == -1){
        perror("sem_init netReadSem failed:\n");
    }
    
    if(sem_init(&(pprSlaveWorker->netReadDoneSem), 0, 0) == -1){
        perror("sem_init netReadDoneSem failed:\n");
    }
    
    if(sem_init(&(pprSlaveWorker->codingDoneSem), 0, 0) == -1){
        perror("sem_init codingDoneSem failed:\n");
    }

    pprSlaveWorker->writingFinishedSems = talloc(sem_t, bufNum);
    pprSlaveWorker->pprWorkerBufsInUseFlag = talloc(int, bufNum);
    for (idx = 0; idx < bufNum; ++idx) {
        PPRWorkerBuf_t *bufPtr = pprSlaveWorker->pprWorkerBufs + idx;
        bufPtr->bufSize = bufSize;
        char *buf = talloc(char, 3*bufSize);
        bufPtr->localIOBuf = buf;
        bufPtr->netIOBuf = bufPtr->localIOBuf + bufSize;
        bufPtr->resultBuf = bufPtr->netIOBuf + bufSize;
        
        bufPtr->netBufFilledSize = 0;
        bufPtr->netBufToFillSize = 0;
        bufPtr->localBufToWriteSize = 0;
        bufPtr->localBufFilledSize = 0;
        bufPtr->resutBufToWriteSize = 0;
        bufPtr->resultFilledSize = 0;
        bufPtr->taskSize = 0;
        
        if(sem_init(&(pprSlaveWorker->writingFinishedSems[idx]), 0, 0) == -1){
            perror("sem_init writingFinishedSems failed:\n");
        }
        
        pprSlaveWorker->pprWorkerBufsInUseFlag[idx] = 0;
    }
    
    pprSlaveWorker->localMat = NULL;
        
    return pprSlaveWorker;
}

void deallocPPRSlaveWorker(PPRSlaveWorker_t *pprSlaveWorker){
    int idx;
    
    pthread_join(pprSlaveWorker->diskReadPid, NULL);
    pthread_join(pprSlaveWorker->netReadPid, NULL);
    pthread_join(pprSlaveWorker->netWritePid, NULL);
    pthread_join(pprSlaveWorker->codingPid, NULL);
    
    for (idx = 0; idx < pprSlaveWorker->bufNum; ++idx) {
        PPRWorkerBuf_t *bufPtr = pprSlaveWorker->pprWorkerBufs + idx;
        free(bufPtr->localIOBuf);
        sem_destroy(&pprSlaveWorker->writingFinishedSems[idx]);
    }
    
    free(pprSlaveWorker->pprWorkerBufs);
    free(pprSlaveWorker->pprWorkerBufsInUseFlag);
    free(pprSlaveWorker->writingFinishedSems);
    
    if (pprSlaveWorker->localMat != NULL) {
        free(pprSlaveWorker->localMat);
    }
    
    sem_destroy(&pprSlaveWorker->diskReadSem);
    sem_destroy(&pprSlaveWorker->diskReadDoneSem);
    sem_destroy(&pprSlaveWorker->netReadSem);
    sem_destroy(&pprSlaveWorker->netReadDoneSem);
    sem_destroy(&pprSlaveWorker->codingDoneSem);
    
}








