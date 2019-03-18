//
//  PPREngine.h
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/7.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ESetStorePPR__PPREngine__
#define __ESetStorePPR__PPREngine__

#include <stdio.h>
#include <pthread.h>

typedef struct{
    pthread_t pid;
    int efd;
    int listenFd;
    int PPRPort;
    
    int bufNum;
    size_t bufSize;
    
    void *serverEnginePtr;
}PPREngine_t;

PPREngine_t *createPPREngine(int PPRPort, int bufNum, size_t bufSize, void *serverEnginePtr);
#endif /* defined(__ESetStorePPR__PPREngine__) */
