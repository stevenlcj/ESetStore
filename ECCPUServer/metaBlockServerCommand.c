//
//  metaBlockServerCommand.c
//  ECServer
//
//  Created by Liu Chengjian on 17/8/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "metaBlockServerCommand.h"
#include "ecCommon.h"
#include "ecUtils.h"
#include "BlockServerEngine.h"

#include <string.h>
#include <stdio.h>

const char heartBeatStr[] = "HeartBeat\r\n\0";
const char recoveryCmdHeader[] = "RecoveryNodes\r\n\0";

const char recoveryNodesOKReplyHeader[] = "RecoveryNodesOK:\0";
const char recoveryNodesFailedReplyHeader[] = "RecoveryNodesFailed:\0";
const char recoveryOverCmdHeader[] = "EndRecoveryNodes\r\n\0";
const char recoveryBlockGroupCmdHeader[] = "RecoveryBlockGroup\r\n\0";
const char recoveryBlockGroupOKCmdHeader[] = "RecoveredBlockGroup\r\n\0";
const char recoveryBlockIdHeader[] = "BlockId:\0";

const char deleteBlockCmd[] = "DeleteBlock\0";

const char ESetIdxHeader[] = "ESetIdx:\0";
const char suffixStr[] = "\r\n\0";

const char deleteOKHeader[] = "DeleteOK\0";
const char deleteFailedHeader[] = "DeleteFailed\0";
const char metaBlockIdStr[] = "BlockId:\0";

MetaServerCommand_t parseMetaCommand(char *buf, size_t bufLen){
    
    if ((bufLen >= strlen(heartBeatStr) ) &&
        strncmp(heartBeatStr, buf, strlen(heartBeatStr)) == 0) {
        return MetaServerCommand_HeartBeat;
    }

    if ((bufLen >= strlen(deleteBlockCmd) ) &&
        strncmp(deleteBlockCmd, buf, strlen(deleteBlockCmd)) == 0) {
        return MetaServerCommand_DeleteBlock;
    }
    
    if ((bufLen >= strlen(recoveryCmdHeader) ) &&
        strncmp(recoveryCmdHeader, buf, strlen(recoveryCmdHeader)) == 0) {
        return MetaServerCommand_RecoverNodes;
    }
    
    if ((bufLen >= strlen(recoveryOverCmdHeader) ) &&
        strncmp(recoveryOverCmdHeader, buf, strlen(recoveryOverCmdHeader)) == 0) {
        return MetaServerCommand_RecoverNodesOver;
    }
    
    if ((bufLen >= strlen(recoveryBlockGroupCmdHeader) ) &&
        strncmp(recoveryBlockGroupCmdHeader, buf, strlen(recoveryBlockGroupCmdHeader)) == 0) {
        return MetaServerCommand_RecoverBlock;
    }
    
    return MetaServerCommand_Unknow;
}

size_t writeRecoveryNodesOKReplyCmd(int setIdx , void *ecBlockServerEnginePtr){
    size_t writeBackOffset = 0, curWriteSize = 0;
    int bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    char tempBuf[bufSize];
    char bufToWrite[bufSize];
    
    writeToBuf(bufToWrite, (void *)recoveryNodesOKReplyHeader, &curWriteSize, &writeBackOffset);
    int_to_str(setIdx, tempBuf, DEFAULT_READ_WRITE_BUF_SIZE);
    writeToBuf(bufToWrite, (void *)tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (void *)suffixStr, &curWriteSize, &writeBackOffset);
    
    bufToWrite[writeBackOffset] = '\0';
    writeBackOffset = writeBackOffset + 1;
    addWriteMsgToMetaServer((ECBlockServerEngine_t *)ecBlockServerEnginePtr, bufToWrite, writeBackOffset);

//    char *cmd = talloc(char, (writeBackOffset + 1));
//    memcpy(cmd, bufToWrite, writeBackOffset);
//    
//    *(cmd + writeBackOffset) = '\0';
    
    printf("writeRecoveryNodesReplyCmd**********:%s\n",bufToWrite);
    
    return writeBackOffset;
}

size_t writeRecoveryNodesFailedReplyCmd(int setIdx, void *ecBlockServerEnginePtr){
    size_t writeBackOffset = 0, curWriteSize = 0;
    int bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    char tempBuf[bufSize];
    char bufToWrite[bufSize];
    
    writeToBuf(bufToWrite, (void *)recoveryNodesFailedReplyHeader, &curWriteSize, &writeBackOffset);
    int_to_str(setIdx, tempBuf, DEFAULT_READ_WRITE_BUF_SIZE);
    writeToBuf(bufToWrite, (void *)tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (void *)suffixStr, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (void *)recoveryNodesFailedReplyHeader, &curWriteSize, &writeBackOffset);
    
    bufToWrite[writeBackOffset] = '\0';
    writeBackOffset = writeBackOffset + 1;
    addWriteMsgToMetaServer((ECBlockServerEngine_t *)ecBlockServerEnginePtr, bufToWrite, writeBackOffset);

//    char *cmd = talloc(char, (writeBackOffset + 1));
//    memcpy(cmd, bufToWrite, writeBackOffset);
//    
//    *(cmd + writeBackOffset) = '\0';
    
    printf("writeRecoveryNodesReplyCmd**********:%s\n",bufToWrite);
    
    return writeBackOffset;
}

/*
 RecoveredBlockGroup\r\n
 ESetIdx:__\r\n
 BlockId:__\r\n\0
 **/
size_t writeRecoveryBlockOverCmd(int setIdx, uint64_t blockIdx, void *ecBlockServerEnginePtr){
    size_t writeBackOffset = 0, curWriteSize = 0;
    int bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    char tempBuf[bufSize];
    char bufToWrite[bufSize];
    
    writeToBuf(bufToWrite, (void *)recoveryBlockGroupOKCmdHeader, &curWriteSize, &writeBackOffset);
    
    writeToBuf(bufToWrite, (void *)ESetIdxHeader, &curWriteSize, &writeBackOffset);
    int_to_str(setIdx, tempBuf, DEFAULT_READ_WRITE_BUF_SIZE);
    writeToBuf(bufToWrite, (void *)tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (void *)suffixStr, &curWriteSize, &writeBackOffset);
        
    writeToBuf(bufToWrite, (void *)recoveryBlockIdHeader, &curWriteSize, &writeBackOffset);
    uint64_to_str(blockIdx, tempBuf, DEFAULT_READ_WRITE_BUF_SIZE);
    writeToBuf(bufToWrite, (void *)tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (void *)suffixStr, &curWriteSize, &writeBackOffset);
    
    
    bufToWrite[writeBackOffset] = '\0';
    writeBackOffset = writeBackOffset + 1;
    addWriteMsgToMetaServer((ECBlockServerEngine_t *)ecBlockServerEnginePtr, bufToWrite, writeBackOffset);

//    char *cmd = talloc(char, (writeBackOffset + 1));
//    memcpy(cmd, bufToWrite, writeBackOffset);
//    
//    *(cmd + writeBackOffset) = '\0';
    
    printf("writeRecoveryBlockOverCmd**********:%s\n",bufToWrite);
    
    return writeBackOffset;
    
}

//DeleteOK\r\nBlockId:__\r\n\0
size_t writeDeleteOKCmd(uint64_t blockIdx,void *ecBlockServerEnginePtr){
    size_t writeBackOffset = 0, curWriteSize = 0;
    int bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    char tempBuf[bufSize];
    char bufToWrite[bufSize];

    writeToBuf(bufToWrite, (void *)deleteOKHeader, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (void *)suffixStr, &curWriteSize, &writeBackOffset);

    writeToBuf(bufToWrite, (void *)metaBlockIdStr, &curWriteSize, &writeBackOffset);
    uint64_to_str(blockIdx, tempBuf, DEFAULT_READ_WRITE_BUF_SIZE);
    writeToBuf(bufToWrite, (void *)tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (void *)suffixStr, &curWriteSize, &writeBackOffset);

    bufToWrite[writeBackOffset] = '\0';
    writeBackOffset = writeBackOffset + 1;
    addWriteMsgToMetaServer((ECBlockServerEngine_t *)ecBlockServerEnginePtr, bufToWrite, writeBackOffset);
    
    printf("writeDeleteOKCmd:%s\n",bufToWrite);    
    return writeBackOffset;
}

//DeleteFailed\r\nBlockId:__\r\n\0
size_t writeDeleteFailedCmd(uint64_t blockIdx,void *ecBlockServerEnginePtr){
    size_t writeBackOffset = 0, curWriteSize = 0;
    int bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    char tempBuf[bufSize];
    char bufToWrite[bufSize];

    writeToBuf(bufToWrite, (void *)deleteFailedHeader, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (void *)suffixStr, &curWriteSize, &writeBackOffset);

    writeToBuf(bufToWrite, (void *)metaBlockIdStr, &curWriteSize, &writeBackOffset);
    uint64_to_str(blockIdx, tempBuf, DEFAULT_READ_WRITE_BUF_SIZE);
    writeToBuf(bufToWrite, (void *)tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (void *)suffixStr, &curWriteSize, &writeBackOffset);

    bufToWrite[writeBackOffset] = '\0';
    writeBackOffset = writeBackOffset + 1;
    addWriteMsgToMetaServer((ECBlockServerEngine_t *)ecBlockServerEnginePtr, bufToWrite, writeBackOffset);
    
    printf("writeDeleteFailedCmd:%s\n",bufToWrite);
    return writeBackOffset;

}
