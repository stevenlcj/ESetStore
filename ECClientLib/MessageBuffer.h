//
//  MessageBuffer.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__MessageBuffer__
#define __ECMeta__MessageBuffer__

#include <stdio.h>

typedef struct ECMessageBuffer{
    char *buf;
    size_t bufSize;
    size_t rOffset;
    size_t wOffset;
    struct ECMessageBuffer *next;
}ECMessageBuffer_t;

ECMessageBuffer_t *createMessageBuf(size_t bufSize);

int msgBufHasChar(ECMessageBuffer_t *ecMsgBuf, char ch);
void dumpMsg(ECMessageBuffer_t *ecMsgBuf);
void printReadBuf(ECMessageBuffer_t *ecMsgBuf);
int dumpMsgWithFlag(ECMessageBuffer_t *ecMsgBuf, const char *strFlag, int flagIdx);
int dumpMsgWithCh(ECMessageBuffer_t *ecMsgBuf, char ch);

void freeMessageBuf(ECMessageBuffer_t *msgBuf);
#endif /* defined(__ECMeta__MessageBuffer__) */
