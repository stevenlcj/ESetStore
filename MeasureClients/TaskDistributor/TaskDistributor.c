//
//  TaskDistributor.c
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/16.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>


#include "TaskRunner.h"

#define ARG_SIZE 5

void usage(){
    printf("Please specify startIdx, endIdx, clients, portToListen and System to measure\n");
    printf("0 indicates ESetStore, 1 indicates Ceph\n");
}

int main(int argc, const char * argv[]) {
    // insert code here...
    if (argc < ARG_SIZE) {
        usage();
        exit(0);
    }
    
    int startIdx = atoi(argv[1]);
    int endIdx = atoi(argv[2]);
    int clientNums = atoi(argv[3]);
    int portToListen = atoi(argv[4]);
    int taskType = atoi(argv[5]);
    
    runTasks(startIdx, endIdx, clientNums, portToListen, taskType);
    
    return 0;
}
