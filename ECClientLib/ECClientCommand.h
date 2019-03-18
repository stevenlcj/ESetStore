//
//  ECClientCommand.h
//  ECClient
//
//  Created by Liu Chengjian on 17/9/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECClient__ECClientCommand__
#define __ECClient__ECClientCommand__

#include <stdio.h>
#include <stdint.h>

char *formCreateFileCmd(const char *fileName);
char *formOpenFileCmd(const char *fileName);
char *formReadFileCmd(const char *fileName);

char *formDeleteFileCmd(const char *fileName);

char *formMetaWriteSizeCmd(const char *fileName, size_t writedSize);

size_t constructCreateBlockCmd(char *cmdBuf, uint64_t blockId);
size_t constructOpenBlockCmd(char *cmdBuf, uint64_t blockId);

size_t constructBlockWriteCmd(char *cmdBuf, uint64_t blockId, int blockFd, size_t writeSize);
size_t constructBlockReadCmd(char *cmdBuf, uint64_t blockId, int blockFd, size_t writeSize);
size_t constructBlockCloseCmd(char *cmdBuf, uint64_t blockId, int blockFd);

size_t formBlockWriteCmd(char *cmdBuf, uint64_t blockId);
char *formBlockReadCmd(uint64_t blockId);

size_t formWriteRawDataCmd(char *cmdBuf, size_t writeSize);

char *formReadRawDataCmd(size_t readSize);

size_t formWriteOverCmd(char *cmdBuf);
size_t formReadOverCmd(char *cmdBuf);

char *formClosingCmd();
#endif /* defined(__ECClient__ECClientCommand__) */
