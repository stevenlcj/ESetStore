//
//  ClientConnection.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ClientConnection.h"
#include "ecCommon.h"
#include "MessageBuffer.h"
#include "ecUtils.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>

ClientConnection_t *createClientConn(int sockFd){
    ClientConnection_t *clientConn = talloc(ClientConnection_t, 1);
    
    clientConn->curState = CLIENT_CONN_STATE_UNREGISTERED;
    clientConn->jobState = CLIENT_CONN_JOB_STATE_WAITING;
    clientConn->sockFd = sockFd;
    //clientConn->logFd = -1;
    clientConn->readMsgBuf = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);
    clientConn->writeMsgBuf = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);
    
    return clientConn;
}

int createLogFd(int sockFd){
    char tempBuf[1024];
    char bufSuffix[] ="sock.Log\0";
    size_t strSize = int_to_str(sockFd, tempBuf, 1024);
    size_t suffixStr = strlen(bufSuffix);
    memcpy(tempBuf + strSize, bufSuffix, suffixStr);
    tempBuf[strSize + suffixStr] = '\0';
    
    int fd =  open(tempBuf, O_RDWR | O_CREAT | O_SYNC, 0666);
    
    return fd;
}

void deallocClientConn(ClientConnection_t *clientConnPtr){
    ECMessageBuffer_t *msgBufPtr = clientConnPtr->writeMsgBuf;
    while (msgBufPtr != NULL){        
        ECMessageBuffer_t *nextBufPtr = msgBufPtr->next;
        freeMessageBuf(msgBufPtr);
        msgBufPtr = nextBufPtr;
    }
    
    msgBufPtr = clientConnPtr->readMsgBuf;
    while (msgBufPtr != NULL){
        ECMessageBuffer_t *nextBufPtr = msgBufPtr->next;
        freeMessageBuf(msgBufPtr);
        msgBufPtr = nextBufPtr;
    }
    
    /*if(clientConnPtr->logFd != -1){
        close(clientConnPtr->logFd);
    }*/

    free(clientConnPtr);
}

void addWriteBuf(ClientConnection_t *clientConn, char *buf, size_t bufSize){
    if (bufSize == 0) {
        return;
    }
    
    ECMessageBuffer_t *curWritePtr = clientConn->writeMsgBuf;
    size_t bufWriteIdx = 0;
    while (curWritePtr->next != NULL) {
        curWritePtr = curWritePtr->next;
    }
    
    while ((curWritePtr->bufSize - curWritePtr->wOffset) < (bufSize - bufWriteIdx)) {
        memcpy((curWritePtr->buf + curWritePtr->wOffset), (buf+bufWriteIdx), (curWritePtr->bufSize - curWritePtr->wOffset));
        bufWriteIdx = bufWriteIdx + (curWritePtr->bufSize - curWritePtr->wOffset);
        curWritePtr->wOffset = curWritePtr->wOffset + (curWritePtr->bufSize - curWritePtr->wOffset);
        ECMessageBuffer_t *msgBuf = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);
        curWritePtr->next = msgBuf;
        curWritePtr = msgBuf;
    }
    
    if ((bufSize - bufWriteIdx) != 0) {
        memcpy((curWritePtr->buf + curWritePtr->wOffset), (buf+bufWriteIdx), (bufSize - bufWriteIdx));
        curWritePtr->wOffset = curWritePtr->wOffset + (bufSize - bufWriteIdx);
    }
}