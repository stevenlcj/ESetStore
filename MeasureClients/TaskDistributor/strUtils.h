//
//  strUtils.h
//  TaskDistributor
//
//  Created by Lau SZ on 2020/1/2.
//  Copyright © 2020 Shenzhen Technology University. All rights reserved.
//

#ifndef strUtils_h
#define strUtils_h

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

long str_to_Long(char *buf);
long getLongValueBetweenStrs(const char *str1, const char *str2, const char *str);

#endif /* strUtils_h */
