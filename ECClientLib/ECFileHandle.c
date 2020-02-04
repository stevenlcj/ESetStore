//
//  ECFileHandle.c
//  ECClient
//
//  Created by Liu Chengjian on 17/9/12.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECFileHandle.h"
#include "ECClientCommon.h"
#include "ecUtils.h"
#include "ECClientCommand.h"
#include "ECNetHandle.h"
#include "ECBlockWorker.h"
#include "GCRSMatrix.h"

#include <string.h>

#define BLOCKS_INCRE_SIZE 50

size_t getFileSizeFromBuf(const char *Buf){
    char str1[]="FileSize:", str2[]="\r\n\0";
    int value = getIntValueBetweenStrings(str1, str2, Buf);
    
    return (size_t)value;
}

void getStripeK(const char *Buf, size_t *stripeK){
    char str1[]="stripeK:\0", str2[]="\r\n\0";
    int value = getIntValueBetweenStrings(str1, str2, Buf);
    
    *stripeK = (size_t)value;
}

void getStripeM(const char *Buf, size_t *stripeM){
    char str1[]="stripeM:\0", str2[]="\r\n\0";
    int value = getIntValueBetweenStrings(str1, str2, Buf);
    
    *stripeM = (size_t)value;
}

void getStripeW(const char *Buf, size_t *stripeW){
    char str1[]="stripeW:\0", str2[]="\r\n\0";
    int value = getIntValueBetweenStrings(str1, str2, Buf);
    
    *stripeW = (size_t)value;
}

void getStreamingSize(const char *Buf, size_t *streamingSize){
    char str1[]="streamingSize:\0", str2[]="\r\n\0";
    int value = getIntValueBetweenStrings(str1, str2, Buf);
    
    *streamingSize = (size_t)value * DEFAULT_STREAMING_UNIT_INBYTES;
}

//void printMatrix(size_t stripeK, size_t stripeM, int *matrix){
//    size_t rIdx, cIdx;
//    
//    for (rIdx = 0; rIdx < (stripeM); ++rIdx) {
//        for (cIdx = 0; cIdx < (stripeK ); ++cIdx) {
//            printf("%d ", *(matrix + rIdx * (stripeK) + cIdx));
//        }
//        
//        printf("\n");
//    }
//}

void getBlockInfo(int blockId, blockInfo_t *block, const char *buf){
    char strBuf[1024], intStr[1024], valueStr[1024];
    char blockStr[]="block\0";
    char endStr[] = "\r\n\0";
    size_t intStrLen = int_to_str(blockId, intStr, 1024);
    size_t blockStrLen = strlen(blockStr);
    size_t startOffset = 0, strLen = 0,endOffset = 0;
    
    memcpy(strBuf, blockStr, blockStrLen);
    memcpy((strBuf+blockStrLen), intStr, intStrLen);
    strBuf[blockStrLen + intStrLen] = ':';
    strBuf[blockStrLen + intStrLen] = '\0';
    strLen = strlen(strBuf);
    
    block->idx = blockId;
    
    while (strncmp((buf + startOffset), strBuf, strLen) != 0) {
        ++startOffset;
        if ((startOffset + strLen) > strlen(buf))  {
            return;
        }
    }
    
    endOffset = startOffset + 1;
    
    while (strncmp((buf + endOffset), endStr, strlen(endStr)) != 0) {
        ++endOffset;
        if ((endOffset + strlen(endStr)) > strlen(buf))  {
            return;
        }
    }
    
    memcpy(valueStr, (buf+startOffset + blockStrLen + intStrLen + 1), (endOffset - (startOffset + blockStrLen + intStrLen + 1)));
    valueStr[endOffset - (startOffset + blockStrLen + intStrLen + 1)] = '\0';
    //printf("blockValueStr:%s\n",valueStr);
    block->blockId = str_to_uint64(valueStr);
    
    char IPStr[] = "IP:";
    
    startOffset = endOffset + strlen(endStr);
    strLen = strlen(IPStr);
    while (strncmp(IPStr, (buf + startOffset), strLen) != 0) {
        ++startOffset;
        if ((startOffset + strLen) > strlen(buf))  {
            return;
        }
    }
    
    endOffset = startOffset + 1;
    
    while (strncmp((buf + endOffset), endStr, strlen(endStr)) != 0) {
        ++endOffset;
        if ((endOffset + strlen(endStr)) > strlen(buf))  {
            return;
        }
    }

    block->IPSize = endOffset - (startOffset + strLen);
    memcpy(block->IPAddr, (buf+startOffset + strLen), block->IPSize);
    block->IPAddr[block->IPSize] = '\0';
    
//    printf("IPAddr:%s\n",block->IPAddr);
    char PortStr[] = "Port:";

    startOffset = endOffset + strlen(endStr);
    strLen = strlen(PortStr);
    while (strncmp(PortStr, (buf + startOffset), strLen) != 0) {
        ++startOffset;
        if ((startOffset + strLen) > strlen(buf))  {
            return;
        }
    }
    
    endOffset = startOffset + 1;
    
    while (strncmp((buf + endOffset), endStr, strlen(endStr)) != 0) {
        ++endOffset;
        if ((endOffset + strlen(endStr)) > strlen(buf))  {
            return;
        }
    }

    memcpy(valueStr, (buf+ startOffset + strLen), (endOffset - (startOffset + strLen)));
    valueStr[endOffset-(startOffset + strLen)] = '\0';
    //printf("Portstr:%s\n", valueStr);
    block->port = str_to_int(valueStr);
    
    char serverStatus[] = "ServerStatus:\0";
    block->serverStatus = getIntValueBetweenStrings(serverStatus, endStr, (buf+startOffset + blockStrLen + intStrLen + 1));
    
//    printf("serverStatus:%d\n", block->serverStatus);

    //printf("serverStatus:%d IPAddr:%s block port:%d\n",block->serverStatus,block->IPAddr,  block->port);
}

void getMatrix(const char *buf, size_t stripeK, size_t stripeM, int *matrix){
    char str1[]="matrix:\0", str2[] = "\r\n\0";
    size_t matSize = stripeK * stripeM;
    
    getMatrixValues(str1, str2, buf, matrix, matSize);
//    printMatrix(matrix, (int)stripeM, (int)stripeK);

}

ECFileManager_t *createECFileMgr(void *clientEnginePtr){
    ECFileManager_t *ecFileMgr = talloc(ECFileManager_t, 1);

    if (ecFileMgr == NULL)
    {
        printf("Error: unable to alloc ecFileMgr\n");
        return NULL;
    }
    
    ecFileMgr->ecFileSize = DEFAULT_FILE_SIZE;
    ecFileMgr->ecFiles = talloc(ECFile_t, ecFileMgr->ecFileSize);
    ecFileMgr->ecFilesIndicator = talloc(size_t, ecFileMgr->ecFileSize);
    ecFileMgr->ecFileNum = 0;
    memset(ecFileMgr->ecFiles, 0, sizeof(ECFile_t) * ecFileMgr->ecFileSize);
    memset(ecFileMgr->ecFilesIndicator, 0, sizeof(size_t) * ecFileMgr->ecFileSize);
    
    ecFileMgr->clientEnginePtr = clientEnginePtr;
    ECBlockWorkerManager_t *blockWorkerMgr = createBlockWorkerMgr(BLOCKS_INCRE_SIZE);
    blockWorkerMgr->clientEnginePtr = clientEnginePtr;
    ecFileMgr->blockWorkersMgr = (void *)blockWorkerMgr;

    return ecFileMgr;
}

int createECFile(ECFileManager_t *ecFileMgr, const char *fileName){
    int idx;
    
    for (idx = 0; idx < (int) ecFileMgr->ecFileSize; ++idx) {
        if (ecFileMgr->ecFilesIndicator[idx] == 0) {
            size_t fileLength = strlen(fileName);
            ECFile_t *ecFile = ecFileMgr->ecFiles + idx;
            ecFile->fileNameSize = fileLength;
            ecFile->fileName = talloc(char, ecFile->fileNameSize + 1);
            
            ecFile->fileCurState = ECFILE_STATE_INIT;
            
            memcpy(ecFile->fileName, fileName, ecFile->fileNameSize);
            *(ecFile->fileName + ecFile->fileNameSize) = '\0';
            ecFileMgr->ecFilesIndicator[idx] = 1;

            ++ecFileMgr->ecFileNum;
            return idx;
        }
    }
    
    return -1;
}

void initOpenFileFromMetaReply(ECFileManager_t *ecFileMgr, int fd){
    ECFile_t *ecFilePtr = ecFileMgr->ecFiles + fd;
    ecFilePtr->fileCurState = ECFILE_STATE_OPEN;
    ecFilePtr->fileSize = 0;
    ecFilePtr->readOffset = 0;
    ecFilePtr->initFlag = 1;
}

void initCreateFileFromMetaReply(ECFileManager_t *ecFileMgr, int fd, const char *replyBuf){
    
    ECFile_t *ecFilePtr = ecFileMgr->ecFiles + fd;
    ecFilePtr->fileCurState = ECFILE_STATE_CREATE;
    getStripeK(replyBuf, &ecFilePtr->stripeK);
    getStripeM(replyBuf, &ecFilePtr->stripeM);
    getStripeW(replyBuf, &ecFilePtr->stripeW);
    getStreamingSize(replyBuf, &ecFilePtr->streamingSize);
    
    ecFilePtr->matrix = talloc(int, (ecFilePtr->stripeK* ecFilePtr->stripeM));
//    printf("start getMatrix\n");
    getMatrix(replyBuf, ecFilePtr->stripeK, ecFilePtr->stripeM, ecFilePtr->matrix);
//    printf("end getMatrix\n");
    ecFilePtr->blockNum = ecFilePtr->stripeK + ecFilePtr->stripeM;
    ecFilePtr->blocks = talloc(blockInfo_t, ecFilePtr->blockNum);
    
    int blockIdx;    
//    printf("StripeK:%lu, StripeM:%lu\n", ecFilePtr->stripeK, ecFilePtr->stripeM);
    
    for (blockIdx = 0; blockIdx < (int)(ecFilePtr->stripeK + ecFilePtr->stripeM); ++blockIdx) {
        blockInfo_t *blockPtr = ecFilePtr->blocks + blockIdx;
        getBlockInfo(blockIdx, blockPtr, replyBuf);
    }
    //printf("init input output bufs OK\n");
    ecFilePtr->blockWorkers = NULL;
    ecFilePtr->blockWorkerNum = 0;
    ecFilePtr->initFlag = 1;
    ecFilePtr->fileSize = 0;
    ecFilePtr->writeDataSize = 0;
}

void initReadFileFromMetaReply(ECFileManager_t *ecFileMgr, int fd, const char *replyBuf){
    char readOKHeader[] = "ReadOK\r\n\0";
    if (strncmp(replyBuf, readOKHeader, strlen(readOKHeader)) != 0) {
        printf("ReadOK\\r\\n != 0\n");
        return;
    }
    
//    printf("Recvd:******\n\n%s\n\n*********\n", replyBuf);
    ECFile_t *ecFilePtr = ecFileMgr->ecFiles + fd;
    
    ecFilePtr->fileCurState = ECFILE_STATE_READ;
    ecFilePtr->degradedReadFlag = 0;

    ecFilePtr->fileSize = getFileSizeFromBuf(replyBuf);
    
    getStripeK(replyBuf, &ecFilePtr->stripeK);
    getStripeM(replyBuf, &ecFilePtr->stripeM);
    getStripeW(replyBuf, &ecFilePtr->stripeW);
    getStreamingSize(replyBuf, &ecFilePtr->streamingSize);
    
    ecFilePtr->matrix = talloc(int, (ecFilePtr->stripeK * ecFilePtr->stripeM));
    
    getMatrix(replyBuf, ecFilePtr->stripeK, ecFilePtr->stripeM, ecFilePtr->matrix);
    
    ecFilePtr->blockNum = ecFilePtr->stripeK + ecFilePtr->stripeM;
    ecFilePtr->blocks = talloc(blockInfo_t, ecFilePtr->blockNum);

    //printf("StripeK:%lu, StripeM:%lu\n", ecFilePtr->stripeK, ecFilePtr->stripeM);
    
    int blockIdx;
    for (blockIdx = 0; blockIdx < (int)(ecFilePtr->stripeK + ecFilePtr->stripeM); ++blockIdx) {
        blockInfo_t *blockPtr = ecFilePtr->blocks + blockIdx;
        getBlockInfo(blockIdx, blockPtr, replyBuf);
    }
    
    blockInfo_t *blockPtr0 = ecFilePtr->blocks;
    blockInfo_t *blockPtr1 = ecFilePtr->blocks + 1;
    
    if((blockPtr0->IPSize == blockPtr1->IPSize) && (strncmp(blockPtr0->IPAddr,blockPtr1->IPAddr,blockPtr0->IPSize) == 0)){
        printf("status changed to degraded\n");
        blockPtr0->serverStatus = 0;
        ecFilePtr->degradedReadFlag = 1;
    }

    
    for (blockIdx = 0; blockIdx < (int) ecFilePtr->stripeK; ++blockIdx) {
        blockInfo_t *blockPtr = ecFilePtr->blocks + blockIdx;
        if (blockPtr->serverStatus == 0) {
            ecFilePtr->degradedReadFlag = 1;
            ecFilePtr->decoding_matrix = NULL;
            ecFilePtr->dm_ids = NULL;
        }
    }
}

int closeECFile(ECFileManager_t *ecFileMgr, int idx){
    if (ecFileMgr->ecFilesIndicator[idx] == 0) {
        return -1;
    }
    
    ECFile_t *ecFile = ecFileMgr->ecFiles + idx;
    
    if (ecFile->matrix != NULL) {
        free(ecFile->matrix);
        ecFile->matrix = NULL;
    }
    
    if (ecFile->decoding_matrix != NULL) {
        free(ecFile->decoding_matrix);
        ecFile->decoding_matrix = NULL;
    }
    
    if (ecFile->dm_ids != NULL) {
        free(ecFile->dm_ids);
        ecFile->dm_ids = NULL;
    }
    
    if (ecFile->mat_idx != NULL) {
        free(ecFile->mat_idx);
        ecFile->mat_idx = NULL;
    }

    ecFileMgr->ecFilesIndicator[idx] = 0;
    
    if (ecFile->blocks != NULL) {
        //printf("Free blocks\n");
        free(ecFile->blocks);
        ecFile->blocks = NULL;
    }
                
    if (ecFile->writeDataSize >0)
    {
        char *metaCmd = formMetaWriteSizeCmd(ecFile->fileName, ecFile->writeDataSize);
        ecFile->writeDataSize = 0;
        ECClientEngine_t *clientEnginePtr = (ECClientEngine_t *)ecFileMgr->clientEnginePtr;
            ssize_t writedSize = writeCmdToMeta(clientEnginePtr, metaCmd);
        if (writedSize != (ssize_t) (strlen(metaCmd) + 1)) {
            printf("write to meta:%ld, should write size:%lu\n ", writedSize, strlen(metaCmd));
        }
    }

    if (ecFile->blockWorkerNum != 0)
    {
        revokeBlockWorkers(ecFileMgr, idx);
    }
    
    if (ecFile->fileNameSize != 0) {
        //printf("Free filename\n");
        free(ecFile->fileName);
        ecFile->fileName = NULL;
        ecFile->fileNameSize = 0;
    }

    memset((void *)ecFile, 0, sizeof(ecFile));
    
    ecFile->fileCurState = ECFILE_STATE_UNINIT;
    
    --ecFileMgr->ecFileNum;
    
    return 0;
}

void deallocECFileMgr(ECFileManager_t *ecFileMgr){
    int idx;
    for (idx = 0; idx < (int) ecFileMgr->ecFileSize; ++idx) {
        if (ecFileMgr->ecFilesIndicator[idx] == 1) {
            closeECFile(ecFileMgr, idx);
        }
    }
    
    deallocBlockWorkerMgr((ECBlockWorkerManager_t *) (ecFileMgr->blockWorkersMgr));

    //printf("free(ecFileMgr->ecFiles\n");
    free(ecFileMgr->ecFiles);
    //printf("free(ecFileMgr->ecFilesIndicator\n");
    free(ecFileMgr->ecFilesIndicator);
    //printf("free(ecFileMgr)\n");
    free(ecFileMgr);
}

ssize_t readECFile(ECFileManager_t *ecFileMgr, int ecFd, char *readBuf, size_t readSize){
    if (readSize == 0) {
        //PRINT_ROUTINE_VALUE(readSize);
        return 0;
    }
    
    ssize_t readedSize = 0;
    ECFile_t *ecFile = ecFileMgr->ecFiles + ecFd;

    if (ecFile->degradedReadFlag == 0)
     {
//         printf("readSize:%lu\n",readSize);
         //PRINT_ROUTINE_VALUE(readSize);
         readedSize = performReadJob(ecFileMgr,  ecFd, readBuf,  readSize);
     }else{
         if (ecFile->decoding_matrix == NULL) {
             int idx, dNum = 0;
             int *erasures = talloc(int, ecFile->stripeM+1);
             ecFile->mat_idx = talloc(int, ecFile->stripeM);
             ecFile->dm_ids = talloc(int, ecFile->stripeK);
             for (idx = 0; idx < (int)ecFile->stripeK; ++idx) {
                 blockInfo_t *blockInfoPtr = ecFile->blocks + idx;
                 if (blockInfoPtr->serverStatus == 0) {
                     erasures[dNum] = idx;
                     ++dNum;
                 }
             }
             
             erasures[dNum] = -1;
             
             ecFile->decoding_matrix = grs_create_decoding_matrix((int)ecFile->stripeK, (int)ecFile->stripeM, (int)ecFile->stripeW, ecFile->matrix, ecFile->mat_idx, erasures, ecFile->dm_ids);
//             printMatrix(ecFile->decoding_matrix, (int)ecFile->stripeK, dNum);
         }
         
//         printf("readSize:%lu\n",readSize);
        readedSize = performDegradedReadJob(ecFileMgr,  ecFd, readBuf,  readSize);
     }
    /*

    if (readSize > ecFile->fileSize) {
        readSize = ecFile->fileSize;
    }
    
    ssize_t readedSize = 0;

    performECRead(ecFile, readBuf, readSize, &readedSize);*/
    
    return readedSize;
}

ssize_t writeECFile(ECFileManager_t *ecFileMgr, int ecFd, char *writeBuf, size_t writeSize){
    ssize_t writtenSize = 0; 

    ECFile_t *ecFile = ecFileMgr->ecFiles + ecFd;

    if (ecFile->fileCurState == ECFILE_STATE_CREATE)
    {
        ecFile->fileCurState = ECFILE_STATE_WRITE;
    }

    if (ecFile->fileCurState != ECFILE_STATE_WRITE)
    {
        printf("Error:ecFile->fileCurState != ECFILE_STATE_WRITE");
        return -1;
    }

//    printf("performWriteJob writeSize:%lu\n",writeSize);
    writtenSize = performWriteJob(ecFileMgr, ecFd, writeBuf, writeSize);
//    printf("writtenSize:%ld, writeSize:%lu\n",writtenSize,writeSize);

    if (writtenSize > 0)
    {
        ecFile->writeDataSize = ecFile->writeDataSize + writtenSize;
    }
    return writtenSize;
}





















