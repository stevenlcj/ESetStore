//
//  PutIO.c
//  ECClientFS
//
//  Created by Liu Chengjian on 18/4/6.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

#include "PutIO.h"
#include "ecUtils.h"

#define WRITE_BUF_NUM (2)
#define DEFAULT_BUF_SIZE (1024 * 1024)

void writeECData(ECClientPutContext_t *ecPutContxt, writeBuf_t *buf){
    size_t writedSize = 0;
    do{
    	printf("start writeFile fd:%d, size:%lu\n",ecPutContxt->ecFileFd, buf->readSize);
    	ssize_t curWritedSize = writeFile(ecPutContxt->clientEngine, ecPutContxt->ecFileFd, (buf->inputBuf + writedSize), (buf->readSize - writedSize));

    	printf("curWritedSize:%ld\n", curWritedSize);

    	if (curWritedSize == 0)
    	{
    		break;
    	}

    	if (curWritedSize < 0)
    	{
    		printf("Write data to ECStore error\n");
    		break;
    	}

    	writedSize = writedSize + (size_t)curWritedSize;

    }while(writedSize != buf->readSize);
   
	return;
}

void *putWriteWorker(void *arg){
	ECClientPutContext_t *ecPutContxt = (ECClientPutContext_t *)arg;

	while(ecPutContxt->writeOverFlag != 1){

		sem_wait(&ecPutContxt->writeJobSem);
		if(ecPutContxt->writeOverFlag == 1){
			break;
		}


		writeBuf_t *buf = ecPutContxt->bufs + ecPutContxt->wIdx;
		writeECData(ecPutContxt, buf);
		ecPutContxt->wIdx = (ecPutContxt->wIdx + 1)% ecPutContxt->bufNum;
		sem_post(&ecPutContxt->writeOverSem);
	}

	return arg;
}

size_t readLocalData(ECClientPutContext_t *ecPutContxt, writeBuf_t *buf, size_t readSize){
	buf->readSize = 0;
	
	do{
		ssize_t readedSize = read(ecPutContxt->localFileFd, buf->inputBuf+buf->readSize, (readSize - buf->readSize));

		if (readedSize == 0)
		{
			break;
		}

		if (readedSize < 0)
		{
			perror("Read local file error");
			break;
		}

		buf->readSize = buf->readSize + (size_t) readedSize;
	}while(buf->readSize != readSize);

	return buf->readSize;
}

void waitBufsWriteDone(ECClientPutContext_t *ecPutContxt){
	int bufIdx;
    for(bufIdx = 0; bufIdx < ecPutContxt->bufNum; ++bufIdx){
    	if(ecPutContxt->bufsInWriteFlags[bufIdx] == 1){
    		sem_wait(&ecPutContxt->writeOverSem);
    		ecPutContxt->bufsInWriteFlags[bufIdx] = 0;
    	}
    }
}

void *putReadWorker(void *arg){
	ECClientPutContext_t *ecPutContxt = (ECClientPutContext_t *)arg;

	while(ecPutContxt->writeOverFlag != 1){
		sem_wait(&ecPutContxt->startWriteSem);

		if(ecPutContxt->writeOverFlag == 1){
			break;
		}

		size_t readSize = 0;
		do{
			if(ecPutContxt->bufsInWriteFlags[ecPutContxt->rIdx] == 1){
				sem_wait(&ecPutContxt->writeOverSem);
			}

			ecPutContxt->bufsInWriteFlags[ecPutContxt->rIdx] = 1;
			writeBuf_t *buf = ecPutContxt->bufs + ecPutContxt->rIdx;
			readSize = readLocalData(ecPutContxt, buf, buf->bufSize);

			//printf("readSize:%lu\n", buf->readSize);
			if (readSize == 0)
			{
				ecPutContxt->bufsInWriteFlags[ecPutContxt->rIdx] = 0;
				break;
			}

			sem_post(&ecPutContxt->writeJobSem);

			ecPutContxt->rIdx = (ecPutContxt->rIdx + 1) % ecPutContxt->bufNum; 
	    }while(readSize != 0);

	    waitBufsWriteDone(ecPutContxt);
	    sem_post(&ecPutContxt->endWriteSem);
	}

    //Wait write finished, then tell 
	waitBufsWriteDone(ecPutContxt);
    sem_post(&ecPutContxt->writeJobSem);

	return arg;
}

ECClientPutContext_t *createECClientPutContext(ECClientEngine_t *clientEngine, int bufNum, size_t bufSize){
	ECClientPutContext_t *ecPutContxt = (ECClientPutContext_t *)malloc(sizeof(ECClientPutContext_t));

	if (ecPutContxt == NULL)
	{
		printf("Unable to malloc ECClientPutContext_t");
		return NULL;
	}

	memset(ecPutContxt, 0, sizeof(ECClientPutContext_t));

	ecPutContxt->clientEngine = clientEngine;

    ecPutContxt->bufNum = bufNum;
    ecPutContxt->eachBufSize = bufSize;
    ecPutContxt->bufsInWriteFlags = (int *)malloc(sizeof(int)*ecPutContxt->bufNum);

    memset(ecPutContxt->bufsInWriteFlags,0, sizeof(int)*ecPutContxt->bufNum);
    ecPutContxt->bufs = (writeBuf_t *)malloc(sizeof(writeBuf_t) * ecPutContxt->bufNum);

    int idx;
    for (idx = 0; idx < ecPutContxt->bufNum; ++idx)
    {
    	writeBuf_t *bufPtr = ecPutContxt->bufs +idx;
    	bufPtr->inputBuf = (char *)malloc(sizeof(char ) * ecPutContxt->eachBufSize);
    	bufPtr->bufSize = ecPutContxt->eachBufSize;
    	bufPtr->readSize = 0;
    }

    if (sem_init(&ecPutContxt->writeJobSem, 0, 0) == -1) {
        perror("writeJobSem sem_init error:");
    }

    if (sem_init(&ecPutContxt->writeOverSem, 0, 0) == -1) {
        perror("writeOverSem sem_init error:");
    }

        if (sem_init(&ecPutContxt->startWriteSem, 0, 0) == -1) {
        perror("writeJobSem sem_init error:");
    }

    if (sem_init(&ecPutContxt->endWriteSem, 0, 0) == -1) {
        perror("writeOverSem sem_init error:");
    }

    return ecPutContxt;
}

void deallocECClientPutContext(ECClientPutContext_t *ecPutContxt){

	int idx;
    for (idx = 0; idx < ecPutContxt->bufNum; ++idx)
    {
    	writeBuf_t *bufPtr = ecPutContxt->bufs +idx;
    	free(bufPtr->inputBuf);
    }

    free(ecPutContxt->bufs);
    free(ecPutContxt->bufsInWriteFlags);

	sem_destroy(&ecPutContxt->writeJobSem);
	sem_destroy(&ecPutContxt->writeOverSem);
	sem_destroy(&ecPutContxt->startWriteSem);
	sem_destroy(&ecPutContxt->endWriteSem);
}

void startPutWork(ECClientEngine_t *clientEngine, char *filesToPut[], int fileNum){
	ECClientPutContext_t *ecPutContxt = createECClientPutContext(clientEngine, WRITE_BUF_NUM, DEFAULT_BUF_SIZE);

	ecPutContxt->writeOverFlag = 0;

	pthread_t readWorker_pid, writeWorker_pid;
	pthread_create(&writeWorker_pid, NULL,putWriteWorker, (void *)ecPutContxt);
	pthread_create(&readWorker_pid, NULL,putReadWorker, (void *)ecPutContxt);

	int idx;

	for(idx = 0; idx < fileNum; ++idx){
		ecPutContxt->localFileFd = open(filesToPut[idx],O_RDONLY);
		ecPutContxt->rIdx = 0;
		ecPutContxt->wIdx = 0;
		if (ecPutContxt->localFileFd < 0)
		{
			perror("File read error:");
			printf("Unable to read file:%s\n", filesToPut[idx]);
			exit(-1);
		}

		ecPutContxt->ecFileFd = createFile(clientEngine, filesToPut[idx]);

		size_t alginedBufSize = alignBufSize(ecPutContxt->eachBufSize,
								getFileStripeK(clientEngine,ecPutContxt->ecFileFd)*getFileStreamingSize(clientEngine,ecPutContxt->ecFileFd));

		if(alginedBufSize != ecPutContxt->eachBufSize){
			ecPutContxt->eachBufSize = alginedBufSize;

			int bIdx;
			for (bIdx = 0; bIdx < ecPutContxt->bufNum; ++bIdx)
			{
				writeBuf_t *bufPtr = ecPutContxt->bufs + bIdx;
				bufPtr->inputBuf = realloc(bufPtr->inputBuf, ecPutContxt->eachBufSize);
				bufPtr->bufSize = ecPutContxt->eachBufSize;
			}
		}

		sem_post(&ecPutContxt->startWriteSem);
		sem_wait(&ecPutContxt->endWriteSem);

		close(ecPutContxt->localFileFd);
		closeFile(ecPutContxt->clientEngine, ecPutContxt->ecFileFd);
	}

	ecPutContxt->writeOverFlag = 1;

	sem_post(&ecPutContxt->startWriteSem);
	pthread_join(readWorker_pid, NULL);
	pthread_join(writeWorker_pid, NULL);
}