//
//  ECClientResponse.h
//  ECServer
//
//  Created by Liu Chengjian on 18/4/16.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ECServer__ECClientResponse__
#define __ECServer__ECClientResponse__

#include <stdio.h>
#include "stdint.h"

size_t constructCreateOKCmd(char *buf, size_t bufSize, uint64_t blockId, int blockFd);
size_t constructCreateFailedCmd(char *buf, size_t bufSize, uint64_t blockId, char *msg, size_t msgSize);

size_t constructOpenOKCmd(char *buf, size_t bufSize, uint64_t blockId, int blockFd);
size_t constructOpenFailedCmd(char *buf, size_t bufSize, uint64_t blockId, char *msg, size_t msgSize);

char *formCreateFileCmd(const char *fileName);
char *formOpenFileCmd(const char *fileName);
char *formReadFileCmd(const char *fileName);

char *formDeleteFileCmd(const char *fileName);

char *formMetaWriteSizeCmd(const char *fileName, size_t writedSize);

char *formBlockWriteCmd(uint64_t blockId);

size_t constructOpenBlockCmd(char *cmdBuf, uint64_t blockId);
size_t constructBlockCloseCmd(char *cmdBuf, uint64_t blockId, int blockFd);
size_t constructBlockReadCmd(char *cmdBuf, uint64_t blockId, int blockFd, size_t writeSize);

char *formWriteRawDataCmd(size_t writeSize);

char *formReadRawDataCmd(size_t readSize);

char *formWriteOverCmd();
char *formReadOverCmd();

char *formClosingCmd();
#endif