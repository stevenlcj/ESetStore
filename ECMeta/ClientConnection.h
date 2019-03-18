//
//  ClientConnection.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__ClientConnection__
#define __ECMeta__ClientConnection__

#include <stdio.h>

#include "MessageBuffer.h"

typedef enum {
    CLIENT_CONN_JOB_STATE_WAITING = 0x01,
    CLIENT_CONN_JOB_STATE_PARSING,
    CLIENT_CONN_JOB_STATE_CREATE,
    CLIENT_CONN_JOB_STATE_APPEND,
    CLIENT_CONN_JOB_STATE_READ,
    CLIENT_CONN_JOB_STATE_WRITE,
    CLIENT_CONN_JOB_STATE_DELETE
}CLIENT_CONN_JOB_STATE;

typedef enum {
    CLIENT_CONN_STATE_UNREGISTERED = 0x01,
    CLIENT_CONN_STATE_READ,
    CLIENT_CONN_STATE_WRITE,
    CLIENT_CONN_STATE_READWRITE,
    CLIENT_CONN_STATE_PENDINGCLOSE
}CLIENT_CONN_STATE;

typedef struct ClientConnection{
    int sockFd;
    
    //int logFd;
    
    int idx;
    int thIdx;
    CLIENT_CONN_STATE curState;
    CLIENT_CONN_JOB_STATE jobState;
    
    struct ClientConnection *next;
    
    ECMessageBuffer_t *readMsgBuf;
    ECMessageBuffer_t *writeMsgBuf;
}ClientConnection_t;

ClientConnection_t *createClientConn(int sockFd);

int createLogFd(int sockFd);
void deallocClientConn(ClientConnection_t *clientConnPtr);
void addWriteBuf(ClientConnection_t *clientConn, char *buf, size_t bufSize);

#endif /* defined(__ECMeta__ClientConnection__) */
