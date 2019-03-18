//
//  configProperties.c
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "configProperties.h"

#include "ecCommon.h"
#include "ecUtils.h"

#include <string.h>

struct configProperties *initCfgProperties(){
    struct configProperties *cfgProperties  = talloc(struct configProperties, 1);
    cfgProperties->size = 0;
    cfgProperties->properties = NULL;
    
    return cfgProperties;
}

void addProperty(struct configProperties * cfg, struct Property *prop){
    
    finalizeProp(prop);
    
    if (cfg->properties == NULL) {
        cfg->properties = prop;
    }else{
        prop->next = cfg->properties;
        cfg->properties = prop;
    }
}

int getIntValue(struct configProperties * cfg, const char *key){
    struct Property *prop = cfg->properties;
    
    while (prop != NULL) {
        if(strncmp(prop->key, key, strlen(key)) == 0){
            if (prop->valueSize > 0) {
                return  str_to_int((const char *) prop->value);
            }
            
            return -1;
        }
        
        prop = prop->next;
    }
    
    return -1;
}

char *getStrValue(struct configProperties * cfg, const char *key){
    struct Property *prop = cfg->properties;
    
    while (prop != NULL) {
        if(strncmp(prop->key, key, strlen(key)) == 0){
            if (prop->valueSize > 0) {
                char *value = talloc(char, prop->valueSize);
                memcpy(value, prop->value, (prop->valueSize));
                return  value;
            }
            
            return NULL;
        }
        
        prop = prop->next;
    }
    
    return NULL;
}

void printProperties(struct configProperties * cfg){
    struct Property *prop = cfg->properties;
    while (prop != NULL ) {
        printProperty(prop);
        prop = prop->next;
    }
}

struct Property *initProperty(){
    struct Property *prop = talloc(struct Property, 1);
    
    prop->next = NULL;
    prop->keySize = 0;
    prop->valueSize = 0;
    
    return prop;
}

void appendKey(struct Property *prop, char ch){
    prop->key[prop->keySize] = ch;
    ++prop->keySize;
}

void appendValue(struct Property *prop, char ch){
    prop->value[prop->valueSize] = ch;
    ++prop->valueSize;
}

void finalizeProp(struct Property *prop){
    prop->key[prop->keySize] = '\0';
    prop->value[prop->valueSize] = '\0';
    ++prop->valueSize;
}

void printProperty(struct Property *prop){
    if (prop->keySize > 0) {
        printf("Key: %s\n", prop->key);
    }
    
    if (prop->valueSize > 0) {
        printf("value: %s\n", prop->value);
    }
}
