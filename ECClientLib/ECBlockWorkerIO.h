//
//  ECBlockWorkerIO.h
//  ECClient
//
//  Created by Liu Chengjian on 18/4/12.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ECCLIENT_ECBlockWorkerIO_HEADER__
#define __ECCLIENT_ECBlockWorkerIO_HEADER__

#include "ECBlockWorker.h"
void *threadBlockWorker(void *arg);

int openBlocksForWrite(ECBlockWorkerManager_t *ecBlockWorkerMgr);
int openBlocksForRead(ECBlockWorkerManager_t *ecBlockWorkerMgr);
void processingMonitoringRq(ECBlockWorkerManager_t *ecBlockWorkerMgr);
int closeBlockInServer(ECBlockWorkerManager_t *ecBlockWorkerMgr, ECBlockWorker_t *blockWorker);
#endif