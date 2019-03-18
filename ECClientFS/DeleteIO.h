//
//  DeleteIO.h
//  ECClientFS
//
//  Created by Liu Chengjian on 18/4/23.
//  Copyright (c) 2018年 csliu. All rights reserved.
//

#ifndef __DeleteIO_Header__

#include <semaphore.h>

#include "ECClientEngine.h"

void startDeleteWork(ECClientEngine_t *clientEngine, char *filesToDelete[], int fileNum);

#define __DeleteIO_Header__
#endif