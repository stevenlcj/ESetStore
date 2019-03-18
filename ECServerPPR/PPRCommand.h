//
//  PPRCommand.h
//  ESetStorePPR
//
//  Created by Liu Chengjian on 18/8/9.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ESetStorePPR__PPRCommand__
#define __ESetStorePPR__PPRCommand__

#include <stdio.h>
#include <stdint.h>

size_t formPPRCmd(char *buf, size_t BufSize, void *pprPtr);

size_t formNextPPRCmd(char *cmdBuf, char *buf, size_t BufSize, int sizeValue);

size_t formPPRRecoveryCmd(char *buf, size_t bufSize, uint64_t blockGroupIdx, size_t blockSize);
#endif /* defined(__ESetStorePPR__PPRCommand__) */
