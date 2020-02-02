//
//  ECClientResponse.c
//  ECServer
//
//  Created by Liu Chengjian on 18/4/16.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#include "ECClientResponse.h"
#include "ecUtils.h"
#include "ecCommon.h"

#include <string.h>

const char blockCreateOK[] = "BlockCreateOK\0";
const char blockCreateFailed[] = "BlockCreateFailed\0";
const char blockOpenOK[] = "BlockOpenOK\0";
const char blockOpenFailed[] = "BlockOpenFailed\0";
const char blockFdStr[] = "BlockFd:\0";
const char blockIdStr[] = "BlockId:\0";
const char ErrorMsg[] = "ErrorMsg:\0";
const char cmdSuffixStr[] = "\r\n\0";

const char createCmd[] = "BlockCreate\r\n\0";
const char openCmd[] = "BlockOpen\r\n\0";
const char readCmd[] = "writeOffset\r\n\0";
const char closeCmd[] = "BlockClose\0";

const char deleteCmd[] = "Delete\r\n\0";

const char blockWriteCmd[] = "BlockWrite\r\n\0";
const char blockReadCmd[] = "BlockRead\r\n\0";

const char writeSizeCmd[] = "WriteSize:\0";
const char writeOverCmd[] = "WriteOver\r\n\0";

const char readSizeCmd[] = "ReadSize:\0";
const char readOverCmd[] = "ReadOver\r\n\0";

const char metaWriteOverCmd[] = "WriteOver:\0";

const char closingCmd[] = "Closing\r\n\r\n\0";

//BlockCreateOK\r\nBlockId:__\r\nBlockFd:__\r\n\0
size_t constructCreateOKCmd(char *buf, size_t bufSize, uint64_t blockId, int blockFd){
	size_t tmpBufSize = 100;
	char tempBuf[tmpBufSize];
	size_t writeBackOffset = 0, curWriteSize = 0;

	writeToBuf(buf, (char *) blockCreateOK,  &curWriteSize, &writeBackOffset);
	writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);

	writeToBuf(buf, (char *) blockIdStr,  &curWriteSize, &writeBackOffset);
	uint64_to_str(blockId, tempBuf, tmpBufSize);
	writeToBuf(buf, (char *) tempBuf,  &curWriteSize, &writeBackOffset);
	writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);

	writeToBuf(buf, (char *) blockFdStr,  &curWriteSize, &writeBackOffset);
	int_to_str(blockFd, tempBuf, tmpBufSize);
	writeToBuf(buf, (char *) tempBuf,  &curWriteSize, &writeBackOffset);
	writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);

	if (writeBackOffset >= bufSize)
	{
		printf("bufSize is not enough for constructCreateOKCmd\n");
		return 0;
	}

	buf[writeBackOffset] = '\0';
	writeBackOffset = writeBackOffset + 1;
	//printf("Constructed createOKCmd cmd size:%lu cmd:%s\n",writeBackOffset, buf);
	return writeBackOffset;
}

//BlockCreateFailed\r\nBlockId:__\r\nBlockFd:__\r\n\0
size_t constructCreateFailedCmd(char *buf, size_t bufSize, uint64_t blockId, char *msg, size_t msgSize){
	size_t tmpBufSize = 100;
	char tempBuf[tmpBufSize];

	size_t writeBackOffset = 0, curWriteSize = 0;

	writeToBuf(buf, (char *) blockCreateFailed,  &curWriteSize, &writeBackOffset);
	writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);

	uint64_to_str(blockId, tempBuf, tmpBufSize);
	writeToBuf(buf, (char *) tempBuf,  &curWriteSize, &writeBackOffset);
	writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);
	
	writeToBuf(buf, (char *) ErrorMsg,  &curWriteSize, &writeBackOffset);
	if (msgSize > 0)
	{
		writeToBuf(buf, (char *) msg,  &curWriteSize, &writeBackOffset);
	}

	writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);

	if (writeBackOffset >= bufSize)
	{
		printf("bufSize is not enough for constructCreateFailedCmd\n");
		return 0;
	}

	buf[writeBackOffset] = '\0';
	writeBackOffset = writeBackOffset + 1;
	//printf("Constructed createFailedCmd:%s\n", buf);
	return writeBackOffset;
}

//BlockOpenOK\r\nBlockId:__\r\nBlockFd:__\r\n\0
size_t constructOpenOKCmd(char *buf, size_t bufSize, uint64_t blockId, int blockFd){
    size_t tmpBufSize = 100;
    char tempBuf[tmpBufSize];
    size_t writeBackOffset = 0, curWriteSize = 0;

    writeToBuf(buf, (char *) blockOpenOK,  &curWriteSize, &writeBackOffset);
    writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);

    writeToBuf(buf, (char *) blockIdStr,  &curWriteSize, &writeBackOffset);
    uint64_to_str(blockId, tempBuf, tmpBufSize);
    writeToBuf(buf, (char *) tempBuf,  &curWriteSize, &writeBackOffset);
    writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);

    writeToBuf(buf, (char *) blockFdStr,  &curWriteSize, &writeBackOffset);
    int_to_str(blockFd, tempBuf, tmpBufSize);
    writeToBuf(buf, (char *) tempBuf,  &curWriteSize, &writeBackOffset);
    writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);

    if (writeBackOffset >= bufSize)
    {
        printf("bufSize is not enough for constructOpenOKCmd\n");
        return 0;
    }

    buf[writeBackOffset] = '\0';
    writeBackOffset = writeBackOffset + 1;
    printf("Constructed constructOpenOKCmd cmd size:%lu cmd:%s\n",writeBackOffset, buf);
    return writeBackOffset;
}

//BlockOpenFailed\r\nBlockId:__\r\nBlockFd:__\r\n\0
size_t constructOpenFailedCmd(char *buf, size_t bufSize, uint64_t blockId, char *msg, size_t msgSize){
    size_t tmpBufSize = 100;
    char tempBuf[tmpBufSize];

    size_t writeBackOffset = 0, curWriteSize = 0;

    writeToBuf(buf, (char *) blockOpenFailed,  &curWriteSize, &writeBackOffset);
    writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);

    writeToBuf(buf, (char *) blockId,  &curWriteSize, &writeBackOffset);
    uint64_to_str(blockId, tempBuf, tmpBufSize);
    writeToBuf(buf, (char *) tempBuf,  &curWriteSize, &writeBackOffset);
    writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);
    
    writeToBuf(buf, (char *) ErrorMsg,  &curWriteSize, &writeBackOffset);
    if (msgSize > 0)
    {
        writeToBuf(buf, (char *) msg,  &curWriteSize, &writeBackOffset);
    }

    writeToBuf(buf, (char *) cmdSuffixStr,  &curWriteSize, &writeBackOffset);

    if (writeBackOffset >= bufSize)
    {
        printf("bufSize is not enough for constructOpenFailedCmd\n");
        return 0;
    }

    buf[writeBackOffset] = '\0';
    writeBackOffset = writeBackOffset + 1;
    printf("Constructed constructOpenFailedCmd:%s\n", buf);
    return writeBackOffset;
}

char *formCreateFileCmd(const char *fileName){
    
    if (fileName == NULL) {
        return NULL;
    }
    
    size_t cmdSize = strlen(createCmd) + strlen(fileName) + strlen(cmdSuffixStr) + 1;
    
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, createCmd, strlen(createCmd));
    memcpy((cmdStr + strlen(createCmd)), fileName, strlen(fileName));
    memcpy((cmdStr + strlen(createCmd)+ strlen(fileName)), cmdSuffixStr, strlen(cmdSuffixStr));
    
    cmdStr[cmdSize - 1] = '\0';
    printf("createCmd:%s\n",cmdStr);
    return cmdStr;
}

char *formDeleteFileCmd(const char *fileName){
    
    if (fileName == NULL) {
        return NULL;
    }
    
    size_t cmdSize = strlen(deleteCmd) + strlen(fileName) + strlen(cmdSuffixStr) + 1;
    
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, deleteCmd, strlen(deleteCmd));
    memcpy((cmdStr + strlen(deleteCmd)), fileName, strlen(fileName));
    memcpy((cmdStr + strlen(deleteCmd)+ strlen(fileName)), cmdSuffixStr, strlen(cmdSuffixStr));
    
    cmdStr[cmdSize - 1] = '\0';
    printf("deleteCmd:%s\n",cmdStr);
    return cmdStr;
}

char *formOpenFileCmd(const char *fileName){
    
    if (fileName == NULL) {
        return NULL;
    }
    
    size_t cmdSize = strlen(openCmd) + strlen(fileName) + strlen(cmdSuffixStr) + 1;
    
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, openCmd, strlen(openCmd));
    memcpy((cmdStr + strlen(openCmd)), fileName, strlen(fileName));
    memcpy((cmdStr + strlen(openCmd)+ strlen(fileName)), cmdSuffixStr, strlen(cmdSuffixStr));
    
    cmdStr[cmdSize - 1] = '\0';
    printf("openCmd:%s\n",cmdStr);
    return cmdStr;
}

char *formReadFileCmd(const char *fileName){
    
    if (fileName == NULL) {
        return NULL;
    }
    
    size_t cmdSize = strlen(readCmd) + strlen(fileName) + strlen(cmdSuffixStr) + 1;
    
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, readCmd, strlen(readCmd));
    memcpy((cmdStr + strlen(readCmd)), fileName, strlen(fileName));
    memcpy((cmdStr + strlen(readCmd)+ strlen(fileName)), cmdSuffixStr, strlen(cmdSuffixStr));
    
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
    
    size_t cmdSize = strlen(metaWriteOverCmd) + strlen(fileName) + strlen(cmdSuffixStr) + strlen(writeSizeCmd) + strSize + strlen(cmdSuffixStr) + 1;
    
    char *cmdStr = talloc(char, cmdSize);
    memcpy(cmdStr, metaWriteOverCmd, strlen(metaWriteOverCmd));
    memcpy((cmdStr + strlen(metaWriteOverCmd)), fileName, strlen(fileName));
    memcpy((cmdStr + strlen(metaWriteOverCmd)+ strlen(fileName)), cmdSuffixStr, strlen(cmdSuffixStr));
    memcpy((cmdStr + strlen(metaWriteOverCmd)+ strlen(fileName) + strlen(cmdSuffixStr)), writeSizeCmd, strlen(writeSizeCmd));
    memcpy((cmdStr + strlen(metaWriteOverCmd)+ strlen(fileName) + strlen(cmdSuffixStr) + strlen(writeSizeCmd)), buf, strSize);
    memcpy((cmdStr + strlen(metaWriteOverCmd)+ strlen(fileName) + strlen(cmdSuffixStr) + strlen(writeSizeCmd)+strSize), cmdSuffixStr, strlen(cmdSuffixStr));

    *(cmdStr + cmdSize - 1) = '\0';
    printf("formMetaWriteSizeCmd:%s\n",cmdStr);
    return cmdStr;
}

//* Form string: BlockWrite/r/nBlockId:blockId/r/n/0

char *formBlockWriteCmd(uint64_t blockId){
    char blockIdBuf[1024];
    
    size_t strSize = uint64_to_str(blockId, blockIdBuf, 1024);
    
    size_t cmdSize = strlen(blockWriteCmd) + strlen(blockIdStr) + strSize + strlen(cmdSuffixStr) + 1;
    
//    printf("cmdSize:%lu, strSize:%lu\n",cmdSize, strSize);
    
    char *cmdStr = talloc(char, cmdSize);

    memcpy(cmdStr, blockWriteCmd, strlen(blockWriteCmd));
    memcpy((cmdStr + strlen(blockWriteCmd) ), blockIdStr, strlen(blockIdStr));
    memcpy((cmdStr + strlen(blockWriteCmd) + strlen(blockIdStr) ), blockIdBuf, strSize);
    memcpy((cmdStr + strlen(blockWriteCmd) + strlen(blockIdStr) + strSize), cmdSuffixStr, strlen(cmdSuffixStr));
    
    cmdStr[cmdSize-1] = '\0';
//    printf("formBlockWriteCmd:%s\n",cmdStr);
    return cmdStr;
}

size_t constructOpenBlockCmd(char *cmdBuf, uint64_t blockId){
    char blockIdBuf[1024];
    size_t writeSize = 0, writeOffset = 0;

    writeToBuf(cmdBuf, (char *)openCmd, &writeSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffixStr, &writeSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockIdStr, &writeSize, &writeOffset);
    uint64_to_str(blockId, blockIdBuf, 1024);
    writeToBuf(cmdBuf, (char *)blockIdBuf, &writeSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffixStr, &writeSize, &writeOffset);

    cmdBuf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
    printf("constructOpenBlockCmd cmd size:%lu cmd content:%s\n", writeOffset, cmdBuf);
    return writeOffset;
}

//* Form string: BlockClose/r/nBlockId:__/r/nBlockFd:__/r/n/0
size_t constructBlockCloseCmd(char *cmdBuf, uint64_t blockId, int blockFd){
    char tempBuf[1024];
    size_t wSize = 0, writeOffset = 0;

    writeToBuf(cmdBuf, (char *)closeCmd, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffixStr, &wSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockIdStr, &wSize, &writeOffset);
    uint64_to_str(blockId, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffixStr, &wSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockFdStr, &wSize, &writeOffset);
    uint64_to_str(blockFd, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffixStr, &wSize, &writeOffset);

    cmdBuf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
    //printf("constructBlockCloseCmd:%s\n",cmdBuf);
    return writeOffset;
}

size_t constructBlockReadCmd(char *cmdBuf, uint64_t blockId, int blockFd, size_t writeSize){
    char tempBuf[1024];
    size_t wSize = 0, writeOffset = 0;

    writeToBuf(cmdBuf, (char *)blockReadCmd, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffixStr, &wSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockIdStr, &wSize, &writeOffset);
    uint64_to_str(blockId, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffixStr, &wSize, &writeOffset);

    writeToBuf(cmdBuf, (char *)blockFdStr, &wSize, &writeOffset);
    uint64_to_str(blockFd, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffixStr, &wSize, &writeOffset);


    writeToBuf(cmdBuf, (char *)readSizeCmd, &wSize, &writeOffset);
    sizeT_to_str(writeSize, tempBuf, 1024);
    writeToBuf(cmdBuf, (char *)tempBuf, &wSize, &writeOffset);
    writeToBuf(cmdBuf, (char *)cmdSuffixStr, &wSize, &writeOffset);

    cmdBuf[writeOffset] = '\0';
    writeOffset = writeOffset + 1;
    printf("constructBlockReadCmd:%s\n",cmdBuf);
    return writeOffset;
}
char *formWriteRawDataCmd(size_t writeSize){
    char buf[1024];
    
    size_t blockStrSize = strlen(writeSizeCmd);
    size_t strSize = sizeT_to_str(writeSize, buf, 1024);
    size_t suffixSize = strlen(cmdSuffixStr);
    
    size_t cmdSize = blockStrSize + strSize + suffixSize + 1;
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, writeSizeCmd, blockStrSize);
    memcpy(cmdStr + blockStrSize, buf, strSize);
    memcpy((cmdStr + blockStrSize + strSize), cmdSuffixStr, suffixSize);
    
    cmdStr[cmdSize - 1] = '\0';
    printf("formWriteRawDataCmd:%s\n",cmdStr);
    return cmdStr;
}

char *formReadRawDataCmd(size_t readSize){
    char buf[1024];
    
    size_t blockStrSize = strlen(readSizeCmd);
    size_t strSize = sizeT_to_str(readSize, buf, 1024);
    size_t suffixSize = strlen(cmdSuffixStr);
    
    size_t cmdSize = blockStrSize + strSize + suffixSize + 1;
    char *cmdStr = talloc(char, cmdSize);
    
    memcpy(cmdStr, readSizeCmd, blockStrSize);
    memcpy(cmdStr + blockStrSize, buf, strSize);
    memcpy((cmdStr + blockStrSize + strSize), cmdSuffixStr, suffixSize);
    
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
