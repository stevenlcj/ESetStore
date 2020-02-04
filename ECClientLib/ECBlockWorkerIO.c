//
//  ECBlockWorkerIO.c
//  ECClient
//
//  Created by Liu Chengjian on 18/4/12.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "ECBlockWorkerIO.h"

#include "ECBlockWorker.h"
#include "ECFileHandle.h"
#include "ECClientCommon.h"
#include "ECNetHandle.h"
#include "ECClientCommand.h"
#include "ECStoreCoder.h"
#include "ecUtils.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>

void forwardWorkerState(ECBlockWorker_t *blockWorkerPtr){
	switch(blockWorkerPtr->curState){
		case Worker_STATE_REQUEST_WRITE:
			blockWorkerPtr->curState = Worker_STATE_WRITE_REQUESTING;
		break;
		case Worker_STATE_REQUEST_READ:
			blockWorkerPtr->curState = Worker_STATE_READ_REQUESTING;
		break;
		case Worker_STATE_WRITE_REQUESTING:
			blockWorkerPtr->curState = Worker_STATE_WRITE;
		break;
		case Worker_STATE_READ_REQUESTING:
			blockWorkerPtr->curState = Worker_STATE_READ;
		break;
		case Worker_STATE_WRITE_OVER:
		case Worker_STATE_READ_OVER:
			blockWorkerPtr->curState = Worker_STATE_CONNECTED;
		break;
		default:
			printf("Unknow state transition for blockWorker\n");
	}
}

void monitoringSockRead(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	if (blockWorkerPtr->curSockState == Worker_SOCK_STATE_READ || 
		blockWorkerPtr->curSockState == Worker_SOCK_STATE_READ_WRITE)
	{
		return;
	}

	if (blockWorkerPtr->curSockState == Worker_SOCK_STATE_WRITE)
	{
		update_event(ecBlockWorkerMgr->eFd, EPOLLIN | EPOLLOUT | EPOLLET, blockWorkerPtr->sockFd, blockWorkerPtr);
		blockWorkerPtr->curSockState = Worker_SOCK_STATE_READ_WRITE;
	}else{
		add_event(ecBlockWorkerMgr->eFd, EPOLLIN | EPOLLET, blockWorkerPtr->sockFd, blockWorkerPtr);
		blockWorkerPtr->curSockState = Worker_SOCK_STATE_READ;
		++ecBlockWorkerMgr->inMonitoringSockNum;
	}
}

void removeSockEvent(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	delete_event(ecBlockWorkerMgr->eFd, blockWorkerPtr->sockFd);
	blockWorkerPtr->curSockState = Worker_SOCK_STATE_INIT;
	--ecBlockWorkerMgr->inMonitoringSockNum;

	//printf("worker:%d removeSockEvent inMonitoringSockNum:%d\n", blockWorkerPtr->workerIdx, ecBlockWorkerMgr->inMonitoringSockNum);
}

void removeSockReadMonitoring(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	if (blockWorkerPtr->curSockState != Worker_SOCK_STATE_READ &&
		blockWorkerPtr->curSockState != Worker_SOCK_STATE_READ_WRITE)
	{
		return;
	}

	if (blockWorkerPtr->curSockState == Worker_SOCK_STATE_READ_WRITE)
	{
		update_event(ecBlockWorkerMgr->eFd, EPOLLOUT | EPOLLET, blockWorkerPtr->sockFd, blockWorkerPtr);
		blockWorkerPtr->curSockState = Worker_SOCK_STATE_WRITE;
		return;
	}

	removeSockEvent(ecBlockWorkerMgr, blockWorkerPtr);	
}

void removeSockWriteMonitoring(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	if (blockWorkerPtr->curSockState != Worker_SOCK_STATE_WRITE &&
		blockWorkerPtr->curSockState != Worker_SOCK_STATE_READ_WRITE)
	{
		return;
	}

	if (blockWorkerPtr->curSockState == Worker_SOCK_STATE_READ_WRITE)
	{
		update_event(ecBlockWorkerMgr->eFd, EPOLLIN | EPOLLET, blockWorkerPtr->sockFd, blockWorkerPtr);
		blockWorkerPtr->curSockState = Worker_SOCK_STATE_READ;
		return;
	}
	
	removeSockEvent(ecBlockWorkerMgr, blockWorkerPtr);	
}

void monitoringSockWrite(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	if (blockWorkerPtr->curSockState == Worker_SOCK_STATE_WRITE || blockWorkerPtr->curSockState == Worker_SOCK_STATE_READ_WRITE)
	{
		return;
	}

	if (blockWorkerPtr->curSockState == Worker_SOCK_STATE_READ)
	{
		update_event(ecBlockWorkerMgr->eFd, EPOLLIN | EPOLLOUT | EPOLLET, blockWorkerPtr->sockFd, blockWorkerPtr);
		blockWorkerPtr->curSockState = Worker_SOCK_STATE_READ_WRITE;

	}else{
		add_event(ecBlockWorkerMgr->eFd, EPOLLOUT | EPOLLET, blockWorkerPtr->sockFd, blockWorkerPtr);
		blockWorkerPtr->curSockState = Worker_SOCK_STATE_WRITE;
		++ecBlockWorkerMgr->inMonitoringSockNum;
	}
}

void getBlockFd(ECBlockWorker_t *blockWorkerPtr){
	char fdStr[] = "BlockFd:\0";
	char suffix[] = "\r\n\0";
	blockWorkerPtr->blockFd = getIntValueBetweenStrings(fdStr, suffix, blockWorkerPtr->readMsgBuf->buf);
}

int recvReplyFromBlockServer(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){

	if (blockWorkerPtr->curState == Worker_STATE_CONNECTED)
	{
		return 1;
	}

	char cmdFlag = '\0';

	ssize_t recvdSize = recv(blockWorkerPtr->sockFd, blockWorkerPtr->readMsgBuf->buf + blockWorkerPtr->readMsgBuf->wOffset,
							 blockWorkerPtr->readMsgBuf->bufSize-blockWorkerPtr->readMsgBuf->wOffset, 0);

	if (recvdSize > 0)
	{
		blockWorkerPtr->readMsgBuf->wOffset = blockWorkerPtr->readMsgBuf->wOffset + recvdSize;
	}

	if (msgBufHasChar(blockWorkerPtr->readMsgBuf, '\0') ==  1)
	{
//		printf("Recvd:%s from server\n", blockWorkerPtr->readMsgBuf->buf);
		switch(blockWorkerPtr->curState){
			case Worker_STATE_WRITE_REQUESTING:
			{
				char blockCreateOK[] = "BlockCreateOK\0";
				char blockCreateFailed[] = "BlockCreateFailed\0";

				if (strncmp(blockWorkerPtr->readMsgBuf->buf, blockCreateOK, strlen(blockCreateOK)) == 0)
				{
					getBlockFd(blockWorkerPtr);
				}else if(strncmp(blockWorkerPtr->readMsgBuf->buf, blockCreateFailed, strlen(blockCreateFailed)) == 0){
					printf("BlockCreateFailed for block:%lu\n", blockWorkerPtr->blockId);
				}else{
					printf("Unknow str for Worker_STATE_WRITE_REQUESTING\n");
				}
			}
			break;
			case Worker_STATE_READ_REQUESTING:{
				char blockOpenOK[] = "BlockOpenOK\0";
				char blockOpenFailed[] = "BlockOpenFailed\0";

				if (strncmp(blockWorkerPtr->readMsgBuf->buf, blockOpenOK, strlen(blockOpenOK)) == 0)
				{
					getBlockFd(blockWorkerPtr);
                    printf("get blockFd:%d for block:%llu\n", blockWorkerPtr->blockFd,blockWorkerPtr->blockId);
				}else if(strncmp(blockWorkerPtr->readMsgBuf->buf, blockOpenFailed, strlen(blockOpenFailed)) == 0){
					printf("BlockOpenFailed for block:%lu\n", blockWorkerPtr->blockId);
				}else{
					printf("Unknow str for Worker_STATE_WRITE_REQUESTING\n");
					printf("Recvd:%s, wOffset:%lu\n",blockWorkerPtr->readMsgBuf->buf,blockWorkerPtr->readMsgBuf->wOffset );
				}

			}
			break;
			default:{
                printf("unknow state for worker with blockId:%llu\n",blockWorkerPtr->blockId);
			}
		}
		//printf("Recvd reply:%s\n", blockWorkerPtr->readMsgBuf->buf);
		dumpMsgWithCh(blockWorkerPtr->readMsgBuf, cmdFlag);
		forwardWorkerState(blockWorkerPtr);
		return 1;
	}

	monitoringSockRead(ecBlockWorkerMgr, blockWorkerPtr);
	return 0;
}

int sendRequestToBlockServer(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	ssize_t writedSize = send(blockWorkerPtr->sockFd, 
							blockWorkerPtr->writeMsgBuf->buf + blockWorkerPtr->writeMsgBuf->rOffset,
							 blockWorkerPtr->writeMsgBuf->wOffset-blockWorkerPtr->writeMsgBuf->rOffset, 0);

	if (writedSize > 0)
	{
		blockWorkerPtr->totalSend = blockWorkerPtr->totalSend + writedSize;
		//printf("totalSend:%lu Writed size:%ld rOffset:%lu, wOffset:%lu\n",blockWorkerPtr->totalSend, writedSize,blockWorkerPtr->writeMsgBuf->rOffset,blockWorkerPtr->writeMsgBuf->wOffset );
		blockWorkerPtr->writeMsgBuf->rOffset = blockWorkerPtr->writeMsgBuf->rOffset + writedSize;
	}


	if (blockWorkerPtr->writeMsgBuf->rOffset == blockWorkerPtr->writeMsgBuf->wOffset)
	{
		//printf("Sended cmd:%s\n", blockWorkerPtr->writeMsgBuf->buf);
		forwardWorkerState(blockWorkerPtr);
		blockWorkerPtr->writeMsgBuf->rOffset = 0;
		blockWorkerPtr->writeMsgBuf->wOffset = 0;

		return 1;
	}

	monitoringSockWrite(ecBlockWorkerMgr, blockWorkerPtr);
	return 0;
}

int openBlockForWrite(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	if (blockWorkerPtr->curState == Worker_STATE_WRITE)
	{
		return 1;
	}

	if (blockWorkerPtr->curState == Worker_STATE_CONNECTED)
	{
		blockWorkerPtr->writeMsgBuf->wOffset = constructCreateBlockCmd(blockWorkerPtr->writeMsgBuf->buf, blockWorkerPtr->blockId);
		//formBlockWriteCmd(blockWorkerPtr->writeMsgBuf->buf, blockWorkerPtr->blockId);
		blockWorkerPtr->curState = Worker_STATE_REQUEST_WRITE;
		return sendRequestToBlockServer(ecBlockWorkerMgr, blockWorkerPtr);
	}

	return 0;
}

int openBlockForRead(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	if (blockWorkerPtr->curState == Worker_STATE_WRITE)
	{
		return 1;
	}

	if (blockWorkerPtr->curState == Worker_STATE_CONNECTED)
	{
		blockWorkerPtr->writeMsgBuf->wOffset = constructOpenBlockCmd(blockWorkerPtr->writeMsgBuf->buf, blockWorkerPtr->blockId);
        printf("openBlock:%llu ForRead\n",blockWorkerPtr->blockId);
		//formBlockWriteCmd(blockWorkerPtr->writeMsgBuf->buf, blockWorkerPtr->blockId);
		blockWorkerPtr->curState = Worker_STATE_REQUEST_READ;
		return sendRequestToBlockServer(ecBlockWorkerMgr, blockWorkerPtr);
	}

	return 0;
}

void iterateUnFinishedBlockworker(ECBlockWorkerManager_t *ecBlockWorkerMgr){
    ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;
    ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
    int bIdx = 0;
    do
    {
        
        if (blockWorkerPtr->curState = Worker_STATE_REQUEST_READ)
        {
            blockWorkerPtr->readMsgBuf->buf[blockWorkerPtr->readMsgBuf->wOffset] = '\0';
            printf("blockId:%lu,Recvd size:%lu, content:%s\n",blockWorkerPtr->blockId,
                   blockWorkerPtr->readMsgBuf->wOffset,blockWorkerPtr->readMsgBuf->buf);
        }
        
        blockWorkerPtr = blockWorkerPtr->next;
        ++bIdx;
    }while((size_t) bIdx != ecFilePtr->stripeK);
}

void processingMonitoringRq(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	if (ecBlockWorkerMgr->inMonitoringSockNum == 0)
	{
		return;
	}

	struct epoll_event *events = (struct epoll_event *)ecBlockWorkerMgr->events;
	int idx, eventsNum;
	eventsNum = epoll_wait(ecBlockWorkerMgr->eFd, events, MAX_EVENTS,CONNECTING_TIME_OUT_IN_MS);

    if (eventsNum == 0) {
        iterateUnFinishedBlockworker(ecBlockWorkerMgr);
    }
    
	for (idx = 0; idx < eventsNum; ++idx)
	{
        ECBlockWorker_t *blockWorkerPtr = (ECBlockWorker_t *)events[idx].data.ptr;

        if (events[idx].events & EPOLLOUT)
        {
        	if (sendRequestToBlockServer(ecBlockWorkerMgr, blockWorkerPtr) == 1)
        	{
        		removeSockWriteMonitoring(ecBlockWorkerMgr, blockWorkerPtr);
//                printf("removed idx:%d, inMonitoringSockNum:%d\n",
//                        blockWorkerPtr->workerIdx, ecBlockWorkerMgr->inMonitoringSockNum);
        	}
        }else if(events[idx].events & EPOLLIN){
        	if (recvReplyFromBlockServer(ecBlockWorkerMgr, blockWorkerPtr) == 1)
        	{
        		removeSockReadMonitoring(ecBlockWorkerMgr, blockWorkerPtr);
//                printf("removed idx:%d, inMonitoringSockNum:%d\n", 
//                        blockWorkerPtr->workerIdx, ecBlockWorkerMgr->inMonitoringSockNum);
        	}
        }else if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)){
                printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
                fprintf(stderr,"processingMonitoringRq epoll error：%d\n",events[idx].events);
                printf("processingMonitoringRq epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
                removeSockEvent(ecBlockWorkerMgr, blockWorkerPtr);
                blockWorkerPtr->curState = Worker_STATE_CONNECTFAILED;
		}else{
			printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
			printf("Unknow status for event%d\n",events[idx].events);
		}
	}

	if (ecBlockWorkerMgr->inMonitoringSockNum > 0)
	{
		processingMonitoringRq(ecBlockWorkerMgr);
	}
}

int openBlocksForWrite(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;
	ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
	int bIdx = 0, openedBlocks = 0;	
	do
	{

		if (openBlockForWrite(ecBlockWorkerMgr, blockWorkerPtr) == 1)
		{
			++openedBlocks;
		}

		blockWorkerPtr = blockWorkerPtr->next;
		++bIdx;
	}while(bIdx != ecFilePtr->blockWorkerNum);

	if (ecBlockWorkerMgr->inMonitoringSockNum != 0)
	{
		processingMonitoringRq(ecBlockWorkerMgr);
	}

	blockWorkerPtr = ecFilePtr->blockWorkers;
	bIdx = 0;
	do
	{
		recvReplyFromBlockServer(ecBlockWorkerMgr, blockWorkerPtr);
		blockWorkerPtr = blockWorkerPtr->next;
		++bIdx;
	}while(bIdx != ecFilePtr->blockWorkerNum);
	
	if (ecBlockWorkerMgr->inMonitoringSockNum != 0)
	{
		processingMonitoringRq(ecBlockWorkerMgr);
	}

	return 1;
}

int openBlocksForRead(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;
	ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
	int bIdx = 0, openedBlocks = 0;	
	do
	{

		if (openBlockForRead(ecBlockWorkerMgr, blockWorkerPtr) == 1)
		{
			++openedBlocks;
		}

		blockWorkerPtr = blockWorkerPtr->next;
		++bIdx;
	}while((size_t) bIdx != ecFilePtr->stripeK);

	if (ecBlockWorkerMgr->inMonitoringSockNum != 0)
	{
		processingMonitoringRq(ecBlockWorkerMgr);
	}

	blockWorkerPtr = ecFilePtr->blockWorkers;
	bIdx = 0;
	do
	{
		recvReplyFromBlockServer(ecBlockWorkerMgr, blockWorkerPtr);
		blockWorkerPtr = blockWorkerPtr->next;
		++bIdx;
	}while(bIdx != ecFilePtr->blockWorkerNum);
	
	if (ecBlockWorkerMgr->inMonitoringSockNum != 0)
	{
		processingMonitoringRq(ecBlockWorkerMgr);
	}

	return 1;
}

int writeDataSize(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	ssize_t writeSize = send(blockWorkerPtr->sockFd, blockWorkerPtr->writeMsgBuf->buf + blockWorkerPtr->writeMsgBuf->rOffset,
							blockWorkerPtr->writeMsgBuf->wOffset - blockWorkerPtr->writeMsgBuf->rOffset, 0);

	if (writeSize > 0)
	{	
		blockWorkerPtr->totalSend = blockWorkerPtr->totalSend + writeSize;
		blockWorkerPtr->writeMsgBuf->rOffset = blockWorkerPtr->writeMsgBuf->rOffset + writeSize;

		if (blockWorkerPtr->writeMsgBuf->rOffset == blockWorkerPtr->writeMsgBuf->wOffset)
		{
//			printf("Sended cmd:%s\n", blockWorkerPtr->writeMsgBuf->buf);
			blockWorkerPtr->writeMsgBuf->rOffset = 0;
			blockWorkerPtr->writeMsgBuf->wOffset = 0;
			blockWorkerPtr->curState = Worker_STATE_WRITING;
			return 1;
		}

	}

	if (blockWorkerPtr->curSockState != Worker_SOCK_STATE_WRITE &&	
			blockWorkerPtr->curSockState != Worker_SOCK_STATE_READ_WRITE)
	{
		monitoringSockWrite(ecBlockWorkerMgr, blockWorkerPtr);
	}
	return 0;
}

int writeDataContent(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	ssize_t writeSize = send(blockWorkerPtr->sockFd, blockWorkerPtr->buf + blockWorkerPtr->bufHandledSize,
							blockWorkerPtr->bufSize - blockWorkerPtr->bufHandledSize, 0);

	if (writeSize > 0)
	{
		blockWorkerPtr->bufHandledSize = blockWorkerPtr->bufHandledSize + writeSize;
		blockWorkerPtr->totalSend = blockWorkerPtr->totalSend + writeSize;
		if (blockWorkerPtr->bufHandledSize == blockWorkerPtr->bufSize)
		{
			removeSockWriteMonitoring(ecBlockWorkerMgr, blockWorkerPtr);

			blockWorkerPtr->curState = Worker_STATE_WRITE;
			return 1;
		}		
	}

	if (blockWorkerPtr->curSockState != Worker_SOCK_STATE_WRITE &&	
		blockWorkerPtr->curSockState != Worker_SOCK_STATE_READ_WRITE)
	{
		monitoringSockWrite(ecBlockWorkerMgr, blockWorkerPtr);
	}
	return 0;

}

int writeDataToBlockServer(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr, 
								char *bufToWrite, size_t sizeToWrite){
	if (sizeToWrite == 0)
	{
		return 0;
	}

	blockWorkerPtr->buf = bufToWrite;
	blockWorkerPtr->bufSize = sizeToWrite;
	blockWorkerPtr->bufHandledSize = 0;

	blockWorkerPtr->writeMsgBuf->wOffset = constructBlockWriteCmd(blockWorkerPtr->writeMsgBuf->buf,
																	blockWorkerPtr->blockId, blockWorkerPtr->blockFd,
																	blockWorkerPtr->bufSize);
	
	if (writeDataSize(ecBlockWorkerMgr, blockWorkerPtr) == 1)
	{
		return	writeDataContent(ecBlockWorkerMgr, blockWorkerPtr);
	}

	return 0;
}

void copyDataToInputBufs(ECFile_t *ecFilePtr, workerBuf_t *workerBufPtr){
	size_t eachAlignedSize = ecFilePtr->stripeK * ecFilePtr->streamingSize;
	int idx;

	while(ecFilePtr->bufHandledSize < ecFilePtr->bufSize){

		if ((workerBufPtr->alignedBufSize - *(workerBufPtr->inputWritedSize)) < ecFilePtr->streamingSize)
		{
			//Buf is full
			break;
		}

		size_t cpSize = (ecFilePtr->bufSize - ecFilePtr->bufHandledSize);

		if (cpSize >= eachAlignedSize)
		{
			cpSize = eachAlignedSize;
			for (idx = 0; idx < (int) workerBufPtr->inputNum; ++idx)
			{	
				//Unknow state transition for blockWorkerprintf("Aligned *(workerBufPtr->inputWritedSize + %d):%d\n",idx, *(workerBufPtr->inputWritedSize + idx));
				memcpy(workerBufPtr->inputBufs[idx] + *(workerBufPtr->inputWritedSize + idx),
						ecFilePtr->buf + ecFilePtr->bufHandledSize, ecFilePtr->streamingSize);
				*(workerBufPtr->inputWritedSize + idx) = *(workerBufPtr->inputWritedSize + idx) + ecFilePtr->streamingSize;
				ecFilePtr->bufHandledSize = ecFilePtr->bufHandledSize + ecFilePtr->streamingSize;
			}
		}else{
			//Alignment is required here
			for (idx = 0; idx < (int) workerBufPtr->inputNum; ++idx)
			{	
				cpSize = ecFilePtr->streamingSize < (ecFilePtr->bufSize - ecFilePtr->bufHandledSize) 
				         ? ecFilePtr->streamingSize 
				         : (ecFilePtr->bufSize - ecFilePtr->bufHandledSize);
				if(cpSize > 0){
					//printf("*(workerBufPtr->inputWritedSize + %d):%d\n",idx, *(workerBufPtr->inputWritedSize + idx));
					memcpy(workerBufPtr->inputBufs[idx] + *(workerBufPtr->inputWritedSize + idx),
					ecFilePtr->buf + ecFilePtr->bufHandledSize, cpSize);
					*(workerBufPtr->inputWritedSize + idx) = *(workerBufPtr->inputWritedSize + idx) + cpSize;
					ecFilePtr->bufHandledSize = ecFilePtr->bufHandledSize + cpSize;
				} 

				if(cpSize < ecFilePtr->streamingSize){
					memset(workerBufPtr->inputBufs[idx] + *(workerBufPtr->inputWritedSize + idx), 0, ecFilePtr->streamingSize - cpSize);
				}
			}

		}
	}

	if (*(workerBufPtr->inputWritedSize) % ecFilePtr->streamingSize == 0)
	{
		workerBufPtr->codingSize = *(workerBufPtr->inputWritedSize);
	}else{
		workerBufPtr->codingSize = (*(workerBufPtr->inputWritedSize)/ ecFilePtr->streamingSize + 1)*ecFilePtr->streamingSize;
	}
		
	for (idx = 0; idx < (int)workerBufPtr->outputNum; ++idx){
		memset(workerBufPtr->outputBufs[idx], 0, workerBufPtr->codingSize);
	}
}

int flushDataToBlockServers(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	if (ecBlockWorkerMgr->inMonitoringSockNum == 0 )
	{
		return 1;
	}

	struct epoll_event *events = (struct epoll_event *)ecBlockWorkerMgr->events;
	int idx, eventsNum;
	eventsNum = epoll_wait(ecBlockWorkerMgr->eFd, events, MAX_EVENTS,CONNECTING_TIME_OUT_IN_MS);

	for (idx = 0; idx < eventsNum; ++idx)
	{
        ECBlockWorker_t *blockWorkerPtr = (ECBlockWorker_t *)events[idx].data.ptr;

        if (events[idx].events & EPOLLOUT)
        {
        	if (blockWorkerPtr->curState == Worker_STATE_WRITE)
        	{
        		if(writeDataSize(ecBlockWorkerMgr, blockWorkerPtr) != 1){
        			continue;
        		}
        	}

        	writeDataContent(ecBlockWorkerMgr, blockWorkerPtr);

        }else if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)){
                printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
                fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
                printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
                removeSockEvent(ecBlockWorkerMgr, blockWorkerPtr);
                blockWorkerPtr->curState = Worker_STATE_CONNECTFAILED;
		}else{
			printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
            fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
            printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
			printf("Unknow status for event\n" );
		}
	}

	return flushDataToBlockServers(ecBlockWorkerMgr);
}

ssize_t writeBufToServers(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr, workerBuf_t *workerBufPtr){
	/*
	Return is forbidden unless all data sent or connection disconnected
	 a. Send write size to block server
	 b. Send content to block server
	 c. 
	**/

	int bIdx = 0;
	ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;

	do{
		if (bIdx < (int)workerBufPtr->inputNum)
		{
			//printf("bIdx:%d, pending write size:%d\n",bIdx, *(workerBufPtr->inputWritedSize + bIdx));
			writeDataToBlockServer(ecBlockWorkerMgr, blockWorkerPtr,
								   workerBufPtr->inputBufs[bIdx], (size_t) *(workerBufPtr->inputWritedSize + bIdx));
			*(workerBufPtr->inputWritedSize + bIdx) = 0;
		}else{
			writeDataToBlockServer(ecBlockWorkerMgr, blockWorkerPtr,
								   workerBufPtr->outputBufs[bIdx-workerBufPtr->inputNum], workerBufPtr->codingSize);
		}
		
		blockWorkerPtr = blockWorkerPtr->next;
		++bIdx;
	}while(bIdx != ecFilePtr->blockWorkerNum);
	
	flushDataToBlockServers(ecBlockWorkerMgr);

	bIdx = 0;
	blockWorkerPtr = ecFilePtr->blockWorkers;

	do{
		
		if (bIdx < (int) ecFilePtr->stripeK)
		{
			ecFilePtr->bufWritedSize = ecFilePtr->bufWritedSize + blockWorkerPtr->bufHandledSize;
		}

		//printf("bIdx:%d, bufSize:%lu, writed size:%lu\n",bIdx, blockWorkerPtr->bufSize, blockWorkerPtr->bufHandledSize);

		blockWorkerPtr->bufHandledSize = 0;
		blockWorkerPtr->bufSize = 0;
		blockWorkerPtr = blockWorkerPtr->next;
		++bIdx;
	}while(bIdx != ecFilePtr->blockWorkerNum);

	//printf("Buf writed size:%lu\n", ecFilePtr->bufWritedSize);
	return ecFilePtr->bufWritedSize;
}

int writeReadDataSize(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	ssize_t writeSize = send(blockWorkerPtr->sockFd, blockWorkerPtr->writeMsgBuf->buf + blockWorkerPtr->writeMsgBuf->rOffset,
							blockWorkerPtr->writeMsgBuf->wOffset - blockWorkerPtr->writeMsgBuf->rOffset, 0);

	if (writeSize > 0)
	{	
		blockWorkerPtr->totalSend = blockWorkerPtr->totalSend + writeSize;
		blockWorkerPtr->writeMsgBuf->rOffset = blockWorkerPtr->writeMsgBuf->rOffset + writeSize;

		if (blockWorkerPtr->writeMsgBuf->rOffset == blockWorkerPtr->writeMsgBuf->wOffset)
		{
//			printf("Sended cmd:%s\n", blockWorkerPtr->writeMsgBuf->buf);
			blockWorkerPtr->writeMsgBuf->rOffset = 0;
			blockWorkerPtr->writeMsgBuf->wOffset = 0;
			//blockWorkerPtr->curState = Worker_STATE_READING;
			removeSockWriteMonitoring(ecBlockWorkerMgr, blockWorkerPtr);
			return 1;
		}

	}

	if (blockWorkerPtr->curSockState != Worker_SOCK_STATE_WRITE &&	
			blockWorkerPtr->curSockState != Worker_SOCK_STATE_READ_WRITE)
	{
		monitoringSockWrite(ecBlockWorkerMgr, blockWorkerPtr);
	}
	return 0;
}

int recvDataContent(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr){
	ssize_t recvdSize = recv(blockWorkerPtr->sockFd, blockWorkerPtr->buf + blockWorkerPtr->bufHandledSize,
							 blockWorkerPtr->bufSize - blockWorkerPtr->bufHandledSize, 0);

	if (recvdSize <= 0)
	{
		monitoringSockRead(ecBlockWorkerMgr, blockWorkerPtr);
		return 0;
	}

	blockWorkerPtr->bufHandledSize = blockWorkerPtr->bufHandledSize + (size_t) recvdSize;

	//printf("workerIdx:%lu, recvd:%ld, bufHandledSize:%lu\n",blockWorkerPtr->workerIdx,recvdSize,blockWorkerPtr->bufHandledSize  );
	if (blockWorkerPtr->bufHandledSize != blockWorkerPtr->bufSize)
	{
		monitoringSockRead(ecBlockWorkerMgr, blockWorkerPtr);
		return 0;
	}

	removeSockReadMonitoring(ecBlockWorkerMgr, blockWorkerPtr);
	//blockWorkerPtr->curState = Worker_STATE_READ;
	return 1;
}

int waitReadFinished(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	if (ecBlockWorkerMgr->inMonitoringSockNum == 0 )
	{
		return 1;
	}

	struct epoll_event *events = (struct epoll_event *)ecBlockWorkerMgr->events;
	int idx, eventsNum;
	eventsNum = epoll_wait(ecBlockWorkerMgr->eFd, events, MAX_EVENTS,CONNECTING_TIME_OUT_IN_MS);

	for (idx = 0; idx < eventsNum; ++idx)
	{
        ECBlockWorker_t *blockWorkerPtr = (ECBlockWorker_t *)events[idx].data.ptr;

        if(events[idx].events & EPOLLIN){
        	recvDataContent(ecBlockWorkerMgr, blockWorkerPtr);
        }else if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)){
                printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
                fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
                printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
                removeSockEvent(ecBlockWorkerMgr, blockWorkerPtr);
                blockWorkerPtr->curState = Worker_STATE_CONNECTFAILED;
		}else{
			printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
            fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
            printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
			printf("Unknow status for event\n" );
		}
	}

	return waitReadFinished(ecBlockWorkerMgr);
}

void setBlockWorkerReadSize(ECFile_t *ecFilePtr, workerBuf_t *workerBufPtr){
	size_t readSize = ecFilePtr->bufHandledSize - ecFilePtr->bufWritedSize;

	if (readSize > (workerBufPtr->alignedBufSize * workerBufPtr->inputNum))
	{
		readSize = (workerBufPtr->alignedBufSize * workerBufPtr->inputNum);
	}

	size_t alignSize = (ecFilePtr->streamingSize * ecFilePtr->stripeK);
	size_t eachBufAlignedSize = readSize / alignSize * ecFilePtr->streamingSize;
	size_t residueSize = readSize % alignSize;
	size_t idx = 0;

	ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
	do{
		size_t extraValue = 0;
		if (residueSize >  ecFilePtr->streamingSize)
		{
			extraValue = ecFilePtr->streamingSize;
			residueSize = residueSize - ecFilePtr->streamingSize;
		}else{
			extraValue = residueSize;
			residueSize = 0;
		}

		blockWorkerPtr->buf = workerBufPtr->inputBufs[idx];
		blockWorkerPtr->bufSize = eachBufAlignedSize + extraValue;
		blockWorkerPtr->bufHandledSize = 0;

		//printf("idx:%lu, bufSize:%lu, bufHandledSize:%lu\n",idx,blockWorkerPtr->bufSize, blockWorkerPtr->bufHandledSize );

		blockWorkerPtr = blockWorkerPtr->next;
		++idx;
	}while(idx != ecFilePtr->stripeK);

}

void setBlockWorkerBufSize(ECFile_t *ecFilePtr, workerBuf_t *workerBufPtr){
	size_t readSize = ecFilePtr->bufSize - ecFilePtr->bufHandledSize;

	if (readSize > (workerBufPtr->alignedBufSize * workerBufPtr->inputNum))
	{
		readSize = (workerBufPtr->alignedBufSize * workerBufPtr->inputNum);
	}

	size_t alignSize = (ecFilePtr->streamingSize * ecFilePtr->stripeK);
	size_t eachBufAlignedSize = readSize / alignSize * ecFilePtr->streamingSize;
	size_t residueSize = readSize % alignSize;
	size_t idx = 0;

	ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
	do{
		size_t extraValue = 0;
		if (residueSize >  ecFilePtr->streamingSize)
		{
			extraValue = ecFilePtr->streamingSize;
			residueSize = residueSize - ecFilePtr->streamingSize;
		}else{
			extraValue = residueSize;
			residueSize = 0;
		}

		blockWorkerPtr->buf = workerBufPtr->inputBufs[idx];
		blockWorkerPtr->bufSize = eachBufAlignedSize + extraValue;
		blockWorkerPtr->bufHandledSize = 0;
		blockWorkerPtr = blockWorkerPtr->next;
		++idx;
	}while(idx != ecFilePtr->stripeK);

	ecFilePtr->bufHandledSize = ecFilePtr->bufHandledSize + readSize;
}

void flushReadCmdToServer(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	if (ecBlockWorkerMgr->inMonitoringSockNum == 0)
	{
		return;
	}

	struct epoll_event *events = (struct epoll_event *)ecBlockWorkerMgr->events;
	int idx, eventsNum;
	eventsNum = epoll_wait(ecBlockWorkerMgr->eFd, events, MAX_EVENTS,CONNECTING_TIME_OUT_IN_MS);

	for (idx = 0; idx < eventsNum; ++idx)
	{
        ECBlockWorker_t *blockWorkerPtr = (ECBlockWorker_t *)events[idx].data.ptr;

        if (events[idx].events & EPOLLOUT)
        {
        	if(writeReadDataSize(ecBlockWorkerMgr, blockWorkerPtr) != 1){
        		continue;
        	}
        	
        }else if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)){
                printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
                fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
                printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
                removeSockEvent(ecBlockWorkerMgr, blockWorkerPtr);
                blockWorkerPtr->curState = Worker_STATE_CONNECTFAILED;
		}else{
			printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
            fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
            printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
			printf("Unknow status for event\n" );
		}
	}

	flushReadCmdToServer(ecBlockWorkerMgr);
}

int sendReadDataCmd(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr){
	ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
	size_t idx = 0;
	do{
		blockWorkerPtr->writeMsgBuf->wOffset = constructBlockReadCmd(blockWorkerPtr->writeMsgBuf->buf,
																	blockWorkerPtr->blockId, blockWorkerPtr->blockFd,
																	blockWorkerPtr->bufSize);
		writeReadDataSize(ecBlockWorkerMgr, blockWorkerPtr);
		blockWorkerPtr = blockWorkerPtr->next;
		++idx;
	}while(idx != ecFilePtr->stripeK);

	flushReadCmdToServer(ecBlockWorkerMgr);

	return 1;
}

int startReadBlockData(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr, workerBuf_t *workerBufPtr){
    
	while( ecFilePtr->bufSize > ecFilePtr->bufHandledSize &&
		ecBlockWorkerMgr->curInFlightReq < ecBlockWorkerMgr->maxInFlightReq){
		setBlockWorkerBufSize(ecFilePtr, workerBufPtr);
		sendReadDataCmd(ecBlockWorkerMgr, ecFilePtr);
		++ecBlockWorkerMgr->curInFlightReq;
	}

	return ecBlockWorkerMgr->curInFlightReq;
}

int recvBlockData(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr, workerBuf_t *workerBufPtr){
	setBlockWorkerReadSize(ecFilePtr, workerBufPtr);
	size_t idx = 0;

	ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
	do{
		recvDataContent(ecBlockWorkerMgr, blockWorkerPtr);
		blockWorkerPtr = blockWorkerPtr->next;
		++idx;
	}while(idx != ecFilePtr->stripeK);

	waitReadFinished(ecBlockWorkerMgr);
	--ecBlockWorkerMgr->curInFlightReq;
	 return 1;
}

void workerPerformWrite(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr; ;
	ECCoderWorker_t *coderWorker = (ECCoderWorker_t *)ecBlockWorkerMgr->coderWorker;
	int idx,bIdx = 0;
	while(ecFilePtr->bufWritedSize < ecFilePtr->bufSize){
		if(ecFilePtr->bufHandledSize < ecFilePtr->bufSize){
			for (idx = 0; idx < ecBlockWorkerMgr->workerBufsNum; ++idx)
			{
				if (*(ecBlockWorkerMgr->bufInUseFlag + idx) == 0)
				{
					*(ecBlockWorkerMgr->bufInUseFlag + idx) = 1;
					workerBuf_t *workerBufPtr = ecBlockWorkerMgr->workerBufs + idx;
					copyDataToInputBufs(ecFilePtr, workerBufPtr);
					sem_post(&coderWorker->waitJobSem);
				}
			}
		}
		
		workerBuf_t *writeWorkerBuf = ecBlockWorkerMgr->workerBufs + bIdx;

		//printf("Wait coding done\n");
		sem_wait(&coderWorker->jobFinishedSem);
		//printf("coding is finished, start writing\n");
		writeBufToServers(ecBlockWorkerMgr, ecFilePtr, writeWorkerBuf);
		//printf("writing is finished\n");
		*(ecBlockWorkerMgr->bufInUseFlag + bIdx) = 0;
		bIdx = (bIdx + 1) % ecBlockWorkerMgr->workerBufsNum;
	}

	ecBlockWorkerMgr->jobDoneFlag = 1;
	coderWorker->jobFinishedFlag = 1;
	sem_post(&coderWorker->waitJobSem);
	sem_post(&ecBlockWorkerMgr->jobFinishedSem);
}

void copyDataToMainBuf(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;

	int cpedFlag=0;
	size_t idx;
	size_t cpOffset = 0, cpSize = 0;
	while(cpedFlag == 0){
		idx = 0;
		ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
		while(idx != ecFilePtr->stripeK){
			cpSize = ecFilePtr->streamingSize;

			if ((cpOffset + cpSize) > blockWorkerPtr->bufHandledSize)
			{
				cpSize = blockWorkerPtr->bufHandledSize - cpOffset;
			}

			if (cpSize == 0)
			{
				//End of copying
				cpedFlag = 1;
				break;
			}

			memcpy(ecFilePtr->buf + ecFilePtr->bufWritedSize, 
					blockWorkerPtr->buf + cpOffset, cpSize);
			ecFilePtr->bufWritedSize = ecFilePtr->bufWritedSize + cpSize;

			blockWorkerPtr = blockWorkerPtr->next;
			++idx;
		}

		cpOffset = cpOffset + ecFilePtr->streamingSize;
		//printf("file size:%lu bufSize:%lu handledSize:%lu writedSize:%lu\n",
		//		ecFilePtr->fileSize, ecFilePtr->bufSize, ecFilePtr->bufHandledSize, ecFilePtr->bufWritedSize );
	}
}

void flushReqToServer(ECBlockWorkerManager_t *ecBlockWorkerMgr){
    
    if (ecBlockWorkerMgr->inMonitoringSockNum == 0)
    {
        return;
    }
    
    struct epoll_event *events = (struct epoll_event *)ecBlockWorkerMgr->events;
    int idx, eventsNum;
    eventsNum = epoll_wait(ecBlockWorkerMgr->eFd, events, MAX_EVENTS,CONNECTING_TIME_OUT_IN_MS);
    
    for (idx = 0; idx < eventsNum; ++idx)
    {
        ECBlockWorker_t *blockWorkerPtr = (ECBlockWorker_t *)events[idx].data.ptr;
        
        if (events[idx].events & EPOLLOUT)
        {
            if(writeReadDataSize(ecBlockWorkerMgr, blockWorkerPtr) != 1){
                continue;
            }
            
        }else if((events[idx].events & EPOLLERR)||
                 (events[idx].events & EPOLLHUP)){
            printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
            fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
            printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
            removeSockEvent(ecBlockWorkerMgr, blockWorkerPtr);
            blockWorkerPtr->curState = Worker_STATE_CONNECTFAILED;
        }else{
            printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
            fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
            printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
            printf("Unknow status for event\n" );
        }
    }
    
    flushReqToServer(ecBlockWorkerMgr);

}

void reqECBlockRead(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr, size_t readSize){
    
    blockWorkerPtr->writeMsgBuf->wOffset = constructBlockReadCmd(blockWorkerPtr->writeMsgBuf->buf,
                                                                 blockWorkerPtr->blockId, blockWorkerPtr->blockFd,
                                                                 readSize);
    
    ++ecBlockWorkerMgr->curInFlightReq;
//    printf("start to send blockId:%llu, read size:%lu\n",blockWorkerPtr->blockId, readSize);
    writeReadDataSize(ecBlockWorkerMgr, blockWorkerPtr);
    
    flushReqToServer(ecBlockWorkerMgr);
//    printf("finish send blockId:%llu, read size:%lu\n",blockWorkerPtr->blockId, readSize);
}

ssize_t performReadDataFromBlockServer(ECBlockWorker_t *blockWorkerPtr, char *buf, size_t readSize){
    
    ssize_t readedSize = recv(blockWorkerPtr->sockFd, buf, readSize, 0);
//    printf("blockId:%llu, recvd size:%ld\n",blockWorkerPtr->blockId, readedSize);
    
    return readedSize;
}
void waitDataFromBlockServer(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr,
                             size_t startOffset, size_t readSize){
    if (ecBlockWorkerMgr->inMonitoringSockNum == 0) {
        return;
    }
    
//    printf("startOffset:%lu, readSize:%lu\n",startOffset, readSize);
    
    struct epoll_event *events = (struct epoll_event *)ecBlockWorkerMgr->events;
    int idx, eventsNum;
    eventsNum = epoll_wait(ecBlockWorkerMgr->eFd, events, MAX_EVENTS,CONNECTING_TIME_OUT_IN_MS);
    
    
    for (idx = 0; idx < eventsNum; ++idx)
    {
        ECBlockWorker_t *blockWorkerPtr = (ECBlockWorker_t *)events[idx].data.ptr;
        
        if (events[idx].events & EPOLLIN)
        {
            ssize_t readedSize = performReadDataFromBlockServer(blockWorkerPtr,
                                                                (ecFilePtr->buf + startOffset), readSize);
            
            if (readedSize < 0) {
                perror("Unable to read data from block server");
                exit(0);
            }
            
            startOffset = startOffset + readedSize;
            readSize = readSize - readedSize;
            
            if (readSize == 0) {
                removeSockReadMonitoring(ecBlockWorkerMgr, blockWorkerPtr);
            }
            
        }else if((events[idx].events & EPOLLERR)||
                 (events[idx].events & EPOLLHUP)){
            printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
            fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
            printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
            removeSockEvent(ecBlockWorkerMgr, blockWorkerPtr);
            blockWorkerPtr->curState = Worker_STATE_CONNECTFAILED;
        }else{
            printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
            fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
            printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
            printf("Unknow status for event\n" );
        }
    }
    
    waitDataFromBlockServer(ecBlockWorkerMgr, ecFilePtr,
                            startOffset, readSize);
    
}

void readDataFromBlockServer(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr,
                             ECBlockWorker_t *blockWorkerPtr,
                             size_t startOffset, size_t readSize){
    
//    printf("startOffset:%lu, readSize:%lu\n",startOffset, readSize);
    
    ssize_t readedSize = performReadDataFromBlockServer(blockWorkerPtr,
                                   (ecFilePtr->buf + startOffset), readSize);
    
    if (readedSize > 0) {
        startOffset = startOffset + readedSize;
        readSize = readSize - readedSize;
    }
    
    if (readSize > 0) {
        monitoringSockRead(ecBlockWorkerMgr, blockWorkerPtr);
    }
    
    waitDataFromBlockServer(ecBlockWorkerMgr, ecFilePtr,
                            startOffset, readSize);
    
}

void startReadECBlock(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECFile_t *ecFilePtr, ECBlockWorker_t *blockWorkerPtr,
    size_t startOffset, size_t margin){
    
    size_t reqSize = 0, readSize = 0;
    size_t reqOffset = startOffset;
    
    do{
        while (ecBlockWorkerMgr->curInFlightReq < ecBlockWorkerMgr->maxInFlightReq) {
            
            if (reqOffset > ecFilePtr->bufSize) {
                break;
            }
            
            reqSize = ecFilePtr->bufSize - reqOffset;
            
            if (reqSize == 0) {
                break;
            }
            
            if (reqSize > ecFilePtr->streamingSize) {
                reqSize = ecFilePtr->streamingSize;
            }
            
            reqECBlockRead(ecBlockWorkerMgr, blockWorkerPtr, reqSize);
            
            ecFilePtr->bufHandledSize = ecFilePtr->bufHandledSize + reqSize;
            reqOffset = reqOffset + margin;
        }
        
        readSize = ecFilePtr->bufSize - startOffset;
        
        if (readSize == 0) {
            break;
        }
        
        if (readSize > ecFilePtr->streamingSize) {
            readSize = ecFilePtr->streamingSize;
        }
        
        readDataFromBlockServer(ecBlockWorkerMgr, ecFilePtr, blockWorkerPtr, startOffset, readSize);
        
        ecFilePtr->bufWritedSize = ecFilePtr->bufWritedSize + readSize;
        startOffset = startOffset + margin;
        --ecBlockWorkerMgr->curInFlightReq;
    }while(ecFilePtr->bufHandledSize != ecFilePtr->bufWritedSize);
}

void workerPerformReadPrint(ECBlockWorkerManager_t *ecBlockWorkerMgr,char *msg){
    ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;
    ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
    int bIdx = 0;
    do
    {
        printf("%sblockId:%llu startWorkerPerformRead bufSize:%lu bufWritedSize:%lu\n",msg,blockWorkerPtr->blockId,ecFilePtr->bufSize,ecFilePtr->bufWritedSize);
        blockWorkerPtr = blockWorkerPtr->next;
        ++bIdx;
    }while((size_t) bIdx != ecFilePtr->stripeK);
}

void workerPerformRead(ECBlockWorkerManager_t *ecBlockWorkerMgr){
	ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;
	ecBlockWorkerMgr->curInFlightReq = 0;
    
//    struct timeval startTime, endTime;
//    size_t writedSize, writeSize;
//    
//    writedSize = ecFilePtr->bufWritedSize;
//    gettimeofday(&startTime, NULL);
    
    ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
    size_t margin = ecFilePtr->streamingSize * ecFilePtr->stripeK;
    size_t startOffset = 0;
    while(ecFilePtr->bufWritedSize < ecFilePtr->bufSize){
//		workerBuf_t *readWorkerBuf = ecBlockWorkerMgr->workerBufs;
//		startReadBlockData(ecBlockWorkerMgr, ecFilePtr, readWorkerBuf);
//		recvBlockData(ecBlockWorkerMgr, ecFilePtr, readWorkerBuf);
//		copyDataToMainBuf(ecBlockWorkerMgr);
        startReadECBlock(ecBlockWorkerMgr, ecFilePtr, blockWorkerPtr, startOffset, margin);
        blockWorkerPtr = blockWorkerPtr->next;
        startOffset = startOffset + ecFilePtr->streamingSize;
	}

//    gettimeofday(&endTime, NULL);
//    writeSize = ecFilePtr->bufWritedSize - writedSize;
//    double timeInterval = timeIntervalInMS(startTime, endTime);
//    double throughput = ((double)writeSize/1024.0/1024.0)/(timeInterval/1000.0);
//    
//    printf("writeSize:%luKB, Interval:%fms,throughput:%fMB/s\n",(writeSize/1024), timeInterval, throughput);

	ecBlockWorkerMgr->jobDoneFlag = 1;
    
    PRINT_ROUTINE_VALUE(ecFilePtr->bufWritedSize);
//    sem_post(&ecBlockWorkerMgr->jobFinishedSem);
}

void waitDegradeDataFromBlockServer(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr, char *buf, size_t readSize){
    if (ecBlockWorkerMgr->inMonitoringSockNum == 0) {
        return;
    }
    
    //    printf("startOffset:%lu, readSize:%lu\n",startOffset, readSize);
    
    struct epoll_event *events = (struct epoll_event *)ecBlockWorkerMgr->events;
    int idx, eventsNum;
    eventsNum = epoll_wait(ecBlockWorkerMgr->eFd, events, MAX_EVENTS,CONNECTING_TIME_OUT_IN_MS);
    
    for (idx = 0; idx < eventsNum; ++idx)
    {
        ECBlockWorker_t *blockWorkerPtr = (ECBlockWorker_t *)events[idx].data.ptr;
        
        if (events[idx].events & EPOLLIN)
        {
            ssize_t readedSize = performReadDataFromBlockServer(
                                                                blockWorkerPtr,
                                                                buf, readSize);
            
            if (readedSize < 0) {
                perror("Unable to read data from block server");
                exit(0);
            }
            
            buf = buf + readedSize;
            readSize = readSize - readedSize;
            
            if (readSize == 0) {
                removeSockReadMonitoring(ecBlockWorkerMgr, blockWorkerPtr);
            }
            
        }else if((events[idx].events & EPOLLERR)||
                 (events[idx].events & EPOLLHUP)){
            printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
            fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
            printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
            removeSockEvent(ecBlockWorkerMgr, blockWorkerPtr);
            blockWorkerPtr->curState = Worker_STATE_CONNECTFAILED;
        }else{
            printf("idx:%d EPOLLERR:%d, EPOLLHUP:%d, EPOLLIN:%d, EPOLLOUT:%d\n",idx, EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
            fprintf(stderr,"flushDataToBlockServers epoll error：%d\n",events[idx].events);
            printf("flushDataToBlockServers epoll error: with IP:%s, port:%d\n",blockWorkerPtr->IPAddr, blockWorkerPtr->port);
            printf("Unknow status for event\n" );
        }
    }
    
    waitDegradeDataFromBlockServer(ecBlockWorkerMgr,
                            blockWorkerPtr, buf, readSize);
    
}

void readDegradedDataFromBlockServer(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr, char *buf, size_t readSize){
    ssize_t readedSize = performReadDataFromBlockServer(blockWorkerPtr,
                                                        buf, readSize);
    
    if (readedSize > 0) {
        buf = buf + readedSize;
        readSize = readSize - readedSize;
    }
    
    if (readSize > 0) {
        monitoringSockRead(ecBlockWorkerMgr, blockWorkerPtr);
    }
    
    waitDegradeDataFromBlockServer(ecBlockWorkerMgr, blockWorkerPtr,
                            buf, readSize);
}

void workerReqBlockRead(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorkerPtr, char *buf, size_t bufSize){
    size_t bufHandledSize = 0, bufOffset = 0, reqSize, readSize;
    ECFile_t *ecFilePtr= ecBlockWorkerMgr->curFilePtr;
    do{
        while (ecBlockWorkerMgr->curInFlightReq < ecBlockWorkerMgr->maxInFlightReq) {
            
            if (bufHandledSize >= bufSize) {
                break;
            }
            
            reqSize = bufSize - bufOffset;
            
            if (reqSize == 0) {
                break;
            }
            
            if (reqSize > ecFilePtr->streamingSize) {
                reqSize = ecFilePtr->streamingSize;
            }
            
            reqECBlockRead(ecBlockWorkerMgr, blockWorkerPtr, reqSize);
            
            bufHandledSize = bufHandledSize + reqSize;
        }
        
        readSize = bufHandledSize - bufOffset;
        
//        printf("blockIdx:%lu,bufOffset:%lu,bufHandledSize:%lu readSize:%lu \n",blockWorkerPtr->blockId,bufOffset,bufHandledSize,readSize);
        
        if (readSize == 0) {
            break;
        }
        
        if (readSize > ecFilePtr->streamingSize) {
            readSize = ecFilePtr->streamingSize;
        }
        
        readDegradedDataFromBlockServer(ecBlockWorkerMgr, blockWorkerPtr, buf + bufOffset, readSize);
        bufOffset = bufOffset + readSize;
        --ecBlockWorkerMgr->curInFlightReq;
    }while (bufOffset < bufHandledSize);
}

void workersReqDegradedBlocksRead(ECBlockWorkerManager_t *ecBlockWorkerMgr, workerBuf_t *workerBufPtr){
    int idx = 0;
    ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;
    ecBlockWorkerMgr->curInFlightReq = 0;
    ECBlockWorker_t *blockWorkerPtr = ecFilePtr->blockWorkers;
    
    do {
        workerReqBlockRead(ecBlockWorkerMgr, blockWorkerPtr, workerBufPtr->inputBufs[idx], workerBufPtr->alignedBufSize);
        blockWorkerPtr = blockWorkerPtr->next;
        ++idx;
        
    }while (idx < (int)ecFilePtr->stripeK);
    
    ecFilePtr->bufHandledSize = ecFilePtr->bufHandledSize + workerBufPtr->alignedBufSize * ecFilePtr->stripeK;
    
}

void cmpBuf(ECFile_t *ecFilePtr, workerBuf_t *workerBuf){
    size_t cmpSize = 16, cmpOffset = 0;
    char *buf1 = workerBuf->inputBufs[0], *buf2 = workerBuf->outputBufs[0];
    
    while (cmpOffset < workerBuf->codingSize) {
        cmpOffset = cmpOffset + cmpSize;
        if (strncmp(buf1 + cmpOffset, buf2 + cmpOffset, cmpSize) != 0) {
            printf("not equal at offset:%lu\n",cmpOffset);
            
            int chIdx;
            char ch1, ch2, ch3;
            for (chIdx = 0; chIdx < cmpSize; ++chIdx) {
                ch1 = *(workerBuf->inputBufs[0] + cmpOffset + chIdx);
                ch2 = *(workerBuf->inputBufs[1] + cmpOffset + chIdx);
                ch3 = *(workerBuf->outputBufs[0] + cmpOffset + chIdx);
                printf("%04x %04x %04x\n",(int)ch1,(int)ch2,(int)ch3);
            }
        }
    }
    
}

void copyCodedToMainBuf(ECFile_t *ecFilePtr, workerBuf_t *workerBuf){
    int idx = 0;
    size_t offset = 0;
    char *buf;
    
//    cmpBuf(ecFilePtr, workerBuf);
    
    do{
        for (idx = 0; idx < (int) ecFilePtr->stripeK; ++idx) {
            blockInfo_t *curBlock = ecFilePtr->blocks + idx;
            buf = NULL;
            if (curBlock->serverStatus == 0) {
                int kIdx = 0;
                for (kIdx = 0; kIdx < (int) ecFilePtr->stripeM; ++kIdx) {
                    if (*(ecFilePtr->mat_idx+kIdx) == idx) {
//                        printf("outputIdx:%d\n",kIdx);
                        buf = workerBuf->outputBufs[kIdx] + offset;
                        break;
                    }
                }
            }else{
//                printf("inputIdx:%d\n",idx);
                buf = workerBuf->inputBufs[idx] + offset;
            }
            
            if(buf == NULL){
                printf("Unable to locate the buf\n");
            }else{
//                printf("bufSize:%lu bufWritedSize:%lu, offset:%lu, streamingSize:%lu",ecFilePtr->bufSize, ecFilePtr->bufWritedSize, offset, ecFilePtr->streamingSize);
                memcpy(ecFilePtr->buf + ecFilePtr->bufWritedSize, buf, ecFilePtr->streamingSize);
                ecFilePtr->bufWritedSize = ecFilePtr->bufWritedSize + ecFilePtr->streamingSize;
            }
        }
        
        offset = offset + ecFilePtr->streamingSize;
        
    }while(offset != workerBuf->alignedBufSize);
}

void workerPerformDegradedRead(ECBlockWorkerManager_t *ecBlockWorkerMgr){
    ECFile_t *ecFilePtr = ecBlockWorkerMgr->curFilePtr;
    ECCoderWorker_t *coderWorker = (ECCoderWorker_t *)ecBlockWorkerMgr->coderWorker;
    int idx, bIdx = 0;
    
    while(ecFilePtr->bufWritedSize < ecFilePtr->bufSize){
        if(ecFilePtr->bufHandledSize < ecFilePtr->bufSize){
            for (idx = 0; idx < ecBlockWorkerMgr->workerBufsNum; ++idx)
            {
                if (*(ecBlockWorkerMgr->bufInUseFlag + idx) == 0)
                {
                    *(ecBlockWorkerMgr->bufInUseFlag + idx) = 1;
                    workerBuf_t *workerBufPtr = ecBlockWorkerMgr->workerBufs + idx;
                    workersReqDegradedBlocksRead(ecBlockWorkerMgr,workerBufPtr);
                    workerBufPtr->codingSize = workerBufPtr->alignedBufSize;
                    
                    int mIdx = 0;
                    for (mIdx = 0; mIdx < (int)workerBufPtr->outputNum; ++mIdx) {
                        memset(workerBufPtr->outputBufs[mIdx], 0, workerBufPtr->codingSize);
                    }
                    sem_post(&coderWorker->waitJobSem);
                }
            }
        }
        
        workerBuf_t *writeWorkerBuf = ecBlockWorkerMgr->workerBufs + bIdx;
        
        sem_wait(&coderWorker->jobFinishedSem);
//        printf("bIdx:%d inUsetFlag:%d\n",bIdx,*(ecBlockWorkerMgr->bufInUseFlag + bIdx));
        copyCodedToMainBuf(ecFilePtr, writeWorkerBuf);
        *(ecBlockWorkerMgr->bufInUseFlag + bIdx) = 0;
        bIdx = (bIdx + 1) % ecBlockWorkerMgr->workerBufsNum;
    }
    
    ecBlockWorkerMgr->jobDoneFlag = 1;
    coderWorker->jobFinishedFlag = 1;
    sem_post(&coderWorker->waitJobSem);
    sem_post(&ecBlockWorkerMgr->jobFinishedSem);
}

void performThreadWorkerJob(ECBlockWorkerManager_t *ecBlockWorkerMgr){

	ECCoderWorker_t *coderWorker = (ECCoderWorker_t *)ecBlockWorkerMgr->coderWorker;
	switch(ecBlockWorkerMgr->curJobState){
		case ECJOB_WRITE:
			coderWorker->jobFinishedFlag = 0;
			sem_post(&coderWorker->jobStartSem);
			workerPerformWrite(ecBlockWorkerMgr);
		break;
        case ECJOB_READ:
            workerPerformReadPrint(ecBlockWorkerMgr,"ECJOB_READ:");
			workerPerformRead(ecBlockWorkerMgr);
		break;
		case ECJOB_DEGRADED_READ:
            //To do
            workerPerformReadPrint(ecBlockWorkerMgr,"ECJOB_DEGRADED_READ:");
            coderWorker->jobFinishedFlag = 0;
            sem_post(&coderWorker->jobStartSem);
			workerPerformDegradedRead(ecBlockWorkerMgr);
		break;
		default:
            workerPerformReadPrint(ecBlockWorkerMgr,"Unknow job:");
			printf("Unknow job\n");
	}
}

void *threadBlockWorker(void *arg){
	ECBlockWorkerManager_t *ecBlockWorkerMgr = (ECBlockWorkerManager_t *)arg;

	while(ecBlockWorkerMgr->exitFlag == 0){
		sem_wait(&ecBlockWorkerMgr->jobStartSem);

		if (ecBlockWorkerMgr->exitFlag == 1)
		{
			break;
		}

		performThreadWorkerJob(ecBlockWorkerMgr);
	}
	

	return arg;
}

int closeBlock(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorker){
	blockWorker->writeMsgBuf->wOffset = constructBlockCloseCmd(blockWorker->writeMsgBuf->buf, blockWorker->blockId, blockWorker->blockFd);
	blockWorker->curState = Worker_STATE_WRITE_OVER;
	return sendRequestToBlockServer(ecBlockWorkerMgr, blockWorker);
}

int closeBlockInServer(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorker){
	switch(blockWorker->curState){
		case Worker_STATE_WRITE:
		case Worker_STATE_READ:
		{
			return closeBlock(ecBlockWorkerMgr, blockWorker);
		}
		break;
		default :
		break;
	}

	return 0;
}
