//
//  utils.h
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __EStoreMeta__utils__
#define __EStoreMeta__utils__

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

int str_to_int(const char *str);

int getDir(const char *path, char *dir);
int readOneLine(int fd, char *buf);

size_t uint64_to_str(uint64_t value, char *buf, size_t bufSize);
size_t str_to_sizeT(char *buf);

int getMatrixValues(const char *str1, const char *str2, const char *str, int *matrix, size_t matSize);

uint64_t getUInt64ValueBetweenStrs(const char *str1, const char *str2, const char *str ,size_t strSize);
size_t getSizeTValueBetweenStrs(const char *str1, const char *str2, const char *str);

void writeToBuf(char *desBuf, char *srcBuf, size_t *writeSize, size_t *writeOffset);
double timeIntervalInMS(struct timeval startTime, struct timeval endTime);

size_t int_to_str(int value, char *buf, size_t bufSize);

int getIntValueBetweenStrings(const char *str1, const char *str2, const char *str);
int hasSubstr(char *buf, char *str, size_t bufSize);

size_t sizeT_to_str(size_t value, char *buf, size_t bufSize);
#endif /* defined(__EStoreMeta__utils__) */
