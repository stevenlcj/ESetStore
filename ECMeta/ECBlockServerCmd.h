//
//  ECBlockServerCmd.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/11/23.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__ECBlockServerCmd__
#define __ECMeta__ECBlockServerCmd__

#include <stdio.h>
#include <stdint.h>
/**
 Msg between meta and server:
    "HeartBeat\r\n\0" -> server  "HeartBeatOK\r\n\0" -> meta
 
*/
size_t writeRecoveryBlockGroupCmd(void *listPtr, void *blockGroup, void *blockServerPtr);

size_t writeRecoveryCmd(void *listPtr, void *enginePtr, void *blockServerPtr);
size_t writeRecoveryOverCmd(void *listPtr, void *blockServerPtr);

size_t writeDeleteBlockCmd(void *blockPtr, void *blockServerPtr);
#endif /* defined(__ECMeta__ECBlockServerCmd__) */
