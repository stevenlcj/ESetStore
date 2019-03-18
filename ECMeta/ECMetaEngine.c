//
//  ECMetaEngine.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/7/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECMetaEngine.h"
#include "ecCommon.h"
#include "ecNetHandle.h"
#include "BlockServersManagement.h"
#include "jerasure.h"
#include "cauchy.h"

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

/* Catch Signal Handler functio */
void ec_signal_callback_handler(int signum){
    printf("Caught signal SIGPIPE %d\n",signum);
}

void mainLoop(struct ECMetaServer *ecMetaServer){
    while (ecMetaServer->exitFlag == 0) {
        start_wait_conn(ecMetaServer);
    }
}

void startBindListen(struct ECMetaServer *ecMetaServer){
    ecMetaServer->efd = create_epollfd();
    
    ecMetaServer->chunkFd = create_bind_listen(ecMetaServer->chunkServerPort);
//    printf("Chunk fd:%d Chunk port:%d\n",ecMetaServer->chunkFd ,ecMetaServer->chunkServerPort );
    ecMetaServer->clientFd = create_bind_listen(ecMetaServer->clientPort);
//    printf("Client fd:%d Client port:%d\n",ecMetaServer->clientFd ,ecMetaServer->clientPort);
}

void createMatrix(struct ECMetaServer *ecMetaServer){
    int *matrix;
    matrix = cauchy_good_general_coding_matrix(ecMetaServer->stripeK, ecMetaServer->stripeM, ecMetaServer->stripeW);
    ecMetaServer->matrix = matrix;
}

void setECProperties(struct ECMetaServer *ecMetaServer){
    ecMetaServer->exitFlag = 0;

    ecMetaServer->chunkServerPort = getIntValue(ecMetaServer->cfg, "metaServer.chunkServerPort");
    
    if (ecMetaServer->chunkServerPort == -1) {
        ecMetaServer->chunkServerPort = DEFAULT_CHUNK_SERVER_PORT;
    }
    
    ecMetaServer->clientPort = getIntValue(ecMetaServer->cfg, "metaServer.clientPort");
    if (ecMetaServer->clientPort == -1) {
        ecMetaServer->clientPort = DEFAULT_CLIENT_PORT;
    }
    
    ecMetaServer->stripeK = getIntValue(ecMetaServer->cfg, "metaServer.StripeK");
    ecMetaServer->stripeM = getIntValue(ecMetaServer->cfg, "metaServer.StripeM");
    ecMetaServer->stripeW = getIntValue(ecMetaServer->cfg, "metaServer.StripeW");
    ecMetaServer->streamingSize = getIntValue(ecMetaServer->cfg, "metaServer.streamingSize");
    ecMetaServer->blockSizeLimitsInMB = getIntValue(ecMetaServer->cfg, "metaServer.blockSizeLimitsInMB");
    ecMetaServer->overlappingFactor = getIntValue(ecMetaServer->cfg, "metaServer.overlappingFactor");

    createMatrix(ecMetaServer);
    
    ecMetaServer->blockServerHeartBeatTimeInterval = getIntValue(ecMetaServer->cfg, "metaServer.heartBeatIntervalInMS");
    ecMetaServer->blockServerStartRecoveryInterval = getIntValue(ecMetaServer->cfg, "metaServer.startRecoveryIntervalInSec");
    
    ecMetaServer->connClientTotalNum = MAX_CLIENT_CONN_NUM;
    ecMetaServer->connChunkTotalNum = MAX_CHUNK_CONN_NUM;
    
    
    ecMetaServer->recoverInterval = getIntValue(ecMetaServer->cfg, "metaServer.recoveryIntervalInSec");
    printf("recoverInterval:%d\n",ecMetaServer->recoverInterval);
    if(ecMetaServer->recoverInterval == -1 ){
        ecMetaServer->recoverInterval = DEFAULT_RECOVERY_TIME_INTERVAL;
    }
    ecMetaServer->serverMarkFailedInterval = getIntValue(ecMetaServer->cfg, "metaServer.markServerFailedInterval");

    printf("clientPort:%d serverPort:%d\n",ecMetaServer->clientPort, ecMetaServer->chunkServerPort);
    printf("stripe k:%d m:%d w:%d, streamingSize:%d \n",ecMetaServer->stripeK, ecMetaServer->stripeM, ecMetaServer->stripeW, ecMetaServer->streamingSize);
    
//    ecMetaServer->ecSlabMgr = new_ECSlab_Manager();
    
    ecMetaServer->blockServerMgr = createBlockServerManager((void *) ecMetaServer);
    
    //client part listen on 0,  server part listen on 1.
    if(pipe(ecMetaServer->serverPipes) != 0){
        perror("pipe(ecMetaServer->serverPipes) failed");
    }

    pthread_t pid;
    pthread_create(&pid, NULL, startBlockServersManagement, (void *)ecMetaServer);
    
    ecMetaServer->placementMgr = createPlacementMgr(ecMetaServer->rMap, ecMetaServer->stripeK + ecMetaServer->stripeM, ecMetaServer->overlappingFactor);
    
    ecMetaServer->keyValueEnginePtr = createKeyValueEngine((void *)ecMetaServer);
    ecMetaServer->ecClientThreadMgr = createClientThreadManager((void *)ecMetaServer);
    
    ecMetaServer->ecRecoveryMgr = createECRecoveryManager((void *)ecMetaServer);
}

void startMeta(const char *cfgFile){
    struct ECMetaServer ecMetaServer;
    
//    assert(cfgFile != NULL);
    ecMetaServer.cfg = loadProperties(cfgFile);
    ecMetaServer.rMap = initRMap(cfgFile);
    
    setECProperties(&ecMetaServer);
    
//    return;
    //IGNORE SIGPIPE
    signal(SIGPIPE, ec_signal_callback_handler);

    startBindListen(&ecMetaServer);
    mainLoop(&ecMetaServer);
}