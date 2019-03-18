//
//  ECClientEngine.h
//  ECClient
//
//  Created by Liu Chengjian on 17/9/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECClient__ECClientEngine__
#define __ECClient__ECClientEngine__

#include <stdio.h>

#include "ECFileHandle.h"

typedef struct {
    char metaIP[16];
    size_t metaIPSize;
    
    int metaPort;
    
    int metaSockFd;
    
    ECFileManager_t *ecFileMgr;
}ECClientEngine_t;

ECClientEngine_t *createECClientEngine(const char *metaIP, int metaPort);

//return file description if succeeded, else -1
int createFile(ECClientEngine_t *clientEnginePtr, const char *FileName);

//return file description if succeeded, else -1
int openFile(ECClientEngine_t *clientEnginePtr, const char *FileName);

size_t getFileStripeK(ECClientEngine_t *clientEnginePtr, int fileFd);
size_t getFileStripeM(ECClientEngine_t *clientEnginePtr, int fileFd);
size_t getFileStripeW(ECClientEngine_t *clientEnginePtr, int fileFd);
size_t getFileStreamingSize(ECClientEngine_t *clientEnginePtr, int fileFd);
size_t getFileSize(ECClientEngine_t *clientEnginePtr, int fileFd);

ssize_t readFile(ECClientEngine_t *clientEnginePtr, int fileFd, char *buf, size_t readSize);
ssize_t writeFile(ECClientEngine_t *clientEnginePtr, int fileFd, char *buf, size_t writeSize);

ssize_t writeCmdToMeta(ECClientEngine_t *clientEnginePtr, char *cmd);
void closeFile(ECClientEngine_t *clientEnginePtr, int fileFd);
int deleteFile(ECClientEngine_t *clientEnginePtr, const char *FileName);
int deleteFiles(ECClientEngine_t *clientEnginePtr, char *filesToDelete[], int fileNum);

void deallocECClientEngine(ECClientEngine_t * clientEnginePtr);
#endif /* defined(__ECClient__ECClientEngine__) */
