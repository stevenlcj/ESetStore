//
//  TaskDistributorUtilities.c
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/16.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#include "TaskDistributorUtilities.h"

size_t numToStr(char *str, int num){
    char tempStr[1024];
    size_t cmdSize =0;
    int idx;
    while (num > 0) {
        tempStr[cmdSize] = '0' + num%10;
        num = num / 10;
        ++cmdSize;
    }
    
    for (idx=0; idx < cmdSize; ++idx) {
        *(str + idx ) = tempStr[cmdSize - idx - 1];
    }
    
    return  cmdSize;
}
