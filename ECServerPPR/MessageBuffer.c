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
#include <stdlib.h>

ECMessageBuffer_t *createMessageBuf(size_t bufSize){
    if (bufSize <= 0) {
        bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    }
    
    ECMessageBuffer_t *ecMsgBuf = talloc(ECMessageBuffer_t, 1);
    ecMsgBuf->buf =  talloc(char, bufSize);
    ecMsgBuf->swpBuf = talloc(char, bufSize);
    ecMsgBuf->bufSize = bufSize;
    
    ecMsgBuf->rOffset = 0;
    ecMsgBuf->wOffset = 0;
    ecMsgBuf->next = NULL;
    
    return ecMsgBuf;
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
            //memcpy(ecMsgBuf->buf, (ecMsgBuf->buf + idx + 1), (ecMsgBuf->wOffset - (idx + 1)));
            //ecMsgBuf->wOffset = ecMsgBuf->wOffset - (idx + 1);

            //break;
        }
    }

    tempBufSize = ecMsgBuf->wOffset - dumpMsgSize;
    
    if (tempBufSize != 0 )
    {
        memcpy(ecMsgBuf->swpBuf, ecMsgBuf->buf + dumpMsgSize, tempBufSize);
        memcpy(ecMsgBuf->buf, ecMsgBuf->swpBuf, tempBufSize);
        ecMsgBuf->wOffset = tempBufSize;

        if (ecMsgBuf->rOffset >= dumpMsgSize)
        {
            ecMsgBuf->rOffset = ecMsgBuf->rOffset - dumpMsgSize;
        }
    } 

    return dumpMsgSize;
}

size_t dumpMetaMsgWithSuffix(ECMessageBuffer_t *ecMsgBuf, char suffix){
    int idx = 0;
    size_t dumpMsgSize = 0;
    size_t tempBufSize = 0;
    for (idx = 0; idx < ecMsgBuf->wOffset; ++idx) {
        if (*(ecMsgBuf->buf + idx) == suffix) {

            printf("dump size:%d, msg:%s\n",idx, ecMsgBuf->buf);
            dumpMsgSize = (size_t) idx + 1;
            if ((idx + 1)  == ecMsgBuf->wOffset) {
                ecMsgBuf->wOffset = 0;
                return dumpMsgSize;
            }
            
            break;
            //break;
        }
    }

    if (dumpMsgSize == 0)
    {
        return dumpMsgSize;
    }
    
    tempBufSize = ecMsgBuf->wOffset - dumpMsgSize;
    
    if (tempBufSize != 0 )
    {
        memcpy(ecMsgBuf->swpBuf, ecMsgBuf->buf + dumpMsgSize, tempBufSize);
        memcpy(ecMsgBuf->buf, ecMsgBuf->swpBuf, tempBufSize);
    } 

    ecMsgBuf->wOffset = tempBufSize;

    return dumpMsgSize;
}

void freeMessageBuf(ECMessageBuffer_t *msgBuf){
    if (msgBuf == NULL) {
        return;
    }
    
    if (msgBuf->bufSize != 0) {
        free(msgBuf->buf);
        free(msgBuf->swpBuf);
    }
    
    free(msgBuf);
}

size_t dumpMsgWithSize(ECMessageBuffer_t *ecMsgBuf, size_t dumpSize){
    size_t tempBufSize = ecMsgBuf->wOffset - dumpSize;

    if (dumpSize > ecMsgBuf->wOffset)
    {
        printf("Error wOffset:%lu dumpSize:%lu\n",ecMsgBuf->wOffset, dumpSize);
    }

    //printf("ecMsgBuf->wOffset:%lu, dumpSize:%lu\n", ecMsgBuf->wOffset, dumpSize);
    if (tempBufSize == 0)
    {
        ecMsgBuf->wOffset = 0;
        return dumpSize;
    }

    /*int idx;
    char bStr[]="BlockWrite\0";
    for (idx = 0; idx < ecMsgBuf->wOffset; ++idx)
    {
        if (strncmp(ecMsgBuf->buf+idx, bStr, strlen(bStr)) == 0)
        {
            printf("Found cmd in idx:%d\n",idx);
            printf("Cmd:%s\n", ecMsgBuf->buf+idx);
        }
    }*/

    memcpy(ecMsgBuf->swpBuf, ecMsgBuf->buf + dumpSize, tempBufSize);
    memcpy(ecMsgBuf->buf, ecMsgBuf->swpBuf, tempBufSize);
    ecMsgBuf->wOffset = tempBufSize;

    //printf("ecMsgBuf->wOffset:%lu after dumMsgWithSize\n", ecMsgBuf->wOffset);
    //printf("Msg:%s\n",ecMsgBuf->buf );

    return dumpSize;
}

size_t dumpMsgWithFlag(ECMessageBuffer_t *ecMsgBuf, const char *strFlag, int flagIdx){
    int idx = 0,flag = 0;
    size_t strSize = strlen(strFlag);
    size_t dumpSize = 0;
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

                dumpSize = dumpSize + (idx + strSize);
                ecMsgBuf->wOffset = (ecMsgBuf->wOffset - (idx + strSize));

//                printf("idx:%d, wOffset:%lu\n", idx,ecMsgBuf->wOffset);
                return dumpSize;
            }
        }
        
        ++idx;
    }
    
//    printf("dumpmsg failed idx:%d, wOffset:%lu\n", idx,ecMsgBuf->wOffset);
    return dumpSize;
}

size_t dumpMsg(ECMessageBuffer_t *ecMsgBuf){
    
//    printf("rOffset:%lu, wOffset:%lu before dumpMsg\n", ecMsgBuf->rOffset, ecMsgBuf->wOffset);
    size_t dumpSize = 0;

    if (ecMsgBuf->rOffset != ecMsgBuf->wOffset) {
        memcpy(ecMsgBuf->buf, (ecMsgBuf->buf+ecMsgBuf->rOffset), (ecMsgBuf->wOffset - ecMsgBuf->rOffset));
    }
    
    dumpSize = ecMsgBuf->rOffset;
    ecMsgBuf->wOffset = (ecMsgBuf->wOffset - ecMsgBuf->rOffset);
    ecMsgBuf->rOffset = 0;
    
//    printf("rOffset:%lu, wOffset:%lu after dumpMsg\n", ecMsgBuf->rOffset, ecMsgBuf->wOffset);

    return dumpSize;
}

void printReadBuf(ECMessageBuffer_t *ecMsgBuf){
    printf("******printReadBuf*****");
    ssize_t idx = 0;
    while (idx != ecMsgBuf->wOffset) {
        char ch = *(ecMsgBuf->buf + idx);
        printf("%c", ch);
        ++idx;
    }
    printf("\n");
}