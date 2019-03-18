//
//  ecUtils.h
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __EStoreMeta__utils__
#define __EStoreMeta__utils__

#include <stdio.h>
#include <sys/time.h>
#include <stdint.h>

int str_to_int(const char *str);
size_t int_to_str(int value, char *buf, size_t bufSize);
size_t uint64_to_str(uint64_t value, char *buf, size_t bufSize);
uint64_t str_to_uint64(char *buf);
size_t str_to_sizeT(char *buf);
size_t getSizeTValueBetweenStrs(const char *str1, const char *str2, const char *str);
size_t alignBufSize(size_t bufSizeToAlign, size_t alignValue);

int getMatrixValues(const char *str1, const char *str2, const char *str, int *matrix, size_t matSize);
int getBitmatrixValues(const char *str1, const char *str2, const char *str, int *bitmatrix, size_t matSize);

size_t sizeT_to_str(size_t value, char *buf, size_t bufSize);

int getDir(const char *path, char *dir);
int readOneLine(int fd, char *buf);

void writeToBuf(char *desBuf, char *srcBuf, size_t *writeSize, size_t *writeOffset);
void copyStrToBufOffset(char *desBuf, char *srcBuf, size_t *writeOffset);

double timeIntervalInMS(struct timeval startTime, struct timeval endTime);

uint64_t getUInt64ValueBetweenStrs(const char *str1, const char *str2, const char *str ,size_t strSize);
int getIntValueBetweenStrings(const char *str1, const char *str2, const char *str);

//Return 0 if no, 1 if yes
int hasSubstr(char *buf, char *str, size_t bufSize);
#endif /* defined(__EStoreMeta__utils__) */
