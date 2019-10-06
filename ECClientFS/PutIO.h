//
//  PutIO.h
//  ECClientFS
//
//  Created by Liu Chengjian on 18/4/6.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __PutIO_Header__

#include <semaphore.h>

#include "ECClientEngine.h"

typedef struct writeBuf{
	char *inputBuf;
	size_t bufSize;
	size_t readSize;
}writeBuf_t;

typedef struct ECClientPutContext
{
	ECClientEngine_t *clientEngine;

	int localFileFd;
	int ecFileFd;
	int writeOverFlag;
	int wIdx;
	int rIdx;
	int bufNum;
	size_t eachBufSize;
	writeBuf_t *bufs;
	int *bufsInWriteFlags;

	sem_t writeJobSem;
	sem_t writeOverSem;

	sem_t startWriteSem;
	sem_t endWriteSem;
}ECClientPutContext_t;

void startPutWork(ECClientEngine_t *clientEngine, char *filesToPut[], int fileNum);

#define __PutIO_Header__
#endif