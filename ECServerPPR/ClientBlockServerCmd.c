//
//  ClientBlockServerCmd.c
//  ECServer
//
//  Created by Liu Chengjian on 17/10/13.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ClientBlockServerCmd.h"
#include "MessageBuffer.h"

#include <string.h>

const char BlockCreate[] = "BlockCreate\r\n\0"; 
const char BlockWrite[] = "BlockWrite\r\n\0";
const char BlockOpen[] = "BlockOpen\r\n\0";
const char BlockRead[] = "BlockRead\r\n\0";
const char BlockAppend[] = "BlockAppend\r\n\0";
const char BlockClose[] = "BlockClose\r\n\0";
const char WriteSize[] = "WriteSize:\0";
const char WriteOver[] = "WriteOver\0";
const char ReadSize[] = "ReadSize:\0";
const char ReadOver[] = "ReadOver\0";

const char InCmdOK[] = "OK\r\n\0";
const char InCmdFAILED[] = "FAILED\r\n\0";

/**
 a. BlockWrite/r/nBlockId:blockId\r\n\0
 b. BlockRead/r/nBlockId:blockId\r\n\0
 c. BlockAppend/r/nBlockId:blockId\r\n\0
 */

ECCLIENT_IN_STATE_t parseClientInCmd(ECClient_t *ecClientPtr){
    ECMessageBuffer_t *ecMsgBuf = ecClientPtr->readMsgBuf;
    
    if (strncmp(ecMsgBuf->buf, BlockCreate, strlen(BlockCreate)) == 0) {
        return ECCLIENT_IN_STATE_CREATE;
    }

    if (strncmp(ecMsgBuf->buf, BlockOpen, strlen(BlockOpen)) == 0) {
        return ECCLIENT_IN_STATE_OPEN;
    }
    
    if (strncmp(ecMsgBuf->buf, BlockWrite, strlen(BlockWrite)) == 0) {
        
        return ECCLIENT_IN_STATE_WRITING;
    }
    
    if (strncmp(ecMsgBuf->buf, BlockRead, strlen(BlockRead)) == 0) {
        return ECCLIENT_IN_STATE_READING;
    }
    
    if (strncmp(ecMsgBuf->buf, BlockClose, strlen(BlockClose)) == 0) {
        return ECCLIENT_IN_STATE_CLOSE;
    }

    return ECClient_IN_STATE_UNKNOW;
}
