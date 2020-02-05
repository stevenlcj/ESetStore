//
//  ECClientHandle.c
//  ECServer
//
//  Created by Liu Chengjian on 18/4/16.
//  Copyright (c) 2018年 csliu. All rights reserved.
//


#include "ECClientHandle.h"
#include "ClientBlockServerCmd.h"
#include "DiskIOHandler.h"
#include "ecNetHandle.h"
#include "ECClientResponse.h"
#include "ecUtils.h"
#include "DiskJobQueue.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>

void addWriteEvent(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
	if (ecClientPtr->connState == CLIENT_CONN_STATE_READWRITE)
	{
		return;
	}

    ecClientPtr->connState = CLIENT_CONN_STATE_READWRITE;
    update_event(ecClientMgr->efd, EPOLLIN | EPOLLOUT  , ecClientPtr->sockFd, ecClientPtr);
}

void removeWriteEvent(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
	if (ecClientPtr->connState != CLIENT_CONN_STATE_READWRITE)
	{
		return;
	}

    update_event(ecClientMgr->efd, EPOLLIN , ecClientPtr->sockFd, ecClientPtr);
    ecClientPtr->connState = CLIENT_CONN_STATE_READ;
}

//cmdFlag is \0
int hasInCmd(char *buf, size_t bufSize, char cmdFlag){
    int parseFlag = 0;
    int idx;
    
    if (bufSize == 0) {
        return parseFlag;
    }
    
    for (idx = 0; idx < bufSize; ++idx) {
        if (*(buf + idx) == cmdFlag){
        	return 1;
        }
    }
    
    return parseFlag;
}

int addCreateOKCmd(ECClient_t *ecClientPtr){
	ECMessageBuffer_t *wMsgBuf = ecClientPtr->writeMsgBuf;
	while(wMsgBuf->next != NULL){
		wMsgBuf = wMsgBuf->next;
	}

	size_t wSize = constructCreateOKCmd(wMsgBuf->buf + wMsgBuf->wOffset,
														wMsgBuf->bufSize-wMsgBuf->wOffset,
														ecClientPtr->blockId, ecClientPtr->fileFd);

	if (wSize == 0)
	{
		wMsgBuf->next =  createMessageBuf(wMsgBuf->bufSize);
		wMsgBuf = wMsgBuf->next;
        wSize = constructCreateOKCmd(wMsgBuf->buf +
                             wMsgBuf->wOffset,
                             wMsgBuf->bufSize-wMsgBuf->wOffset,
                             ecClientPtr->blockId, ecClientPtr->fileFd);
	}
    
    wMsgBuf->wOffset = wMsgBuf->wOffset + wSize;
    ecClientPtr->pendingWriteSizeToSock = ecClientPtr->pendingWriteSizeToSock + wSize;
	
	return 1;
								
}

int addCreateFailedCmd(ECClient_t *ecClientPtr){
	ECMessageBuffer_t *wMsgBuf = ecClientPtr->writeMsgBuf;

	while(wMsgBuf->next != NULL){
		wMsgBuf = wMsgBuf->next;
	}

	size_t wSize = constructCreateFailedCmd(wMsgBuf->buf + wMsgBuf->wOffset,
                                            wMsgBuf->bufSize-wMsgBuf->wOffset,
                                            ecClientPtr->blockId,NULL, 0);

	if (wSize == 0)
	{
		wMsgBuf->next =  createMessageBuf(wMsgBuf->bufSize);
		wMsgBuf = wMsgBuf->next;
		wSize = constructCreateFailedCmd(wMsgBuf->buf + wMsgBuf->wOffset,
                                        wMsgBuf->bufSize-wMsgBuf->wOffset,
                                        ecClientPtr->blockId,
                                        NULL, 0);
	
	}
    
    wMsgBuf->wOffset = wMsgBuf->wOffset + wSize;
    ecClientPtr->pendingWriteSizeToSock = ecClientPtr->pendingWriteSizeToSock + wSize;
	
	return 1;
								
}

int processClientCreateRequest(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
	ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)ecClientMgr->serverEnginePtr;
	DiskIOManager_t *diskIOPtr = blockServerEnginePtr->diskIOMgr;
	char blockIdStr[] = "BlockId:\0";
	char suffixStr[] = "\r\n\0";
	ecClientPtr->blockId = getUInt64ValueBetweenStrs(blockIdStr, suffixStr, ecClientPtr->readMsgBuf->buf, ecClientPtr->readMsgBuf->wOffset);
	
	//printf("Create file with blockId:%lu\n", ecClientPtr->blockId);
    printf("sockFd:%d create block:%llu\n",ecClientPtr->sockFd, ecClientPtr->blockId);

	ecClientPtr->fileFd = startWriteFile(ecClientPtr->blockId, diskIOPtr, ecClientPtr->sockFd);

	if (ecClientPtr->fileFd >= 0)
	{
		addCreateOKCmd(ecClientPtr);
	}else{
		addCreateFailedCmd(ecClientPtr);
	}

	if (ecClientPtr->connState != CLIENT_CONN_STATE_READWRITE)
	{
		size_t writeSize = 0;

		processClientOutMsg(ecClientMgr, ecClientPtr, &writeSize);
	}

	return 1;
}

int addOpenOKCmd(ECClient_t *ecClientPtr){
	ECMessageBuffer_t *wMsgBuf = ecClientPtr->writeMsgBuf;
	while(wMsgBuf->next != NULL){
		wMsgBuf = wMsgBuf->next;
	}
    
	size_t wSize = constructOpenOKCmd(wMsgBuf->buf + wMsgBuf->wOffset,
                                    wMsgBuf->bufSize-wMsgBuf->wOffset,
                                    ecClientPtr->blockId, ecClientPtr->fileFd);

	if (wSize == 0)
	{
		wMsgBuf->next =  createMessageBuf(wMsgBuf->bufSize);
		wMsgBuf = wMsgBuf->next;
		wSize = constructOpenOKCmd(wMsgBuf->buf + wMsgBuf->wOffset,
                                wMsgBuf->bufSize-wMsgBuf->wOffset,
                                ecClientPtr->blockId, ecClientPtr->fileFd);
	
	}
    
    ecClientPtr->pendingWriteSizeToSock = ecClientPtr->pendingWriteSizeToSock + wSize;
    wMsgBuf->wOffset = wMsgBuf->wOffset + wSize;
	
	return 1;
								
}

int addOpenFailedCmd(ECClient_t *ecClientPtr){
	ECMessageBuffer_t *wMsgBuf = ecClientPtr->writeMsgBuf;

	while(wMsgBuf->next != NULL){
		wMsgBuf = wMsgBuf->next;
	}

	size_t wSize = constructOpenFailedCmd(wMsgBuf->buf + wMsgBuf->wOffset,
                                        wMsgBuf->bufSize-wMsgBuf->wOffset,
                                        ecClientPtr->blockId,
                                        NULL, 0);

	if (wSize == 0)
	{
		wMsgBuf->next =  createMessageBuf(wMsgBuf->bufSize);
		wMsgBuf = wMsgBuf->next;
		wSize = constructOpenFailedCmd(wMsgBuf->buf + wMsgBuf->wOffset,
                                    wMsgBuf->bufSize-wMsgBuf->wOffset,
                                    ecClientPtr->blockId,
                                    NULL, 0);
	
	}
    
    ecClientPtr->pendingWriteSizeToSock = ecClientPtr->pendingWriteSizeToSock + wSize;
    wMsgBuf->wOffset = wMsgBuf->wOffset + wSize;

	
	return 1;
								
}

int processClientOpenRequest(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
	ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)ecClientMgr->serverEnginePtr;
	DiskIOManager_t *diskIOPtr = blockServerEnginePtr->diskIOMgr;
	char blockIdStr[] = "BlockId:\0";
	char suffixStr[] = "\r\n\0";
	ecClientPtr->blockId = getUInt64ValueBetweenStrs(blockIdStr, suffixStr, ecClientPtr->readMsgBuf->buf, ecClientPtr->readMsgBuf->wOffset);
    
	ecClientPtr->fileFd = startReadFile(ecClientPtr->blockId, diskIOPtr,ecClientPtr->sockFd);

	if (ecClientPtr->fileFd >= 0)
	{
        printf("Open OK for sock:%d To read:%llu\n",ecClientPtr->sockFd, ecClientPtr->blockId);
		addOpenOKCmd(ecClientPtr);
	}else{
        printf("Open Failed for sock:%d To read:%llu\n",ecClientPtr->sockFd, ecClientPtr->blockId);
		addOpenFailedCmd(ecClientPtr);
	}

	if (ecClientPtr->connState != CLIENT_CONN_STATE_READWRITE)
	{
		size_t writeSize = 0;

		processClientOutMsg(ecClientMgr, ecClientPtr, &writeSize);
        printf("write:%lu to sock:%d To read:%llu\n",writeSize,ecClientPtr->sockFd, ecClientPtr->blockId);
	}

	return 0;
}

//BlockWrite/r/nBlockId:__/r/nBlockFd:__/r/nWriteSize:__/r/n/0
int processClientWritingRequest(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
	char blockIdStr[] = "BlockId:\0";
	char blockFdStr[] = "BlockFd:\0";
	char writeSize[] = "WriteSize:\0";
	char suffixStr[] = "\r\n\0";

	ecClientPtr->blockId = getUInt64ValueBetweenStrs(blockIdStr, suffixStr, ecClientPtr->readMsgBuf->buf, ecClientPtr->readMsgBuf->wOffset);
	ecClientPtr->fileFd = getIntValueBetweenStrings(blockFdStr, suffixStr, ecClientPtr->readMsgBuf->buf);
	ecClientPtr->reqSize =	getIntValueBetweenStrings(writeSize, suffixStr, ecClientPtr->readMsgBuf->buf);
	ecClientPtr->handledSize = 0;

	//printf("blockId:%lu, fd:%d, reqSize:%lu\n",ecClientPtr->blockId, ecClientPtr->fileFd, ecClientPtr->reqSize);
	return 0;
}

size_t readRawDataToBuf(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr, char *buf, size_t reqSize){
	ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)ecClientMgr->serverEnginePtr;
    DiskIOManager_t *diskIOMgr = blockServerEnginePtr->diskIOMgr;
    size_t readedSize = 0;
    ssize_t readSize = 0;
    do{
    	readSize = readFile(ecClientPtr->fileFd, buf + readedSize, reqSize, diskIOMgr, ecClientPtr->sockFd);
    	if (readSize <= 0)
    	{
    		perror("Unable to read");
 			exit(0);
    	}
    	readedSize = readedSize + (readSize);
    }while(readedSize != reqSize);

    return readSize;
}

int submitReadJobToDiskWorker(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
    
    ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)ecClientMgr->serverEnginePtr;
    DiskIOManager_t *diskIOMgr = blockServerEnginePtr->diskIOMgr;
    DiskJobWorker_t *diskJobWorkerPtr = diskIOMgr->diskJobWorkerPtr;

    size_t handledSize = 0, handlingSize = 0;
    
    while (handledSize != ecClientPtr->reqSize) {
        DiskJob_t *diskJobPtr = requestDiskJob(diskJobWorkerPtr);
        diskJobPtr->sourcePtr = (void *)ecClientPtr;
        diskJobPtr->diskFd = ecClientPtr->fileFd;
        
        handlingSize = ecClientPtr->reqSize > diskJobPtr->bufSize ?
                        diskJobPtr->bufSize: ecClientPtr->reqSize;
        diskJobPtr->bufReqSize = handlingSize;
        diskJobPtr->bufHandledSize = 0;
        diskJobPtr->jobType = DISK_IO_JOB_READ;
        handledSize = handledSize + handlingSize;
        
        //printf("Submit read job with size:%lu\n", handlingSize);
        putDiskIOJob(diskJobWorkerPtr, diskJobPtr);
    }

    return 0;
}

//BlockRead/r/nBlockId:__/r/nBlockFd:__/r/nReadSize:__/r/n/0
int processClientReadingRequest(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
	char blockIdStr[] = "BlockId:\0";
	char blockFdStr[] = "BlockFd:\0";
	char readSize[] = "ReadSize:\0";
	char suffixStr[] = "\r\n\0";

	ecClientPtr->blockId = getUInt64ValueBetweenStrs(blockIdStr, suffixStr, ecClientPtr->readMsgBuf->buf, ecClientPtr->readMsgBuf->wOffset);
	ecClientPtr->fileFd = getIntValueBetweenStrings(blockFdStr, suffixStr, ecClientPtr->readMsgBuf->buf);
	ecClientPtr->reqSize =	getIntValueBetweenStrings(readSize, suffixStr, ecClientPtr->readMsgBuf->buf);
	ecClientPtr->handledSize = 0;
    
    ecClientPtr->readRequestTotal = ecClientPtr->readRequestTotal + ecClientPtr->reqSize;
    
//    printf("sockFd:%d, blockId:%llu, reqReadSize:%lu\n",ecClientPtr->sockFd, ecClientPtr->blockId, ecClientPtr->reqSize );
	return submitReadJobToDiskWorker(ecClientMgr, ecClientPtr);
}

//BlockClose/r/nBlockId:__/r/nBlockFd:__/r/n/0
int processClientCloseRequest(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
	char blockIdStr[] = "BlockId:\0";
	char blockFdStr[] = "BlockFd:\0";
	char suffixStr[] = "\r\n\0";

	ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)ecClientMgr->serverEnginePtr;
	DiskIOManager_t *diskIOPtr = blockServerEnginePtr->diskIOMgr;

	ecClientPtr->blockId = getUInt64ValueBetweenStrs(blockIdStr, suffixStr, ecClientPtr->readMsgBuf->buf, ecClientPtr->readMsgBuf->wOffset);
	ecClientPtr->fileFd = getIntValueBetweenStrings(blockFdStr, suffixStr, ecClientPtr->readMsgBuf->buf);

    printf("Sock:%d, Close block:%lu, fd:%d\n",ecClientPtr->sockFd, ecClientPtr->blockId, ecClientPtr->fileFd);
	closeFile(ecClientPtr->fileFd, diskIOPtr);

	return 0;
}

int writeDataToDisk(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
	ssize_t writeSize = 0;
	size_t totalWriteSize = 0, canWriteSize = 0;

	ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)ecClientMgr->serverEnginePtr;
	DiskIOManager_t *diskIOPtr = blockServerEnginePtr->diskIOMgr;
	ECMessageBuffer_t *rMsgBuf = ecClientPtr->readMsgBuf;

	size_t remainHandledSize = (ecClientPtr->reqSize - ecClientPtr->handledSize);

	if (remainHandledSize > rMsgBuf->wOffset)
	{
		canWriteSize = rMsgBuf->wOffset;
	}else{
		canWriteSize = remainHandledSize;
	}

	do{
		writeSize = writeFile(ecClientPtr->fileFd, rMsgBuf->buf + totalWriteSize, (canWriteSize - totalWriteSize), diskIOPtr, ecClientPtr->sockFd);
		if (writeSize > 0)
		{
			totalWriteSize = totalWriteSize + (size_t)writeSize;
		}else{
			perror("Unable to write data to disk");
		}
	}while(canWriteSize != totalWriteSize);

	ecClientPtr->handledSize = ecClientPtr->handledSize + totalWriteSize;

	//printf("reqSize:%lu write to disk size:%lu, handledSize:%lu\n",ecClientPtr->reqSize, totalWriteSize, ecClientPtr->handledSize);
	//printf("\t\tdump:%lu, wOffset:%lu\n", ecClientMgr->totalDump, rMsgBuf->wOffset);
	ecClientMgr->totalDump = ecClientMgr->totalDump +  dumpMsgWithSize(rMsgBuf, totalWriteSize);
	//printf("recv:%lu, dump:%lu,offset:%d, wOffset:%lu\n",ecClientMgr->totalRecv, ecClientMgr->totalDump, 
	//		((int)ecClientMgr->totalRecv - (int)ecClientMgr->totalDump),rMsgBuf->wOffset);
	if (ecClientPtr->reqSize == ecClientPtr->handledSize)
	{
		return 1;
	}

	return 0;
}	

int getClientInCmd(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
	ECCLIENT_IN_STATE_t inCmdState = parseClientInCmd(ecClientPtr);
//    printf("recv cmd from scok:%d\n",ecClientPtr->sockFd);
	switch(inCmdState){
		case ECCLIENT_IN_STATE_CREATE:{
			processClientCreateRequest(ecClientMgr, ecClientPtr);
		}
		break;
		case ECCLIENT_IN_STATE_OPEN:{
			processClientOpenRequest(ecClientMgr, ecClientPtr);
		}
		break;
		case ECCLIENT_IN_STATE_WRITING:{
			processClientWritingRequest(ecClientMgr, ecClientPtr);
			ecClientPtr->clientInState = ECCLIENT_IN_STATE_WRITING;
		}
		break;
		case ECCLIENT_IN_STATE_READING:{
			processClientReadingRequest(ecClientMgr, ecClientPtr);
		}
		break;
		case ECCLIENT_IN_STATE_CLOSE:{
			processClientCloseRequest(ecClientMgr, ecClientPtr);
		}
		break;
		default :
			printf("Unkow msg:%s\n", ecClientPtr->readMsgBuf->buf);
			printf("recvd:%lu, dump:%lu\n", ecClientMgr->totalRecv, ecClientMgr->totalDump);
			exit(0);
		break;
	}

	return 0;
}	

int processClientInCmd(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr){
	ECMessageBuffer_t *rMsgBuf = ecClientPtr->readMsgBuf;

	if (rMsgBuf->wOffset == 0)
	{
		return 0;
	}

	char cmdFlag = '\0';
	switch(ecClientPtr->clientInState){
		case ECCLIENT_IN_STATE_WAIT:{
			if (hasInCmd(rMsgBuf->buf, rMsgBuf->wOffset, cmdFlag) == 1)
			{
				//printf("Recv cmd:%s\n", rMsgBuf->buf);
				getClientInCmd(ecClientMgr, ecClientPtr);

				//if (ecClientPtr->clientInState == ECCLIENT_IN_STATE_WAIT){
				//	printf("            dump:%lu, wOffset:%lu Dump msg:%s\n",ecClientMgr->totalDump, rMsgBuf->wOffset, rMsgBuf->buf);
				//}

				ecClientMgr->totalDump = ecClientMgr->totalDump +  dumpMsgWithCh(rMsgBuf, cmdFlag);

				//if (ecClientPtr->clientInState == ECCLIENT_IN_STATE_WAIT)
				//{
				//		printf("recv:%lu, dump:%lu,offset:%d, wOffset:%lu\n",ecClientMgr->totalRecv, ecClientMgr->totalDump, ((int)ecClientMgr->totalRecv - (int)ecClientMgr->totalDump),rMsgBuf->wOffset);
				//}

				return processClientInCmd(ecClientMgr, ecClientPtr);
			}
		}
		break;
		case ECCLIENT_IN_STATE_WRITING:{
			if (writeDataToDisk(ecClientMgr, ecClientPtr) == 1)
			{
				//Done writing
				//printf("Done writing\n");
				ecClientPtr->clientInState = ECCLIENT_IN_STATE_WAIT;
				return processClientInCmd(ecClientMgr, ecClientPtr);
			}
		}
		break;
		default:{
			printf("Unknow state for clientInState\n");
		}
	}

	return 0;
}

int processClientInMsg(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr, size_t *readSize){
	ECMessageBuffer_t *rMsgBuf = ecClientPtr->readMsgBuf;
	ssize_t recvdSize = recv(ecClientPtr->sockFd, (void *)(rMsgBuf->buf + rMsgBuf->wOffset), (rMsgBuf->bufSize-rMsgBuf->wOffset), 0);

	//printf("Recvd %ld bytes from client\n", recvdSize);

	if (recvdSize < 0)
	{
		return (int ) recvdSize;
	}

	if (recvdSize >= 0)
	{
		ecClientMgr->totalRecv = ecClientMgr->totalRecv + recvdSize;
		*readSize = *readSize + (size_t)recvdSize;
		rMsgBuf->wOffset = rMsgBuf->wOffset + (size_t)recvdSize;
	}

	if (rMsgBuf->wOffset == rMsgBuf->bufSize) {
            return 1;
    }

    return 0;
}

int processClientOutMsg(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr, size_t *writeSize){
	ECMessageBuffer_t *wMsgBuf = ecClientPtr->writeMsgBuf;
    
    while (wMsgBuf->rOffset != wMsgBuf->wOffset ) {
             
        ssize_t wSize = send(ecClientPtr->sockFd,  wMsgBuf->buf+ wMsgBuf->rOffset, (wMsgBuf->wOffset-wMsgBuf->rOffset), 0);
        
        if (wSize <= 0) {
            addWriteEvent(ecClientMgr, ecClientPtr);
            return 1;
        }
        
        if (ecClientPtr->readPendingToWriteToSockTotal > 0) {
            ecClientPtr->readPendingToWriteToSockTotal = ecClientPtr->readPendingToWriteToSockTotal - wSize;
        }
        
        ecClientPtr->pendingWriteSizeToSock = ecClientPtr->pendingWriteSizeToSock - wSize;
        ecClientPtr->writedSizeToSock = ecClientPtr->writedSizeToSock + wSize;
        
        *writeSize = *writeSize + wSize;

        wMsgBuf->rOffset = wMsgBuf->rOffset + wSize;
        
        if (wMsgBuf->rOffset == wMsgBuf->wOffset) {
            if (wMsgBuf->next != NULL) {
                ecClientPtr->writeMsgBuf = wMsgBuf->next;
                freeMessageBuf(wMsgBuf);
                wMsgBuf = ecClientPtr->writeMsgBuf;
            }else{
                wMsgBuf->rOffset = 0;
                wMsgBuf->wOffset = 0;
            }
        }
    }
    
    if (ecClientPtr->connState == CLIENT_CONN_STATE_READWRITE) {
        removeWriteEvent(ecClientMgr,ecClientPtr);
    }
    
    return 0;	
}

void writeContentToClient(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr, DiskJob_t *diskJobPtr){
    size_t cpSize = 0,cpedSize = 0;
    ECMessageBuffer_t *writeMsgBufPtr = ecClientPtr->writeMsgBuf;
    
    while (writeMsgBufPtr->next != NULL) {
        writeMsgBufPtr = writeMsgBufPtr->next;
    }
    
    
    while (diskJobPtr->bufReqSize != diskJobPtr->bufHandledSize) {
        cpSize = (writeMsgBufPtr->bufSize - writeMsgBufPtr->wOffset) > (diskJobPtr->bufReqSize - diskJobPtr->bufHandledSize)
        ? (diskJobPtr->bufReqSize - diskJobPtr->bufHandledSize)
        : (ecClientPtr->reqSize - cpedSize);
        
        memcpy(writeMsgBufPtr->buf + writeMsgBufPtr->wOffset,
               diskJobPtr->buf + diskJobPtr->bufHandledSize, cpSize);
        
        
        writeMsgBufPtr->wOffset = writeMsgBufPtr->wOffset + cpSize;
        
        diskJobPtr->bufHandledSize = diskJobPtr->bufHandledSize + cpSize;
        
        if (diskJobPtr->bufHandledSize != diskJobPtr->bufReqSize) {
            writeMsgBufPtr->next = createMessageBuf(writeMsgBufPtr->bufSize);
            writeMsgBufPtr = writeMsgBufPtr->next;
        }
    }
    
    ecClientPtr->readTotal =  ecClientPtr->readTotal + diskJobPtr->bufHandledSize;
    ecClientPtr->readPendingToWriteToSockTotal = ecClientPtr->readPendingToWriteToSockTotal + diskJobPtr->bufHandledSize;
    ecClientPtr->pendingWriteSizeToSock = ecClientPtr->pendingWriteSizeToSock + diskJobPtr->bufHandledSize;
    
    size_t writeSize = 0;
    processClientOutMsg(ecClientMgr, ecClientPtr, &writeSize);
}

int writeClientReadContent(ECClientManager_t *ecClientMgr){
    DiskJob_t *diskJobPtr = NULL;
    ECBlockServerEngine_t *blockServerEnginePtr = (ECBlockServerEngine_t *)ecClientMgr->serverEnginePtr;
    DiskIOManager_t *diskIOMgr = blockServerEnginePtr->diskIOMgr;
    DiskJobWorker_t *diskJobWorkerPtr = diskIOMgr->diskJobWorkerPtr;
    
    while ((diskJobPtr = dequeueDiskJob(ecClientMgr->handlingJobQueue)) != NULL) {
        if (diskJobPtr->jobType == DISK_IO_JOB_READDONE) {
            gettimeofday(&diskJobPtr->startClient, NULL);
            diskJobPtr->bufHandledSize = 0;
            
            ECClient_t *ecClientPtr = (ECClient_t *)diskJobPtr->sourcePtr;
            writeContentToClient(ecClientMgr, ecClientPtr, diskJobPtr);
        }
        
        revokeDiskJob(diskJobWorkerPtr, diskJobPtr);
    }
    
    return 1;
}












