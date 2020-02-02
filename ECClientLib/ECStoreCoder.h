//
//  ECStoreCoder.h
//  ECClient
//
//  Created by Liu Chengjian on 18/4/9.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECClient__ECStoreCoder__
#define __ECClient__ECStoreCoder__

#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdint.h>

#include "ECBlockWorker.h"
/**
 coderWorker->waitJob->performing job->
*/

typedef struct ECCoderWorker{
	pthread_t pid;

	int exitFlag;

	int coderInUseFlag;
	int jobFinishedFlag;

	sem_t jobStartSem;

	sem_t waitJobSem;
	sem_t jobFinishedSem;

	void *ecBlockWorkerMgr;
}ECCoderWorker_t;

void *ecStoreCoderWorker(void *arg);

ECCoderWorker_t *createECCoderWorker(void *ecBlockWorkerMgr);

void printCoderWorkerMem(ECCoderWorker_t *coderWorker);
void deallocECCoderWorker(ECCoderWorker_t *coderWorker);
#endif


