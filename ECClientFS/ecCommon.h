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

#define K_SIZE 10
#define BUF_NUM 2
#define EACH_BUF_SIZE (256 * 1024)

#define MAX_CFG_KEY_VALUE_SIZE 255

#define DEFAULT_CHUNK_SERVER_PORT 30000
#define DEFAULT_CLIENT_PORT 20000

#define MAX_CLIENT_CONN_NUM 1024
#define MAX_CHUNK_CONN_NUM 10240

#define DEFAULT_RECOVERY_TIME_INTERVAL 10

#define MAX_CLIENT_THREAD_NUM 16
#define MAX_EVENTS 16
#define DEFAULT_READ_WRITE_BUF_SIZE (64*(1<<10)) //64 KB for conn read write buf

#define DEFAULT_TIMEOUT_IN_Millisecond 500
#define DEFAULT_TIMEOUT_IN_MillisecondForBlockServers 1000
#define talloc(type, num) (type *)malloc(sizeof(type) * num)

#define DEFAULT_CLIENT_THREAD_NUM 8
#define CLIENT_CONN_SIZE (1024 * 128)

#define MAX_CONN_EVENTS 8

#define RACK_INCRE_SIZE 10
#define SERVER_INCRE_SIZE 10

#define ITEM_ALLOC_SIZE 10

#define DEFAULT_KEY_WORKER_THREAD_NUM 8

#define KEY_CHAIN_SIZE (10 * 1024 * 1024)

#define ECSTORE_ITEM_SIZE_INCRE_SIZE (1024 * 1024 * 1024)
#define ECSTORE_ITEM_SIZE_REQ_SIZE (128 * 1024 * 1024)

#define ITEM_SLAB_BASE_SIZE (32)
#define ITEM_SLAB_POWER (1.5)
#define ITEM_SLAB_MAX_SIZE (1024*1024)

#define DEFAUT_MESSGAE_READ_WRITE_BUF 1024

#define EC_BLOCKGROUP_START_OFFSET 8

#define EC_BLOCKSLAB_INCRE_SIZE (1024*1024)
#define EC_THREADBLOCKSLAB_ALLOC_SIZE (1024)

typedef enum {
    BLOCK_SERVER_STATE_INIT = 0,
    BLOCK_SERVER_STATE_WORKING = 1,
    BLOCK_SERVER_STATE_DOWN = 2,
    BLOCK_SERVER_STATE_RECOVER = 3
}BlockServer_State;

#endif
