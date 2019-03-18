//
//  metaBlockServerCommand.h
//  ECServer
//
//  Created by Liu Chengjian on 17/8/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECServer__metaBlockServerCommand__
#define __ECServer__metaBlockServerCommand__

#include <stdio.h>
#include <stdint.h>

typedef enum {
    MetaServerCommand_HeartBeat = 0x01,
    MetaServerCommand_CreateBlock = 0x02,
    MetaServerCommand_DeleteBlock = 0x03,
    MetaServerCommand_RecoverNodes = 0x04,
    MetaServerCommand_RecoverNodesOver = 0x05,
    MetaServerCommand_RecoverNodesAbort = 0x06,
    MetaServerCommand_RecoverBlock = 0x07,
    MetaServerCommand_Unknow
}MetaServerCommand_t;

MetaServerCommand_t parseMetaCommand(char *buf, size_t bufLen);

size_t writeRecoveryNodesOKReplyCmd(int setIdx, void *ecBlockServerEnginePtr);
size_t writeRecoveryNodesFailedReplyCmd(int setIdx, void *ecBlockServerEnginePtr);
size_t writeRecoveryBlockOverCmd(int setIdx, uint64_t blockIdx,void *ecBlockServerEnginePtr);

size_t writeDeleteOKCmd(uint64_t blockIdx,void *ecBlockServerEnginePtr);
size_t writeDeleteFailedCmd(uint64_t blockIdx,void *ecBlockServerEnginePtr);
#endif /* defined(__ECServer__metaBlockServerCommand__) */
