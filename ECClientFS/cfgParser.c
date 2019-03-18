//
//  lineParser.c
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "cfgParser.h"
#include "ecUtils.h"
#include "ecCommon.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

void dry_one_line(int fd){
    char ch;
    
    while (read(fd, (void *) &ch, 1) == 1) {
        if (ch == '\r' || ch == '\n') {
            return;
        }
    }
}

struct configProperties *loadConfigProperties(int fd){
    struct configProperties * cfgProperties = initCfgProperties();
    struct Property *newPro = NULL;
    
    char ch;
    int  readKey = 1;
    int  newLine = 1;
    
    while (read(fd, (void *) &ch, 1) == 1) {

        if (newLine == 1) {
            if (ch == '#') {
                // line for comments
                dry_one_line(fd);
                continue;
            }
            
            if (ch == '\r' || ch == '\n' || ch == ' ') {
                continue;
            }
            
            assert(newPro == NULL);
            
            newPro = initProperty();
            newLine = 0;
            readKey = 1;
            appendKey(newPro, ch);
        }else{
            if (ch == '\r' || ch == '\n') {
                // This line is ended
                newLine = 1;
                readKey = 1;
                addProperty(cfgProperties, newPro);
                newPro = NULL;
                continue;
            }
            
            if (ch == '=') {
                // Start to read value
                readKey = 0;
                continue;
            }
            
            if (readKey == 1) {
                appendKey(newPro, ch);
            }else{
                appendValue(newPro, ch);
            }
        }
    }
    
    close(fd);
    
    return cfgProperties;
}

struct configProperties *loadProperties(const char *file){
    
    assert(file != NULL);
    
    int fd = open(file, O_RDONLY);
    
    if (fd < 0 ) {
        perror("File open error");
        printf("unable to open %s for read\n", file);
        exit(-1);
    }
    
    return loadConfigProperties(fd);
}