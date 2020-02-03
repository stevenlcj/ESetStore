//
//  ECBlockWorker.c
//  ECClient
//
//  Created by Liu Chengjian on 18/4/9.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "ECBlockWorker.h"
#include "ECBlockWorkerIO.h"
#include "ECClientCommon.h"
#include "ECNetHandle.h"
#include "ECStoreCoder.h"
#include "ecUtils.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>

#include <pthread.h>

void setWorkerBuf(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr, workerBuf_t *workerBufPtr){
	size_t idx;

	if (workerBufPtr->inputNum < ecFilePtr->stripeK)
	{
		if (workerBufPtr->inputNum == 0)
		{
			workerBufPtr->inputBufs  = (char **)malloc(sizeof(char *) * ecFilePtr->stripeK);

			workerBufPtr->inputNum = ecFilePtr->stripeK;

			workerBufPtr->inputWritedSize = (int *)malloc(sizeof(int) * ecFilePtr->stripeK);
			memset(workerBufPtr->inputWritedSize, 0, sizeof(int) * ecFilePtr->stripeK);

			workerBufPtr->eachBufSize = ecBlockWorkerMgr->alignedBufSize;

			for (idx = 0; idx < workerBufPtr->inputNum; ++idx)
			{
				workerBufPtr->inputBufs[idx] = (char *)malloc(sizeof(char ) * workerBufPtr->eachBufSize);
			}
			
		}else{
			workerBufPtr->inputBufs  = (char **)realloc(workerBufPtr->inputBufs, sizeof(char *) * ecFilePtr->stripeK);
			workerBufPtr->inputWritedSize = (int *)realloc(workerBufPtr->inputWritedSize, sizeof(int) * ecFilePtr->stripeK);

			memset(workerBufPtr->inputWritedSize, 0, sizeof(int) * ecFilePtr->stripeK);

			for (idx = workerBufPtr->inputNum; idx < ecFilePtr->stripeK; ++idx)
			{
				workerBufPtr->inputBufs[idx] = (char *)malloc(sizeof(char ) * workerBufPtr->eachBufSize);
			}

			workerBufPtr->inputNum = ecFilePtr->stripeK;
		}
	}

	if (workerBufPtr->outputNum < ecFilePtr->stripeM)
	{
		if (workerBufPtr->outputNum == 0)
		{
			workerBufPtr->outputBufs = (char **)malloc(sizeof(char *) * ecFilePtr->stripeM); ;
			workerBufPtr->outputNum = ecFilePtr->stripeM;

			for (idx = 0; idx < workerBufPtr->outputNum; ++idx)
			{
				workerBufPtr->outputBufs[idx] = (char *)malloc(sizeof(char ) * workerBufPtr->eachBufSize);
			}
			
		}else{
			workerBufPtr->outputBufs  = (char **)realloc(workerBufPtr->outputBufs, sizeof(char *) * ecFilePtr->stripeK);

			for (idx = workerBufPtr->outputNum; idx < ecFilePtr->stripeM; ++idx)
			{
				workerBufPtr->outputBufs[idx] = (char *)malloc(sizeof(char ) * workerBufPtr->eachBufSize);
			}
			workerBufPtr->outputNum = ecFilePtr->stripeK;

		}
	}

	if(workerBufPtr->eachBufSize < ecBlockWorkerMgr->alignedBufSize){
		workerBufPtr->eachBufSize = ecBlockWorkerMgr->alignedBufSize;
		for (idx = 0; idx < workerBufPtr->inputNum; ++idx)
		{
			workerBufPtr->inputBufs[idx] = (char *)realloc(workerBufPtr->inputBufs[idx], sizeof(char ) * workerBufPtr->eachBufSize);
		}

		for (idx = 0; idx < workerBufPtr->outputNum; ++idx)
		{
			workerBufPtr->outputBufs[idx] = (char *)realloc(workerBufPtr->outputBufs[idx], sizeof(char ) * workerBufPtr->eachBufSize);
		}
	}

	workerBufPtr->alignedBufSize = ecBlockWorkerMgr->alignedBufSize;
	workerBufPtr->codingSize = 0;
}

void setWorkerBufs(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr){
	if ((ecBlockWorkerMgr->alignedBufSize % ecFilePtr->streamingSize) != 0)
	{
		ecBlockWorkerMgr->alignedBufSize = alignBufSize(ecBlockWorkerMgr->alignedBufSize, ecFilePtr->streamingSize);
	}

	int idx;

	for (idx = 0; idx < ecBlockWorkerMgr->workerBufsNum; ++idx)
	{
		workerBuf_t *workerBufPtr = ecBlockWorkerMgr->workerBufs + idx;
		setWorkerBuf(ecBlockWorkerMgr, ecFilePtr, workerBufPtr);
	}
}

void initBlockWorkers(ECBlockWorker_t *blockWorkers, int allocSize){
	int idx;
	for (idx = 0; idx < allocSize; ++idx)
	{
		ECBlockWorker_t *workerPtr = blockWorkers + idx;

		workerPtr->workerIdx = -1;
    	workerPtr->workerInUseFlag = 0;
    	workerPtr->sockFd = 0;
    	workerPtr->curSockState = Worker_SOCK_STATE_UNINIT;

   		workerPtr->curState = Worker_STATE_UNINIT;
    	workerPtr->blockIdToHandle = 0;
    	workerPtr->next = NULL;

    	workerPtr->IPSize = 0;
    	workerPtr->port = 0;

    	workerPtr->readMsgBuf = createMessageBuf(DEFAUT_MESSGAE_READ_WRITE_BUF);
    	workerPtr->writeMsgBuf = createMessageBuf(DEFAUT_MESSGAE_READ_WRITE_BUF);

    	workerPtr->totalSend = 0;
	}

}

void createWorkerBufs(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	//Configurable in the future?
	ecBlockWorkerMgr->workerBufsNum = DEFAULT_BUF_SIZE;
	ecBlockWorkerMgr->defaultBufSize = DEFAULT_WORKER_BUF_SIZE;
	ecBlockWorkerMgr->alignedBufSize = DEFAULT_WORKER_BUF_SIZE;

	ecBlockWorkerMgr->workerBufs = (workerBuf_t *)malloc(sizeof(workerBuf_t) * ecBlockWorkerMgr->workerBufsNum);
	ecBlockWorkerMgr->bufInUseFlag = (int *)malloc(sizeof(int) * ecBlockWorkerMgr->workerBufsNum);

	memset(ecBlockWorkerMgr->bufInUseFlag, 0, sizeof(int) * ecBlockWorkerMgr->workerBufsNum);

	int idx;

	for (idx = 0; idx < ecBlockWorkerMgr->workerBufsNum; ++idx)
	{
		workerBuf_t *workerBufPtr = ecBlockWorkerMgr->workerBufs + idx;
		workerBufPtr->inputBufs = NULL;
		workerBufPtr->outputBufs = NULL;
		workerBufPtr->inputWritedSize = NULL;
		workerBufPtr->inputNum = 0;
		workerBufPtr->outputNum = 0;
		workerBufPtr->codingSize = 0;
		workerBufPtr->eachBufSize = 0;
	}

	return;
}

void deallocWorkerBuf(workerBuf_t *workerBufPtr){
	int idx;
	if (workerBufPtr->inputNum != 0)
	{
		//printf("free(workerBufPtr->inputBufs\n");
		for (idx = 0; idx < (int)workerBufPtr->inputNum; ++idx)
		{
			free(workerBufPtr->inputBufs[idx]);
		}

		//printf("workerBufPtr->inputWritedSize\n");
		free(workerBufPtr->inputWritedSize);
		//printf("workerBufPtr->inputBufs\n");
		free(workerBufPtr->inputBufs);
	}

	if (workerBufPtr->outputNum != 0)
	{
		//printf("workerBufPtr->outputBufs idx\n");
		for (idx = 0; idx < (int)workerBufPtr->outputNum; ++idx)
		{
			free(workerBufPtr->outputBufs[idx]);
		}
		//printf("workerBufPtr->outputBufs\n");
		free(workerBufPtr->outputBufs);
	}
}

void deallocWorkerBufs(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	int idx;

	for (idx = 0; idx < ecBlockWorkerMgr->workerBufsNum; ++idx)
	{
		workerBuf_t *workerBufPtr = ecBlockWorkerMgr->workerBufs + idx;
		if (workerBufPtr->inputNum != 0 || workerBufPtr->outputNum != 0)
		{
			deallocWorkerBuf(workerBufPtr);
		}
	}

	//printf("free(ecBlockWorkerMgr->workerBufs)\n");
	free(ecBlockWorkerMgr->workerBufs);
	//printf("free(ecBlockWorkerMgr->bufInUseFlag)\n");
	free(ecBlockWorkerMgr->bufInUseFlag);
}

ECBlockWorkerManager_t *createBlockWorkerMgr(int increSize){
	ECBlockWorkerManager_t *ecBlockWorkerMgr = (ECBlockWorkerManager_t *)malloc(sizeof(ECBlockWorkerManager_t));

	if (ecBlockWorkerMgr == NULL)
	{
		printf("Unable to alloc ecBlockWorkerMgr\n");
		return NULL;
	}

	ecBlockWorkerMgr->coderWorker = createECCoderWorker((void *)ecBlockWorkerMgr);
	
	createWorkerBufs(ecBlockWorkerMgr);

	ecBlockWorkerMgr->eFd = create_epollfd();

	ecBlockWorkerMgr->blockWorkers = (ECBlockWorker_t *)malloc(sizeof(ECBlockWorker_t)*increSize);

	initBlockWorkers(ecBlockWorkerMgr->blockWorkers, increSize);

	ecBlockWorkerMgr->blockWorkerIncreSize = increSize;
	ecBlockWorkerMgr->blockWorkerNum = increSize;

	ecBlockWorkerMgr->events = (void *)malloc(sizeof(struct epoll_event) * MAX_CONN_EVENTS);
	ecBlockWorkerMgr->inMonitoringSockNum = 0;
	ecBlockWorkerMgr->maxInFlightReq = MAX_IN_FLIGHT_REQ;
	ecBlockWorkerMgr->curInFlightReq = 0;

	sem_init(&ecBlockWorkerMgr->jobStartSem,0 ,0);
	sem_init(&ecBlockWorkerMgr->jobFinishedSem,0 ,0);

	ecBlockWorkerMgr->curFilePtr = NULL;

	pthread_create(&ecBlockWorkerMgr->workerPid, NULL, threadBlockWorker , (void *)ecBlockWorkerMgr);
    pthread_create(&ecBlockWorkerMgr->coderPid, NULL, ecStoreCoderWorker, ecBlockWorkerMgr->coderWorker);
	return ecBlockWorkerMgr;
}

void deallocBlockWorker(ECBlockWorker_t *blockWorkerPtr){
	if (blockWorkerPtr->curState == Worker_STATE_CONNECTED)
	{
		close(blockWorkerPtr->sockFd);
	}

	freeMessageBuf(blockWorkerPtr->readMsgBuf);
	freeMessageBuf(blockWorkerPtr->writeMsgBuf);
}

void deallocBlockWorkerMgr(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	ECCoderWorker_t *coderWorker = (ECCoderWorker_t *)ecBlockWorkerMgr->coderWorker;
	ecBlockWorkerMgr->exitFlag = 1;
	coderWorker->exitFlag = 1;

	sem_post(&ecBlockWorkerMgr->jobStartSem);
	sem_post(&coderWorker->jobStartSem);
    pthread_join(ecBlockWorkerMgr->coderPid, NULL);//Wait coder thread exit before freeing
    
	deallocWorkerBufs(ecBlockWorkerMgr);

	close(ecBlockWorkerMgr->eFd);
	//printf("free(ecBlockWorkerMgr->events\n");
	free(ecBlockWorkerMgr->events);

	int idx;

	for (idx = 0; idx < ecBlockWorkerMgr->blockWorkerNum; ++idx)
	{
		ECBlockWorker_t *blockWorkerPtr = ecBlockWorkerMgr->blockWorkers + idx;
		deallocBlockWorker(blockWorkerPtr);
	}

	//printf("ecBlockWorkerMgr->blockWorkers\n");
	free(ecBlockWorkerMgr->blockWorkers);
    
    pthread_join(coderWorker->pid, NULL);
	deallocECCoderWorker(coderWorker);

	sem_destroy(&ecBlockWorkerMgr->jobStartSem);
	sem_destroy(&ecBlockWorkerMgr->jobFinishedSem);
}

ECBlockWorker_t *allocABlockWorker(ECBlockWorkerManager_t *ecBlockWorkerMgr, blockInfo_t *curBlockInfo){
	ECBlockWorker_t *unConnectedBlockWorkerPtr = NULL;
	ECBlockWorker_t *connectedBlockWorkerPtr = NULL;

	int idx;
	for (idx = 0; idx < ecBlockWorkerMgr->blockWorkerNum; ++idx)
	{
		ECBlockWorker_t *curBlockWorker = ecBlockWorkerMgr->blockWorkers + idx;
		if (curBlockWorker->workerInUseFlag == 1)
		{
			continue;
		}

		if (curBlockWorker->curState == Worker_STATE_CONNECTED)
		{
			if ((curBlockWorker->IPSize == (int) curBlockInfo->IPSize) && 
				strncmp(curBlockWorker->IPAddr, curBlockInfo->IPAddr, curBlockInfo->IPSize) == 0 &&
				curBlockWorker->port == curBlockInfo->port)
			{
				connectedBlockWorkerPtr = curBlockWorker;
				break;
			}

			continue;
		}

		if(unConnectedBlockWorkerPtr == NULL){
			//record a worker with no connection
			unConnectedBlockWorkerPtr = curBlockWorker;
		}
	}

	if (connectedBlockWorkerPtr != NULL)
	{
		connectedBlockWorkerPtr->workerInUseFlag = 1;
		return connectedBlockWorkerPtr;
	}

	if (unConnectedBlockWorkerPtr == NULL)
	{
		ecBlockWorkerMgr->blockWorkers = (ECBlockWorker_t *)realloc(ecBlockWorkerMgr->blockWorkers,
											sizeof(ECBlockWorker_t)*(ecBlockWorkerMgr->blockWorkerNum + ecBlockWorkerMgr->blockWorkerIncreSize));
		initBlockWorkers(ecBlockWorkerMgr->blockWorkers + ecBlockWorkerMgr->blockWorkerNum, ecBlockWorkerMgr->blockWorkerIncreSize);
		unConnectedBlockWorkerPtr = ecBlockWorkerMgr->blockWorkers + ecBlockWorkerMgr->blockWorkerNum;
	}

	unConnectedBlockWorkerPtr->workerInUseFlag = 1;
	memcpy(unConnectedBlockWorkerPtr->IPAddr, curBlockInfo->IPAddr, curBlockInfo->IPSize);
	unConnectedBlockWorkerPtr->IPSize = curBlockInfo->IPSize;
	unConnectedBlockWorkerPtr->port = curBlockInfo->port;
	unConnectedBlockWorkerPtr->IPAddr[unConnectedBlockWorkerPtr->IPSize] = '\0';

	return unConnectedBlockWorkerPtr;
}

void connectABlockWorker(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	if (blockWorkerPtr->curState == Worker_STATE_CONNECTED)
	{
		//printf("server IPAddr:%s, port:%d connected\n", blockWorkerPtr->IPAddr, blockWorkerPtr->port);
		return; //The socket already connected 
	}

	if (blockWorkerPtr->curState == Worker_STATE_UNINIT)
	{
		blockWorkerPtr->sockFd = create_TCP_sockFd();
		blockWorkerPtr->curState = Worker_STATE_INIT;
		blockWorkerPtr->curSockState = Worker_SOCK_STATE_INIT;
		make_non_blocking_sock(blockWorkerPtr->sockFd);
	}

	struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(blockWorkerPtr->IPAddr);
    
    if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
        printf("Error on convert blockServer IP addr\n");
        printf("blockServer IP:%s\n", blockWorkerPtr->IPAddr);
    }
    
    serv_addr.sin_port = htons(blockWorkerPtr->port);
    
    if (connect(blockWorkerPtr->sockFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    	if (errno != EINPROGRESS)
    	{
    		printf("IPAddr: %s port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port );
    		perror("What happened");
    	}

    	//printf("IPAddr: %s port:%d is connecting\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port );
    	blockWorkerPtr->curState = Worker_STATE_CONNECTING;
    	add_event(ecBlockWorkerMgr->eFd, EPOLLOUT | EPOLLET, blockWorkerPtr->sockFd, (void *)blockWorkerPtr);
    	blockWorkerPtr->curSockState = Worker_SOCK_STATE_WRITE;
    	++ecBlockWorkerMgr->inMonitoringSockNum;

    }else{
    	blockWorkerPtr->curState = Worker_STATE_CONNECTED;
    	printf("Connected for server with IP:%s, port:%d\n", blockWorkerPtr->IPAddr, blockWorkerPtr->port);
    }

    blockWorkerPtr->curSockState = Worker_SOCK_STATE_INIT;
}

void removeUnConnected(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	if (ecBlockWorkerMgr->inMonitoringSockNum == 0)
	{
		return;
	}

	ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;

	ECBlockWorker_t *blockWorkerPtr = (ECBlockWorker_t *)ecFilePtr->blockWorkers;
	int iterateNum = 0;
	while(iterateNum != ecFilePtr->blockWorkerNum){
		if (blockWorkerPtr->curState == Worker_STATE_CONNECTING &&
			blockWorkerPtr->curSockState == Worker_SOCK_STATE_WRITE)
		{
			blockWorkerPtr->curState = Worker_STATE_CONNECTFAILED;
			delete_event(ecBlockWorkerMgr->eFd, blockWorkerPtr->sockFd);
			blockWorkerPtr->curSockState = Worker_SOCK_STATE_INIT;
			printf("Connected server failed with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
		}

		blockWorkerPtr = blockWorkerPtr->next;
		++iterateNum;
	}
}

void waitConnections(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	int idx, eventsNum;

	struct epoll_event *events = (struct epoll_event *)ecBlockWorkerMgr->events;
	eventsNum = epoll_wait(ecBlockWorkerMgr->eFd, events, MAX_EVENTS,CONNECTING_TIME_OUT_IN_MS);

	for (idx = 0; idx < eventsNum; ++idx)
	{
        ECBlockWorker_t *blockWorkerPtr = (ECBlockWorker_t *) (events[idx].data.ptr);
        //printf("Connected for server with IP:%s, port:%d, ecBlockWorkerMgr->inMonitoringSockNum:%d\n", 
        //	blockWorkerPtr->IPAddr, blockWorkerPtr->port, ecBlockWorkerMgr->inMonitoringSockNum);
		--ecBlockWorkerMgr->inMonitoringSockNum;
		delete_event(ecBlockWorkerMgr->eFd, blockWorkerPtr->sockFd);
		blockWorkerPtr->curSockState = Worker_SOCK_STATE_INIT;

        if (events[idx].events & EPOLLOUT)
        {
        	blockWorkerPtr->curState = Worker_STATE_CONNECTED;
        	continue;
        }

        if((events[idx].events & EPOLLERR) ||
               (events[idx].events & EPOLLHUP)){
                printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
                fprintf(stderr,"Connection epoll error：%d\n",events[idx].events);
                printf("Connected server failed with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
                blockWorkerPtr->curState = Worker_STATE_CONNECTFAILED;

                continue;
		}else{
			printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
			printf("Unknow status for %d for blockServer IPAddr:%s, port:%d\n",events[idx].events, blockWorkerPtr->IPAddr, blockWorkerPtr->port);
		}

	}
}

void connectBlockWorkersForWrite(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;

	ECBlockWorker_t *curBlockWorkerPtr = (ECBlockWorker_t *)ecFilePtr->blockWorkers;
	int connectingNum = 0;

	while(connectingNum < ecFilePtr->blockWorkerNum){

		connectABlockWorker(ecBlockWorkerMgr, curBlockWorkerPtr);
		curBlockWorkerPtr = curBlockWorkerPtr->next;
		++connectingNum;
	}

	if (ecBlockWorkerMgr->inMonitoringSockNum != 0)
	{
		struct  timeval startTime, endTime;
		gettimeofday(&startTime, NULL);
		double waitInterval = 0.0;
		do{
			waitConnections(ecBlockWorkerMgr);

			if (ecBlockWorkerMgr->inMonitoringSockNum == 0)
			{
				break;
			}

			gettimeofday(&endTime, NULL);
			waitInterval = timeIntervalInMS(startTime, endTime);
		}while(waitInterval < CONNECTING_TIME_OUT_IN_MS);
		//We assume connection will be succeeded, We don't do anything to handle failed connection , plan to do in the future. 

		if (ecBlockWorkerMgr->inMonitoringSockNum != 0)
		{
			printf("We have connection didn't connected\n");
			removeUnConnected(ecBlockWorkerMgr);
			//Exception handle, do something here in the future.
		}
	}
}

void connectBlockWorkersForRead(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;

	ECBlockWorker_t *curBlockWorkerPtr = (ECBlockWorker_t *)ecFilePtr->blockWorkers;
	int connectingNum = 0;

	while(connectingNum < ecFilePtr->blockWorkerNum){

		connectABlockWorker(ecBlockWorkerMgr, curBlockWorkerPtr);
		curBlockWorkerPtr = curBlockWorkerPtr->next;
		++connectingNum;
	}

	if (ecBlockWorkerMgr->inMonitoringSockNum != 0)
	{
		struct  timeval startTime, endTime;
		gettimeofday(&startTime, NULL);
		double waitInterval = 0.0;
		do{
			waitConnections(ecBlockWorkerMgr);

			if (ecBlockWorkerMgr->inMonitoringSockNum == 0)
			{
				break;
			}

			gettimeofday(&endTime, NULL);
			waitInterval = timeIntervalInMS(startTime, endTime);
		}while(waitInterval < CONNECTING_TIME_OUT_IN_MS);
		//We assume connection will be succeeded, We don't do anything to handle failed connection , plan to do in the future. 

		if (ecBlockWorkerMgr->inMonitoringSockNum != 0)
		{
			printf("We have connection didn't connected\n");
			removeUnConnected(ecBlockWorkerMgr);
			//Exception handle, do something here in the future.
		}
	}
}

void allocBlockWorkersToFile(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr){
	int idx, allocNum = ecFilePtr->stripeK + ecFilePtr->stripeM;
	ECBlockWorker_t *curBlockWorkerPtr = NULL;
	for (idx = 0; idx < allocNum; ++idx)
	{
		blockInfo_t *curBlockInfo = ecFilePtr->blocks + idx;

		if (curBlockWorkerPtr == NULL)
		{
			curBlockWorkerPtr = allocABlockWorker(ecBlockWorkerMgr, curBlockInfo);
			ecFilePtr->blockWorkers = curBlockWorkerPtr;
		}else{
			curBlockWorkerPtr->next = allocABlockWorker(ecBlockWorkerMgr, curBlockInfo);
			curBlockWorkerPtr = curBlockWorkerPtr->next;
		}

		curBlockWorkerPtr->workerIdx = curBlockInfo->idx;
		curBlockWorkerPtr->blockId = curBlockInfo->blockId;
		++ecFilePtr->blockWorkerNum;
	}
}

void allockBlockWorkersForNormalRead(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr){
	int idx, allocNum = ecFilePtr->stripeK;
	ECBlockWorker_t *curBlockWorkerPtr = NULL;
	for (idx = 0; idx < allocNum; ++idx)
	{
		blockInfo_t *curBlockInfo = ecFilePtr->blocks + idx;

		if (curBlockWorkerPtr == NULL)
		{
			curBlockWorkerPtr = allocABlockWorker(ecBlockWorkerMgr, curBlockInfo);
			ecFilePtr->blockWorkers = curBlockWorkerPtr;
		}else{
			curBlockWorkerPtr->next = allocABlockWorker(ecBlockWorkerMgr, curBlockInfo);
			curBlockWorkerPtr = curBlockWorkerPtr->next;
		}

		curBlockWorkerPtr->workerIdx = curBlockInfo->idx;
		curBlockWorkerPtr->blockId = curBlockInfo->blockId;
		++ecFilePtr->blockWorkerNum;
	}
}

void allockBlockWorkersForDegradeRead(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr ){
    int idx, allocNum = ecFilePtr->stripeK;
    ECBlockWorker_t *curBlockWorkerPtr = NULL;
    for (idx = 0; idx < allocNum; ++idx)
    {
        blockInfo_t *curBlockInfo = ecFilePtr->blocks + *(ecFilePtr->dm_ids + idx);
        
        if (curBlockWorkerPtr == NULL)
        {
            curBlockWorkerPtr = allocABlockWorker(ecBlockWorkerMgr, curBlockInfo);
            ecFilePtr->blockWorkers = curBlockWorkerPtr;
        }else{
            curBlockWorkerPtr->next = allocABlockWorker(ecBlockWorkerMgr, curBlockInfo);
            curBlockWorkerPtr = curBlockWorkerPtr->next;
        }
        
        curBlockWorkerPtr->workerIdx = curBlockInfo->idx;
        curBlockWorkerPtr->blockId = curBlockInfo->blockId;
        ++ecFilePtr->blockWorkerNum;
    }
}

void submitReadWriteJob(ECBlockWorkerManager_t *ecBlockWorkerMgr){
//    printf("submitReadWriteJob\n");
    workerPerformReadPrint(ecBlockWorkerMgr,"submitReadWriteJob:");
	sem_post(&ecBlockWorkerMgr->jobStartSem);
}

void waitReadWriteJobDone(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	//printf("Start wait write job\n");
	sem_wait(&ecBlockWorkerMgr->jobFinishedSem);
    workerPerformReadPrint(ecBlockWorkerMgr,"waitReadWriteJobDone:");
	//printf("writeJob is done\n");
}

ssize_t performWriteJob(ECFileManager_t *ecFileMgr, int ecFd, char *writeBuf, size_t writeSize){
	ECBlockWorkerManager_t *ecBlockWorkerMgr = (ECBlockWorkerManager_t *)ecFileMgr->blockWorkersMgr;
	ECFile_t *ecFilePtr = ecFileMgr->ecFiles + ecFd;
	ecFilePtr->buf = writeBuf;
	ecFilePtr->bufSize = writeSize;
    ecFilePtr->bufHandledSize = 0;
    ecFilePtr->bufWritedSize = 0;

    ecBlockWorkerMgr->jobDoneFlag = 0;

	if (ecBlockWorkerMgr->curFilePtr == NULL || 
		ecBlockWorkerMgr->curFilePtr != ecFilePtr || 
		ecBlockWorkerMgr->curJobState != ECJOB_WRITE)
	{
		ecBlockWorkerMgr->curFilePtr = ecFilePtr;
		ecBlockWorkerMgr->curJobState = ECJOB_WRITE;

		if (ecFilePtr->blockWorkerNum == 0)
		{
			allocBlockWorkersToFile(ecBlockWorkerMgr, ecFilePtr);
			connectBlockWorkersForWrite(ecBlockWorkerMgr);
			openBlocksForWrite(ecBlockWorkerMgr);
		}

		setWorkerBufs(ecBlockWorkerMgr, ecFilePtr);
	}
   
	submitReadWriteJob(ecBlockWorkerMgr);
	waitReadWriteJobDone(ecBlockWorkerMgr);

	return (ssize_t) ecFilePtr->bufWritedSize;
}

ssize_t performReadJob(ECFileManager_t *ecFileMgr, int ecFd, char *readBuf, size_t readSize){
	ECBlockWorkerManager_t *ecBlockWorkerMgr = (ECBlockWorkerManager_t *)ecFileMgr->blockWorkersMgr;
	ECFile_t *ecFilePtr = ecFileMgr->ecFiles + ecFd;
	ecFilePtr->buf = readBuf;
	ecFilePtr->bufSize = readSize;
    ecFilePtr->bufHandledSize = 0;
    ecFilePtr->bufWritedSize = 0;

    ecBlockWorkerMgr->jobDoneFlag = 0;

	if (ecBlockWorkerMgr->curFilePtr == NULL || 
		ecBlockWorkerMgr->curFilePtr != ecFilePtr || 
		ecBlockWorkerMgr->curJobState != ECJOB_READ)
	{
		ecBlockWorkerMgr->curFilePtr = ecFilePtr;
		ecBlockWorkerMgr->curJobState = ECJOB_READ;

		if (ecFilePtr->blockWorkerNum == 0)
		{
			allockBlockWorkersForNormalRead(ecBlockWorkerMgr, ecFilePtr);
			connectBlockWorkersForRead(ecBlockWorkerMgr);
			openBlocksForRead(ecBlockWorkerMgr);
            
            workerPerformReadPrint(ecBlockWorkerMgr,"alreadyOpenBlocksForRead:");

		}

		setWorkerBufs(ecBlockWorkerMgr, ecFilePtr);
	}

	submitReadWriteJob(ecBlockWorkerMgr);
	waitReadWriteJobDone(ecBlockWorkerMgr);

	return (ssize_t) ecFilePtr->bufHandledSize;
}

ssize_t performDegradedReadJob(ECFileManager_t *ecFileMgr, int ecFd, char *readBuf, size_t readSize){
		ECBlockWorkerManager_t *ecBlockWorkerMgr = (ECBlockWorkerManager_t *)ecFileMgr->blockWorkersMgr;
	ECFile_t *ecFilePtr = ecFileMgr->ecFiles + ecFd;
	ecFilePtr->buf = readBuf;
	ecFilePtr->bufSize = readSize;
    ecFilePtr->bufHandledSize = 0;
    ecFilePtr->bufWritedSize = 0;

    ecBlockWorkerMgr->jobDoneFlag = 0;
    
//    printf("start degraded read\n");

	if (ecBlockWorkerMgr->curFilePtr == NULL || 
		ecBlockWorkerMgr->curFilePtr != ecFilePtr || 
		ecBlockWorkerMgr->curJobState != ECJOB_DEGRADED_READ)
	{
		ecBlockWorkerMgr->curFilePtr = ecFilePtr;
		ecBlockWorkerMgr->curJobState = ECJOB_DEGRADED_READ;

		if (ecFilePtr->blockWorkerNum == 0)
		{
			allockBlockWorkersForDegradeRead(ecBlockWorkerMgr, ecFilePtr);
			connectBlockWorkersForRead(ecBlockWorkerMgr);
			openBlocksForRead(ecBlockWorkerMgr);
		}

		setWorkerBufs(ecBlockWorkerMgr, ecFilePtr);
	}

    submitReadWriteJob(ecBlockWorkerMgr);
    waitReadWriteJobDone(ecBlockWorkerMgr);

	return (ssize_t) ecFilePtr->bufWritedSize;
}

void revokeBlockWorkers(ECFileManager_t *ecFileMgr, int ecFd){

	ECBlockWorkerManager_t *ecBlockWorkerMgr = (ECBlockWorkerManager_t *)ecFileMgr->blockWorkersMgr;
	ECFile_t *ecFilePtr = ecFileMgr->ecFiles + ecFd;
	int wIdx = 0, blockWorkerNum = ecFilePtr->blockWorkerNum;

	ECBlockWorker_t *blockWorkerPtr = (ECBlockWorker_t *)ecFilePtr->blockWorkers;

	if (ecFilePtr == ecBlockWorkerMgr->curFilePtr)
	{
		ecBlockWorkerMgr->curFilePtr = NULL;
		ecBlockWorkerMgr->curJobState = ECJOB_NONE;
	}

	while(wIdx != blockWorkerNum){
		closeBlockInServer(ecBlockWorkerMgr, blockWorkerPtr);
		blockWorkerPtr->workerInUseFlag = 0;
		blockWorkerPtr = blockWorkerPtr->next;
		++wIdx;
	}

	if (ecBlockWorkerMgr->inMonitoringSockNum != 0	)
	{
		//Send WriteOver or ReadOver Cmd
		processingMonitoringRq(ecBlockWorkerMgr);
	}

	ecFilePtr->blockWorkerNum = 0;
}
