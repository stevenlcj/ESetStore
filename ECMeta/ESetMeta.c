//
//  main.c
//  ESetMeta
//
//  Created by Liu Chengjian on 17/7/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include <stdio.h>
#include "ECMetaEngine.h"

int main(int argc, const char * argv[]) {
    // insert code here...
    
    if (argc < 2) {
        printf("Please specify the config file\n");
        return -1;
    }
    
    startMeta(argv[1]);
    
    return 0;
}
