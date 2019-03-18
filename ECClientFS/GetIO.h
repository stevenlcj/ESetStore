//
//  GetIO.h
//  ECClientFS
//
//  Created by Liu Chengjian on 18/4/6.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __GetIO_Header__
#define __GetIO_Header__

#include <semaphore.h>

#include "ECClientEngine.h"

typedef struct readBuf{
	char *inputBuf;
	size_t bufSize;
	size_t readSize;
}readBuf_t;

typedef struct ECClientGetContext
{
	ECClientEngine_t *clientEngine;

	int localFileFd;
	int ecFileFd;
	int readOverFlag;
	int wIdx;
	int rIdx;
	int bufNum;
	size_t eachBufSize;
	size_t fileSize;
	size_t fileReadSize;
	readBuf_t *bufs;
	int *bufsInReadFlags;

	sem_t writeJobSem;
	sem_t writeOverSem;

	sem_t startReadSem;
	sem_t endReadSem;
}ECClientGetContext_t;

void startGetWork(ECClientEngine_t *clientEngine, char *filesToPut[], int fileNum);

#endif