//
//  ECBlockWorker.h
//  ECClient
//
//  Created by Liu Chengjian on 18/4/9.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ECBLOCKWORKER_HEADER__
#define __ECBLOCKWORKER_HEADER__

#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>

#include "MessageBuffer.h"
#include "ECFileHandle.h"
/**
 state transition: UNINIT->INIT->CONNECTED->   START_WRITE->REQUEST_WRITE->WRITE_REQUESTING->WRITE->WRITING-> REQUEST_WRITE -> WRITE_OVER ->CONNECTED
                                          ->   START_READ ->REQUEST_READ->READ_REQUESTING->READ-> READING -> REQUEST_READ  -> READ_OVER  ->CONNECTED
*/

typedef enum{
    Worker_STATE_UNINIT = 0x01,
    Worker_STATE_INIT,
    Worker_STATE_CONNECTING,
    Worker_STATE_CONNECTFAILED,
    Worker_STATE_CONNECTED,
    Worker_STATE_REQUEST_WRITE,
    Worker_STATE_WRITE_REQUESTING,
    Worker_STATE_WRITE, 
    Worker_STATE_WRITING,
    Worker_STATE_WRITE_OVER,
    Worker_STATE_REQUEST_READ,
    Worker_STATE_READ_REQUESTING,
    Worker_STATE_READ,
    Worker_STATE_READING,
    Worker_STATE_READ_OVER
}Worker_STATE_t;

typedef enum 
{	
	Worker_SOCK_STATE_UNINIT = 0x01, //didn't 
	Worker_SOCK_STATE_INIT ,
	Worker_SOCK_STATE_READ,
	Worker_SOCK_STATE_WRITE,
	Worker_SOCK_STATE_READ_WRITE
}Worker_SOCK_STATE_t;

typedef enum{
	ECJOB_NONE = 0x01,
	ECJOB_READ,
	ECJOB_DEGRADED_READ,
	ECJOB_WRITE
}ECJOB_STATE_t;

typedef struct ECBlockWorker{
    int workerIdx;
    uint64_t blockId;
    int blockFd;

    int workerInUseFlag;

    int sockFd;
    Worker_SOCK_STATE_t curSockState;

    char IPAddr[16];
    int IPSize;
    int port;

    Worker_STATE_t curState;
    uint64_t blockIdToHandle;

    struct ECBlockWorker *next;

    ECMessageBuffer_t *readMsgBuf;
    ECMessageBuffer_t *writeMsgBuf;

    char *buf;
    size_t bufSize;
    size_t bufHandledSize;

    size_t totalSend;
}ECBlockWorker_t;

typedef struct workerBuf
{
	char **inputBufs;
	char **outputBufs;

	size_t inputNum;
	size_t outputNum;

	int *inputWritedSize;

	size_t codingSize;

	size_t alignedBufSize; // The max size that can be copyed each time due to alignment
	size_t eachBufSize;
}workerBuf_t;

typedef struct
{
	pthread_t workerPid;
	pthread_t coderPid;

	workerBuf_t *workerBufs;
	int *bufInUseFlag;
	int workerBufsNum;

	size_t defaultBufSize;
	size_t alignedBufSize;

	int eFd;

	int workerThreadInUseFlag;
	int jobDoneFlag;
	int exitFlag;

	ECBlockWorker_t *blockWorkers;
	int blockWorkerNum;
	int blockWorkerIncreSize;

	int maxInFlightReq;
	int curInFlightReq;

	void *events;
	int eventsNum;
	int inMonitoringSockNum;

	sem_t jobStartSem;
	sem_t jobFinishedSem;

	ECJOB_STATE_t curJobState;

	ECFile_t *curFilePtr;
	
	void *coderWorker;
	void *clientEnginePtr;

}ECBlockWorkerManager_t;

ECBlockWorkerManager_t *createBlockWorkerMgr(int increSize);
void deallocBlockWorkerMgr(ECBlockWorkerManager_t *ecBlockWorkerMgr);

ssize_t performWriteJob(ECFileManager_t *ecFileMgr, int ecFd, char *writeBuf, size_t writeSize);
ssize_t performReadJob(ECFileManager_t *ecFileMgr, int ecFd, char *readBuf, size_t readSize);
ssize_t performDegradedReadJob(ECFileManager_t *ecFileMgr, int ecFd, char *readBuf, size_t readSize);

void revokeBlockWorkers(ECFileManager_t *ecFileMgr, int ecFd);

#endif

