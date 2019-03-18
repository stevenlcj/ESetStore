//
//  metaServerBlockServerProtocol.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/8/25.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "metaServerBlockServerProtocol.h"

#include <string.h>

const char HeartBeatReply[] = "HeartBeatOK\r\n\0";
const char recoveryNodesOKReplyHeader[] = "RecoveryNodesOK:\0";
const char recoveryNodesFailedReplyHeader[] = "RecoveryNodesFailed:\0";
const char recoveryBlockGroupOKCmdHeader[] = "RecoveredBlockGroup\r\n\0";

const char deleteBlockOKCmdHeader[] = "DeleteOK\r\n\0";
const char deleteBlockFailedCmdHeader[] = "DeleteFailed\r\n\0";

MetaServerCommand_t parseBlockServerReplyCommand(char *buf, int *cmdLen){
    if (strncmp(HeartBeatReply, buf, strlen(HeartBeatReply)) == 0) {
        *cmdLen = (int ) strlen(HeartBeatReply);
        return MetaServerCommand_HeartBeatReply;
    }

    if (strncmp(deleteBlockOKCmdHeader, buf, strlen(deleteBlockOKCmdHeader)) == 0) {
        *cmdLen = (int ) strlen(deleteBlockOKCmdHeader);
        return MetaServerCommand_DeleteBlockOKReply;
    }

    if (strncmp(deleteBlockFailedCmdHeader, buf, strlen(deleteBlockFailedCmdHeader)) == 0) {
        *cmdLen = (int ) strlen(deleteBlockFailedCmdHeader);
        return MetaServerCommand_DeleteBlockFailedReply;
    }
    
    if (strncmp(recoveryNodesOKReplyHeader, buf, strlen(recoveryNodesOKReplyHeader)) == 0) {
        *cmdLen = (int ) strlen(recoveryNodesOKReplyHeader);
        return MetaServerCommand_RecoveryNodesOKReply;
    }
    
    if (strncmp(recoveryNodesFailedReplyHeader, buf, strlen(recoveryNodesFailedReplyHeader)) == 0) {
        *cmdLen = (int ) strlen(recoveryNodesFailedReplyHeader);
        return MetaServerCommand_RecoveryNodesFAILEDReply;
    }
    
    if (strncmp(recoveryBlockGroupOKCmdHeader, buf, strlen(recoveryBlockGroupOKCmdHeader)) == 0) {
        *cmdLen = (int ) strlen(recoveryBlockGroupOKCmdHeader);
        return MetaServerCommand_RecoveryBlockOKReply;
    }


    return MetaServerCommand_UnknowReply;
}