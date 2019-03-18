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
    size_t rOffset; // rOffset refers to the offset to read data for the buf
    size_t wOffset; // wOffset refers to the offset to write data for the buf
    struct ECMessageBuffer *next;
}ECMessageBuffer_t;

ECMessageBuffer_t *createMessageBuf(size_t bufSize);

int dumpMsgWithFlag(ECMessageBuffer_t *ecMsgBuf, const char *strFlag, int flagIdx);

size_t dumpMsgWithCh(ECMessageBuffer_t *ecMsgBuf, char ch);
void freeMessageBuf(ECMessageBuffer_t *msgBuf);
#endif /* defined(__ECMeta__MessageBuffer__) */
