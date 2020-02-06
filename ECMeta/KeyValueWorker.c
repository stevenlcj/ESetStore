//
//  KeyValueWorker.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "KeyValueWorker.h"
#include "ECMetaEngine.h"
#include "KeyValueEngine.h"
#include "ecUtils.h"
#include "rackMap.h"
#include "ecNetHandle.h"

#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

KeyValueWorker_t *createKeyValueWorkers(void *metaServerPtr){
    KeyValueWorker_t *keyValueWorkers = talloc(KeyValueWorker_t, DEFAULT_KEY_WORKER_THREAD_NUM);
    
    if (keyValueWorkers == NULL) {
        return NULL;
    }
    
    int idx;
    size_t hashRange = KEY_CHAIN_SIZE / DEFAULT_KEY_WORKER_THREAD_NUM;
    for (idx = 0; idx < DEFAULT_KEY_WORKER_THREAD_NUM; ++idx) {
        KeyValueWorker_t *workerPtr = (keyValueWorkers+idx);
        workerPtr->workerIdx = idx;
        workerPtr->pipeIdx = 0;
        workerPtr->curPipeState = WORKER_PIPE_WRITEABLE;
        //workerPtr->workerLogFd = createWorkerLogFd(idx);
        workerPtr->hashTableSize = KEY_CHAIN_SIZE;
        workerPtr->metaServerPtr = metaServerPtr;
        
        if(pipe(workerPtr->workerPipes) != 0){
            perror("pipe(workerPtr->workerPipes) failed\n");
        }
        
        make_non_blocking_sock(workerPtr->workerPipes[0]);

        pthread_mutex_init(&workerPtr->threadLock, NULL);
        
        workerPtr->hashStartIdx = hashRange * idx;
        workerPtr->hashEndIdx = workerPtr->hashStartIdx + hashRange - 1;
        
        workerPtr->pendingJobQueue = createKeyValueJobQueue();
        workerPtr->handlingJobQueue = createKeyValueJobQueue();

        sem_init(&workerPtr->waitJobSem,0 ,0);
    }
    
    return keyValueWorkers;
}

void writeOKFeedBack(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    char OKStr[] = "OK\r\n\0";
    
    keyValueJob->feedbackBufSize = strlen(OKStr) + 1;
    keyValueJob->feedbackContent = talloc(char , keyValueJob->feedbackBufSize);
    
    memcpy(keyValueJob->feedbackContent, OKStr, keyValueJob->feedbackBufSize -1);
    
    *(keyValueJob->feedbackContent + keyValueJob->feedbackBufSize - 1) = '\0';
    
    return;
}

void writeFailedFeedBack(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    char FailedStr[] = "Failed\r\n\0";
    
    keyValueJob->feedbackBufSize = strlen(FailedStr) + 1;
    keyValueJob->feedbackContent = talloc(char , keyValueJob->feedbackBufSize);
    
    memcpy(keyValueJob->feedbackContent, FailedStr, keyValueJob->feedbackBufSize -1);
    
    *(keyValueJob->feedbackContent + keyValueJob->feedbackBufSize - 1) = '\0';
    
    return;
}

void writeDeleteFailedFeedBack(KeyValueWorker_t *workerPtr, KeyValueJob_t *jobPtr){
    char writeFeedBackBuf[DEFAUT_MESSGAE_READ_WRITE_BUF];

    char deleteOKStr[] = "DeleteOK\r\n\0";
    char filenameStr[] = "Filename:\0";
    char suffix[] = "\r\n\0";
    size_t writeSize = 0, writeOffset = 0;

    writeToBuf(writeFeedBackBuf, deleteOKStr, &writeSize, &writeOffset);
    writeToBuf(writeFeedBackBuf, filenameStr, &writeSize, &writeOffset);
    memcpy(writeFeedBackBuf+writeOffset, jobPtr->key, jobPtr->keySize);
    writeOffset = writeOffset + jobPtr->keySize;
    writeToBuf(writeFeedBackBuf, suffix, &writeSize, &writeOffset);
    
    jobPtr->feedbackBufSize = writeOffset + 1;
    jobPtr->feedbackContent = talloc(char , jobPtr->feedbackBufSize);
    memcpy(jobPtr->feedbackContent, writeFeedBackBuf, writeOffset);
    *(jobPtr->feedbackContent + writeOffset) = '\0';
}

void writeOpenFailedCmd(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    char openFailedStr[] = "OpenFailed\r\n\0";

    keyValueJob->feedbackBufSize = strlen(openFailedStr) + 1;
    keyValueJob->feedbackContent = talloc(char , keyValueJob->feedbackBufSize);
    
    memcpy(keyValueJob->feedbackContent, openFailedStr, keyValueJob->feedbackBufSize -1);
    
    *(keyValueJob->feedbackContent + keyValueJob->feedbackBufSize - 1) = '\0';

    return;
}

void writeOpenSucceedCmd(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    char openSucceedStr[] = "OpenOK\r\n\0";
    
    keyValueJob->feedbackBufSize = strlen(openSucceedStr) + 1;
    keyValueJob->feedbackContent = talloc(char , keyValueJob->feedbackBufSize);
    
    memcpy(keyValueJob->feedbackContent, openSucceedStr, keyValueJob->feedbackBufSize -1);
    
    *(keyValueJob->feedbackContent + keyValueJob->feedbackBufSize - 1) = '\0';
    
    return;
}

void writeReadFailedCmd(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    char readFailedStr[] = "ReadFailed\r\n\0";
    
    keyValueJob->feedbackBufSize = strlen(readFailedStr) + 1;
    keyValueJob->feedbackContent = talloc(char , keyValueJob->feedbackBufSize);
    
    memcpy(keyValueJob->feedbackContent, readFailedStr, keyValueJob->feedbackBufSize -1);
    
    *(keyValueJob->feedbackContent + keyValueJob->feedbackBufSize - 1) = '\0';
    
    return;
}

void writeReadSucceedFeedback(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob,keyValueItem_t *curItem){
    /**
     * Message we are going to format:
     ReadOK\r\nStripeK:value\r\nStripeM:value\r\nStripeW:value\r\nStreamSize:value\r\n
     FileSize:_\r\n
     matrix:values\r\n
     block0:value\r\n
     IP: value\r\n
     Port: value\r\n
     ServerStatus:_\r\n
     block1:value\r\n
     IP: value\r\n
     Port: value\r\n...
     block(k+m-1):value
     IP: value\r\n
     Port: value\r\n\0
     */
    
    size_t writeBackOffset = 0, curWriteSize = 0;
    ECMetaServer_t *ecMetaServerPtr = (ECMetaServer_t *)workerPtr->metaServerPtr;
    
    char writeFeedBackBuf[DEFAUT_MESSGAE_READ_WRITE_BUF];
    char readFeedBackHeader[] = "ReadOK\r\n\0";
    char fileSizeStr[] = "FileSize:\0";
    char stripeKHeader[] = "stripeK:\0";
    char stripeMHeader[] = "stripeM:\0";
    char stripeWHeader[] = "stripeW:\0";
    char streamingHeader[] = "streamingSize:\0";
    char matrixHeader[] = "matrix:\0";
    char blockHeader[] = "block\0";
    char serverStatusHeader[] = "ServerStatus:\0";
    char IPStr[] = "IP:";
    char PortStr[] = "Port:";
    char coma = ':';
    char space = ' ';
    char sufixStr[] = "\r\n\0";
    char buf[DEFAUT_MESSGAE_READ_WRITE_BUF];

    writeToBuf(writeFeedBackBuf, readFeedBackHeader, &curWriteSize, &writeBackOffset);
    
    writeToBuf(writeFeedBackBuf, fileSizeStr, &curWriteSize, &writeBackOffset);
    
    int_to_str((int)curItem->fileSize, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
    
    writeToBuf(writeFeedBackBuf, stripeKHeader, &curWriteSize, &writeBackOffset);
    int_to_str(ecMetaServerPtr->stripeK, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
    
    writeToBuf(writeFeedBackBuf, stripeMHeader, &curWriteSize, &writeBackOffset);
    int_to_str(ecMetaServerPtr->stripeM, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
    
    writeToBuf(writeFeedBackBuf, stripeWHeader, &curWriteSize, &writeBackOffset);
    int_to_str(ecMetaServerPtr->stripeW, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
    
    writeToBuf(writeFeedBackBuf, streamingHeader, &curWriteSize, &writeBackOffset);
    int_to_str(ecMetaServerPtr->streamingSize, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
    
    writeToBuf(writeFeedBackBuf, matrixHeader, &curWriteSize, &writeBackOffset);
    
    size_t idx, matrixSize = (ecMetaServerPtr->stripeK * ecMetaServerPtr->stripeM);
    
    for (idx = 0; idx < matrixSize; ++idx) {
        int value = *(ecMetaServerPtr->matrix + idx);
        int_to_str(value, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
        memcpy((writeFeedBackBuf + writeBackOffset), &space, 1);
        writeBackOffset = writeBackOffset + 1;
    }
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
    
    size_t blockSize = curItem->blockGroupsTail->blocksNum;
    
    for (idx = 0; idx < blockSize; ++idx) {
        ECBlock_t *curBlock = curItem->blockGroupsTail->blocks + idx;
        Rack_t *curRack = ecMetaServerPtr->rMap->racks + curBlock->rackId;
        Server_t *curServer = curRack->servers + curBlock->serverId;
        
        writeToBuf(writeFeedBackBuf, blockHeader, &curWriteSize, &writeBackOffset);
        int_to_str((int)idx, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
        memcpy((writeFeedBackBuf + writeBackOffset), &coma, 1);
        writeBackOffset = writeBackOffset + 1;
        
        uint64_to_str(curBlock->blockId, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
        writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
        
        writeToBuf(writeFeedBackBuf, IPStr, &curWriteSize, &writeBackOffset);
        
        memcpy((writeFeedBackBuf + writeBackOffset), curServer->serverIP, curServer->IPSize);
        writeBackOffset = writeBackOffset + curServer->IPSize;
        writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
        
        writeToBuf(writeFeedBackBuf, PortStr, &curWriteSize, &writeBackOffset);
        int_to_str(curServer->serverPort, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
        writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
        
        //ServerStatus:0 is off 1 is connected\r\n
        writeToBuf(writeFeedBackBuf, serverStatusHeader, &curWriteSize, &writeBackOffset);
        
        if (curServer->curState == SERVER_CONNECTED) {
            int_to_str(1, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        }else{
            int_to_str(0, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        }
        
        writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
        writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);

    }
    
    keyValueJob->feedbackBufSize = writeBackOffset + 1;
    keyValueJob->feedbackContent = talloc(char , keyValueJob->feedbackBufSize);
    memcpy(keyValueJob->feedbackContent, writeFeedBackBuf, writeBackOffset);
    *(keyValueJob->feedbackContent + writeBackOffset) = '\0';
    
    //printf("keyValueJob->feedbackBufSize:%lu, Content size:%lu, Content to write:%s\n",keyValueJob->feedbackBufSize, strlen(keyValueJob->feedbackContent), keyValueJob->feedbackContent);
}


void writeCreateFailFeedBack(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    char createFailedStr[] = "CreateFailed\r\n\0";
    
    keyValueJob->feedbackBufSize = strlen(createFailedStr) + 1;
    keyValueJob->feedbackContent = talloc(char , keyValueJob->feedbackBufSize);
    
    memcpy(keyValueJob->feedbackContent, createFailedStr, keyValueJob->feedbackBufSize -1);
    
    *(keyValueJob->feedbackContent + keyValueJob->feedbackBufSize - 1) = '\0';
    
    return;
//    keyValueJob->feedbackBufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
//    keyValueJob->feedbackContent = talloc(char, keyValueJob->feedbackBufSize);
    
}

void writeCreateFeedBack(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob,keyValueItem_t *curItem){
/**
 * Message we are going to format:
   CreateOK\r\nfilename\r\nStripeK:value\r\nStripeM:value\r\nStripeW:value\r\nStreamSize:value\r\n
   matrix:values\r\n
   block0:value\r\n
   IP: value\r\n
   Port: value\r\n
   block1:value\r\n
   IP: value\r\n
   Port: value\r\n...
   block(k+m-1):value
   IP: value\r\n
   Port: value\r\n\0
 */
    size_t writeBackOffset = 0, curWriteSize = 0;
    ECMetaServer_t *ecMetaServerPtr = (ECMetaServer_t *)workerPtr->metaServerPtr;
    
    char writeFeedBackBuf[DEFAUT_MESSGAE_READ_WRITE_BUF];
    char createFeedBackHeader[] = "CreateOK\r\n\0";
    char stripeKHeader[] = "stripeK:\0";
    char stripeMHeader[] = "stripeM:\0";
    char stripeWHeader[] = "stripeW:\0";
    char streamingHeader[] = "streamingSize:\0";
    char matrixHeader[] = "matrix:\0";
    char blockHeader[] = "block\0";
    char IPStr[] = "IP:";
    char PortStr[] = "Port:";
    char coma = ':';
    char space = ' ';
    char sufixStr[] = "\r\n\0";
    char buf[DEFAUT_MESSGAE_READ_WRITE_BUF];
    
    writeToBuf(writeFeedBackBuf, createFeedBackHeader, &curWriteSize, &writeBackOffset);
    
    memcpy((writeFeedBackBuf + writeBackOffset),curItem->key, curItem->keySize);
    writeBackOffset = writeBackOffset + curItem->keySize;
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
    
    writeToBuf(writeFeedBackBuf, stripeKHeader, &curWriteSize, &writeBackOffset);
    int_to_str(ecMetaServerPtr->stripeK, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
    
    writeToBuf(writeFeedBackBuf, stripeMHeader, &curWriteSize, &writeBackOffset);
    int_to_str(ecMetaServerPtr->stripeM, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);

    writeToBuf(writeFeedBackBuf, stripeWHeader, &curWriteSize, &writeBackOffset);
    int_to_str(ecMetaServerPtr->stripeW, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
    
    writeToBuf(writeFeedBackBuf, streamingHeader, &curWriteSize, &writeBackOffset);
    int_to_str(ecMetaServerPtr->streamingSize, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);

    writeToBuf(writeFeedBackBuf, matrixHeader, &curWriteSize, &writeBackOffset);
    
    size_t idx, matrixSize = (ecMetaServerPtr->stripeK * ecMetaServerPtr->stripeM);
    
    for (idx = 0; idx < matrixSize; ++idx) {
        int value = *(ecMetaServerPtr->matrix + idx);
        int_to_str(value, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
        memcpy((writeFeedBackBuf + writeBackOffset), &space, 1);
        writeBackOffset = writeBackOffset + 1;
    }
    writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);

    size_t blockSize = curItem->blockGroupsTail->blocksNum;
    
    for (idx = 0; idx < blockSize; ++idx) {
        ECBlock_t *curBlock = curItem->blockGroupsTail->blocks + idx;
        Rack_t *curRack = ecMetaServerPtr->rMap->racks + curBlock->rackId;
        Server_t *curServer = curRack->servers + curBlock->serverId;;
        
        if (curServer->curState != SERVER_CONNECTED) {
            int sIdx = 0;
            for (sIdx = 0; idx < blockSize; ++sIdx){
                if (sIdx == idx) {
                    continue;
                }
                ECBlock_t *theBlock = curItem->blockGroupsTail->blocks + sIdx;
                Rack_t *theRack = ecMetaServerPtr->rMap->racks + theBlock->rackId;
                curServer = theRack->servers + theBlock->serverId;
                
                if (curServer->curState != SERVER_DOWN) {
                    break;
                }
            }
        }


        writeToBuf(writeFeedBackBuf, blockHeader, &curWriteSize, &writeBackOffset);
        int_to_str((int)idx, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
        memcpy((writeFeedBackBuf + writeBackOffset), &coma, 1);
        writeBackOffset = writeBackOffset + 1;
        
        uint64_to_str(curBlock->blockId, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
        writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);

        writeToBuf(writeFeedBackBuf, IPStr, &curWriteSize, &writeBackOffset);
        
        memcpy((writeFeedBackBuf + writeBackOffset), curServer->serverIP, curServer->IPSize);
        writeBackOffset = writeBackOffset + curServer->IPSize;
        writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);

        writeToBuf(writeFeedBackBuf, PortStr, &curWriteSize, &writeBackOffset);
        int_to_str(curServer->serverPort, buf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        writeToBuf(writeFeedBackBuf, buf, &curWriteSize, &writeBackOffset);
        writeToBuf(writeFeedBackBuf, sufixStr, &curWriteSize, &writeBackOffset);
    }
    
    keyValueJob->feedbackBufSize = writeBackOffset + 1;
    keyValueJob->feedbackContent = talloc(char , keyValueJob->feedbackBufSize);
    memcpy(keyValueJob->feedbackContent, writeFeedBackBuf, writeBackOffset);
    *(keyValueJob->feedbackContent + writeBackOffset) = '\0';
    
//    printf("keyValueJob->feedbackBufSize:%lu, Content size:%lu, Content to write:%s\n",keyValueJob->feedbackBufSize, strlen(keyValueJob->feedbackContent), keyValueJob->feedbackContent);
    
}

void writeDeleteOKFeedBack(KeyValueWorker_t *workerPtr, KeyValueJob_t *jobPtr){
    //ECMetaServer_t *ecMetaServerPtr = (ECMetaServer_t *)workerPtr->metaServerPtr;
    keyValueItem_t *curItem = (keyValueItem_t *)jobPtr->itemPtr;

    char writeFeedBackBuf[DEFAUT_MESSGAE_READ_WRITE_BUF];

    char deleteOKStr[] = "DeleteOK\r\n\0";
    char filenameStr[] = "Filename:\0";
    char suffix[] = "\r\n\0";
    size_t writeSize = 0, writeOffset = 0;

    writeToBuf(writeFeedBackBuf, deleteOKStr, &writeSize, &writeOffset);
    writeToBuf(writeFeedBackBuf, filenameStr, &writeSize, &writeOffset);
    memcpy(writeFeedBackBuf+writeOffset, curItem->key, curItem->keySize);
    writeOffset = writeOffset + curItem->keySize;
    writeToBuf(writeFeedBackBuf, suffix, &writeSize, &writeOffset);
    
    jobPtr->feedbackBufSize = writeOffset + 1;
    jobPtr->feedbackContent = talloc(char , jobPtr->feedbackBufSize);
    memcpy(jobPtr->feedbackContent, writeFeedBackBuf, writeOffset);
    *(jobPtr->feedbackContent + writeOffset) = '\0';

    //printf("writeDeleteOKFeedBack:%s\n", jobPtr->feedbackContent);
}

KeyValueJob_t *performCreateJob(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    //What should I do here. Roadmap: First check if the key exist, second if not insert and return succeeded, else return failed
    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)workerPtr->metaServerPtr;
    
    keyValueItem_t *curItem = insertKeyItem(workerPtr,metaServerPtr->keyValueEnginePtr , keyValueJob->key, keyValueJob->keySize, keyValueJob->hashValue);
    
    if (curItem == NULL) {
        //printf("curItem == NULL  insert Key failed\n");
        writeCreateFailFeedBack(workerPtr, keyValueJob);
    }else{
        //printf("curItem != NULL  insert Key succeed\n");
        writeCreateFeedBack(workerPtr, keyValueJob, curItem);
    }
    
    return keyValueJob;
}

void performOpenJob(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)workerPtr->metaServerPtr;
    
    keyValueItem_t *curItem = locateKeyValueItem(metaServerPtr->keyValueEnginePtr, keyValueJob->key,keyValueJob->keySize, keyValueJob->hashValue);
    
    if (curItem == NULL) {
        writeOpenFailedCmd(workerPtr, keyValueJob);
    }else{
        writeOpenSucceedCmd(workerPtr, keyValueJob);
    }
}

void performReadJob(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)workerPtr->metaServerPtr;
    
    keyValueItem_t *curItem = locateKeyValueItem(metaServerPtr->keyValueEnginePtr, keyValueJob->key,keyValueJob->keySize, keyValueJob->hashValue);

    if (curItem == NULL) {
        writeReadFailedCmd(workerPtr, keyValueJob);
    }else{
        writeReadSucceedFeedback(workerPtr, keyValueJob, curItem);
    }
}

void performWriteJob(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    
}

void performDeleteJob(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    //gettimeofday(&jobPtr->workerInTime, NULL);

    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)workerPtr->metaServerPtr;

    keyValueItem_t *curItem = deleteKeyValueItem(metaServerPtr->keyValueEnginePtr, keyValueJob->key,keyValueJob->keySize, keyValueJob->hashValue);
    
    if (curItem == NULL) {
        writeDeleteFailedFeedBack(workerPtr, keyValueJob);
    }else{
        keyValueJob->itemPtr = (void *)curItem;
        
    }
}

void performWriteOverJob(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)workerPtr->metaServerPtr;
    keyValueItem_t *curItem = locateKeyValueItem(metaServerPtr->keyValueEnginePtr, keyValueJob->key, keyValueJob->keySize, keyValueJob->hashValue);
    
    if (curItem == NULL) {
        printf("Error: cannot find the item for writeover\n");
        return;
    }
    
    itemIncreFileSize(metaServerPtr->keyValueEnginePtr, curItem,  keyValueJob->writeSize);
    
//    printf("Current file size :%lu\n", curItem->fileSize);
    return;
}

void performDeleteDoneJob(KeyValueWorker_t *workerPtr, KeyValueJob_t *keyValueJob){
    //gettimeofday(&keyValueJob->workerOutTime, NULL);

    keyValueItem_t *curItem = (keyValueItem_t *)keyValueJob->itemPtr;
    writeDeleteOKFeedBack(workerPtr, keyValueJob);
    revokeItem(workerPtr, curItem);
}

void writeJobFeedBack(KeyValueWorker_t *workerPtr, KeyValueJob_t *jobPtr){
    ECMetaServer_t *metaServerPtr = workerPtr->metaServerPtr;
    ECClientThreadManager_t *clientThMgr = metaServerPtr->ecClientThreadMgr;
    ECClientThread_t *curClientThread = clientThMgr->clientThreads + jobPtr->clientThreadIdx;
    
//    write(workerPtr->workerLogFd, jobPtr->feedbackContent, jobPtr->feedbackBufSize);
    char tmpBuf[1024], writeSizeStr[] = "PipeWriteSize:\0", intBuf[30];
    size_t tmpBufSize = 0;
    
    tmpBufSize = strlen(writeSizeStr);
    memcpy(tmpBuf, writeSizeStr, tmpBufSize);
    
    pthread_mutex_lock(&curClientThread->threadLock);
    
    putJob(curClientThread->incomingJobQueue, jobPtr);
    char ch = 'D';
    if(curClientThread->curPipeState == PIPE_WRITEABLE
       && (write(curClientThread->pipes[1], &ch, 1) != 1)){
//        curClientThread->curPipeState = PIPE_UNABLETOWRITE;
        perror("Error");
        printf("ecClientThread:%lu pipe unable to write\n", curClientThread->idx);
        monitoringPipeWriteble(curClientThread);
    }else{
        ++workerPtr->pipeIdx;
    }
//    ssize_t writeSize = write(curClientThread->pipes[1], pipeBuf, pipeSize);
//    
//    if(writeSize < 0 || (size_t)writeSize != pipeSize){
//        perror("pipe write error");
//        printf("pipeSize:%lu, writeSize%ld\n",pipeSize, writeSize);
//    }
    //printf("write feedback to thread:%lu\n",curClientThread->idx);
    
    pthread_mutex_unlock(&curClientThread->threadLock);
    
    size_t intBufSize = int_to_str(workerPtr->pipeIdx, intBuf, 30);
    memcpy(tmpBuf + tmpBufSize, intBuf, intBufSize);
    tmpBufSize = tmpBufSize  + intBufSize;
    tmpBuf[tmpBufSize] = '\r';
    tmpBuf[tmpBufSize+1] = '\n';
    tmpBufSize = tmpBufSize + 2;
//    write(workerPtr->workerLogFd, tmpBuf, tmpBufSize);
}

/*void printTimeInterval(KeyValueJob_t *jobPtr){
    double clientToWorkerTimeConsume = timeIntervalInMS(jobPtr->clientInTime, jobPtr->workerInTime);
    double workerToServerTimeConsume = timeIntervalInMS(jobPtr->workerInTime, jobPtr->serverInTime);
    double metaAndServerRoundTimeConsume = timeIntervalInMS(jobPtr->serverInTime, jobPtr->serverOutTime);
    double serverToWorkerTimeConsume = timeIntervalInMS(jobPtr->serverOutTime, jobPtr->workerOutTime);

    printf("clientToWorkerTimeConsume:%fms\n", clientToWorkerTimeConsume);
    printf("workerToServerTimeConsume:%fms\n", workerToServerTimeConsume);
    printf("metaAndServerRoundTimeConsume:%fms\n", metaAndServerRoundTimeConsume);
    printf("serverToWorkerTimeConsume:%fms\n", serverToWorkerTimeConsume);
}*/

void performWorkerJob(KeyValueWorker_t *workerPtr){
    //printf("worker:%lu start to perform job\n", workerPtr->workerIdx);
    
    while (workerPtr->handlingJobQueue->jobSize != 0) {
        KeyValueJob_t *jobPtr = getKeyValueJob(workerPtr->handlingJobQueue);
        
        switch (jobPtr->jobType) {
            case KEYVALUEJOB_CREATE:
                performCreateJob(workerPtr, jobPtr);
                break;
            case KEYVALUEJOB_READ:
                performReadJob(workerPtr,jobPtr);
                break;
            case KEYVALUEJOB_OPEN:
                performOpenJob(workerPtr,jobPtr);
                break;
            case KEYVALUEJOB_WRITESIZE:
                //printf("Perform writesize job\n");
                performWriteOverJob(workerPtr,jobPtr);
                break;
            case KEYVALUEJOB_WRITE:
                performWriteJob(workerPtr,jobPtr);
                break;
            case KEYVALUEJOB_DELETE:
            {
                performDeleteJob(workerPtr, jobPtr);

                if (jobPtr->itemPtr != NULL )
                {
                    ECMetaServer_t *metaServer = (ECMetaServer_t *)workerPtr->metaServerPtr;
                    jobPtr->workerIdx = workerPtr->workerIdx;
                    submitJobToBlockMgr(metaServer->blockServerMgr, jobPtr);
                    continue;
                }
            }
                break;
            case KEYVALUEJOB_DELETE_DONE:{
                performDeleteDoneJob(workerPtr, jobPtr);
                //printTimeInterval(jobPtr);
            }
                break;
            default:
                printf("Unknow keyvalue job\n");
                break;
        }
        //
        writeJobFeedBack(workerPtr, jobPtr);

    }
}

int createWorkerLogFd(int workerIdx){
    char tempBuf[1024];
    char bufSuffix[] ="worker.Log\0";
    size_t strSize = int_to_str(workerIdx, tempBuf, 1024);
    size_t suffixStr = strlen(bufSuffix);
    memcpy(tempBuf + strSize, bufSuffix, suffixStr);
    tempBuf[strSize + suffixStr] = '\0';
    
    int fd =  open(tempBuf, O_RDWR | O_CREAT | O_SYNC, 0666);
    
    return fd;

}

void *startWorker(void *arg){
    KeyValueWorker_t *workerPtr = (KeyValueWorker_t *)arg;
    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)workerPtr->metaServerPtr;
    
//    printf("Key value worker :%d started\n", workerPtr->workerIdx);
    
    while (metaServerPtr->exitFlag == 0) {
        
//        printf("Start key value worker:%lu\n", workerPtr->workerIdx);
        sem_wait(&workerPtr->waitJobSem);
        
//        printf("worker:%lu start working\n",workerPtr->workerIdx);
                
        pthread_mutex_lock(&workerPtr->threadLock);
        dry_pipe_msg(workerPtr->workerPipes[0]);
        //Recv read, write and delete job
        while (workerPtr->pendingJobQueue->jobSize != 0) {
            putJob(workerPtr->handlingJobQueue, getKeyValueJob(workerPtr->pendingJobQueue));
        }
        pthread_mutex_unlock(&workerPtr->threadLock);
        
        //Perform job and write feedback...
        performWorkerJob(workerPtr);
        
    }
    
    return arg;
}

void *workerMonitor(void *arg){
    KeyValueEngine_t *keyValueEnginePtr = (KeyValueEngine_t *)arg;
    ECMetaServer_t *metaServerPtr = (ECMetaServer_t *)keyValueEnginePtr->metaEnginePtr;

    int idx, eventsNum;
    
    //    struct epoll_event clientEvent, serverEvent;
    struct epoll_event* events;
    
    events = talloc(struct epoll_event, MAX_CONN_EVENTS);

    if (events == NULL) {
        perror("unable to alloc memory for events");
        return arg;
    }

    for (idx = 0; idx < keyValueEnginePtr->workerNum; ++idx)
    {
        KeyValueWorker_t *workerPtr = (keyValueEnginePtr->workers + idx);
        int status = add_event(keyValueEnginePtr->efd, EPOLLIN , 
                                workerPtr->workerPipes[0], (void *)workerPtr);
        if(status ==-1)
        {
            perror("epoll_ctl add_event blockServer");
            return arg;
        }
    }

    while (metaServerPtr->exitFlag == 0){
        eventsNum = epoll_wait(keyValueEnginePtr->efd, events, 
                                MAX_CONN_EVENTS,DEFAULT_TIMEOUT_IN_MillisecondForWorkers);

        for (idx = 0 ; idx < eventsNum; ++idx) {
            KeyValueWorker_t *workerPtr = (KeyValueWorker_t *) events[idx].data.ptr;
            if (events[idx].events & EPOLLIN){
                sem_post(&workerPtr->waitJobSem);
            }else if(events[idx].events & EPOLLOUT){
                pthread_mutex_lock(&workerPtr->threadLock);
                delete_event(keyValueEnginePtr->efd, workerPtr->workerPipes[1]);
                workerPtr->curPipeState = WORKER_PIPE_WRITEABLE;
                pthread_mutex_unlock(&workerPtr->threadLock);
                sem_post(&workerPtr->waitJobSem);
            }else{
                printf("Error value EPOLLERR:%d  EPOLLHUP: %d EPOLLIN: %d EPOLLOUT:%d\n",EPOLLERR,EPOLLHUP,EPOLLIN,EPOLLOUT);
                printf("error on worker :%lu,  error:%d\n", workerPtr->workerIdx, events[idx].events);
            }
        }
    }

    return arg;
}

void monitoringWorkerPipeWritable(KeyValueWorker_t *workerPtr){
    ECMetaServer_t *ecMetaServerPtr = (ECMetaServer_t *)workerPtr->metaServerPtr;
    KeyValueEngine_t *keyValueEnginePtr = ecMetaServerPtr->keyValueEnginePtr;
    add_event(keyValueEnginePtr->efd, EPOLLOUT | EPOLLET, workerPtr->workerPipes[1], workerPtr);
    workerPtr->curPipeState = WORKER_PIPE_UNABLETOWRITE;
}
