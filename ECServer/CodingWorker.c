//
//  CodingWorker.c
//  ECStoreCPUServer
//
//  Created by Liu Chengjian on 18/1/15.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "jerasure.h"

#include "CodingWorker.h"
#include "RecoveryThreadWorker.h"

#include <semaphore.h>
#include <string.h>

void recoveryBufAlignment(RecoveryThreadManager_t *recoveryThMgr,recoveryBuf_t *curBuf){
    int idx, alignSize = curBuf->taskSize;
    
    for (idx = 0; idx < recoveryThMgr->sourceNodesSize; ++idx) {
        if (*(curBuf->bufToWriteSize + idx) != alignSize) {
            memset(curBuf->inputBufsPtrs[idx] + *(curBuf->bufToWriteSize + idx), 0, (alignSize - *(curBuf->bufToWriteSize + idx)));
        }
    }
}

void startCoding(RecoveryThreadManager_t *recoveryThMgr, int bufIdx){
//    printf("start coding with idx:%d\n",bufIdx);
    recoveryBuf_t *curBuf = recoveryThMgr->recoveryBufs + bufIdx;
    recoveryBufAlignment(recoveryThMgr, curBuf);
    
//    printf("coding size:%d recoveryThMgr->sourceNodesSize:%d, recoveryThMgr->targetNodesSize:%d w:%d\n",curBuf->taskSize, recoveryThMgr->sourceNodesSize, recoveryThMgr->targetNodesSize, recoveryThMgr->stripeW);
    jerasure_matrix_encode(recoveryThMgr->sourceNodesSize, recoveryThMgr->targetNodesSize, recoveryThMgr->stripeW, recoveryThMgr->decodingMatrix
                           , curBuf->inputBufsPtrs, curBuf->outputBufsPtrs, curBuf->taskSize);
//    jerasure_bitmatrix_encode(recoveryThMgr->sourceNodesSize, recoveryThMgr->targetNodesSize, recoveryThMgr->stripeW, recoveryThMgr->decodingBitmatrix, curBuf->inputBufsPtrs, curBuf->outputBufsPtrs, curBuf->taskSize, 8);
    
}

void *thCodingThread(void *arg){
    RecoveryThreadManager_t *recoveryThMgr = (RecoveryThreadManager_t *)arg;
    int bufIdx = 0;
    
    while(recoveryThMgr->recoveryOverFlag == 0) {
        sem_wait(&recoveryThMgr->codingSem);
        
        if (recoveryThMgr->recoveryOverFlag == 1) {
            break;
        }
        
        startCoding(recoveryThMgr, bufIdx);
        sem_post(&recoveryThMgr->ioWriteSem);
        
        if (recoveryThMgr->recoveryBufNum != 1) {
            bufIdx = (bufIdx + 1) % recoveryThMgr->recoveryBufNum;
        }
    }
    
    return arg;

}