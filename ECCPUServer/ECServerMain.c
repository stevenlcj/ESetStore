//
//  main.c
//  ECServer
//
//  Created by Liu Chengjian on 17/7/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include <stdio.h>
#include "BlockServerEngine.h"

int main(int argc, const char * argv[]) {
    // insert code here...
//    printf("Hello, World!\n");
    if (argc < 2) {
        printf("Please specify the configuration file for block server\n");
        return -1;
    }
    
    startBlockServer(argv[1]);
    return 0;
}
