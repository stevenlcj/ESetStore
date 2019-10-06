//
//  ECFileHandle.h
//  ECClient
//
//  Created by Liu Chengjian on 17/9/12.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECClient__ECFileHandle__
#define __ECClient__ECFileHandle__

#include <stdio.h>
#include <stdint.h>

typedef struct {
    int idx;
    uint64_t blockId;
    char IPAddr[16];
    size_t IPSize;
    int port;
    
    int serverStatus;
}blockInfo_t;


typedef enum{
    ECFILE_STATE_UNINIT = 0x01,
    ECFILE_STATE_INIT,
    ECFILE_STATE_CREATE,
    ECFILE_STATE_OPEN ,
    ECFILE_STATE_READ,
    ECFILE_STATE_WRITE,
    ECFILE_STATE_APPEND
}ECFILE_STATE_TYPE;

typedef struct {
    int initFlag;
    char *fileName;
    size_t fileNameSize;
    
    size_t stripeK;
    size_t stripeM;
    size_t stripeW;
    size_t streamingSize;
    
    size_t fileWriteSize;
    size_t sizePerBlock;
    
    int *matrix;
    
    int *mat_idx;
    int *dm_ids;
    int *decoding_matrix;
    
    size_t blockNum;
    blockInfo_t *blocks;
    
    void *blockWorkers;
    int blockWorkerNum;
    
    char *buf; //the buf from write, read call
    size_t bufSize;
    size_t bufHandledSize;
    size_t bufWritedSize;
      
    size_t fileSize;
    size_t readOffset;
    
    size_t writeDataSize;
    
    ECFILE_STATE_TYPE  fileCurState;
        
    int degradedReadFlag;
}ECFile_t;

typedef struct {
    
    ECFile_t *ecFiles;
    size_t *ecFilesIndicator;
    size_t ecFileSize;
    size_t ecFileNum;

    void *blockWorkersMgr;
    void *clientEnginePtr;
}ECFileManager_t;

ECFileManager_t *createECFileMgr(void *clientEnginePtr);
int createECFile(ECFileManager_t *ecFileMgr, const char *fileName);

ssize_t readECFile(ECFileManager_t *ecFileMgr, int ecFd, char *readBuf, size_t readSize);
ssize_t writeECFile(ECFileManager_t *ecFileMgr, int ecFd, char *writeBuf, size_t writeSize);

void initCreateFileFromMetaReply(ECFileManager_t *ecFileMgr, int fd, const char *replyBuf);
void initOpenFileFromMetaReply(ECFileManager_t *ecFileMgr, int fd);
void initReadFileFromMetaReply(ECFileManager_t *ecFileMgr, int fd, const char *replyBuf);

void deallocECFileMgr(ECFileManager_t *ecFileMgr);
int closeECFile(ECFileManager_t *ecFileMgr, int idx);

#endif /* defined(__ECClient__ECFileHandle__) */
