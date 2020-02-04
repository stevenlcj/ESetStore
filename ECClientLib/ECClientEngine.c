//
//  ECClientEngine.c
//  ECClient
//
//  Created by Liu Chengjian on 17/9/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECClientEngine.h"
#include "ECClientCommon.h"
#include "ECNetHandle.h"
#include "ECClientCommand.h"
#include "ECFileHandle.h"
#include <string.h>
#include <pthread.h>

ECClientEngine_t *createECClientEngine(const char *metaIP, int metaPort){
    //printf ("tid:%ld, __FUNCTION__ = %s\n",pthread_self(), __FUNCTION__);
    if (metaPort < 0 || metaIP == NULL ) {
        //printf("%ld metaPort < 0 || metaIP == NULL\n",pthread_self());
        return NULL;
    }
    
    ECClientEngine_t *clientEnginePtr = talloc(ECClientEngine_t, 1);
    clientEnginePtr->metaIPSize = strlen(metaIP);
    memcpy(clientEnginePtr->metaIP, metaIP, clientEnginePtr->metaIPSize);
    clientEnginePtr->metaIP[clientEnginePtr->metaIPSize] = '\0';
    
    clientEnginePtr->metaPort = metaPort;
    
    if(connectMetaEngine(clientEnginePtr) == -1){
        free(clientEnginePtr);
        return NULL;
    }
    
    clientEnginePtr->ecFileMgr = createECFileMgr((void *) clientEnginePtr);
    
    //printf ("tid:%ld, __FUNCTION__ = %s Done\n",pthread_self(), __FUNCTION__);

    return clientEnginePtr;
}

void deallocECClientEngine(ECClientEngine_t * clientEnginePtr){
    //printf ("tid:%ld, __FUNCTION__ = %s\n",pthread_self(), __FUNCTION__);

    if (clientEnginePtr == NULL) {
        return;
    }
    
    closeSockFd(clientEnginePtr->metaSockFd);
    
    deallocECFileMgr(clientEnginePtr->ecFileMgr);
    
    free(clientEnginePtr);
    //printf ("tid:%ld, __FUNCTION__ = %sdone\n",pthread_self(), __FUNCTION__);

}

//Send cmd:Create\r\nFileName\r\n\0

int createFile(ECClientEngine_t *clientEnginePtr, const char *FileName){
    char *createCmd = formCreateFileCmd(FileName);
    
    if (createCmd == NULL) {
        return -1;
    }
    
//    printf("Start create file cmd:%s\n", createCmd);
    int fileFd = createECFile(clientEnginePtr->ecFileMgr, FileName);
    
    if (fileFd == -1) {
        printf("createECFile failed \n");
        return fileFd;
    }
    size_t cmdSize = strlen(createCmd) + 1;
    ssize_t writeSize = writeCmdToSock(clientEnginePtr->metaSockFd, createCmd, cmdSize );
    free(createCmd);
    
    if ( writeSize != (ssize_t) cmdSize){
        printf("Write size:%lu cmd size:%lu\n", writeSize, cmdSize);
    }
    
    //
    
    char recvBuf[1024];
    
    recvMetaReply(clientEnginePtr->metaSockFd, recvBuf, 1024);
    
//    printf("Recvd:%s ****from meta server\n", recvBuf);
    
    char createOK[] ="CreateOK\0";
    if (strncmp(createOK, recvBuf, strlen(createOK)) != 0) {
        closeECFile(clientEnginePtr->ecFileMgr, fileFd);
        return -1;
    }
    
    initCreateFileFromMetaReply(clientEnginePtr->ecFileMgr, fileFd, recvBuf);
    
    return fileFd;
}

//Send cmd:Open\r\nFileName\r\n\0
//open for read or open for append
int openFile(ECClientEngine_t *clientEnginePtr, const char *FileName){
    char *openCmd = formReadFileCmd(FileName);
    
    if (openCmd == NULL) {
        return -1;
    }
    
//    printf("Start open file cmd:%s\n", openCmd);
    int fileFd = createECFile(clientEnginePtr->ecFileMgr, FileName);
    
    if (fileFd == -1) {
        printf("createECFile failed \n");
        return fileFd;
    }
    size_t cmdSize = strlen(openCmd) + 1;
    ssize_t writeSize = writeCmdToSock(clientEnginePtr->metaSockFd, openCmd, cmdSize);
    free(openCmd);
    
    if ( writeSize!= (ssize_t)cmdSize){
        printf("Write size:%lu cmd size:%lu\n", writeSize, cmdSize);
    }
    
    char recvBuf[1024];
    
    recvMetaReply(clientEnginePtr->metaSockFd, recvBuf, 1024);
    
    //printf("Recvd:%s ****from meta server\n", recvBuf);
    
    char readOK[] ="ReadOK\0";
    if (strncmp(readOK, recvBuf, strlen(readOK)) != 0) {
        closeECFile(clientEnginePtr->ecFileMgr, fileFd);
        return -1;
    }
    
    initReadFileFromMetaReply(clientEnginePtr->ecFileMgr, fileFd, recvBuf);
    
//    initFileFromMetaReply(clientEnginePtr->ecFileMgr, fileFd, recvBuf);
    
    return fileFd;
}

size_t getFileStripeK(ECClientEngine_t *clientEnginePtr, int fileFd){
    ECFile_t *ecFile = clientEnginePtr->ecFileMgr->ecFiles + fileFd;
    return ecFile->stripeK;
}

size_t getFileStripeM(ECClientEngine_t *clientEnginePtr, int fileFd){
    ECFile_t *ecFile = clientEnginePtr->ecFileMgr->ecFiles + fileFd;
    return ecFile->stripeM;
}

size_t getFileStripeW(ECClientEngine_t *clientEnginePtr, int fileFd){
    ECFile_t *ecFile = clientEnginePtr->ecFileMgr->ecFiles + fileFd;
    return ecFile->stripeW;
}

size_t getFileStreamingSize(ECClientEngine_t *clientEnginePtr, int fileFd){
    ECFile_t *ecFile = clientEnginePtr->ecFileMgr->ecFiles + fileFd;
    return ecFile->streamingSize;
}

size_t getFileSize(ECClientEngine_t *clientEnginePtr, int fileFd){
    ECFile_t *ecFile = clientEnginePtr->ecFileMgr->ecFiles + fileFd;
    return ecFile->fileSize;
}

ssize_t readFile(ECClientEngine_t *clientEnginePtr, int fileFd, char *buf, size_t readSize){
    //printf ("tid:%ld, __FUNCTION__ = %s\n",pthread_self(), __FUNCTION__);
    ECFile_t *ecFile = clientEnginePtr->ecFileMgr->ecFiles + fileFd;
    
    //PRINT_ROUTINE_VALUE(readSize);
    
    if(ecFile->fileCurState != ECFILE_STATE_READ && ecFile->fileCurState != ECFILE_STATE_OPEN){
        printf("fileCurState != ECFILE_STATE_READ && fileCurState != ECFILE_STATE_OPEN \n");
        return -1;
    }
    
    if (ecFile->fileCurState == ECFILE_STATE_OPEN) {
        //Retive file meta infomation
        char *readCmd = formReadFileCmd(ecFile->fileName);
        char recvBuf[1024];
        
        //ssize_t writeSize =  
        writeCmdToSock(clientEnginePtr->metaSockFd, readCmd, strlen(readCmd));
        free(readCmd);
        
        recvMetaReply(clientEnginePtr->metaSockFd, recvBuf, 1024);
//        printf("Recvd:%s ****from meta server\n", recvBuf);
        initReadFileFromMetaReply(clientEnginePtr->ecFileMgr, fileFd, recvBuf);
    }

    if (ecFile->fileCurState == ECFILE_STATE_READ ) {
        //printf("call readECFile\n");
        ssize_t readedSize = readECFile(clientEnginePtr->ecFileMgr, fileFd, buf, readSize);
        //printf ("tid:%ld, __FUNCTION__ = %s done\n",pthread_self(), __FUNCTION__);
        return readedSize;
    }
    
    printf("ecFile->fileCurState != ECFILE_STATE_READ\n");
    return -1;
}

ssize_t writeFile(ECClientEngine_t *clientEnginePtr, int fileFd, char *buf, size_t writeSize){
    //printf("to writeFile writeSize:%lu\n", writeSize);
    //printf ("tid:%ld, __FUNCTION__ = %s\n",pthread_self(), __FUNCTION__);
    ssize_t writedSize = writeECFile(clientEnginePtr->ecFileMgr, fileFd, buf, writeSize);
    //printf ("tid:%ld, __FUNCTION__ = %s writedSize:%ld\n",pthread_self(), __FUNCTION__,writedSize);

    if (writedSize <= 0) {
        return 0;
    }
    
    if (writedSize == (ssize_t) writeSize) {
        return writedSize;
    }
    
    return (writedSize + writeFile(clientEnginePtr, fileFd, buf + writedSize, writeSize - (size_t) writedSize));
}

int deleteFile(ECClientEngine_t *clientEnginePtr, const char *FileName){
    //printf ("tid:%ld, __FUNCTION__ = %s\n",pthread_self(), __FUNCTION__);

    char *deleteCmd = formDeleteFileCmd(FileName);
    
    ssize_t writeSize = writeCmdToMeta(clientEnginePtr, deleteCmd) ;
    
    if (writeSize != (ssize_t) (strlen(deleteCmd) + 1)) {
        printf("write cmd error: deleteFile, cmd:%s\n",deleteCmd);
        return -1;
    }
    free(deleteCmd);

    char recvBuf[1024];
    char OKStr[]="DeleteOK\0";
    ssize_t recvSize = recvMetaReply(clientEnginePtr->metaSockFd, recvBuf, 1024);
    
    //printf ("tid:%ld, __FUNCTION__ = %sdone\n",pthread_self(), __FUNCTION__);

    //printf("deleteFile recvd:%s\n", recvBuf);
    if (recvSize <= (ssize_t) strlen(OKStr) || strncmp(recvBuf, OKStr, strlen(OKStr)) != 0) {
        return -1;
    }
    
    return 0;
}

size_t canParseRecvdCmd(char *buf, size_t bufSize, char cmdFlag){
    if (bufSize == 0)
    {
        return 0;
    }

    size_t idx;
    for (idx = 1; idx < bufSize; ++idx)
    {
        if (*(buf + idx) == cmdFlag)
        {
            return idx;
        }
    }

    return 0;
}

void printRemainStr(char *buf, size_t strSize){
    size_t idx;
    printf("******\n");
    for (idx = 0; idx < strSize; ++idx)
    {
        printf("%c", *(buf + idx));
    }
    printf("******\n");
}

int deleteFiles(ECClientEngine_t *clientEnginePtr, char *filesToDelete[], int fileNum){
    int idx, sNum = 0;
    for (idx = 0; idx < fileNum; ++idx)
    {
        char *deleteCmd = formDeleteFileCmd(filesToDelete[idx]);
        ssize_t writeSize = writeCmdToMeta(clientEnginePtr, deleteCmd);
    
        if (writeSize !=  ((ssize_t)strlen(deleteCmd) + 1)) {
            printf("write cmd error: deleteFile %s\n", deleteCmd);
            return -1;
        }

        free(deleteCmd);
    }

    size_t bufSize = 1024;
    char recvBuf[bufSize], tempBuf[bufSize];
    char OKStr[]="DeleteOK\0";
    size_t recvdSize = 0, curParseOffset, parseOffset;
    idx = 0;
    do{
        ssize_t recvSize = recvMetaReply(clientEnginePtr->metaSockFd, recvBuf + recvdSize, (bufSize - recvdSize));
        
        if (recvSize <= 0)
        {
            perror("deleteFiles:recvSize <= 0");
            return -1;
        } 

        recvdSize = recvdSize + (size_t) recvSize;

       // printf("recvdSize:%lu\n", recvdSize);

        parseOffset = 0;
        curParseOffset = 0;
        while((parseOffset = canParseRecvdCmd(recvBuf + curParseOffset,(recvdSize - curParseOffset), '\0')) > 0){
            if (strncmp(recvBuf+curParseOffset, OKStr, strlen(OKStr)) == 0) {
                ++sNum;
            }

            ++idx;
           // printf("idx:%d, recvdSize:%lu, curParseOffset:%lu Recvd:%s\n",idx, recvdSize, curParseOffset, (recvBuf+curParseOffset));

            curParseOffset = curParseOffset + parseOffset + 1;

            if (curParseOffset == recvdSize)
             {
                break;
            }
        }
        
        recvdSize = recvdSize - curParseOffset;
        //printf("recvdSize:%lu idx:%d\n", recvdSize,idx);
        if (recvdSize != 0 && curParseOffset != 0)
        {
            memcpy(tempBuf, recvBuf+curParseOffset, recvdSize);
            memcpy(recvBuf, tempBuf, recvdSize);
            printRemainStr(recvBuf, recvdSize);
        }
    }while(idx != fileNum);

    return sNum;
}

void closeFile(ECClientEngine_t *clientEnginePtr, int fileFd){
    closeECFile(clientEnginePtr->ecFileMgr, fileFd);
}

ssize_t writeCmdToMeta(ECClientEngine_t *clientEnginePtr, char *cmd){
    return writeCmdToSock(clientEnginePtr->metaSockFd, cmd, strlen(cmd) + 1);
}
