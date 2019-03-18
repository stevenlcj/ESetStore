//
//  ECClientCommand.c
//  ECClient
//
//  Created by Liu Chengjian on 17/9/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECClientCommand.h"
#include "ecCommon.h"
#include "ecUtils.h"
#include "ecString.h"

#include <string.h>

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
    printf("createCmd:%s\n",cmdStr);
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
    printf("deleteCmd:%s\n",cmdStr);
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
    printf("readCmd:%s\n",cmdStr);
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
    printf("formMetaWriteSizeCmd:%s\n",cmdStr);
    return cmdStr;
}

//* Form string: BlockWrite/r/nBlockId:blockId/r/n/0

char *formBlockWriteCmd(uint64_t blockId){
    char blockIdBuf[1024];
    
    size_t strSize = uint64_to_str(blockId, blockIdBuf, 1024);
    
    size_t cmdSize = strlen(blockWriteCmd) + strlen(blockIdStr) + strSize + strlen(cmdSuffix) + 1;
    
//    printf("cmdSize:%lu, strSize:%lu\n",cmdSize, strSize);
    
    char *cmdStr = talloc(char, cmdSize);

    memcpy(cmdStr, blockWriteCmd, strlen(blockWriteCmd));
    memcpy((cmdStr + strlen(blockWriteCmd) ), blockIdStr, strlen(blockIdStr));
    memcpy((cmdStr + strlen(blockWriteCmd) + strlen(blockIdStr) ), blockIdBuf, strSize);
    memcpy((cmdStr + strlen(blockWriteCmd) + strlen(blockIdStr) + strSize), cmdSuffix, strlen(cmdSuffix));
    
    cmdStr[cmdSize-1] = '\0';
//    printf("formBlockWriteCmd:%s\n",cmdStr);
    return cmdStr;
}

//* Form string: BlockRead/r/nBlockId:blockId/r/n/0
char *formBlockReadCmd(uint64_t blockId){
    char blockIdBuf[1024];
    
    size_t strSize = uint64_to_str(blockId, blockIdBuf, 1024);
    
    size_t cmdSize = strlen(blockReadCmd) + strlen(blockIdStr) + strSize + strlen(cmdSuffix) + 1;
    
//    printf("cmdSize:%lu, strSize:%lu\n",cmdSize, strSize);
    
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, blockReadCmd, strlen(blockReadCmd));
    memcpy((cmdStr + strlen(blockReadCmd) ), blockIdStr, strlen(blockIdStr));
    memcpy((cmdStr + strlen(blockReadCmd) + strlen(blockIdStr) ), blockIdBuf, strSize);
    memcpy((cmdStr + strlen(blockReadCmd) + strlen(blockIdStr) + strSize), cmdSuffix, strlen(cmdSuffix));
    
    cmdStr[cmdSize-1] = '\0';
    printf("formBlockReadCmd:%s\n",cmdStr);
    return cmdStr;
}

char *formWriteRawDataCmd(size_t writeSize){
    char buf[1024];
    
    size_t blockStrSize = strlen(writeSizeCmd);
    size_t strSize = sizeT_to_str(writeSize, buf, 1024);
    size_t suffixSize = strlen(cmdSuffix);
    
    size_t cmdSize = blockStrSize + strSize + suffixSize + 1;
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, writeSizeCmd, blockStrSize);
    memcpy(cmdStr + blockStrSize, buf, strSize);
    memcpy((cmdStr + blockStrSize + strSize), cmdSuffix, suffixSize);
    
    cmdStr[cmdSize - 1] = '\0';
    printf("formWriteRawDataCmd:%s\n",cmdStr);
    return cmdStr;
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
//    printf("formReadRawDataCmd:%s\n",cmdStr);
    return cmdStr;
}

char *formWriteOverCmd(){
    size_t cmdSize = strlen(writeOverCmd) + 1;
    char *cmdStr = talloc(char, cmdSize);
    memcpy(cmdStr, writeOverCmd, cmdSize - 1);
    
    cmdStr[cmdSize - 1] = '\0';
    printf("formWriteOverCmd:%s\n",cmdStr);
    return cmdStr;
}

char *formReadOverCmd(){
    size_t cmdSize = strlen(readOverCmd) + 1;
    char *cmdStr = talloc(char, cmdSize);
    memcpy(cmdStr, readOverCmd, cmdSize - 1);
    
    cmdStr[cmdSize - 1] = '\0';
    printf("formReadOverCmd:%s\n",cmdStr);
    return cmdStr;
}


char *formClosingCmd(){
    size_t cmdSize = strlen(closingCmd) + 1;
    char *cmdStr = talloc(char, cmdSize);
    memcpy(cmdStr, closingCmd, cmdSize - 1);
    
    cmdStr[cmdSize - 1] = '\0';
    printf("formClosingCmd:%s\n",cmdStr);
    return cmdStr;
}















