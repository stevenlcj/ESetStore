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
    char *swpBuf;
    
    size_t bufSize;
    size_t rOffset;
    size_t wOffset;
    struct ECMessageBuffer *next;
}ECMessageBuffer_t;

ECMessageBuffer_t *createMessageBuf(size_t bufSize);

size_t dumpMsg(ECMessageBuffer_t *ecMsgBuf);
void printReadBuf(ECMessageBuffer_t *ecMsgBuf);
size_t dumpMsgWithSize(ECMessageBuffer_t *ecMsgBuf, size_t dumpSize);
size_t dumpMsgWithFlag(ECMessageBuffer_t *ecMsgBuf, const char *strFlag, int flagIdx);
size_t dumpMsgWithCh(ECMessageBuffer_t *ecMsgBuf, char ch);
size_t dumpMetaMsgWithSuffix(ECMessageBuffer_t *ecMsgBuf, char suffix);

void freeMessageBuf(ECMessageBuffer_t *msgBuf);
#endif /* defined(__ECMeta__MessageBuffer__) */
