//
//  metaServerBlockServerProtocol.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/8/25.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__metaServerBlockServerProtocol__
#define __ECMeta__metaServerBlockServerProtocol__

#include <stdio.h>

typedef enum {
    MetaServerCommand_HeartBeatReply = 0x01,
    MetaServerCommand_CreateBlockReply = 0x02,
    MetaServerCommand_DeleteBlockReply = 0x03,
    MetaServerCommand_RecoveryNodesOKReply = 0x04,
    MetaServerCommand_RecoveryNodesFAILEDReply = 0x05,
    MetaServerCommand_RecoveryBlockOKReply = 0x06,
    MetaServerCommand_RecoveryBlocKFailedReply = 0x07,
    MetaServerCommand_DeleteBlockOKReply,
    MetaServerCommand_DeleteBlockFailedReply,
    MetaServerCommand_UnknowReply
}MetaServerCommand_t;

MetaServerCommand_t parseBlockServerReplyCommand(char *buf, int *cmdLen);

#endif /* defined(__ECMeta__metaServerBlockServerProtocol__) */
