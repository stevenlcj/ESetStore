//
//  MessageBuffer.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "MessageBuffer.h"
#include "ECClientCommon.h"

#include <string.h>

ECMessageBuffer_t *createMessageBuf(size_t bufSize){
    if (bufSize <= 0) {
        bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    }
    
    ECMessageBuffer_t *ecMsgBuf = talloc(ECMessageBuffer_t, 1);
    ecMsgBuf->buf = talloc(char, bufSize);
    ecMsgBuf->bufSize = bufSize;
    
    ecMsgBuf->rOffset = 0;
    ecMsgBuf->wOffset = 0;
    ecMsgBuf->next = NULL;
    
    return ecMsgBuf;
}

int msgBufHasChar(ECMessageBuffer_t *ecMsgBuf, char ch){
    int idx = 0;
    while(idx < (int)ecMsgBuf->wOffset){
        if (*(ecMsgBuf->buf+ idx) == ch)
        {
            return 1;
        }
        ++idx;
    }

    return 0;
}

int dumpMsgWithCh(ECMessageBuffer_t *ecMsgBuf, char ch){
    size_t idx = 0;
    
    for (idx = 0; idx <  ecMsgBuf->wOffset; ++idx) {
        if (*(ecMsgBuf->buf + idx) == ch) {
            if ((idx + 1)  == ecMsgBuf->wOffset) {
                ecMsgBuf->wOffset = 0;
                return idx;
            }
            
            memcpy(ecMsgBuf->buf, (ecMsgBuf->buf + idx + 1), (ecMsgBuf->wOffset - (idx + 1)));
            ecMsgBuf->wOffset = ecMsgBuf->wOffset - (idx + 1);
        }
    }
    
    return 0;
}

void freeMessageBuf(ECMessageBuffer_t *msgBuf){
    if (msgBuf == NULL) {
        return;
    }
    
    if (msgBuf->bufSize != 0) {
        free(msgBuf->buf);
    }
    
    free(msgBuf);
}

int dumpMsgWithFlag(ECMessageBuffer_t *ecMsgBuf, const char *strFlag, int flagIdx){
    size_t idx = 0,flag = 0;
    size_t strSize = strlen(strFlag);
    
//    printf("wOffset:%lu strSize:%lu\n", ecMsgBuf->wOffset, strSize);
    
    while ((idx+strSize) <= ecMsgBuf->wOffset) {
        if ( strncmp((ecMsgBuf->buf+idx), strFlag, strSize) == 0)
        {
            ++flag;
//            printf("flag:%d idx:%d flagIdx:%d\n",flag, idx, flagIdx);
//            int value = (int) *(ecMsgBuf->buf+ idx + strSize);
//            char nullValueCh = '\0';
//            printf("last value:%d, nullValueCh:%d\n", value, nullValueCh);
            if ((int)flag == flagIdx) {
                if (((idx+strSize) != ecMsgBuf->wOffset) &&*(ecMsgBuf->buf+ idx + strSize) == '\0') {
//                    printf("= empty ch\n");
                    idx = idx + 1;
                    memcpy(ecMsgBuf->buf, (ecMsgBuf->buf+idx + strSize), (ecMsgBuf->wOffset - (idx + strSize)));
                }else{
//                    printf("!= empty ch\n");
                    memcpy(ecMsgBuf->buf, (ecMsgBuf->buf+idx + strSize), (ecMsgBuf->wOffset - (idx + strSize)));
                }

                ecMsgBuf->wOffset = (ecMsgBuf->wOffset - (idx + strSize));

//                printf("idx:%d, wOffset:%lu\n", idx,ecMsgBuf->wOffset);
                return 0;
            }
        }
        
        ++idx;
    }
    
//    printf("dumpmsg failed idx:%d, wOffset:%lu\n", idx,ecMsgBuf->wOffset);
    return -1;
}

void dumpMsg(ECMessageBuffer_t *ecMsgBuf){
    
//    printf("rOffset:%lu, wOffset:%lu before dumpMsg\n", ecMsgBuf->rOffset, ecMsgBuf->wOffset);
    
    if (ecMsgBuf->rOffset != ecMsgBuf->wOffset) {
        memcpy(ecMsgBuf->buf, (ecMsgBuf->buf+ecMsgBuf->rOffset), (ecMsgBuf->wOffset - ecMsgBuf->rOffset));
    }
    
    ecMsgBuf->wOffset = (ecMsgBuf->wOffset - ecMsgBuf->rOffset);
    ecMsgBuf->rOffset = 0;
    
//    printf("rOffset:%lu, wOffset:%lu after dumpMsg\n", ecMsgBuf->rOffset, ecMsgBuf->wOffset);
}

void printReadBuf(ECMessageBuffer_t *ecMsgBuf){
    printf("******printReadBuf*****");
    ssize_t idx = 0;
    while (idx != (int)ecMsgBuf->wOffset) {
        char ch = *(ecMsgBuf->buf + idx);
        printf("%c", ch);
        ++idx;
    }
    printf("\n");
}