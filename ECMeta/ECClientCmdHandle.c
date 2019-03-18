//
//  ECClientCmdHandle.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/11/1.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECClientCmdHandle.h"

#include "hash.h"
#include "ECMetaEngine.h"

#include <unistd.h>

void getNameOffsetBetweenBreakLine(char *buf, size_t *startOffset, size_t *endOffset,size_t bufSize){
    int parseFlag = 0,idx;
    for (idx = 0; idx < (bufSize-1); ++idx) {
        if (*(buf+idx) == '\r' && *(buf+idx+1) == '\n') {
            if (parseFlag == 0) {
                *startOffset = idx + 2;
            }
            
            if (parseFlag == 1) {
                *endOffset = idx - 1 ;
                return;
            }
            
            ++parseFlag;
        }
        
        if (parseFlag == 2) {
            return ;
        }
    }
}

void processRecvdCmd(ECClientThread_t *clientThreadPtr, ClientConnection_t *clientConnPtr, KEYVALUEJOB_t keyValueJobType){
    size_t fileNameStartOffset, fileNameEndOffset;
    getNameOffsetBetweenBreakLine(clientConnPtr->readMsgBuf->buf, &fileNameStartOffset, &fileNameEndOffset,
                      clientConnPtr->readMsgBuf->wOffset);
    
    uint32_t hashValue = hash((clientConnPtr->readMsgBuf->buf + fileNameStartOffset), (fileNameEndOffset - fileNameStartOffset + 1), 0);
    
    //    printf("file start offset:%lu end offset:%lu\n", fileNameStartOffset, fileNameEndOffset);
    //write(clientConnPtr->logFd, clientConnPtr->readMsgBuf->buf, clientConnPtr->readMsgBuf->wOffset);
    KeyValueJob_t *keyValueJob =  createECJob((clientConnPtr->readMsgBuf->buf + fileNameStartOffset),
                                              (fileNameEndOffset - fileNameStartOffset + 1), 
                                              hashValue, clientConnPtr->thIdx, clientConnPtr->idx, 
                                              keyValueJobType, 0);
    
    ECMetaServer_t *metaServer = (ECMetaServer_t *)clientThreadPtr->ecMetaEnginePtr;
    
    size_t keyValueThreadNum = metaServer->keyValueEnginePtr->workerNum;
    size_t chainSize = metaServer->keyValueEnginePtr->keyChainSize;
    
    size_t rangeEach = chainSize / keyValueThreadNum;
    size_t threadToReq = (hashValue % chainSize)/rangeEach;
    
    KeyValueWorker_t *keyValueWorker = metaServer->keyValueEnginePtr->workers + threadToReq;
    
    //printf("Recvd:%s file start offset:%lu end offset:%lu put to worker:%lu\n",clientConnPtr->readMsgBuf->buf, fileNameStartOffset, fileNameEndOffset, keyValueWorker->workerIdx);

    pthread_mutex_lock(&keyValueWorker->threadLock);
    putJob(keyValueWorker->pendingJobQueue, keyValueJob);
    char ch='D';
    
    if(keyValueWorker->curPipeState == WORKER_PIPE_WRITEABLE &&
       (write(keyValueWorker->workerPipes[1], &ch, 1) != 1)){
        monitoringWorkerPipeWritable(keyValueWorker);
    }
    pthread_mutex_unlock(&keyValueWorker->threadLock);
}