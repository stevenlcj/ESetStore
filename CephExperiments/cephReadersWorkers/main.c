#include <stdio.h>
#include <stdlib.h>
#include "ThreadWorker.h"

#define ARG_SIZE 4

void usage(){
        printf("Please specify startIdx, endIdx and threadNum\n");
}

int main(int argc, const char * argv[]) {
        
if (argc < ARG_SIZE) {
                    exit(0);
                        }
            
            int startIdx = atoi(argv[1]);
                int endIdx = atoi(argv[2]);
                    int threadNum = atoi(argv[3]);
                        
                        startThreadMgr(startIdx, endIdx, threadNum);
                            return 0;
}
