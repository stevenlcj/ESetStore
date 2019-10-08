//
//  ECClientManagement.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECClientManagement.h"
#include "ecCommon.h"
#include "ECMetaEngine.h"
#include "ecNetHandle.h"
#include "ECClientCommand.h"
#include "hash.h"
#include "ECJob.h"
#include "ecUtils.h"
#include "ECClientCmdHandle.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>

#define DEFAULT_ACCEPT_TIME_OUT_IN_SEC 100000

void addClientWriteEvent(ECClientThread_t *ecClientThread, ClientConnection_t *curConn){
    if (curConn->curState == CLIENT_CONN_STATE_READWRITE) {
        return;
    }
    
    curConn->curState = CLIENT_CONN_STATE_READWRITE;
    update_event(ecClientThread->efd, EPOLLIN | EPOLLOUT , curConn->sockFd, curConn);
}

void removeClientWriteEvent(ECClientThread_t *ecClientThread, ClientConnection_t *curConn){
    update_event(ecClientThread->efd, EPOLLIN  , curConn->sockFd, curConn);
    curConn->curState = CLIENT_CONN_STATE_READ;
}

void writeToClientConn(ECClientThread_t *ecClientThread, ClientConnection_t *curConn){
    
    while (curConn->writeMsgBuf->rOffset != curConn->writeMsgBuf->wOffset ) {
        ECMessageBuffer_t *writeMsgBuf = curConn->writeMsgBuf;
        
        ssize_t wSize = send(curConn->sockFd,  writeMsgBuf->buf+ writeMsgBuf->rOffset, (writeMsgBuf->wOffset-writeMsgBuf->rOffset), 0);
        
        if (wSize <= 0) {
            //Add it to ...
            addClientWriteEvent(ecClientThread, curConn);
            return;
        }
        
        //write(curConn->logFd, writeMsgBuf->buf+ writeMsgBuf->rOffset, wSize);
        
        writeMsgBuf->rOffset = writeMsgBuf->rOffset + wSize;
        
        if (writeMsgBuf->rOffset == writeMsgBuf->wOffset) {
            if (writeMsgBuf->next != NULL) {
                curConn->writeMsgBuf = curConn->writeMsgBuf->next;
                freeMessageBuf(writeMsgBuf);
            }else{
                writeMsgBuf->rOffset = 0;
                writeMsgBuf->wOffset = 0;
            }
        }
    }
    
    if (curConn->curState == CLIENT_CONN_STATE_READWRITE) {
        removeClientWriteEvent(ecClientThread,curConn);
    }
}

//Client Cmd:
/**
 a: Create:
    Create
 */
int canStartParse(char *buf, size_t bufSize){
    int parseFlag = 0;
    int idx;
    for (idx = 0; idx < (bufSize-1); ++idx) {
        if (*(buf+idx) == '\r' && *(buf+idx+1) == '\n') {
            ++parseFlag;
        }
        
        if (parseFlag == 2) {
            return parseFlag;
        }
    }
    
    return parseFlag;
}

void dumpClientBufMessage(ECMessageBuffer_t *dumMsgBuf){
    size_t bufSize = dumMsgBuf->wOffset;
    char *buf = dumMsgBuf->buf;
    int parseFlag = 0;
    int idx;
    for (idx = 0; idx < (bufSize-1); ++idx) {
        if (*(buf+idx) == '\r' && *(buf+idx+1) == '\n') {
            ++parseFlag;
        }
        
        if (parseFlag == 2) {
            break;
        }
    }
    
    memcpy(buf, (buf + idx + 2), dumMsgBuf->wOffset - (idx+2));
    dumMsgBuf->wOffset = dumMsgBuf->wOffset - (idx+2);
}

void getFileNameOffset(char *buf, size_t *startOffset, size_t *endOffset,size_t bufSize){
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

size_t getWriteOverFileNameOffsetWriteSize(char *buf, size_t *startOffset, size_t *endOffset,size_t bufSize){
    char WriteOverStr[] = "WriteOver:\0";
    char WriteSizeStr[] = "WriteSize:\0";
    char suffixStr[] = "\r\n\0";
    char writeSizeBuf[1024];
    size_t writeSize = 0;
    size_t index;
    *startOffset = 0;
    *endOffset = 0;
    for (index = 0; index < (bufSize - strlen(WriteOverStr)); ++index) {
        if (strncmp(WriteOverStr, buf+ index, strlen(WriteOverStr)) == 0) {
            *startOffset = index + strlen(WriteOverStr);
            break;
        }
    }
    
    for (index = *startOffset; index < (bufSize - strlen(suffixStr)); ++index) {
        if (strncmp(suffixStr, buf+ index, strlen(suffixStr)) == 0) {
            *endOffset = index - 1;
            break;
        }
    }
    
    index = index + strlen(suffixStr);
    size_t cpSize = (bufSize-index) > 1023? 1023: (bufSize-index);
    
    memcpy(writeSizeBuf, buf + index, cpSize);
    writeSizeBuf[cpSize] = '\0';
    
    writeSize = getSizeTValueBetweenStrs(WriteSizeStr, suffixStr, writeSizeBuf);
    
    return writeSize;
}

void recvWriteOverCmd(ECClientThread_t *clientThreadPtr, ClientConnection_t *clientConnPtr){
    //WriteOver:FileName\r\nWriteSize:__\r\n\0
    size_t fileNameStartOffset, fileNameEndOffset, writeSize;

    writeSize = getWriteOverFileNameOffsetWriteSize(clientConnPtr->readMsgBuf->buf, &fileNameStartOffset, &fileNameEndOffset, clientConnPtr->readMsgBuf->wOffset);

    uint32_t hashValue = hash((clientConnPtr->readMsgBuf->buf + fileNameStartOffset), (fileNameEndOffset - fileNameStartOffset + 1), 0);

    KeyValueJob_t *keyValueJob = createECJob((clientConnPtr->readMsgBuf->buf + fileNameStartOffset), (fileNameEndOffset - fileNameStartOffset + 1), hashValue, clientConnPtr->thIdx, clientConnPtr->idx, KEYVALUEJOB_WRITESIZE, writeSize);
    
    //gettimeofday(&keyValueJob->clientInTime, NULL);

    ECMetaServer_t *metaServer = (ECMetaServer_t *)clientThreadPtr->ecMetaEnginePtr;
    
    size_t keyValueThreadNum = metaServer->keyValueEnginePtr->workerNum;
    size_t chainSize = metaServer->keyValueEnginePtr->keyChainSize;
    
    size_t rangeEach = chainSize / keyValueThreadNum;
    size_t threadToReq = (hashValue % chainSize)/rangeEach;
    
    printf("submit KEYVALUEJOB_WRITESIZE job to worker:%lu with size:%lu\n",threadToReq, writeSize);
    
    KeyValueWorker_t *keyValueWorker = metaServer->keyValueEnginePtr->workers + threadToReq;
    pthread_mutex_lock(&keyValueWorker->threadLock);
    putJob(keyValueWorker->pendingJobQueue, keyValueJob);
    char ch='D';
    write(keyValueWorker->workerPipes[1], &ch, 1);
    pthread_mutex_unlock(&keyValueWorker->threadLock);

}

void recvCreateCmd(ECClientThread_t *clientThreadPtr, ClientConnection_t *clientConnPtr){
    size_t fileNameStartOffset, fileNameEndOffset;
    getFileNameOffset(clientConnPtr->readMsgBuf->buf, &fileNameStartOffset, &fileNameEndOffset,
                      clientConnPtr->readMsgBuf->wOffset);
    
    uint32_t hashValue = hash((clientConnPtr->readMsgBuf->buf + fileNameStartOffset), (fileNameEndOffset - fileNameStartOffset + 1), 0);
    
//    printf("file start offset:%lu end offset:%lu\n", fileNameStartOffset, fileNameEndOffset);
    
    KeyValueJob_t *keyValueJob =  createECJob((clientConnPtr->readMsgBuf->buf + fileNameStartOffset), (fileNameEndOffset - fileNameStartOffset + 1), hashValue, clientConnPtr->thIdx, clientConnPtr->idx, KEYVALUEJOB_CREATE, 0);

    ECMetaServer_t *metaServer = (ECMetaServer_t *)clientThreadPtr->ecMetaEnginePtr;
    
    size_t keyValueThreadNum = metaServer->keyValueEnginePtr->workerNum;
    size_t chainSize = metaServer->keyValueEnginePtr->keyChainSize;
    
    size_t rangeEach = chainSize / keyValueThreadNum;
    size_t threadToReq = (hashValue % chainSize)/rangeEach;
    
    KeyValueWorker_t *keyValueWorker = metaServer->keyValueEnginePtr->workers + threadToReq;
    pthread_mutex_lock(&keyValueWorker->threadLock);
    putJob(keyValueWorker->pendingJobQueue, keyValueJob);
    char ch='D';
    write(keyValueWorker->workerPipes[1], &ch, 1);
    pthread_mutex_unlock(&keyValueWorker->threadLock);
}

void recvOpenCmd(ECClientThread_t *clientThreadPtr, ClientConnection_t *clientConnPtr){
    size_t fileNameStartOffset, fileNameEndOffset;
    getFileNameOffset(clientConnPtr->readMsgBuf->buf, &fileNameStartOffset, &fileNameEndOffset,
                      clientConnPtr->readMsgBuf->wOffset);
    uint32_t hashValue = hash((clientConnPtr->readMsgBuf->buf + fileNameStartOffset), (fileNameEndOffset - fileNameStartOffset + 1), 0);
    
    //    printf("file start offset:%lu end offset:%lu\n", fileNameStartOffset, fileNameEndOffset);
    
    KeyValueJob_t *keyValueJob =  createECJob((clientConnPtr->readMsgBuf->buf + fileNameStartOffset), (fileNameEndOffset - fileNameStartOffset + 1), hashValue, clientConnPtr->thIdx, clientConnPtr->idx, KEYVALUEJOB_OPEN, 0);
    ECMetaServer_t *metaServer = (ECMetaServer_t *)clientThreadPtr->ecMetaEnginePtr;
    
    size_t keyValueThreadNum = metaServer->keyValueEnginePtr->workerNum;
    size_t chainSize = metaServer->keyValueEnginePtr->keyChainSize;
    
    size_t rangeEach = chainSize / keyValueThreadNum;
    size_t threadToReq = (hashValue % chainSize)/rangeEach;
    
    KeyValueWorker_t *keyValueWorker = metaServer->keyValueEnginePtr->workers + threadToReq;
    pthread_mutex_lock(&keyValueWorker->threadLock);
    putJob(keyValueWorker->pendingJobQueue, keyValueJob);
    char ch='D';
    write(keyValueWorker->workerPipes[1], &ch, 1);
    pthread_mutex_unlock(&keyValueWorker->threadLock);
}

//Cmd to be processed:
/**
a. Create\r\nFileName\r\n\0
b. Open\r\nFileName\r\n\0
c. Write\r\nFileName\r\n\0
d. WriteOver:FileName\r\nWriteSize:__\r\n\0
e. Append\r\nFileName\r\n\0
f. Read\r\nFileName\r\n\0
g. Delete\r\nFileName\r\n\0
f. Closing\r\n\r\n\0
 */

void processClientCmd(ECClientThread_t *clientThreadPtr, ClientConnection_t *clientConnPtr)
{
    size_t cmdSize;
//    printf("processClientCmd rOffset:%lu, wOffset:%lu\n",clientConnPtr->readMsgBuf->rOffset,clientConnPtr->readMsgBuf->wOffset);
    if (canStartParse(clientConnPtr->readMsgBuf->buf, clientConnPtr->readMsgBuf->wOffset) != 2) {
        return;
    }
    
    ECClientCommand_t clientCmd = parseClientCmd(clientConnPtr->readMsgBuf->buf, &cmdSize);
    
    switch (clientCmd) {
        case ECClientCommand_UNKNOWN:{
            printf("Unknow message, dump it\n");
            dumpClientBufMessage(clientConnPtr->readMsgBuf);
            processClientCmd(clientThreadPtr, clientConnPtr);
            return;
        }
            break;
        case ECClientCommand_CREATE:{
            //recvCreateCmd(clientThreadPtr, clientConnPtr);
            processRecvdCmd(clientThreadPtr, clientConnPtr, KEYVALUEJOB_CREATE);
        }
            break;
        case ECClientCommand_OPEN:{
            //recvOpenCmd(clientThreadPtr, clientConnPtr);
            processRecvdCmd(clientThreadPtr, clientConnPtr, KEYVALUEJOB_OPEN);
        }
            break;
        case ECClientCommand_WRITE:{
        }
            break;
        case ECClientCommand_WRITEOVER:{
            recvWriteOverCmd(clientThreadPtr, clientConnPtr);
        }
            break;
        case ECClientCommand_READ:{
            processRecvdCmd(clientThreadPtr, clientConnPtr, KEYVALUEJOB_READ);
        }
            break;
        case ECClientCommand_CLOSING:
            return;
            break;
        case ECClientCommand_DELETE:{
            printf("Recvd delete cmd:%s\n", clientConnPtr->readMsgBuf->buf);
            processRecvdCmd(clientThreadPtr, clientConnPtr, KEYVALUEJOB_DELETE);
        }
        default:
            break;
    }
    
    dumpMsgWithCh(clientConnPtr->readMsgBuf, '\0');
    
    if (clientConnPtr->readMsgBuf->wOffset != 0) {
        processClientCmd(clientThreadPtr, clientConnPtr);
    }
    
//    if (clientCmd == ECClientCommand_UNKNOWN) {
//        /*Dump msg*/
//    }else if (clientCmd == ECClientCommand_CREATE) {
//        
//    }else if(clientCmd == ECClientCommand_READ){
//        
//    }else if (clientCmd == ECClientCommand_WRITE){
//    
//    }else if (clientCmd == ECClientCommand_WRITEOVER){
//        //Delete cmd
//        
//    }
    
}

ssize_t processClientConnRead(ECClientThread_t *clientThreadPtr, ClientConnection_t *clientConnPtr){
    ssize_t readSize;
    ECMessageBuffer_t *rMsgBuf = clientConnPtr->readMsgBuf;
    int bufFullFlag = 0;
    //printf("Recv data from sock:%d conn idx:%d\n", clientConnPtr->sockFd, clientConnPtr->idx);
    
    readSize = recv(clientConnPtr->sockFd, (void *)(rMsgBuf->buf + rMsgBuf->wOffset), (rMsgBuf->bufSize-rMsgBuf->wOffset), 0);
    
    if (readSize > 0) {
        rMsgBuf->wOffset = rMsgBuf->wOffset + readSize;
        if (rMsgBuf->wOffset == rMsgBuf->bufSize) {
            bufFullFlag = 1;
        }
    }
    
    if (clientConnPtr->readMsgBuf->wOffset != 0 && clientConnPtr->curState != CLIENT_CONN_STATE_PENDINGCLOSE) {
        processClientCmd(clientThreadPtr, clientConnPtr);
    }
    
    if (readSize <= 0 ) {
        return 0;
    }
    
    if (bufFullFlag == 1 && clientConnPtr->curState != CLIENT_CONN_STATE_PENDINGCLOSE) {
        return readSize + processClientConnRead(clientThreadPtr, clientConnPtr);
    }
    
    return readSize;
}

void processClientConnWrite(ECClientThread_t *clientThreadPtr, ClientConnection_t *clientConnPtr){

    writeToClientConn(clientThreadPtr, clientConnPtr);
    return ;
}

ECClientThreadManager_t *createClientThreadManager(void *ecMetaEnginePtr){
    ECClientThreadManager_t *clientThreadMgr = talloc(ECClientThreadManager_t, 1);
    
    if (clientThreadMgr == NULL) {
        return NULL;
    }
    
    clientThreadMgr->clientThreads = talloc(ECClientThread_t, DEFAULT_CLIENT_THREAD_NUM);
    clientThreadMgr->clientThreadsNum = DEFAULT_CLIENT_THREAD_NUM;
    clientThreadMgr->ecMetaEnginePtr = ecMetaEnginePtr;
    int idx;
    pthread_t tid;
    for (idx = 0; idx < clientThreadMgr->clientThreadsNum; ++idx) {
        ECClientThread_t *clientThreadPtr = clientThreadMgr->clientThreads+ idx;
        clientThreadPtr->idx = idx;
        clientThreadPtr->totalPipeRSize = 0;
//        clientThreadPtr->threadLogFd = createThreadLog(idx);
        
        clientThreadPtr->ecMetaEnginePtr = ecMetaEnginePtr;
        initECClientThread(clientThreadPtr);
        pthread_create(&tid, NULL, startECClientThread, (void *)clientThreadPtr);
    }
    
    return clientThreadMgr;
}

void monitoringPipeWriteble(ECClientThread_t *ecClientThread){
    add_event(ecClientThread->efd, EPOLLOUT , ecClientThread->pipes[1], ecClientThread->clientConn[0]);
    ecClientThread->curPipeState = PIPE_UNABLETOWRITE;
}
void initECClientThread(ECClientThread_t *ecClientThread){
    if(pipe(ecClientThread->pipes) != 0){
        perror("pipe(ecClientThread->pipes) failed\n");
    }
    
    make_non_blocking_sock(ecClientThread->pipes[0]);
    ecClientThread->curPipeState = PIPE_WRITEABLE;
    
    pthread_mutex_init(&ecClientThread->threadLock, NULL);
    
    ecClientThread->clientConn = talloc(ClientConnection_t *, CLIENT_CONN_SIZE);
    ecClientThread->clientConnIndicator = talloc(int, CLIENT_CONN_SIZE);

    int idx;
    ecClientThread->clientConnSize = CLIENT_CONN_SIZE;
    ecClientThread->clientConnNum = 0;
    ecClientThread->incomingConn = NULL;
    
    ecClientThread->incomingJobQueue = createKeyValueJobQueue();
    ecClientThread->writeJobQueue = createKeyValueJobQueue();
    
    ecClientThread->clientConnIndicator[0] = 1;
    
    for (idx = 1; idx < CLIENT_CONN_SIZE; ++idx) {
        *(ecClientThread->clientConnIndicator + idx) = 0;
        *(ecClientThread->clientConn + idx) = NULL;
    }
}

int dry_pipe_msg(int sockFd){
    int readedSize = 0;
    ssize_t readSize;
    char readBuf[1024];
    while((readSize =  read(sockFd, readBuf, 1024) > 0 )){
        readedSize = readedSize + (int)readSize;
    }
    
    return readedSize;
}

void dry_clientPipe(ECClientThread_t *ecClientThread){
    ssize_t readSize;
    char readBuf[1024];
    while((readSize =  read(ecClientThread->pipes[0], readBuf, 1024) > 0 )){
//        write(ecClientThread->threadLogFd, readBuf, (size_t)readSize);
    }
}

void threadMonitorNewConnection(ECClientThread_t *ecClientThread, ClientConnection_t *clientConnPtr){
    int idx;
    
    for (idx = 1; idx < ecClientThread->clientConnSize; ++idx) {
        if (ecClientThread->clientConnIndicator[idx] == 0) {
            break;
        }
    }
    
    if (idx > ecClientThread->clientConnSize) {
        return;
    }
    
    ecClientThread->clientConnIndicator[idx] = 1;

    clientConnPtr->idx = idx;
    clientConnPtr->thIdx = ecClientThread->idx;
    ecClientThread->clientConn[idx] = clientConnPtr;
    ecClientThread->clientConn[idx]->curState =  CLIENT_CONN_STATE_READ;
    
    printf("Add EPOLLIN to sock:%d\n", ecClientThread->clientConn[idx]->sockFd);
    int status = add_event(ecClientThread->efd, EPOLLIN , ecClientThread->clientConn[idx]->sockFd, ecClientThread->clientConn[idx]);
    
    if(status ==-1)
    {
        perror("epoll_ctl threadMonitorNewConnection");
        return;
    }

}

void threadHandleNewConnections(ECClientThread_t *ecClientThread){
    while (ecClientThread->incomingConn != NULL) {
        ClientConnection_t *clientConnPtr = ecClientThread->incomingConn;
        threadMonitorNewConnection(ecClientThread, clientConnPtr);
        
        ecClientThread->incomingConn = clientConnPtr->next;
    }
}

void startWriteJob(ECClientThread_t *ecClientThread){
    size_t idx;
    //printf("thread:%lu startWriteJob\n", ecClientThread->idx);
    
    while (ecClientThread->writeJobQueue->jobSize != 0) {
        KeyValueJob_t *keyValueJob = getKeyValueJob(ecClientThread->writeJobQueue);
        //printf("write to fd:%d size:%lu\n",ecClientThread->threadLogFd, keyValueJob->feedbackBufSize);
        //write(ecClientThread->threadLogFd, keyValueJob->feedbackContent, keyValueJob->feedbackBufSize);
        if (ecClientThread->clientConnIndicator[keyValueJob->clientConnIdx] == 0) {
//            printf("dump job since conn is dealloced\n");
            deallocECJob(keyValueJob);
            continue;
        }
        ClientConnection_t *clientConn = ecClientThread->clientConn[keyValueJob->clientConnIdx];
        //printf("Add write content to client:%s\n", keyValueJob->feedbackContent);
        //write(clientConn->logFd, keyValueJob->feedbackContent, keyValueJob->feedbackBufSize);
        addWriteBuf(clientConn, keyValueJob->feedbackContent, keyValueJob->feedbackBufSize);
        deallocECJob(keyValueJob);
    }
    
    for (idx = 1; idx < ecClientThread->clientConnSize; ++idx) {

        if (ecClientThread->clientConnIndicator[idx] != 0 ) {
            ClientConnection_t *curConn = ecClientThread->clientConn[idx];
            writeToClientConn(ecClientThread, curConn);
        }
    }
}

void startMonitoringConn(ECClientThread_t *ecClientThread){
    ECMetaServer_t *metaServer = (ECMetaServer_t *)ecClientThread->ecMetaEnginePtr;
    
    int idx, eventsNum;
    struct epoll_event* events;
    
    events = talloc(struct epoll_event, MAX_CONN_EVENTS);
    
    if (events == NULL) {
        perror("unable to alloc memory for events");
        return;
        
    }

    ecClientThread->efd = create_epollfd();
    ClientConnection_t *clientConnPtr = createClientConn(ecClientThread->pipes[0]);
    make_non_blocking_sock(ecClientThread->pipes[1]);
    
    ecClientThread->clientConn[0] = clientConnPtr;
    
    add_event(ecClientThread->efd, EPOLLIN , ecClientThread->clientConn[0]->sockFd, ecClientThread->clientConn[0]);
    ecClientThread->clientConn[0]->curState = CLIENT_CONN_STATE_READ;
    
    while (metaServer->exitFlag == 0) {
        //printf("client :%lu wait event\n", ecClientThread->idx);
        eventsNum =epoll_wait(ecClientThread->efd, events, MAX_CONN_EVENTS,DEFAULT_ACCEPT_TIME_OUT_IN_SEC);
        if (eventsNum  <= 0) {
            //            printf("Time out\n");
            continue;
        }
        
        //printf("client :%lu process event\n", ecClientThread->idx);
        for (idx = 0 ; idx < eventsNum; ++idx) {
            if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)||
               (!(events[idx].events & EPOLLIN) && !(events[idx].events & EPOLLOUT)))
            {
                printf("EPOLLERR %d EPOLLHUP:%d EPOLLIN:%d EPOLLOUT:%d\n ",EPOLLERR, EPOLLHUP, EPOLLIN, EPOLLOUT);
                fprintf(stderr,"epoll error event id:%d\n", events[idx].events);
                ClientConnection_t *curClientConn = (ClientConnection_t *) events[idx].data.ptr;

                perror("error on the client fd, have to close\n");
                removeConnection(ecClientThread, curClientConn);
                continue;
                
            }else {
                /*Notification on the client*/
                ClientConnection_t *curClientConn = (ClientConnection_t *) events[idx].data.ptr;
                
                if (curClientConn->curState == CLIENT_CONN_STATE_PENDINGCLOSE) {
                    removeConnection(ecClientThread, curClientConn);
                    continue;
                }
                
                if (curClientConn == ecClientThread->clientConn[0]) {
                    //Msg from pipe, thread has work to do
                    char pipeId[] = "pipeId\0",pipeRSize[] = "PipeReadSize:\0", rSize[30];
                    char tmpBuf[1024];
                    size_t tmpBufSize = 0;
                    pthread_mutex_lock(&ecClientThread->threadLock);
                    
                    if(events[idx].events & EPOLLOUT &&
                       ecClientThread->curPipeState == PIPE_UNABLETOWRITE){
                        printf("ecClientThread:%lu pipe writable\n", ecClientThread->idx);
                        ecClientThread->curPipeState = PIPE_WRITEABLE;
                        delete_event(ecClientThread->efd, ecClientThread->pipes[1]);
                    }
                    
                    //dry_clientPipe(curClientConn);
                    int totalRead = dry_pipe_msg(curClientConn->sockFd);
                    
                    if(totalRead == 0){
                        perror("Unable to read pipe");
                        printf("pipefd:%d, sockFd:%d\n", ecClientThread->pipes[0], curClientConn->sockFd);
                        exit(0);
                    }
                    
                    if (ecClientThread->incomingConn != NULL) {
                        threadHandleNewConnections(ecClientThread);
                    }
                    
                    if(totalRead > 0){
                        ecClientThread->totalPipeRSize = ecClientThread->totalPipeRSize + totalRead;
                    }
                    
//                    tmpBufSize = strlen(pipeRSize);
//                    memcpy(tmpBuf, pipeRSize, tmpBufSize);
//                    size_t intSize = int_to_str(ecClientThread->totalPipeRSize, rSize, 30);
//                    memcpy(tmpBuf + tmpBufSize, rSize, intSize);
//                    tmpBufSize = tmpBufSize + intSize;
//                    memcpy(tmpBuf + tmpBufSize, pipeId, strlen(pipeId));
//                    tmpBufSize = tmpBufSize + strlen(pipeId);
//                    intSize = int_to_str(curClientConn->sockFd, rSize, 30);
//                    memcpy(tmpBuf + tmpBufSize, rSize, intSize);
//                    tmpBufSize = tmpBufSize + intSize;
//                    tmpBuf[tmpBufSize] = '\r';
//                    tmpBuf[tmpBufSize + 1] = '\n';
//                    tmpBufSize = tmpBufSize + 2;
//                    write(ecClientThread->threadLogFd, tmpBuf, tmpBufSize);
                    
                    //printf("write(ecClientThread->threadLogFd:%d\n",ecClientThread->threadLogFd);
                    
                    while (ecClientThread->incomingJobQueue->jobSize != 0) {
                        KeyValueJob_t *keyValueJob = getKeyValueJob(ecClientThread->incomingJobQueue);
//                        write(ecClientThread->threadLogFd, keyValueJob->feedbackContent, keyValueJob->feedbackBufSize);
                        putJob(ecClientThread->writeJobQueue, keyValueJob);
                    }
                    
                    pthread_mutex_unlock(&ecClientThread->threadLock);
                    startWriteJob(ecClientThread);
                    
                }else{
                    if ((events[idx].events & EPOLLIN)) {
                        //printf("EPOLLIN from client\n");
                        ssize_t readSize = processClientConnRead(ecClientThread, curClientConn);
                        
                        if (readSize == 0 || curClientConn->curState == CLIENT_CONN_STATE_PENDINGCLOSE) {
                            
                            removeConnection(ecClientThread, curClientConn);
                            
                        }
                    }else{
                        //printf("EPOLLOUT from client\n");
                        processClientConnWrite(ecClientThread, curClientConn);
                    }
                }
            }
            
        }
    }
}

void *startECClientThread(void *arg){
    ECClientThread_t *ecClientThread = (ECClientThread_t *)arg;
    printf("start client thread:%lu\n",ecClientThread->idx);
    
    startMonitoringConn(ecClientThread);
    
    return arg;
}

int createThreadLog(int idx){
    char tempBuf[1024];
    char bufSuffix[] ="ClientConn.Log\0";
    size_t strSize = int_to_str(idx, tempBuf, 1024);
    size_t suffixStr = strlen(bufSuffix);
    memcpy(tempBuf + strSize, bufSuffix, suffixStr);
    tempBuf[strSize + suffixStr] = '\0';
    
    int fd =  open(tempBuf, O_RDWR | O_CREAT | O_SYNC, 0666);
    
    return fd;
}

void addConnection(ECClientThread_t *ecClientThread, ClientConnection_t *clientConn){
    if (clientConn == NULL) {
        printf("unable to add NULL for addConnection\n");
        return;
    }
    
    pthread_mutex_lock(&ecClientThread->threadLock);
    
    clientConn->next = ecClientThread->incomingConn;
    ecClientThread->incomingConn = clientConn;
    ++ecClientThread->clientConnNum;
    char ch = 'C';
    write(ecClientThread->pipes[1], &ch, 1);
    
    pthread_mutex_unlock(&ecClientThread->threadLock);
}

void removeConnection(ECClientThread_t *ecClientThread, ClientConnection_t *clientConn){
    printf("Connection closed on sock:%d remove from monitoring\n", clientConn->sockFd);
    delete_event(ecClientThread->efd, clientConn->sockFd);
    close(clientConn->sockFd);
    
    int idx = clientConn->idx;
    
    ecClientThread->clientConnIndicator[idx] = 0;
    
    deallocClientConn(clientConn);
    
    ecClientThread->clientConn[idx] = NULL;
    --ecClientThread->clientConnNum;
}



