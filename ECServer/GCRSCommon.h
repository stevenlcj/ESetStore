//
//  GCRSCommon.h
//  NoBitmatrixOpt
//
//  Created by Liu Chengjian on 17/8/28.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef NoBitmatrixOpt_GCRSCommon_h
#define NoBitmatrixOpt_GCRSCommon_h

#define MIN_K 1
#define MAX_K 45

#define MIN_M 1
#define MAX_M 4
#define MIN_W 4
#define MAX_W 8

#define MAX_K_MULIPLY_M (MAX_K * MAX_M)

#define WARM_UP_SIZE 4

#define MAX_THREAD_NUM 128
#define THREADS_PER_BLOCK 128
#define WARP_SIZE 32

#define DEFAULT_GPU_BUF_SIZE (8*1024*1024)

#define DEVICE_SELCTED_ID 0

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define talloc(type, num) (type *)malloc(sizeof(type) * (num))

#endif
