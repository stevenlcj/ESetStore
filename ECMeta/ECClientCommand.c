//
//  ECClientCommand.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include "ECClientCommand.h"

//Client to meta Cmd:
/**
 Create:
    Create:
 */

const char createCmd[] = "Create\r\n\0";
const char openCmd[] = "Open\r\n\0";
const char readCmd[] = "Read\r\n\0";
const char writeCmd[] = "Write\r\n\0";
const char writeOverCmd[] = "WriteOver\0";
const char deleteCmd[] = "Delete\r\n\0";
const char closingCmd[] = "Closing\r\n\0";
ECClientCommand_t parseClientCmd(char *parseBuf,size_t *cmdSize){
    if (strncmp(closingCmd, parseBuf, strlen(closingCmd)) == 0) {
        *cmdSize = strlen(closingCmd);
        return ECClientCommand_CLOSING;
    }
    
    if (strncmp(createCmd, parseBuf, strlen(createCmd)) == 0) {
        *cmdSize = strlen(createCmd);
        return ECClientCommand_CREATE;
    }
    
    if (strncmp(openCmd, parseBuf, strlen(openCmd)) == 0) {
        *cmdSize = strlen(openCmd);
        return ECClientCommand_OPEN;
    }
    
    if (strncmp(readCmd, parseBuf, strlen(readCmd)) == 0) {
        *cmdSize = strlen(createCmd);
        return ECClientCommand_READ;
    }
    
    if (strncmp(writeOverCmd, parseBuf, strlen(writeOverCmd)) == 0) {
        return ECClientCommand_WRITEOVER;
    }

    if (strncmp(writeCmd, parseBuf, strlen(writeCmd)) == 0) {
        *cmdSize = strlen(writeCmd);
        return ECClientCommand_WRITE;
    }
    
    if (strncmp(deleteCmd, parseBuf, strlen(deleteCmd)) == 0) {
        *cmdSize = strlen(deleteCmd);
        return ECClientCommand_DELETE;
    }
    
    return ECClientCommand_UNKNOWN;

}