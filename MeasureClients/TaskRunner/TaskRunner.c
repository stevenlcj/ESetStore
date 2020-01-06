//
//  TaskRunner.c
//  TaskRunner
//
//  Created by Lau SZ on 2019/12/26.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

#include "TaskRunnerInstance.h"

#define ARG_SIZE 5

void Usage(){
    printf("Please specify: IPAddr port taskType fileSize\n");
    return;
}

int main(int argc, const char * argv[]) {
    // insert code here...
    
    if (argc != ARG_SIZE) {
        Usage();
        return -1;
    }
    
    int port = atoi(argv[2]);
    int taskType = atoi(argv[3]);
    int fileSize = atoi(argv[4]);
    
    runTask(argv[1], port, taskType, fileSize);
    
    return 0;
}
