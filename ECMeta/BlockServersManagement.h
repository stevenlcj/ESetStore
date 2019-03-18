//
//  BlockServersManagement.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/8/17.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECMeta__BlockServersManagement__
#define __ECMeta__BlockServersManagement__

#include <stdio.h>
#include "BlockServer.h"

void *startBlockServersManagement(void *arg);

ssize_t writeToConn(BlockServerManager_t *blockServerMgr, BlockServer_t *blockServerPtr);
#endif /* defined(__ECMeta__BlockServersManagement__) */
