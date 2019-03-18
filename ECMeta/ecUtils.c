//
//  ecUtils.c
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ecUtils.h"

#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

int str_to_int(const char *str){
    int value = 0, chValue = '0';
    
    if (str == NULL) {
        return -1;
    }
    
    while (*str != '\0') {
        if (*str >= '0' && *str <= '9') {
            value = value * 10 + *str - chValue;
        }
        ++str;
    }
    
    return value;
}

size_t uint64_to_str(uint64_t value, char *buf, size_t bufSize){

    size_t strSize, digitInDeci = 1;
    uint64_t divideDeci = 10, valueToDiv = value;
    
    while ((valueToDiv = (valueToDiv / divideDeci)) > 0) {
        ++digitInDeci;
    }
    
    strSize = digitInDeci;
    
//    printf("bufSize:%lu, strSize:%lu\n", bufSize, strSize);
    
    if (bufSize < strSize) {
        return 0;
    }
    
    valueToDiv = value;
    buf[strSize] = '\0';
    while (digitInDeci != 0) {
        uint64_t valueToChar = valueToDiv % divideDeci;
        valueToDiv = valueToDiv / divideDeci;
        char ch = valueToChar + '0';
        buf[digitInDeci-1] = ch;
        --digitInDeci;
    }
    
//    printf("Value:%lu str:%s\n", value, buf);
    return strSize;
}

size_t int_to_str(int value, char *buf, size_t bufSize){
    int strSize, digitInDeci = 1;
    int divideDeci = 10, valueToDiv = value;
    
    while ((valueToDiv = (valueToDiv / divideDeci)) > 0) {
        ++digitInDeci;
    }
    
    strSize = digitInDeci;
    
    if (bufSize < strSize) {
        return 0;
    }
    
    valueToDiv = value;
    buf[strSize] = '\0';
    while (digitInDeci > 0) {
        int valueToChar = valueToDiv % divideDeci;
        valueToDiv = valueToDiv / divideDeci;
        char ch = valueToChar + '0';
        buf[digitInDeci-1] = ch;
        --digitInDeci;
    }
    
    return strSize;
}

int readOneLine(int fd, char *buf){
    if (fd < 0) {
        return -1;
    }
    
    int rIdx= 0;
    char ch;
    
    while (read(fd, &ch, 1) == 1) {
        
        if (ch == '\r' || ch == '\n') {
            //the line is over
            break;
        }
        
        *(buf + rIdx) = ch;
        ++rIdx;
    }
    
    *(buf + rIdx) = '\0';
    
    return rIdx;
}

int getDir(const char *path, char *dir){
    int pathSize = 0, dirSize;
    
    if (path == NULL || dir == NULL) {
        return -1;
    }
    
    while (*(path + pathSize) != '\0') {
        ++pathSize;
    }
    
    dirSize = pathSize;
    
    while (*(path + dirSize - 1) != '/') {
        --dirSize;
    }
    
    memcpy(dir, path, dirSize);
    
    return dirSize;
}

double timeIntervalInMS(struct timeval startTime, struct timeval endTime){
    double timeIntervalInMS = 0.0;
    
    timeIntervalInMS = ((double)(endTime.tv_sec - startTime.tv_sec))*1000.0 + ((double)(endTime.tv_usec - startTime.tv_usec))/1000.0;
    
    return timeIntervalInMS;
}

void writeToBuf(char *desBuf, char *srcBuf, size_t *writeSize, size_t *writeOffset){
    *writeSize = strlen(srcBuf);
    
    memcpy((desBuf + *writeOffset), srcBuf, *writeSize);
    *writeOffset = *writeOffset + *writeSize;
}

size_t str_to_sizeT(char *buf){
    uint64_t value = 0;
    int idx = 0;
    char ch = '0';
    while (*(buf +idx) != '\0') {
        value = value * 10 + *(buf +idx) - ch;
        ++idx;
    }
    
    return value;
}

size_t getSizeTValueBetweenStrs(const char *str1, const char *str2, const char *str){
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
    
//    printf("str** %s **to extract sizeT value\n ", str);
    
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
    
    value = str_to_sizeT(valueBuf);
    
    //    printf("valueBuf:%s, value:%d\n",valueBuf, value);
    
    return value;
}

uint64_t str_to_uint64(char *buf){
    uint64_t value = 0;
    int idx = 0;
    char ch = '0';
    while (*(buf +idx) != '\0') {
        value = value * 10 + *(buf +idx) - ch;
        ++idx;
    }
    
    return value;
}

uint64_t getUInt64ValueBetweenStrs(const char *str1, const char *str2, const char *str ,size_t strSize){
    size_t str1Len = strlen(str1), str2Len = strlen(str2);
    int idx = 0, idx1;
    uint64_t value;
    char valueBuf[1024];
    
    //    printf("strSize:%lu  str1Len:%lu\n", strSize,str1Len);
    
    while (strncmp(str1, str+idx, str1Len) != 0) {
        ++idx;
        
        if ((idx + str1Len) > strSize) {
            return -1;
        }
    }
    
    //    printf("idx:%d\n", idx);
    
    idx1 = idx + (int)str1Len + 1;
    
    while (strncmp(str2, str+idx1, str2Len) != 0) {
        ++idx1;
        
        if ((idx1 + str2Len) > strSize) {
            return -1;
        }
    }
    
    //    printf("idx:%d, str1Len:%lu, idx1:%d\n", idx, str1Len,idx1);
    
    memcpy(valueBuf, (str + idx + str1Len), (idx1 - (idx + str1Len )));
    valueBuf[(idx1 - (idx + str1Len ))] = '\0';
    
    value = str_to_uint64(valueBuf);
    
    //    printf("valueBuf:%s, value:%lu\n",valueBuf, value);
    
    return value;
}

int getIntValueBetweenStrings(const char *str1, const char *str2, const char *str){
    size_t str1Len = strlen(str1), str2Len = strlen(str2), strLen = strlen(str);
    int idx = 0, idx1, value;
    char valueBuf[1024];
    
    while (strncmp(str1, str+idx, str1Len) != 0) {
        ++idx;
        
        if ((idx + str1Len) > strLen) {
            return -1;
        }
    }
    
    idx1 = idx + (int)str1Len + 1;
    
    while (strncmp(str2, str+idx1, str2Len) != 0) {
        ++idx1;
        
        if ((idx1 + str2Len) > strLen) {
            return -1;
        }
    }
    
    memcpy(valueBuf, (const char *) (str + idx + str1Len), (idx1 - (idx + str1Len )));
    valueBuf[(idx1 - (idx + str1Len ))] = '\0';
    
    value = str_to_int(valueBuf);
    
    return value;
}