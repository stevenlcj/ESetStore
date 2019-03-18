//
//  configProperties.h
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __EStoreMeta__configProperties__
#define __EStoreMeta__configProperties__

#include <stdio.h>

struct Property{
    char key[255];
    char value[255];
    int keySize;
    int valueSize;
    struct Property *next;
};

struct configProperties{
    struct Property *properties;
    int size;
};

struct configProperties *initCfgProperties();
void addProperty(struct configProperties * cfg, struct Property *prop);

int getIntValue(struct configProperties * cfg, const char *key);
char *getStrValue(struct configProperties * cfg, const char *key);

void printProperties(struct configProperties * cfg);

struct Property *initProperty();
void appendKey(struct Property *prop, char ch);
void appendValue(struct Property *prop, char ch);
void finalizeProp(struct Property *prop);

void printProperty(struct Property *prop);
#endif /* defined(__EStoreMeta__configProperties__) */
