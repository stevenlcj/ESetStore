//
//  ECClientHandle.h
//  ECServer
//
//  Created by Liu Chengjian on 18/4/16.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ECServer__ECClientHandle__
#define __ECServer__ECClientHandle__

#include "ClientHandle.h"

int processClientInCmd(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr);

int processClientInMsg(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr, size_t *readSize);

int processClientOutMsg(ECClientManager_t *ecClientMgr, ECClient_t *ecClientPtr, size_t *writeSize);

int writeClientReadContent(ECClientManager_t *ecClientMgr);
#endif
