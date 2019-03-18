//
//  NetIOWorker.h
//  ECStoreCPUServer
//
//  Created by Liu Chengjian on 18/1/9.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __ECStoreCPUServer__NetIOWorker__
#define __ECStoreCPUServer__NetIOWorker__

#include <stdio.h>

#include "RecoveryThreadWorker.h"

void *netDiskIOWorker(void *arg);
#endif /* defined(__ECStoreCPUServer__NetIOWorker__) */
