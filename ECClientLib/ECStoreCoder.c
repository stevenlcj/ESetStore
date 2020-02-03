//
//  ECStoreCoder.h
//  ECClient
//
//  Created by Liu Chengjian on 18/4/9.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECStoreCoder.h"
#include "ECFileHandle.h"
#include "ECBlockWorker.h"

#include "jerasure.h"

void performEncodingJob(ECCoderWorker_t *coderWorker, ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *filePtr){

	int bIdx = 0;
	while(ecBlockWorkerMgr->jobDoneFlag == 0){
		sem_wait(&coderWorker->waitJobSem);

		if(ecBlockWorkerMgr->jobDoneFlag == 1){
			return;
		}

		workerBuf_t *workerBufPtr = ecBlockWorkerMgr->workerBufs + bIdx;
        jerasure_matrix_encode((int)filePtr->stripeK, (int)filePtr->stripeM, (int)filePtr->stripeW, filePtr->matrix, workerBufPtr->inputBufs, workerBufPtr->outputBufs, (int)workerBufPtr->codingSize);
		sem_post(&coderWorker->jobFinishedSem);
		bIdx = (bIdx + 1) % ecBlockWorkerMgr->workerBufsNum;
	}
}

void performDecodingJob(ECCoderWorker_t *coderWorker, ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *filePtr){
    int bIdx = 0;
    
    while(ecBlockWorkerMgr->jobDoneFlag == 0){

		sem_wait(&coderWorker->waitJobSem);
        if(ecBlockWorkerMgr->jobDoneFlag == 1){
            return;
        }
        
        workerBuf_t *workerBufPtr = ecBlockWorkerMgr->workerBufs + bIdx;
//        printf("coding with bufIdx:%d\n",bIdx);

        jerasure_matrix_encode((int)filePtr->stripeK, (int)filePtr->stripeM, (int)filePtr->stripeW, filePtr->decoding_matrix, workerBufPtr->inputBufs, workerBufPtr->outputBufs, (int)workerBufPtr->codingSize);
        
//        printf("%04x %0x4 %04x\n",(int)*(workerBufPtr->inputBufs[0]),(int)*(workerBufPtr->inputBufs[1]),(int)*(workerBufPtr->outputBufs[0]));
		sem_post(&coderWorker->jobFinishedSem);
        bIdx = (bIdx + 1) % ecBlockWorkerMgr->workerBufsNum;
    }
}


void performCodingJob(ECCoderWorker_t *coderWorker){
	while(coderWorker->jobFinishedFlag == 0){
		ECBlockWorkerManager_t *ecBlockWorkerMgr = (ECBlockWorkerManager_t *)coderWorker->ecBlockWorkerMgr;
		ECFile_t *filePtr = ecBlockWorkerMgr->curFilePtr;

		switch(ecBlockWorkerMgr->curJobState){
			case ECJOB_WRITE:
				performEncodingJob(coderWorker, ecBlockWorkerMgr, filePtr);
			break;
			case ECJOB_DEGRADED_READ:
				performDecodingJob(coderWorker, ecBlockWorkerMgr, filePtr);
			break;
			default:
				printf("Unknow job for coder\n");
				//exit(0);
		}

	}
}

void *ecStoreCoderWorker(void *arg){
	ECCoderWorker_t *coderWorker = (ECCoderWorker_t *) arg;

	while(coderWorker->exitFlag == 0){
		sem_wait(&coderWorker->jobStartSem);
//        printf("coderWorker->exitFlag:%d\n",coderWorker->exitFlag);
		if (coderWorker->exitFlag == 1)
		{
			break;
		}

		performCodingJob(coderWorker);
	}

	return arg;
}

ECCoderWorker_t *createECCoderWorker(void *ecBlockWorkerMgr){
    ECCoderWorker_t *coderWorker = (ECCoderWorker_t *)malloc(sizeof(ECCoderWorker_t));
    
    if (coderWorker == NULL)
    {
    	printf("Unable to alloc coderWorker");
    }

    coderWorker->ecBlockWorkerMgr = ecBlockWorkerMgr;
	
    coderWorker->exitFlag = 0;
    coderWorker->coderInUseFlag = 0;
    coderWorker->jobFinishedFlag = 0;

	sem_init(&coderWorker->jobStartSem,0, 0);
    sem_init(&coderWorker->waitJobSem,0, 0);
    sem_init(&coderWorker->jobFinishedSem,0, 0);

    return coderWorker;
}

void deallocECCoderWorker(ECCoderWorker_t *coderWorker){

	sem_destroy(&coderWorker->jobStartSem);
	sem_destroy(&coderWorker->waitJobSem);
	sem_destroy(&coderWorker->jobFinishedSem);

	free(coderWorker);
}
