//
//  GetIO.c
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

#include "GetIO.h"
#include "ecUtils.h"
#include "ecCommon.h"

#define WRITE_BUF_NUM (2)
#define DEFAULT_BUF_SIZE (K_SIZE * BUF_NUM * EACH_BUF_SIZE)

void readECData(ECClientGetContext_t *ecGetContxt, readBuf_t *buf){
    size_t readedSize = 0;
    struct timeval startTime, endTime;
    gettimeofday(&startTime, NULL);
    
    do{
    	ssize_t curReadSize = readFile(ecGetContxt->clientEngine, ecGetContxt->ecFileFd, (buf->inputBuf + readedSize), (buf->readSize - readedSize));

    	if (curReadSize == 0)
    	{
    		break;
    	}

    	if (curReadSize < 0)
    	{
    		printf("Read data from ECStore error\n");
    		break;
    	}

    	readedSize = readedSize + (size_t)curReadSize;
    	ecGetContxt->fileReadSize = ecGetContxt->fileReadSize + curReadSize;
    	//printf("fileSize:%lu, fileReadSize:%lu, curReadSize:%lu\n", ecGetContxt->fileSize, ecGetContxt->fileReadSize,  curReadSize);
    }while(readedSize != buf->readSize);
   
//    gettimeofday(&endTime, NULL);
//    double timeInterval = timeIntervalInMS(startTime, endTime);
//    double throughput = ((double)buf->readSize/1024.0/1024.0)/(timeInterval/1000.0);
    
//    printf("readECData size:%luMB, throughput:%fMB/s timeInterval:%fms\n",(buf->readSize/1024/1024),throughput, timeInterval);
    
	return;
}

void waitBufsReadDone(ECClientGetContext_t *ecGetContxt){
	int bufIdx;
    for(bufIdx = 0; bufIdx < ecGetContxt->bufNum; ++bufIdx){
    	if(ecGetContxt->bufsInReadFlags[bufIdx] == 1){
    		sem_wait(&ecGetContxt->writeOverSem);
    		ecGetContxt->bufsInReadFlags[bufIdx] = 0;
    	}
    }
}

void *getReadWorker(void *arg){
	ECClientGetContext_t *ecGetContxt = (ECClientGetContext_t *)arg;

	while(ecGetContxt->readOverFlag != 1){
		sem_wait(&ecGetContxt->startReadSem);

		if(ecGetContxt->readOverFlag == 1){
			break;
		}

		do{
			if (ecGetContxt->bufsInReadFlags[ecGetContxt->rIdx] == 1)
			{
				sem_wait(&ecGetContxt->writeOverSem);
				ecGetContxt->bufsInReadFlags[ecGetContxt->rIdx] = 0;
			}

			readBuf_t *buf = ecGetContxt->bufs + ecGetContxt->rIdx;
			size_t pendingReadSize = ecGetContxt->fileSize - ecGetContxt->fileReadSize;

			if (pendingReadSize > buf->bufSize)
			{
				buf->readSize = buf->bufSize;
			}else{
				buf->readSize = pendingReadSize;
			}

			//printf("readECData rIdx:%d\n",ecGetContxt->rIdx);
//            printf("readECData readSize:%lu\n", buf->readSize);
			readECData(ecGetContxt, buf);
			ecGetContxt->bufsInReadFlags[ecGetContxt->rIdx] = 1;
			ecGetContxt->rIdx = (ecGetContxt->rIdx + 1) % ecGetContxt->bufNum;
			sem_post(&ecGetContxt->writeJobSem);

		}while(ecGetContxt->fileSize != ecGetContxt->fileReadSize);

		waitBufsReadDone(ecGetContxt);
		sem_post(&ecGetContxt->endReadSem);
	}

	sem_post(&ecGetContxt->writeJobSem);
	return arg;
}

size_t writeLocalData(ECClientGetContext_t *ecGetContxt, readBuf_t *buf, size_t writeSize){
	size_t writedSize = 0;
	//printf("write local data:%lu\n", writeSize);
    
//    struct timeval startTime, endTime;
//    gettimeofday(&startTime, NULL);

	do{
		ssize_t curWriteSize = write(ecGetContxt->localFileFd, buf->inputBuf+writedSize, (writeSize - writedSize));

		if (curWriteSize == 0)
		{
			break;
		}

		if (curWriteSize < 0)
		{
			perror("Write local file error");
			break;
		}

		writedSize = writedSize + (size_t) curWriteSize;
	}while(writeSize != writedSize);

//    gettimeofday(&endTime, NULL);
//    double timeInterval = timeIntervalInMS(startTime, endTime);
//    double throughput = ((double)writedSize/1024.0/1024.0)/(timeInterval/1000.0);
//
//    printf("writeLocalData size:%luMB, throughput:%fMB/s timeInterval:%fms\n",(writedSize/1024/1024),throughput, timeInterval);

	return writedSize;
}

void *getWriteWorker(void *arg){
	ECClientGetContext_t *ecGetContxt = (ECClientGetContext_t *)arg;

	while(ecGetContxt->readOverFlag != 1){
		sem_wait(&ecGetContxt->writeJobSem);
		if(ecGetContxt->readOverFlag == 1){
			break;
		}

		readBuf_t *buf = ecGetContxt->bufs + ecGetContxt->wIdx;
		//printf("writeLocalData wIdx:%d\n",ecGetContxt->wIdx);
		writeLocalData(ecGetContxt, buf, buf->readSize);
		ecGetContxt->wIdx = (ecGetContxt->wIdx + 1) % ecGetContxt->bufNum; 
	    sem_post(&ecGetContxt->writeOverSem);
	}

	return arg;
}

ECClientGetContext_t *createECClientGetContext(ECClientEngine_t *clientEngine, int bufNum, size_t bufSize){
	ECClientGetContext_t *ecGetContxt = (ECClientGetContext_t *)malloc(sizeof(ECClientGetContext_t));

	if (ecGetContxt == NULL)
	{
		printf("Unable to malloc ECClientGetContext_t");
		return NULL;
	}

	memset(ecGetContxt, 0, sizeof(ECClientGetContext_t));

	ecGetContxt->clientEngine = clientEngine;

    ecGetContxt->bufNum = bufNum;
    ecGetContxt->eachBufSize = bufSize;
    printf("eachBufSize:%lu\n",bufSize);
    ecGetContxt->bufsInReadFlags = (int *)malloc(sizeof(int)*ecGetContxt->bufNum);

    memset(ecGetContxt->bufsInReadFlags,0, sizeof(int)*ecGetContxt->bufNum);
    ecGetContxt->bufs = (readBuf_t *)malloc(sizeof(readBuf_t) * ecGetContxt->bufNum);

    int idx;
    for (idx = 0; idx < ecGetContxt->bufNum; ++idx)
    {
    	readBuf_t *bufPtr = ecGetContxt->bufs +idx;
    	bufPtr->inputBuf = (char *)malloc(sizeof(char ) * ecGetContxt->eachBufSize);
    	bufPtr->bufSize = ecGetContxt->eachBufSize;
    	bufPtr->readSize = 0;
    }

    if (sem_init(&ecGetContxt->writeJobSem, 0, 0) == -1) {
        perror("readJobSem sem_init error:");
    }

    if (sem_init(&ecGetContxt->writeOverSem, 0, 0) == -1) {
        perror("readOverSem sem_init error:");
    }

        if (sem_init(&ecGetContxt->startReadSem, 0, 0) == -1) {
        perror("startReadSem sem_init error:");
    }

    if (sem_init(&ecGetContxt->endReadSem, 0, 0) == -1) {
        perror("endReadSem sem_init error:");
    }

    return ecGetContxt;
}

void deallocECClientGetContext(ECClientGetContext_t *ecGetContxt){

	int idx;
    for (idx = 0; idx < ecGetContxt->bufNum; ++idx)
    {
    	readBuf_t *bufPtr = ecGetContxt->bufs +idx;
    	free(bufPtr->inputBuf);
    }

    free(ecGetContxt->bufs);
    free(ecGetContxt->bufsInReadFlags);

	sem_destroy(&ecGetContxt->writeJobSem);
	sem_destroy(&ecGetContxt->writeOverSem);
	sem_destroy(&ecGetContxt->startReadSem);
	sem_destroy(&ecGetContxt->endReadSem);
}

void startGetWork(ECClientEngine_t *clientEngine, char *filesToPut[], int fileNum){
	ECClientGetContext_t *ecGetContxt = createECClientGetContext(clientEngine, WRITE_BUF_NUM, DEFAULT_BUF_SIZE);

	ecGetContxt->readOverFlag = 0;

	pthread_t readWorker_pid, writeWorker_pid;
	pthread_create(&readWorker_pid, NULL,getReadWorker, (void *)ecGetContxt);
	pthread_create(&writeWorker_pid, NULL,getWriteWorker, (void *)ecGetContxt);

	int idx;

	for(idx = 0; idx < fileNum; ++idx){
		ecGetContxt->localFileFd = open(filesToPut[idx],O_CREAT | O_RDWR);
		ecGetContxt->rIdx = 0;
		ecGetContxt->wIdx = 0;
		if (ecGetContxt->localFileFd < 0)
		{
			perror("File open error:");
			printf("Unable to open file:%s\n", filesToPut[idx]);
			exit(-1);
		}

		ecGetContxt->ecFileFd = openFile(clientEngine, filesToPut[idx]);
		ecGetContxt->fileSize = getFileSize(clientEngine, ecGetContxt->ecFileFd);
		size_t alginedBufSize = alignBufSize(ecGetContxt->eachBufSize,
								getFileStripeK(clientEngine,ecGetContxt->ecFileFd)*
								getFileStreamingSize(clientEngine,ecGetContxt->ecFileFd));
		ecGetContxt->fileReadSize = 0;

		if(alginedBufSize != ecGetContxt->eachBufSize){
			ecGetContxt->eachBufSize = alginedBufSize;

			int bIdx;
			for (bIdx = 0; bIdx < ecGetContxt->bufNum; ++bIdx)
			{
				readBuf_t *bufPtr = ecGetContxt->bufs + bIdx;
				bufPtr->inputBuf = realloc(bufPtr->inputBuf, ecGetContxt->eachBufSize);
				bufPtr->bufSize = ecGetContxt->eachBufSize;
			}
		}

		sem_post(&ecGetContxt->startReadSem);
		sem_wait(&ecGetContxt->endReadSem);

		close(ecGetContxt->localFileFd);
		closeFile(ecGetContxt->clientEngine, ecGetContxt->ecFileFd);
	}

	printf("Read file work done\n");
	ecGetContxt->readOverFlag = 1;

	sem_post(&ecGetContxt->startReadSem);
	pthread_join(readWorker_pid, NULL);
	pthread_join(writeWorker_pid, NULL);

	deallocECClientGetContext(ecGetContxt);
}