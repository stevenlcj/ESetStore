//
//  ECMetaEngine.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/7/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__ECMetaEngine__
#define __ECMeta__ECMetaEngine__

#include <stdio.h>
#include "cfgParser.h"
#include "rackMap.h"
#include "ESetPlacement.h"
#include "BlockServer.h"

#include "KeyValueEngine.h"
#include "ECClientManagement.h"
#include "ECRecoveryManagement.h"

typedef struct ECMetaServer{
    struct configProperties *cfg;
    
    int efd; //epoll fd for monitoring incoming connections for chunk and client fd
    
    int chunkServerPort;
    int chunkFd;
    
    int clientPort;
    int clientFd;
    
    int stripeK;
    int stripeM;
    int stripeW;
    
    int *matrix;
    //int *bitmatrix;
    
    int streamingSize;
    
    int blockSizeLimitsInMB;
    
    int overlappingFactor;
    
    int stripePacketSizeInLong;
    int *stripeBitmatrix;
    
    int exitFlag;
    
    int blockServerHeartBeatTimeInterval;
    int blockServerStartRecoveryInterval;
    
//    size_t connEachSize;
    size_t connClientTotalNum;
    size_t connChunkTotalNum;
    
//    struct ECSlabManager * ecSlabMgr;
    
    struct rackMap *rMap;
    
    BlockServerManager_t *blockServerMgr;
    PlacementManager_t *placementMgr;
    ECClientThreadManager_t *ecClientThreadMgr;
    ECRecoveryManager_t *ecRecoveryMgr;
    
    KeyValueEngine_t *keyValueEnginePtr;
    int     serverPipes[2];
    
    int    recoverInterval;
    int    serverMarkFailedInterval;
}ECMetaServer_t;

void startMeta(const char *cfgFile);

#endif /* defined(__ECMeta__ECMetaEngine__) */
