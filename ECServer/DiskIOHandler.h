//
//  DiskIOHandler.h
//  ECServer
//
//  Created by Liu Chengjian on 17/10/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECServer__DiskIOHandler__
#define __ECServer__DiskIOHandler__

#include <stdio.h>
#include <pthread.h>
#include <stdint.h>

#include "ecCommon.h"
#include "DiskJobWorker.h"

typedef struct {
    char fileName[FILE_NAME_BUF_SIZE];
    char fileMetaName[FILE_NAME_BUF_SIZE];
    size_t fileNameSize;
    size_t fileSize;
    
    uint64_t fileCheckSum;
    uint64_t blockId;
    
    int fileFd;
    int fileMetaFd;
}DiskIO_t;

typedef struct {
    void *serverEngine;
    
    char *filePath;
    
    DiskIO_t *diskIOPtrs;
    int *diskIOIndicators;
    size_t diskIOSize;
    size_t diskIONum;
    size_t diskIORemainPtrs;
    
    pthread_mutex_t lock;
    
    DiskJobWorker_t *diskJobWorkerPtr;
}DiskIOManager_t;

DiskIOManager_t *createDiskIOMgr(void *serverEngine);

int startWriteFile(uint64_t fileId,DiskIOManager_t *diskIOMgr);
int startAppendFile(uint64_t fileId,DiskIOManager_t *diskIOMgr);
int startReadFile(uint64_t fileId,DiskIOManager_t *diskIOMgr);

ssize_t writeFile(int fd, char *buf, size_t writeSize, DiskIOManager_t *diskIOMgr);
ssize_t readFile(int fd, char *buf, size_t readSize, DiskIOManager_t *diskIOMgr);

void closeFile(int fd, DiskIOManager_t *diskIOMgr);

int deleteFile(uint64_t fileId,DiskIOManager_t *diskIOMgr);
#endif /* defined(__ECServer__DiskIOHandler__) */
