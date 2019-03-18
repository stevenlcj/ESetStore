//
//  ECClientCmdHandle.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/11/1.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__ECClientCmdHandle__
#define __ECMeta__ECClientCmdHandle__

#include <stdio.h>

#include "ECClientCommand.h"
#include "ECClientManagement.h"
#include "ECJob.h"

void processRecvdCmd(ECClientThread_t *clientThreadPtr, ClientConnection_t *clientConnPtr, KEYVALUEJOB_t keyValueJobType);


#endif /* defined(__ECMeta__ECClientCmdHandle__) */
