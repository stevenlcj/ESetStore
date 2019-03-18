//
//  common.h
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef EStoreMeta_common_h
#define EStoreMeta_common_h

#include <stdlib.h>

#define EC_DEBUG_MODE

#define MAX_CFG_KEY_VALUE_SIZE 255

#define DEFAULT_CHUNK_SERVER_PORT 30000
#define DEFAULT_CLIENT_PORT 20000

#define MAX_CLIENT_CONN_NUM 1024
#define MAX_CHUNK_CONN_NUM 10240
#define MAX_CLIENT_THREAD_NUM 16
#define MAX_EVENTS 16

#define DEFAULT_READ_WRITE_BUF_SIZE (1*(1<<20)) //1 MB for conn read write buf

#define DEFAULT_DISK_IO_JOB_STEP 10
#define DEFAULT_DISK_IO_JOB_BUF_SIZE (1<<18) //256 K

#define DEFAULT_TIMEOUT_IN_Millisecond 500
#define talloc(type, num) (type *)malloc(sizeof(type) * num)

#define RACK_INCRE_SIZE 10
#define SERVER_INCRE_SIZE 10

#define DISK_IO_PTR_SIZE 1024

#define FILE_NAME_BUF_SIZE 1024

#define MAX_CONN_EVENTS 8

#define DEFAUT_MESSGAE_READ_WRITE_BUF 1024

#define DEFAULT_RECOVERY_BUF_NUM 2

typedef enum {
    BLOCK_SERVER_STATE_INIT = 0,
    BLOCK_SERVER_STATE_WORKING = 1,
    BLOCK_SERVER_STATE_DOWN = 2,
    BLOCK_SERVER_STATE_RECOVER = 3
}BlockServer_State;

#endif
