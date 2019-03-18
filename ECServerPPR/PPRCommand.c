//
//  PPRCommand.c
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/9.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "PPRCommand.h"

#include "PPRMasterWorker.h"
#include "ecUtils.h"

#include <string.h>

/*
 Size: \r\n
 IP: \r\n
 Idx: \r\n
 Value: \r\n
 IP: \r\n
 Idx: \r\n
 Value: \r\n
 ...
 */

size_t formPPRCmd(char *buf, size_t BufSize, void *pprPtr){
    PPRMasterWorker_t *pprMasterWorkerPtr = (PPRMasterWorker_t *)pprPtr;
    int idx;
    size_t bufSize = 1024;
    char tmpBuf[1024];
    char sizeStr[] = "Size:\0", suffixStr[] = "\r\n\0", IPStr[] = "IP:\0", IdxStr[] = "Idx:\0", ValueStr[]= "Value:\0";
    size_t writeOffset = 0, writeSize = 0;
    
    writeSize = strlen(sizeStr);
    writeToBuf(buf, sizeStr, &writeSize, &writeOffset);
    writeSize = sizeT_to_str(pprMasterWorkerPtr->sourceNodesSize-1, tmpBuf, bufSize);
    writeToBuf(buf, tmpBuf, &writeSize, &writeOffset);
    writeSize = strlen(suffixStr);
    writeToBuf(buf, suffixStr, &writeSize, &writeOffset);

    for (idx = 1; idx < pprMasterWorkerPtr->sourceNodesSize; ++idx) {
        blockInfo_t *blockInfoPtr = pprMasterWorkerPtr->blockInfoPtrs + idx;
        
        writeSize = strlen(IPStr);
        writeToBuf(buf, IPStr, &writeSize, &writeOffset);
        writeSize = (size_t)blockInfoPtr->IPSize;
        writeToBuf(buf, blockInfoPtr->IPAddr, &writeSize, &writeOffset);
        writeSize = strlen(suffixStr);
        writeToBuf(buf, suffixStr, &writeSize, &writeOffset);
        
        writeSize = strlen(IdxStr);
        writeToBuf(buf, IdxStr, &writeSize, &writeOffset);
        writeSize = int_to_str(blockInfoPtr->idx, tmpBuf, bufSize);
        writeToBuf(buf, tmpBuf, &writeSize, &writeOffset);
        writeSize = strlen(suffixStr);
        writeToBuf(buf, suffixStr, &writeSize, &writeOffset);
        
        writeSize = strlen(ValueStr);
        writeToBuf(buf, ValueStr, &writeSize, &writeOffset);
        writeSize = int_to_str(*(pprMasterWorkerPtr->decodingVector + idx), tmpBuf, bufSize);
        writeToBuf(buf, tmpBuf, &writeSize, &writeOffset);
        writeSize = strlen(suffixStr);
        writeToBuf(buf, suffixStr, &writeSize, &writeOffset);
    }
    
    buf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
    
    printf("pprCmd:%s\n",buf);
    
    return writeOffset;
}

size_t formNextPPRCmd(char *cmdBuf, char *buf, size_t BufSize, int sizeValue){
    size_t bufSize = 1024;
    char tmpBuf[1024];
    char sizeStr[] = "Size:\0", suffixStr[] = "\r\n\0";
    size_t writeOffset = 0, writeSize = 0;
    
    writeSize = strlen(sizeStr);
    writeToBuf(cmdBuf, sizeStr, &writeSize, &writeOffset);
    writeSize = sizeT_to_str((size_t)sizeValue, tmpBuf, bufSize);
    writeToBuf(cmdBuf, tmpBuf, &writeSize, &writeOffset);
    writeSize = strlen(suffixStr);
    writeToBuf(cmdBuf, suffixStr, &writeSize, &writeOffset);

    memcpy(cmdBuf + writeOffset, buf, BufSize);
    
    writeOffset = writeOffset + BufSize;
    
    printf("pprCmd:%s\n",cmdBuf);

    return writeOffset;
}

size_t formPPRTaskOverCmd(char *buf){
    char taskOverStr[] = "TaskOver\r\n\0";
    
    size_t writeOffset = 0, writeSize = 0;
    
    writeSize = strlen(taskOverStr);
    writeToBuf(buf, taskOverStr, &writeSize, &writeOffset);

    buf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
    
    return  writeOffset;
}

/**
BlockGroupIdx:__\r\n
BlockSize:__\r\n
 */
size_t formPPRRecoveryCmd(char *buf, size_t bufSize, uint64_t blockGroupIdx, size_t blockSize){
    char blockGroupIdxStr[] = "BlockGroupIdx:\0",suffixStr[] = "\r\n\0", blockSizeStr[] = "BlockSize:";
    char tmpBuf[1024];
    size_t writeOffset = 0, writeSize = 0;
    writeSize = strlen(blockGroupIdxStr);
    writeToBuf(buf, blockGroupIdxStr, &writeSize, &writeOffset);
    writeSize = uint64_to_str(blockGroupIdx, tmpBuf, 1024);
    writeToBuf(buf, tmpBuf, &writeSize, &writeOffset);
    writeSize = strlen(suffixStr);
    writeToBuf(buf, suffixStr, &writeSize, &writeOffset);
    
    writeSize = strlen(blockSizeStr);
    writeToBuf(buf, blockSizeStr, &writeSize, &writeOffset);
    writeSize = sizeT_to_str(blockSize, tmpBuf, 1024);
    writeToBuf(buf, tmpBuf, &writeSize, &writeOffset);
    writeSize = strlen(suffixStr);
    writeToBuf(buf, suffixStr, &writeSize, &writeOffset);

    
    buf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
    
    printf("pprRecoveryCmd:%s\n", buf);
    
    return writeOffset;
}






















