//
//  strUtils.c
//  TaskDistributor
//
//  Created by Lau SZ on 2020/1/2.
//  Copyright © 2020 Shenzhen Technology University. All rights reserved.
//

#include "strUtils.h"

#include <string.h>

long str_to_Long(char *buf){
    long value = 0;
    int idx = 0;
    char ch = '0';
    while (*(buf +idx) != '\0') {
        value = value * 10 + *(buf +idx) - ch;
        ++idx;
    }
    
    return value;
}


long getLongValueBetweenStrs(const char *str1, const char *str2, const char *str){
    size_t str1Len = strlen(str1), str2Len = strlen(str2), strLen = strlen(str);
    int idx = 0, idx1;
    size_t value;
    char valueBuf[1024];
    
    while (strncmp(str1, str+idx, str1Len) != 0) {
        ++idx;
        
        if ((idx + str1Len) > strLen) {
            return 0;
        }
    }
    
    idx1 = idx + (int)str1Len + 1;
    
    while (strncmp(str2, str+idx1, str2Len) != 0) {
        ++idx1;
        
        if ((idx1 + str2Len) > strLen) {
            return 0;
        }
    }
    
    //    printf("idx:%d, str1Len:%lu, idx1:%d\n", idx, str1Len,idx1);
    
    memcpy(valueBuf, (str + idx + str1Len), (idx1 - (idx + str1Len )));
    valueBuf[(idx1 - (idx + str1Len ))] = '\0';
    
    value = str_to_Long(valueBuf);
    
    //    printf("valueBuf:%s, value:%d\n",valueBuf, value);
    
    return value;
}
