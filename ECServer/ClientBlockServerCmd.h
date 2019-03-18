//
//  ClientBlockServerCmd.h
//  ECServer
//
//  Created by Liu Chengjian on 17/10/13.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECServer__ClientBlockServerCmd__
#define __ECServer__ClientBlockServerCmd__

#include <stdio.h>
#include "ClientHandle.h"

ECCLIENT_IN_STATE_t parseClientInCmd(ECClient_t *ecClientPtr);

ECCLIENT_IN_STATE_t parseClientStartWriteCmd(ECClient_t *ecClientPtr);
ECCLIENT_IN_STATE_t parseClientStartAppendCmd(ECClient_t *ecClientPtr);
ECCLIENT_IN_STATE_t parseClientStartReadCmd(ECClient_t *ecClientPtr);

#endif /* defined(__ECServer__ClientBlockServerCmd__) */
