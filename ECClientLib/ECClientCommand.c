//
//  ECClientCommand.c
//  ECClient
//
//  Created by Liu Chengjian on 17/9/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECClientCommand.h"
#include "ECClientCommon.h"
#include "ecUtils.h"

#include <string.h>

const char BlockCreate[] = "BlockCreate\0"; 
const char BlockOpen[] = "BlockOpen\0";
const char BlockWrite[] = "BlockWrite\0";
const char BlockClose[] = "BlockClose\0";
const char blockFdStr[] = "BlockFd:\0";

const char createCmd[] = "Create\r\n\0";
const char openCmd[] = "Open\r\n\0";
const char readCmd[] = "Read\r\n\0";
const char cmdSuffix[] = "\r\n\0";

const char deleteCmd[] = "Delete\r\n\0";

const char blockWriteCmd[] = "BlockWrite\0";
const char blockReadCmd[] = "BlockRead\0";
const char blockIdStr[] = "BlockId:\0";

const char writeSizeCmd[] = "WriteSize:\0";
const char writeOverCmd[] = "WriteOver\r\n\0";

const char readSizeCmd[] = "ReadSize:\0";
const char readOverCmd[] = "ReadOver\r\n\0";

const char metaWriteOverCmd[] = "WriteOver:\0";

const char closingCmd[] = "Closing\r\n\r\n\0";

char *formCreateFileCmd(const char *fileName){
    
    if (fileName == NULL) {
        return NULL;
    }
    
    size_t cmdSize = strlen(createCmd) + strlen(fileName) + strlen(cmdSuffix) + 1;
    
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, createCmd, strlen(createCmd));
    memcpy((cmdStr + strlen(createCmd)), fileName, strlen(fileName));
    memcpy((cmdStr + strlen(createCmd)+ strlen(fileName)), cmdSuffix, strlen(cmdSuffix));
    
    cmdStr[cmdSize - 1] = '\0';
    //printf("createCmd:%s\n",cmdStr);
    return cmdStr;
}

char *formDeleteFileCmd(const char *fileName){
    
    if (fileName == NULL) {
        return NULL;
    }
    
    size_t cmdSize = strlen(deleteCmd) + strlen(fileName) + strlen(cmdSuffix) + 1;
    
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, deleteCmd, strlen(deleteCmd));
    memcpy((cmdStr + strlen(deleteCmd)), fileName, strlen(fileName));
    memcpy((cmdStr + strlen(deleteCmd)+ strlen(fileName)), cmdSuffix, strlen(cmdSuffix));
    
    cmdStr[cmdSize - 1] = '\0';
//    printf("deleteCmd:%s\n",cmdStr);
    return cmdStr;
}

char *formOpenFileCmd(const char *fileName){
    
    if (fileName == NULL) {
        return NULL;
    }
    
    size_t cmdSize = strlen(openCmd) + strlen(fileName) + strlen(cmdSuffix) + 1;
    
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, openCmd, strlen(openCmd));
    memcpy((cmdStr + strlen(openCmd)), fileName, strlen(fileName));
    memcpy((cmdStr + strlen(openCmd)+ strlen(fileName)), cmdSuffix, strlen(cmdSuffix));
    
    cmdStr[cmdSize - 1] = '\0';
    printf("openCmd:%s\n",cmdStr);
    return cmdStr;
}

char *formReadFileCmd(const char *fileName){
    
    if (fileName == NULL) {
        return NULL;
    }
    
    size_t cmdSize = strlen(readCmd) + strlen(fileName) + strlen(cmdSuffix) + 1;
    
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, readCmd, strlen(readCmd));
    memcpy((cmdStr + strlen(readCmd)), fileName, strlen(fileName));
    memcpy((cmdStr + strlen(readCmd)+ strlen(fileName)), cmdSuffix, strlen(cmdSuffix));
    
    cmdStr[cmdSize - 1] = '\0';
//    printf("readCmd:%s\n",cmdStr);
    return cmdStr;
}

//cmd: WriteOver:filename\r\nWriteSize:__\r\n\0
char *formMetaWriteSizeCmd(const char *fileName, size_t writedSize){
    if (fileName == NULL) {
        return NULL;
    }
    
    char buf[1024];
    
    size_t strSize = sizeT_to_str(writedSize, buf, 1024);
    
    size_t cmdSize = strlen(metaWriteOverCmd) + strlen(fileName) + strlen(cmdSuffix) + strlen(writeSizeCmd) + strSize + strlen(cmdSuffix) + 1;
    
    char *cmdStr = talloc(char, cmdSize);
    memcpy(cmdStr, metaWriteOverCmd, strlen(metaWriteOverCmd));
    memcpy((cmdStr + strlen(metaWriteOverCmd)), fileName, strlen(fileName));
    memcpy((cmdStr + strlen(metaWriteOverCmd)+ strlen(fileName)), cmdSuffix, strlen(cmdSuffix));
    memcpy((cmdStr + strlen(metaWriteOverCmd)+ strlen(fileName) + strlen(cmdSuffix)), writeSizeCmd, strlen(writeSizeCmd));
    memcpy((cmdStr + strlen(metaWriteOverCmd)+ strlen(fileName) + strlen(cmdSuffix) + strlen(writeSizeCmd)), buf, strSize);
    memcpy((cmdStr + strlen(metaWriteOverCmd)+ strlen(fileName) + strlen(cmdSuffix) + strlen(writeSizeCmd)+strSize), cmdSuffix, strlen(cmdSuffix));

    *(cmdStr + cmdSize - 1) = '\0';
//    printf("formMetaWriteSizeCmd:%s\n",cmdStr);
    return cmdStr;
}

// BlockCreate/r/nBlockId:__/r/n/0
size_t constructCreateBlockCmd(char *cmdBuf, uint64_t blockId){
    char blockIdBuf[1024];
    size_t writeSize = 0, writeOffset = 0;

    writeToBuf(cmdBuf, (char *)BlockCreate, &writeSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &writeSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockIdStr, &writeSize, &writeOffset);
    uint64_to_str(blockId, blockIdBuf, 1024);
    writeToBuf(cmdBuf, (char *)blockIdBuf, &writeSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &writeSize, &writeOffset);

    cmdBuf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
    //printf("constructCreateBlockCmd cmd size:%lu cmd content:%s\n", writeOffset, cmdBuf);
    return writeOffset;
}

// BlockOpen/r/nBlockId:__/r/n/0
size_t constructOpenBlockCmd(char *cmdBuf, uint64_t blockId){
    char blockIdBuf[1024];
    size_t writeSize = 0, writeOffset = 0;

    writeToBuf(cmdBuf, (char *)BlockOpen, &writeSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &writeSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockIdStr, &writeSize, &writeOffset);
    uint64_to_str(blockId, blockIdBuf, 1024);
    writeToBuf(cmdBuf, (char *)blockIdBuf, &writeSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &writeSize, &writeOffset);

    cmdBuf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
//    printf("constructOpenBlockCmd cmd size:%lu cmd content:%s\n", writeOffset, cmdBuf);
    return writeOffset;
}

// BlockWrite/r/nBlockId:__/r/nBlockFd:__/r/nWriteSize:__/r/n/0
size_t constructBlockWriteCmd(char *cmdBuf, uint64_t blockId, int blockFd, size_t writeSize){
    char tempBuf[1024];
    size_t wSize = 0, writeOffset = 0;

    writeToBuf(cmdBuf, (char *)BlockWrite, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockIdStr, &wSize, &writeOffset);
    uint64_to_str(blockId, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockFdStr, &wSize, &writeOffset);
    uint64_to_str(blockFd, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);


    writeToBuf(cmdBuf, (char *)writeSizeCmd, &wSize, &writeOffset);
    sizeT_to_str(writeSize, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);

    cmdBuf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
   // printf("constructBlockWriteCmd:%s\n",cmdBuf);
    return writeOffset;
}

// BlockRead/r/nBlockId:__/r/nBlockFd:__/r/nReadSize:__/r/n/0
size_t constructBlockReadCmd(char *cmdBuf, uint64_t blockId, int blockFd, size_t writeSize){
    char tempBuf[1024];
    size_t wSize = 0, writeOffset = 0;

    writeToBuf(cmdBuf, (char *)blockReadCmd, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockIdStr, &wSize, &writeOffset);
    uint64_to_str(blockId, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockFdStr, &wSize, &writeOffset);
    uint64_to_str(blockFd, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);


    writeToBuf(cmdBuf, (char *)readSizeCmd, &wSize, &writeOffset);
    sizeT_to_str(writeSize, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);

    cmdBuf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
    //printf("constructBlockReadCmd:%s\n",cmdBuf);
    return writeOffset;
}

//* Form string: BlockClose/r/nBlockId:__/r/nBlockFd:__/r/n/0
size_t constructBlockCloseCmd(char *cmdBuf, uint64_t blockId, int blockFd){
    char tempBuf[1024];
    size_t wSize = 0, writeOffset = 0;

    writeToBuf(cmdBuf, (char *)BlockClose, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockIdStr, &wSize, &writeOffset);
    uint64_to_str(blockId, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockFdStr, &wSize, &writeOffset);
    uint64_to_str(blockFd, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffix, &wSize, &writeOffset);

    cmdBuf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
    //printf("constructBlockCloseCmd:%s\n",cmdBuf);
    return writeOffset;
}

//* Form string: BlockWrite/r/nBlockId:blockId/r/n/0

size_t formBlockWriteCmd(char *cmdBuf, uint64_t blockId){
    char blockIdBuf[1024];
    
    size_t strSize = uint64_to_str(blockId, blockIdBuf, 1024);
    
    size_t cmdSize = strlen(blockWriteCmd) + strlen(blockIdStr) + strSize + strlen(cmdSuffix) + 1;
    
//    printf("cmdSize:%lu, strSize:%lu\n",cmdSize, strSize);
    
    memcpy(cmdBuf, blockWriteCmd, strlen(blockWriteCmd));
    memcpy((cmdBuf + strlen(blockWriteCmd) ), blockIdStr, strlen(blockIdStr));
    memcpy((cmdBuf + strlen(blockWriteCmd) + strlen(blockIdStr) ), blockIdBuf, strSize);
    memcpy((cmdBuf + strlen(blockWriteCmd) + strlen(blockIdStr) + strSize), cmdSuffix, strlen(cmdSuffix));
    
    cmdBuf[cmdSize-1] = '\0';
    //printf("formBlockWriteCmd:%s\n",cmdBuf);
    return cmdSize;
}

//* Form string: BlockRead/r/nBlockId:blockId/r/n/0
char *formBlockReadCmd(uint64_t blockId){
    char blockIdBuf[1024];
    
    size_t strSize = uint64_to_str(blockId, blockIdBuf, 1024);
    
    size_t cmdSize = strlen(blockReadCmd) + strlen(blockIdStr) + strSize + strlen(cmdSuffix) + 1;
    
    printf("cmdSize:%lu, strSize:%lu\n",cmdSize, strSize);
    
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, blockReadCmd, strlen(blockReadCmd));
    memcpy((cmdStr + strlen(blockReadCmd) ), blockIdStr, strlen(blockIdStr));
    memcpy((cmdStr + strlen(blockReadCmd) + strlen(blockIdStr) ), blockIdBuf, strSize);
    memcpy((cmdStr + strlen(blockReadCmd) + strlen(blockIdStr) + strSize), cmdSuffix, strlen(cmdSuffix));
    
    cmdStr[cmdSize-1] = '\0';
//    printf("formBlockReadCmd:%s\n",cmdStr);
    return cmdStr;
}

size_t formWriteRawDataCmd(char *cmdBuf, size_t writeSize){
    char buf[1024];
    
    size_t blockStrSize = strlen(writeSizeCmd);
    size_t strSize = sizeT_to_str(writeSize, buf, 1024);
    size_t suffixSize = strlen(cmdSuffix);
    
    size_t cmdSize = blockStrSize + strSize + suffixSize + 1;
    
    memcpy(cmdBuf, writeSizeCmd, blockStrSize);
    memcpy(cmdBuf + blockStrSize, buf, strSize);
    memcpy((cmdBuf + blockStrSize + strSize), cmdSuffix, suffixSize);
    
    cmdBuf[cmdSize - 1] = '\0';
    //printf("formWriteRawDataCmd:%s\n",cmdStr);
    return cmdSize;
}

char *formReadRawDataCmd(size_t readSize){
    char buf[1024];
    
    size_t blockStrSize = strlen(readSizeCmd);
    size_t strSize = sizeT_to_str(readSize, buf, 1024);
    size_t suffixSize = strlen(cmdSuffix);
    
    size_t cmdSize = blockStrSize + strSize + suffixSize + 1;
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, readSizeCmd, blockStrSize);
    memcpy(cmdStr + blockStrSize, buf, strSize);
    memcpy((cmdStr + blockStrSize + strSize), cmdSuffix, suffixSize);
    
    cmdStr[cmdSize - 1] = '\0';
    printf("formReadRawDataCmd:%s\n",cmdStr);
    return cmdStr;
}

size_t formWriteOverCmd(char *cmdBuf){
    size_t cmdSize = strlen(writeOverCmd) + 1;
    memcpy(cmdBuf, writeOverCmd, cmdSize - 1);
    
    cmdBuf[cmdSize - 1] = '\0';
    //printf("formWriteOverCmd:%s\n",cmdStr);
    return cmdSize;
}

size_t formReadOverCmd(char *cmdBuf){
    size_t cmdSize = strlen(readOverCmd) + 1;
    memcpy(cmdBuf, readOverCmd, cmdSize - 1);
    
    cmdBuf[cmdSize - 1] = '\0';
    //printf("formWriteOverCmd:%s\n",cmdStr);
    return cmdSize;
}

char *formClosingCmd(){
    size_t cmdSize = strlen(closingCmd) + 1;
    char *cmdStr = talloc(char, cmdSize);
    memcpy(cmdStr, closingCmd, cmdSize - 1);
    
    cmdStr[cmdSize - 1] = '\0';
    //printf("formClosingCmd:%s\n",cmdStr);
    return cmdStr;
}















