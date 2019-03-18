//
//  BlockServerEngine.c
//  ECServer
//
//  Created by Liu Chengjian on 17/8/9.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "BlockServerEngine.h"
#include "cfgParser.h"
#include "ecNetHandle.h"
#include "ECMetaServerHandle.h"

#include <string.h>
#include <unistd.h>

void startBindAndListen(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    ecBlockServerEnginePtr->eClientfd = create_epollfd();
    ecBlockServerEnginePtr->clientFd = create_bind_listen(ecBlockServerEnginePtr->chunkServerPort);
    ecBlockServerEnginePtr->metaFd = create_TCP_sockFd();
}

void startNetIO(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    startBindAndListen(ecBlockServerEnginePtr);
    
    startMetaHandle(ecBlockServerEnginePtr);
    
    start_wait_conn(ecBlockServerEnginePtr);
}

void setECProperties(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    char *metaServerIP = getStrValue(ecBlockServerEnginePtr->cfg, "chunkServer.MetaServerIP");
    ecBlockServerEnginePtr->metaServerIPSize = (int) strlen(metaServerIP);
    memcpy(ecBlockServerEnginePtr->metaServerIP, metaServerIP, ecBlockServerEnginePtr->metaServerIPSize);
    ecBlockServerEnginePtr->metaServerIP[ecBlockServerEnginePtr->metaServerIPSize] = '\0';
    ecBlockServerEnginePtr->metaServerPort = getIntValue(ecBlockServerEnginePtr->cfg, "chunkServer.MetaServerPort");
    
    char *chunkServerIP = getStrValue(ecBlockServerEnginePtr->cfg, "chunkServer.ChunkServerIP");
    ecBlockServerEnginePtr->chunkServerIPSize = (int) strlen(chunkServerIP);
    memcpy(ecBlockServerEnginePtr->chunkServerIP, chunkServerIP, ecBlockServerEnginePtr->chunkServerIPSize);
    ecBlockServerEnginePtr->chunkServerIP[ecBlockServerEnginePtr->chunkServerIPSize] = '\0';
    ecBlockServerEnginePtr->chunkServerPort = getIntValue(ecBlockServerEnginePtr->cfg, "chunkServer.ChunkServerPort");
    
    ecBlockServerEnginePtr->recoveryBufSize = getIntValue(ecBlockServerEnginePtr->cfg, "chunkServer.recoveryBufSizeInKB") * 1024;
    
    ecBlockServerEnginePtr->recoveryBufNum = getIntValue(ecBlockServerEnginePtr->cfg, "chunkServer.recoveryBufNum");
    
    printf("recoveryBufSize:%d\n", ecBlockServerEnginePtr->recoveryBufSize);
    
    if (ecBlockServerEnginePtr->recoveryBufNum == -1) {
        ecBlockServerEnginePtr->recoveryBufNum = DEFAULT_RECOVERY_BUF_NUM;
    }
    
    printf("recoveryBufNum:%d\n",ecBlockServerEnginePtr->recoveryBufNum);
    
    ecBlockServerEnginePtr->exitFlag = 0;
    
    ecBlockServerEnginePtr->diskIOMgr = createDiskIOMgr(ecBlockServerEnginePtr);
    
    ecBlockServerEnginePtr->ecClientMgr = createClientManager((void *)ecBlockServerEnginePtr);
    startECClientManager(ecBlockServerEnginePtr->ecClientMgr);
    
    ecBlockServerEnginePtr->recoveryThMgr = NULL;
    
}

void startBlockServer(const char *cfgFile){
    ECBlockServerEngine_t ecBlockServerEngine;
    
    ecBlockServerEngine.cfg = loadProperties(cfgFile);
    
    ecBlockServerEngine.readMsgBuf = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);
    ecBlockServerEngine.writeMsgBuf = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);
    
    if (pipe(ecBlockServerEngine.serverRecoveryPipes) != 0){
        fprintf (stderr, "Create serverRecoveryPipes failed.\n");
    }
    
    make_non_blocking_sock(ecBlockServerEngine.serverRecoveryPipes[0]);
    
    setECProperties(&ecBlockServerEngine);
    
    startNetIO(&ecBlockServerEngine);
}

size_t addWriteMsgToMetaServer(ECBlockServerEngine_t *blockServerEnginePtr, char *msgBuf, size_t bufMsgSize){
    
    if (blockServerEnginePtr == NULL) {
        return 0;
    }
    
    ECMessageBuffer_t *writeMsgBufPtr = blockServerEnginePtr->writeMsgBuf;
    
    while (writeMsgBufPtr->next != NULL) {
        writeMsgBufPtr = writeMsgBufPtr->next;
    }
    
    size_t cpedSize = 0, cpSize = 0;
    

    while (cpedSize != bufMsgSize) {
        if (writeMsgBufPtr->wOffset == writeMsgBufPtr->bufSize) {
            writeMsgBufPtr->next = createMessageBuf(DEFAULT_READ_WRITE_BUF_SIZE);
            writeMsgBufPtr = writeMsgBufPtr->next;
        }
        
        cpSize = (writeMsgBufPtr->bufSize -  writeMsgBufPtr->wOffset) > (bufMsgSize - cpedSize) ? (bufMsgSize - cpedSize) : (writeMsgBufPtr->bufSize - writeMsgBufPtr->wOffset);
        
        //printf("before addWrite cpSize:%lu wOffset:%lu rOffset:%lu bufMsgSize:%lu\n", cpSize,writeMsgBufPtr->wOffset, 
        //                                                                                writeMsgBufPtr->rOffset,bufMsgSize);
        memcpy((writeMsgBufPtr->buf + writeMsgBufPtr->wOffset), (msgBuf + cpedSize), cpSize);
        writeMsgBufPtr->wOffset = writeMsgBufPtr->wOffset + cpSize;
        cpedSize = cpedSize + cpSize;

        //printf("after addWrite cpedSize:%lu wOffset:%lu rOffset:%lu bufMsgSize:%lu\n", cpedSize,writeMsgBufPtr->wOffset, 
        //                                                                                writeMsgBufPtr->rOffset,bufMsgSize);
    }
    
    return cpedSize;
}