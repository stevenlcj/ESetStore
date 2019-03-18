//
//  PPRMasterWorkerIOHandle.h
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/10.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "PPRMasterWorkerIOHandle.h"
#include "PPRMasterWorker.h"
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

void *diskReadWorker(void *arg){
    PPRMasterWorker_t *pprMasterWorkerPtr = (PPRMasterWorker_t *)arg;
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)pprMasterWorkerPtr->enginePtr;

    int bufIdx = 0, readFd = -1;
    
    while (pprMasterWorkerPtr->recoveryOverFlag != 1) {
        
        sem_wait(&pprMasterWorkerPtr->diskReadSem);
        
        if (pprMasterWorkerPtr->recoveryOverFlag == 1) {
            if (readFd >= 0) {
                closeFile(readFd, ecBlockServerEnginePtr->diskIOMgr);
                readFd = -1;
            }
            break;
        }
        
        if (pprMasterWorkerPtr->curBlockDone == 1) {
            bufIdx = 0;
            if (readFd >= 0) {
                closeFile(readFd, ecBlockServerEnginePtr->diskIOMgr);
                readFd = -1;
            }

            continue;
        }
        
        if (readFd == -1) {
            readFd = startReadFile(pprMasterWorkerPtr->blockInfoPtrs->blockId, ecBlockServerEnginePtr->diskIOMgr);
        }
        
        char *buf = (pprMasterWorkerPtr->recoveryBufs + bufIdx)->localIOBuf;
        int readSize = (int) (pprMasterWorkerPtr->recoveryBufs + bufIdx)->localBufToWriteSize;
        
        int readedSize = readLocalFile(readFd, buf, readSize, ecBlockServerEnginePtr->diskIOMgr);
        
        if (readedSize <= 0 || readedSize != readSize) {
            printf("readLocalFile error readedSize:%d for blockIdx:%lu\n",readedSize, pprMasterWorkerPtr->blockInfoPtrs->blockId);
            break;
        }
        
        (pprMasterWorkerPtr->recoveryBufs + bufIdx)->localBufFilledSize =(pprMasterWorkerPtr->recoveryBufs + bufIdx)->localBufFilledSize + readedSize;

        sem_post(&pprMasterWorkerPtr->diskReadDoneSem);
        
//        printf("local read size:%d, bIdx:%d\n", readSize, bufIdx);
        bufIdx = (bufIdx + 1) % pprMasterWorkerPtr->recoveryBufNum;
    }

    return arg;
}

void *diskWriteWorker(void *arg){
    PPRMasterWorker_t *pprMasterWorkerPtr = (PPRMasterWorker_t *)arg;
    ECBlockServerEngine_t *ecBlockServerEnginePtr = (ECBlockServerEngine_t *)pprMasterWorkerPtr->enginePtr;

    int bufIdx = 0, writeFd = -1;
    while (pprMasterWorkerPtr->recoveryOverFlag != 1) {
        
        sem_wait(&pprMasterWorkerPtr->codingDoneSem);
        if (pprMasterWorkerPtr->recoveryOverFlag == 1) {
            break;
        }
        
        if (pprMasterWorkerPtr->curBlockDone == 1) {
            bufIdx = 0;
            if (writeFd != -1) {
                closeFile(writeFd, ecBlockServerEnginePtr->diskIOMgr);
                writeFd = -1;
            }
            continue;
        }
        
        if (writeFd == -1) {
            writeFd = startWriteFile((pprMasterWorkerPtr->blockInfoPtrs + pprMasterWorkerPtr->sourceNodesSize)->blockId, ecBlockServerEnginePtr->diskIOMgr);
        }
        
        char *buf = (pprMasterWorkerPtr->recoveryBufs + bufIdx)->resultBuf;
        
        size_t writedSize = 0, pendingWriteSize = (size_t)(pprMasterWorkerPtr->recoveryBufs + bufIdx)->resutBufToWriteSize;
        
        do{
            ssize_t writingSize = writeFile(writeFd, (buf + writedSize), (pendingWriteSize - writedSize), ecBlockServerEnginePtr->diskIOMgr);
            
            if (writingSize > 0) {
                writedSize = writedSize + writingSize;
            }
        }while (writedSize < pendingWriteSize);
        
//        printf("disk write:%lu, bIdx:%d\n", writedSize, bufIdx);
        (pprMasterWorkerPtr->recoveryBufs + bufIdx)->resultFilledSize = writedSize;
        
        sem_post(&pprMasterWorkerPtr->writingFinishedSems[bufIdx]);
        bufIdx = (bufIdx + 1) % pprMasterWorkerPtr->recoveryBufNum;

    }

    return arg;
}

void *netReadWorker(void *arg){
    PPRMasterWorker_t *pprMasterWorkerPtr = (PPRMasterWorker_t *)arg;
    int bufIdx = 0;

    while (pprMasterWorkerPtr->recoveryOverFlag != 1) {
        
        sem_wait(&pprMasterWorkerPtr->netReadSem);
        
        if (pprMasterWorkerPtr->recoveryOverFlag == 1) {
            break;
        }
        
        if (pprMasterWorkerPtr->curBlockDone == 1) {
            bufIdx = 0;
            continue;
        }
        
        
        char *buf = (pprMasterWorkerPtr->recoveryBufs + bufIdx)->netIOBuf;
        size_t pendingSize = (pprMasterWorkerPtr->recoveryBufs + bufIdx)->netBufToFillSize;
        size_t recvdSize = 0;
        
//        printf("Net pending readSize:%lu\n",pendingSize);
        
        do{
            ssize_t recvingSize = recv(pprMasterWorkerPtr->firstSlaveFd, buf + recvdSize, (pendingSize - recvdSize), 0);
            
            if (recvingSize > 0) {
                recvdSize = recvdSize + (size_t)recvingSize;
            }
            
        }while (recvdSize < pendingSize);
        
        (pprMasterWorkerPtr->recoveryBufs + bufIdx)->netBufFilledSize = recvdSize;
        
        sem_post(&pprMasterWorkerPtr->netReadDoneSem);
//        printf("net read:%lu, bIdx:%d\n", recvdSize, bufIdx);
        bufIdx = (bufIdx + 1) % pprMasterWorkerPtr->recoveryBufNum;
    }

    return arg;
}

void *codingWorker(void *arg){
    PPRMasterWorker_t *pprMasterWorkerPtr = (PPRMasterWorker_t *)arg;
    int bufIdx = 0;
    
    char **dataPtrs, **codingPtr;
    dataPtrs = talloc(char *, 2);
    codingPtr = talloc(char *, 1);
    while (pprMasterWorkerPtr->recoveryOverFlag != 1) {
        sem_wait(&pprMasterWorkerPtr->diskReadDoneSem);
        sem_wait(&pprMasterWorkerPtr->netReadDoneSem);
        if (pprMasterWorkerPtr->recoveryOverFlag == 1) {
            break;
        }
        
        if (pprMasterWorkerPtr->curBlockDone == 1) {
            bufIdx = 0;
            continue;
        }
        
        dataPtrs[0] = (pprMasterWorkerPtr->recoveryBufs + bufIdx)->localIOBuf;
        dataPtrs[1] = (pprMasterWorkerPtr->recoveryBufs + bufIdx)->netIOBuf;
        codingPtr[0] = (pprMasterWorkerPtr->recoveryBufs + bufIdx)->resultBuf;
        
        jerasure_matrix_encode(2, 1, pprMasterWorkerPtr->stripeW, pprMasterWorkerPtr->localMat, dataPtrs, codingPtr, (pprMasterWorkerPtr->recoveryBufs + bufIdx)->taskSize);
        
        sem_post(&pprMasterWorkerPtr->codingDoneSem);
//        printf("coding:%d, bIdx:%d\n", (pprMasterWorkerPtr->recoveryBufs + bufIdx)->taskSize, bufIdx);
        bufIdx = (bufIdx + 1) % pprMasterWorkerPtr->recoveryBufNum;
    }

    free(dataPtrs);
    free(codingPtr);
    
    return arg;
}
