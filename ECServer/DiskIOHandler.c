//
//  DiskIOHandler.c
//  ECServer
//
//  Created by Liu Chengjian on 17/10/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "DiskIOHandler.h"
#include "ecUtils.h"
#include "BlockServerEngine.h"
#include "cfgParser.h"
#include "configProperties.h"

#include "ecCommon.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/stat.h>

const char fileSuffix[] = ".raw\0";
const char fileMetaSuffix[] = ".meta\0";

DiskIOManager_t *createDiskIOMgr(void *serverEngine){
    
    DiskIOManager_t *diskIOMgr = talloc(DiskIOManager_t, 1);
    diskIOMgr->serverEngine = serverEngine;
    diskIOMgr->diskIOSize = DISK_IO_PTR_SIZE;
    diskIOMgr->diskIOPtrs = talloc(DiskIO_t, diskIOMgr->diskIOSize);
    diskIOMgr->diskIOIndicators = talloc(int, diskIOMgr->diskIOSize);
    memset(diskIOMgr->diskIOPtrs, 0, sizeof(DiskIO_t)*diskIOMgr->diskIOSize);
    memset(diskIOMgr->diskIOIndicators, 0, sizeof(int)*diskIOMgr->diskIOSize);
    
    diskIOMgr->diskIONum = 0;
    
    pthread_mutex_init(&diskIOMgr->lock, NULL);
    
    ECBlockServerEngine_t *enginePtr = (ECBlockServerEngine_t *)serverEngine;
    
    diskIOMgr->filePath = getStrValue(enginePtr->cfg, "chunkServer.blockPath");
    
    diskIOMgr->diskJobWorkerPtr = createDiskJobWorker((void *)diskIOMgr);
    
    pthread_create(&(diskIOMgr->diskJobWorkerPtr->pid), NULL, startDiskJobWorker, (void *)diskIOMgr->diskJobWorkerPtr);
    return diskIOMgr;
}

int checkFileId(DiskIOManager_t *diskIOMgr, uint64_t fileId){
    int idx;
    for (idx = 0; idx < diskIOMgr->diskIOSize; ++idx) {
        if ((*(diskIOMgr->diskIOIndicators+idx) == 1) && (diskIOMgr->diskIOPtrs + idx)->blockId == fileId)  {
            return -1;
        }
    }
    
    return 0;
}

int getFileId(DiskIOManager_t *diskIOMgr, uint64_t fileId){
    int idx;
    for (idx = 0; idx < diskIOMgr->diskIOSize; ++idx) {
        if ((*(diskIOMgr->diskIOIndicators+idx) == 1) && (diskIOMgr->diskIOPtrs + idx)->blockId == fileId)  {
            return idx;
        }
    }
    
    return -1;
}

int initReadFileInfo(DiskIO_t *diskIOPtr, uint64_t fileId, char *dirPath, int sockFd){
    struct stat st; /*declare stat variable*/
         
    if(stat(absFilePath,&st)!=0){
        perror("stat file failed");
        return -1;
    }

    diskIOPtr->blockId = fileId;
    diskIOPtr->openedBySock = sockFd;
    diskIOPtr->fileNameSize = uint64_to_str(fileId, diskIOPtr->fileName, FILE_NAME_BUF_SIZE);
    
    diskIOPtr->fileSize = st.st_size;
    diskIOPtr->fileOffset = 0;

    size_t strSize = strlen(fileMetaSuffix);
    memcpy(diskIOPtr->fileMetaName, diskIOPtr->fileName, diskIOPtr->fileNameSize);
    memcpy((diskIOPtr->fileMetaName + diskIOPtr->fileNameSize), fileMetaSuffix, strSize);
    *(diskIOPtr->fileMetaName + diskIOPtr->fileNameSize + strSize) = '\0';
    
    strSize = strlen(fileSuffix);
    memcpy(diskIOPtr->fileName + diskIOPtr->fileNameSize, fileSuffix, strSize);
    diskIOPtr->fileNameSize = diskIOPtr->fileNameSize + strSize;
    *(diskIOPtr->fileName + diskIOPtr->fileNameSize) = '\0';
    
    diskIOPtr->fileSize = 0;
    diskIOPtr->fileCheckSum = 0;
    
    char absFilePath[1024];
    memcpy(absFilePath, dirPath, strlen(dirPath));
    memcpy((absFilePath + strlen(dirPath)), diskIOPtr->fileName, strlen(diskIOPtr->fileName));
    absFilePath[strlen(dirPath) + strlen(diskIOPtr->fileName)] = '\0';
    
    printf("Start open file:%s\n",absFilePath);
    
    if((diskIOPtr->fileFd = open(absFilePath, O_RDONLY)) == -1){
        perror("Open file failed");
        memset(diskIOPtr, 0, sizeof(DiskIO_t));
        
        return -1;
    }
     
    //    if ((diskIOPtr->fileMetaFd = open(diskIOPtr->fileMetaName, O_RDWR | O_CREAT)) == -1) {
    //        close(diskIOPtr->fileFd);
    //
    //        perror("Open file meta failed");
    //        memset(diskIOPtr, 0, sizeof(DiskIO_t));
    //
    //        return -1;
    //    }
    
    printf("fd: %d for read\n", diskIOPtr->fileFd);
    return 0;
}

//Form fileName:fileId.raw fileMetaName:fileId.Meta
int initCreateFileInfo(DiskIO_t *diskIOPtr, uint64_t fileId, char *dirPath, int sockFd){
    diskIOPtr->blockId = fileId;
    diskIOPtr->openedBySock = sockFd;
    diskIOPtr->fileNameSize = uint64_to_str(fileId, diskIOPtr->fileName, FILE_NAME_BUF_SIZE);
    
    size_t strSize = strlen(fileMetaSuffix);
    memcpy(diskIOPtr->fileMetaName, diskIOPtr->fileName, diskIOPtr->fileNameSize);
    memcpy((diskIOPtr->fileMetaName + diskIOPtr->fileNameSize), fileMetaSuffix, strSize);
    *(diskIOPtr->fileMetaName + diskIOPtr->fileNameSize + strSize) = '\0';
    
    strSize = strlen(fileSuffix);
    memcpy(diskIOPtr->fileName + diskIOPtr->fileNameSize, fileSuffix, strSize);
    diskIOPtr->fileNameSize = diskIOPtr->fileNameSize + strSize;
    *(diskIOPtr->fileName + diskIOPtr->fileNameSize) = '\0';
    
    diskIOPtr->fileSize = 0;
    diskIOPtr->fileCheckSum = 0;
    
    char absFilePath[1024];
    memcpy(absFilePath, dirPath, strlen(dirPath));
    memcpy((absFilePath + strlen(dirPath)), diskIOPtr->fileName, strlen(diskIOPtr->fileName));
    absFilePath[strlen(dirPath) + strlen(diskIOPtr->fileName)] = '\0';
    
    printf("Start open file:%s\n",absFilePath);
    
    if((diskIOPtr->fileFd = open(absFilePath, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1){
        perror("Open file failed");
        memset(diskIOPtr, 0, sizeof(DiskIO_t));
        
        return -1;
    }
    
    
//    printf("Opened file with fd:%d\n", diskIOPtr->fileFd);
    
//    if ((diskIOPtr->fileMetaFd = open(diskIOPtr->fileMetaName, O_RDWR | O_CREAT)) == -1) {
//        close(diskIOPtr->fileFd);
//        
//        perror("Open file meta failed");
//        memset(diskIOPtr, 0, sizeof(DiskIO_t));
//        
//        return -1;
//    }
    
    return 0;
}

int startWriteFile(uint64_t fileId, DiskIOManager_t *diskIOMgr, int sockFd){
    if (diskIOMgr->diskIOSize == diskIOMgr->diskIONum) {
        return -1;
    }
    
    if (checkFileId(diskIOMgr, fileId) == -1) {
        return -1;
    }
    
    pthread_mutex_lock(&diskIOMgr->lock);

    int idx;
    for (idx = 0; idx < diskIOMgr->diskIOSize; ++idx) {
        
        if (*(diskIOMgr->diskIOIndicators+idx) == 0) {
            
            if (initCreateFileInfo((diskIOMgr->diskIOPtrs + idx), fileId, diskIOMgr->filePath, sockFd) == -1) {
                pthread_mutex_unlock(&diskIOMgr->lock);
                return -1;
            }
            
            *(diskIOMgr->diskIOIndicators+idx) = 1;
            pthread_mutex_unlock(&diskIOMgr->lock);
            
            return idx;
        }
    }
    
    pthread_mutex_unlock(&diskIOMgr->lock);
    return -1;
}

int startAppendFile(uint64_t fileId,DiskIOManager_t *diskIOMgr, int sockFd){
    if (diskIOMgr->diskIOSize == diskIOMgr->diskIONum) {
        return -1;
    }
    
    if (checkFileId(diskIOMgr, fileId) == -1) {
        return -1;
    }
    
    pthread_mutex_lock(&diskIOMgr->lock);
    int idx;
    for (idx = 0; idx < diskIOMgr->diskIOSize; ++idx) {
    }
    pthread_mutex_unlock(&diskIOMgr->lock);

    return -1;
}

int startReadFile(uint64_t fileId,DiskIOManager_t *diskIOMgr, int sockFd){
    if (diskIOMgr->diskIOSize == diskIOMgr->diskIONum) {
        return -1;
    }
    
/*   if (checkFileId(diskIOMgr, fileId) == -1) {
        printf("checkFileId(diskIOMgr, fileId) == -1 is true\n");
        return -1;
    }*/
    
    pthread_mutex_lock(&diskIOMgr->lock);
    int idx;
    for (idx = 0; idx < diskIOMgr->diskIOSize; ++idx) {
        if (*(diskIOMgr->diskIOIndicators+idx) == 0) {
            if (initReadFileInfo((diskIOMgr->diskIOPtrs + idx), fileId, diskIOMgr->filePath, sockFd) == -1) {
                pthread_mutex_unlock(&diskIOMgr->lock);
                return -1;
            }
            *(diskIOMgr->diskIOIndicators+idx) = 1;
            pthread_mutex_unlock(&diskIOMgr->lock);
            return idx;
        }
    }
    pthread_mutex_unlock(&diskIOMgr->lock);
    
    return -1;
}


ssize_t writeFile(int fd, char *buf, size_t writeSize, DiskIOManager_t *diskIOMgr){
    if (fd < 0 || fd > diskIOMgr->diskIOSize) {
        printf("fd:%d, diskIOMgr->diskIOSize:%lu\n",fd, diskIOMgr->diskIOSize);
        return -1;
    }

    DiskIO_t *diskIOPtr = (diskIOMgr->diskIOPtrs + fd);
    
    if (diskIOPtr->fileFd <= 0) {
#ifdef EC_DEBUG_MODE
        printf("error: diskIOPtr->fileFd <= 0\n");
#endif
        return -1;
    }
    
    int fileFd = diskIOPtr->fileFd;
    ssize_t writedSize = write(fileFd, buf, writeSize);
    if (writedSize > 0) {
        diskIOPtr->fileSize =  diskIOPtr->fileSize + writedSize;
    }else{
        printf("writedSize:%ld, writeSize:%ld diskIOFd:%d fd:%d\n",writedSize, writeSize, fd, fileFd);
        perror("Cannot write\n");
    }

//#ifdef EC_DEBUG_MODE
//    printf("write size:%ld to file:%s\n",writedSize, diskIOPtr->fileName);
//#endif
    
    return writedSize;
}

ssize_t getFileSizeByFd(int fd, DiskIOManager_t *diskIOMgr){
    if (fd < 0 || fd > diskIOMgr->diskIOSize) {
        return -1;
    }
    
    DiskIO_t *diskIOPtr = (diskIOMgr->diskIOPtrs + fd);
    
    if (diskIOPtr->fileFd <= 0) {
        return -1;
    }
    
    return (ssize_t)diskIOPtr->fileSize;
}

ssize_t getFileOffsetByFd(int fd, DiskIOManager_t *diskIOMgr){
    if (fd < 0 || fd > diskIOMgr->diskIOSize) {
        return -1;
    }
    
    DiskIO_t *diskIOPtr = (diskIOMgr->diskIOPtrs + fd);
    
    if (diskIOPtr->fileFd <= 0) {
        return -1;
    }
    
    return (ssize_t)diskIOPtr->fileOffset;
}


ssize_t readFile(int fd, char *buf, size_t readSize, DiskIOManager_t *diskIOMgr){
    if (fd < 0 || fd > diskIOMgr->diskIOSize) {
        return -1;
    }
    
    DiskIO_t *diskIOPtr = (diskIOMgr->diskIOPtrs + fd);
    
    if (diskIOPtr->fileFd <= 0) {
        return -1;
    }
    
    int fileFd = diskIOPtr->fileFd;
    
//    printf("fd:%d for reading\n", fileFd);
    
    ssize_t readedSize = read(fileFd, buf, readSize);
    
    if (readedSize > 0) {
        diskIOPtr->fileOffset = diskIOPtr->fileOffset +readedSize;
    }
    
    return readedSize;
}

void closeFileBySockFd(int sockFd, DiskIOManager_t *diskIOMgr){
    pthread_mutex_lock(&diskIOMgr->lock);
    int idx;
    for (idx = 0; idx < diskIOMgr->diskIOSize; ++idx) {
        if (*(diskIOMgr->diskIOIndicators+idx) == 1) {
            DiskIO_t *diskIOPtr = diskIOMgr->diskIOPtrs+idx;
            if (diskIOPtr->openedBySock == sockFd) {
                closeFile(idx, diskIOMgr);
            }
        }
    }
    pthread_mutex_unlock(&diskIOMgr->lock);
}

void closeFile(int fd, DiskIOManager_t *diskIOMgr){
    if (fd < 0 || fd > diskIOMgr->diskIOSize) {
        return;
    }
    
    
    DiskIO_t *diskIOPtr = diskIOMgr->diskIOPtrs + fd;
    
    if (*(diskIOMgr->diskIOIndicators + fd) == 0) {
        return;
    }

#ifdef EC_DEBUG_MODE
    printf("file:%s with fd:%d closed\n", diskIOPtr->fileName, diskIOPtr->fileFd);
#endif
    
    close(diskIOPtr->fileFd);
    
//    printf("closed file diskFd:%d, fd:%d\n", fd, diskIOPtr->fileFd);
    diskIOPtr->fileFd = 0;
    diskIOPtr->blockId = 0;
    
    *(diskIOMgr->diskIOIndicators+fd) = 0;
//    close(diskIOPtr->fileMetaFd);
}
int startRemoveFile(uint64_t fileId, char *dirPath){
    char fileName[255];
    size_t fileNameSize = uint64_to_str(fileId, fileName, FILE_NAME_BUF_SIZE);
        
    size_t strSize = strlen(fileSuffix);
    memcpy(fileName + fileNameSize, fileSuffix, strSize);
    fileNameSize = fileNameSize + strSize;
    *(fileName + fileNameSize) = '\0';
    
    char absFilePath[1024];
    memcpy(absFilePath, dirPath, strlen(dirPath));
    memcpy((absFilePath + strlen(dirPath)), fileName, strlen(fileName));
    absFilePath[strlen(dirPath) + strlen(fileName)] = '\0';
    
    printf("Start delete file:%s\n",absFilePath);
    
    return remove((const char *)absFilePath);
}

int deleteFile(uint64_t fileId, DiskIOManager_t *diskIOMgr){
   
    int ret;

    //struct timeval startTime, endTime;

    pthread_mutex_lock(&diskIOMgr->lock);

    while (checkFileId(diskIOMgr, fileId) == -1) {
        //printf("Error:unable to delete file with id:%lu as it in use\n", fileId);
        closeFile(getFileId(diskIOMgr, fileId), diskIOMgr);
    }
    
    pthread_mutex_unlock(&diskIOMgr->lock);

    //gettimeofday(&startTime, NULL);
    ret = startRemoveFile(fileId, diskIOMgr->filePath);
    //gettimeofday(&endTime, NULL);
    
    pthread_mutex_unlock(&diskIOMgr->lock);

   // double deleteTimeInterval = timeIntervalInMS(startTime, endTime);
    //printf("deleteTimeInterval:%fms\n", deleteTimeInterval);
    return ret;
}
