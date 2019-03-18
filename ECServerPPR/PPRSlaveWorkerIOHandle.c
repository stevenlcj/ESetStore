//
//  PPRSlaveWorkerIOHandle.c
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/12.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "PPRSlaveWorkerIOHandle.h"
#include "PPRSlaveWorker.h"

#include "DiskIOHandler.h"
#include "BlockServerEngine.h"
#include "jerasure.h"

#include <semaphore.h>
#include <unistd.h>
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


void *slaveDiskReadWorker(void *arg){
    PPRSlaveWorker_t *pprSlaveWorker = (PPRSlaveWorker_t *)arg;
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)pprSlaveWorker->serverEnginePtr;
    
    int bufIdx = 0, readFd = -1;
    
    while (pprSlaveWorker->recoveryOverFlag != 1) {
        
        sem_wait(&pprSlaveWorker->diskReadSem);

        if (pprSlaveWorker->recoveryOverFlag == 1) {
            if (readFd >= 0) {
                closeFile(readFd, ecBlockServerEnginePtr->diskIOMgr);
                readFd = -1;
            }
            break;
        }
        
        if (pprSlaveWorker->curBlockDone == 1) {
            bufIdx = 0;
            if (readFd >= 0) {
                closeFile(readFd, ecBlockServerEnginePtr->diskIOMgr);
                readFd = -1;
            }
            
            continue;
        }
        
        if (readFd == -1) {
            readFd = startReadFile(pprSlaveWorker->blockId, ecBlockServerEnginePtr->diskIOMgr);
        }
        
        char *buf = (pprSlaveWorker->pprWorkerBufs + bufIdx)->localIOBuf;
        int readSize = (int) (pprSlaveWorker->pprWorkerBufs + bufIdx)->localBufToWriteSize;
        
        ssize_t readedSize = readLocalFile(readFd, buf, readSize, ecBlockServerEnginePtr->diskIOMgr);
        
        if (readedSize <= 0 || readedSize != readSize) {
            printf("readLocalFile error readedSize:%ld for blockIdx:%lu\n",readedSize, pprSlaveWorker->blockId);
            break;
        }
        
        (pprSlaveWorker->pprWorkerBufs + bufIdx)->localBufFilledSize =(pprSlaveWorker->pprWorkerBufs + bufIdx)->localBufFilledSize + readedSize;
        
        sem_post(&pprSlaveWorker->diskReadDoneSem);
//        printf("disk read:%d, bIdx:%d\n",readSize, bufIdx);

        bufIdx = (bufIdx + 1) % pprSlaveWorker->bufNum;
    }
    
    return arg;
}

void *slaveNetWriteWorker(void *arg){
    PPRSlaveWorker_t *pprSlaveWorker = (PPRSlaveWorker_t *)arg;
    int bufIdx = 0;
    
    while (pprSlaveWorker->recoveryOverFlag != 1) {
        
        sem_wait(&pprSlaveWorker->codingDoneSem);
        
        if (pprSlaveWorker->recoveryOverFlag == 1) {
            break;
        }
        
        if (pprSlaveWorker->curBlockDone == 1) {
            bufIdx = 0;
            continue;
        }
        
        
        char *buf = (pprSlaveWorker->pprWorkerBufs + bufIdx)->resultBuf;
        size_t pendingSize = (pprSlaveWorker->pprWorkerBufs + bufIdx)->resutBufToWriteSize;
        size_t sendedSize = 0;
        
        do{
            ssize_t sendingSize = send(pprSlaveWorker->outSockFd, buf + sendedSize, (pendingSize - sendedSize), 0);
            
            if (sendingSize > 0) {
                sendedSize = sendedSize + (size_t)sendingSize;
            }
            
        }while (sendedSize < pendingSize);
        
        (pprSlaveWorker->pprWorkerBufs + bufIdx)->resultFilledSize = sendedSize;
        
        sem_post(&pprSlaveWorker->writingFinishedSems[bufIdx]);
//        printf("net write:%lu, bIdx:%d\n",sendedSize, bufIdx);

        bufIdx = (bufIdx + 1) % pprSlaveWorker->bufNum;
    }
    
    return arg;
}

void *slaveNetReadWorker(void *arg){
    PPRSlaveWorker_t *pprSlaveWorker = (PPRSlaveWorker_t *)arg;
    int bufIdx = 0;
    
    while (pprSlaveWorker->recoveryOverFlag != 1) {
        
        sem_wait(&pprSlaveWorker->netReadSem);
        
        if (pprSlaveWorker->recoveryOverFlag == 1) {
            break;
        }
        
        if (pprSlaveWorker->curBlockDone == 1) {
            bufIdx = 0;
            continue;
        }
        
        
        char *buf = (pprSlaveWorker->pprWorkerBufs + bufIdx)->netIOBuf;
        size_t pendingSize = (pprSlaveWorker->pprWorkerBufs + bufIdx)->netBufToFillSize;
        size_t recvdSize = 0;
        
        do{
            ssize_t recvingSize = recv(pprSlaveWorker->inSockFd, buf + recvdSize, (pendingSize - recvdSize), 0);
            
            if (recvingSize > 0) {
                recvdSize = recvdSize + (size_t)recvingSize;
            }
            
        }while (recvdSize < pendingSize);
        
        (pprSlaveWorker->pprWorkerBufs + bufIdx)->netBufFilledSize = recvdSize;
        
        sem_post(&pprSlaveWorker->netReadDoneSem);
//        printf("net read:%lu, bIdx:%d\n",recvdSize, bufIdx);

        bufIdx = (bufIdx + 1) % pprSlaveWorker->bufNum;
    }
    
    return arg;
}

void *slaveCodingWorker(void *arg){
    PPRSlaveWorker_t *pprSlaveWorker = (PPRSlaveWorker_t *)arg;
    int bufIdx = 0;
    
    char **dataPtrs, **codingPtr;
    dataPtrs = talloc(char *, 2);
    codingPtr = talloc(char *, 1);
    
    while (pprSlaveWorker->recoveryOverFlag != 1) {
        sem_wait(&pprSlaveWorker->diskReadDoneSem);
        if (pprSlaveWorker->sizeValue > 1) {
            sem_wait(&pprSlaveWorker->netReadDoneSem);
        }
        if (pprSlaveWorker->recoveryOverFlag == 1) {
            break;
        }
        
        if (pprSlaveWorker->curBlockDone == 1) {
            bufIdx = 0;
            continue;
        }
        
        dataPtrs[0] = (pprSlaveWorker->pprWorkerBufs + bufIdx)->localIOBuf;
        dataPtrs[1] = (pprSlaveWorker->pprWorkerBufs + bufIdx)->netIOBuf;
        codingPtr[0] = (pprSlaveWorker->pprWorkerBufs + bufIdx)->resultBuf;
        
        if (pprSlaveWorker->sizeValue > 1) {
            jerasure_matrix_encode(2, 1, 8, pprSlaveWorker->localMat, dataPtrs, codingPtr, (pprSlaveWorker->pprWorkerBufs + bufIdx)->taskSize);
        }else{
            jerasure_matrix_encode(1, 1, 8, pprSlaveWorker->localMat, dataPtrs, codingPtr, (pprSlaveWorker->pprWorkerBufs + bufIdx)->taskSize);
        }
        
        sem_post(&pprSlaveWorker->codingDoneSem);
//        printf("coding:%d, bIdx:%d\n", (pprSlaveWorker->pprWorkerBufs + bufIdx)->taskSize, bufIdx);

        bufIdx = (bufIdx + 1) % pprSlaveWorker->bufNum;
        
    }
    
    free(dataPtrs);
    free(codingPtr);
    
    return arg;
}
