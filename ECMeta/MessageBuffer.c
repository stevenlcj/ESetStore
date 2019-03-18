//
//  MessageBuffer.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "MessageBuffer.h"
#include "ecCommon.h"

#include <string.h>

ECMessageBuffer_t *createMessageBuf(size_t bufSize){
    if (bufSize <= 0) {
        return NULL;
    }
    
    ECMessageBuffer_t *ecMsgBuf = talloc(ECMessageBuffer_t, 1);
    ecMsgBuf->buf = talloc(char, bufSize);
    ecMsgBuf->swpBuf = talloc(char, bufSize);
    ecMsgBuf->bufSize = bufSize;
    
    ecMsgBuf->rOffset = 0;
    ecMsgBuf->wOffset = 0;
    ecMsgBuf->next = NULL;
    
    return ecMsgBuf;
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
    int idx = 0,flag = 0;
    size_t strSize = strlen(strFlag);
    
    if (strSize == 0) {
        printf("dumpmsg failed: strFlag size is zero\n");
        return -1;
    }
    
//    printf("wOffset:%lu strSize:%lu\n", ecMsgBuf->wOffset, strSize);
    
    while ((idx+strSize) <= ecMsgBuf->wOffset) {
        if ( strncmp((ecMsgBuf->buf+idx), strFlag, strSize) == 0)
        {
            ++flag;
//            printf("flag:%d idx:%d flagIdx:%d\n",flag, idx, flagIdx);
            //            int value = (int) *(ecMsgBuf->buf+ idx + strSize);
            //            char nullValueCh = '\0';
            //            printf("last value:%d, nullValueCh:%d\n", value, nullValueCh);
            if (flag == flagIdx) {
                if (((idx+strSize) != ecMsgBuf->wOffset) &&*(ecMsgBuf->buf+ idx + strSize) == '\0') {
                    //                    printf("= empty ch\n");
                    idx = idx + 1;
                    memcpy(ecMsgBuf->buf, (ecMsgBuf->buf+idx + strSize), (ecMsgBuf->wOffset - (idx + strSize)));
                }else{
                    //                    printf("!= empty ch\n");
                    memcpy(ecMsgBuf->buf, (ecMsgBuf->buf+idx + strSize), (ecMsgBuf->wOffset - (idx + strSize)));
                }
                
                ecMsgBuf->wOffset = (ecMsgBuf->wOffset - (idx + strSize));
                
                printf("idx:%d, wOffset:%lu\n", idx,ecMsgBuf->wOffset);
                return 0;
            }
        }
        
        ++idx;
    }
    
    printf("dumpmsg failed idx:%d, wOffset:%lu\n", idx,ecMsgBuf->wOffset);
    return -1;
}

size_t dumpMsgWithCh(ECMessageBuffer_t *ecMsgBuf, char ch){
    int idx = 0;
    size_t dumpMsgSize = 0;
    size_t tempBufSize = 0;
    for (idx = 0; idx < ecMsgBuf->wOffset; ++idx) {
        if (*(ecMsgBuf->buf + idx) == ch) {

            //printf("dump size:%d, msg:%s\n",idx, ecMsgBuf->buf);
            dumpMsgSize = (size_t) idx + 1;
            if ((idx + 1) == ecMsgBuf->wOffset) {
                ecMsgBuf->wOffset = 0;
                ecMsgBuf->rOffset = 0;
                return dumpMsgSize;
            }
            
            break;
        }
    }

    tempBufSize = ecMsgBuf->wOffset - dumpMsgSize;
    
    if (tempBufSize != 0 )
    {
        memcpy(ecMsgBuf->swpBuf, ecMsgBuf->buf + dumpMsgSize, tempBufSize);
        memcpy(ecMsgBuf->buf, ecMsgBuf->swpBuf, tempBufSize);
        ecMsgBuf->wOffset = tempBufSize;
    } 

    return dumpMsgSize;
}