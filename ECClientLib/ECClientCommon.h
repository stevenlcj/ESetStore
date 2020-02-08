//
//  ECClientCommon.h
//  ECClient
//
//  Created by Liu Chengjian on 17/9/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef ECClient_Header_h
#define ECClient_Header_h

#include <stdio.h>
#include <stdlib.h>

#define talloc(type, num) (type *)malloc(sizeof(type) * num)

#define DEFAULT_FILE_SIZE (100*1024)

#define DEFAULT_BLOCK_READ_WRITE_BUF_SIZE (8 * 1024 * 1024)

#define DEFAULT_STREAMING_UNIT_INBYTES (1024)

#define DEFAUT_MESSGAE_READ_WRITE_BUF (64 * 1024)

#define MAX_CONN_EVENTS 8

#define CONNECTING_TIME_OUT_IN_MS 10000

#define DEFAULT_WORKER_BUF_SIZE (256 * 1024)

#define DEFAULT_BUF_SIZE 2

#define MAX_IN_FLIGHT_REQ 2

#define MAX_EVENTS MAX_CONN_EVENTS

#ifdef DEBUG_ESetStoreCLIENTS
#define PRINT_ROUTINE { \
char theHost[1024]; \
gethostname(theHost, 1024);\
printf("host:%s, func:%s\n",theHost, __FUNCTION__);\
}

#define PRINT_ROUTINE_MSG(msg) { \
char theHost[1024]; \
gethostname(theHost, 1024);\
printf("host:%s, func:%s, msg:%s\n",theHost, __FUNCTION__,msg);\
}

#define PRINT_ROUTINE_VALUE(value) { \
char theHost[1024]; \
gethostname(theHost, 1024);\
printf("host:%s, func:%s\, value:%d\n",theHost, __FUNCTION__,value);\
}

#else

#define PRINT_ROUTINE
#define PRINT_ROUTINE_MSG(msg)
#define PRINT_ROUTINE_VALUE(value)

#endif

#endif
