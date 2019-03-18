//
//  rackMap.c
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "rackMap.h"

#include "ecCommon.h"
#include "ecUtils.h"

#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>     /* realloc, free, exit, NULL */

const char serverFileName[20] = "Servers\0";

void printServerInfo(struct Server *server){
    printf("ServerIP:%s , Port:%d, RackId:%d\n", server->serverIP, server->serverPort, server->rackId);
}

void printRackAndServer(struct rackMap *rMap){
    int rIdx, sIdx;
    
    for (rIdx = 0; rIdx < rMap->rackNum; ++rIdx) {
        struct Rack *rackPtr = rMap->racks + rIdx;
        printf("rIdx:%d, serverNum:%d\n",rIdx, rackPtr->serverNum);
        for (sIdx = 0; sIdx < rackPtr->serverNum; ++sIdx) {
            struct Server *serverPtr = rackPtr->servers + sIdx;
            printServerInfo(serverPtr);
        }
    }
}

//line format: serverIP Port RackID
void getServerFromStr(struct Server *server, char *str){
    int portOffset, rackOffset;
    char buf[255];
    server->IPSize = 0;
    
    while (*(str + server->IPSize) != ' ') {
        server->serverIP[server->IPSize] = *(str + server->IPSize);
        ++server->IPSize;
    }
    
    portOffset = server->IPSize + 1;
    
    while (*(str + portOffset) != ' '){
        buf[portOffset - server->IPSize - 1] = *(str + portOffset);
        ++portOffset;
    }
    
    buf[portOffset - server->IPSize] = '\0';
    server->serverPort = str_to_int(buf);
    
    rackOffset = portOffset + 1;
//    printf("rackOffset:%d portOffset:%d\n", rackOffset, portOffset);
    while (*(str + rackOffset) != '\0') {
        
        buf[rackOffset - portOffset - 1] = *(str + rackOffset);
        ++rackOffset;
    }
//    printf("rackOffset:%d portOffset:%d\n", rackOffset, portOffset);
    buf[rackOffset - portOffset-1] = '\0';
    
    server->rackId = str_to_int(buf);
    return;
}

void addServerToRack(struct Server *server, struct Rack *rack){
//    printServerInfo(server);
    if (rack->serverNum == rack->serverRemainSize) {
        rack->servers = (struct Server *) realloc(rack->servers, rack->serverRemainSize + SERVER_INCRE_SIZE);
        rack->serverRemainSize = rack->serverRemainSize + SERVER_INCRE_SIZE;
    }
    
    struct Server *curServer = (rack->servers+rack->serverNum);
    memcpy(curServer->serverIP, server->serverIP, server->IPSize);
    
    curServer->IPSize = server->IPSize;
    curServer->serverIP[curServer->IPSize] = '\0';
    curServer->serverPort = server->serverPort;
    curServer->rackId = server->rackId;
    curServer->serverId = rack->serverNum;
    
    curServer->groupId = -1;
    curServer->groupRowId = -1;
    curServer->groupColId = -1;
    
    curServer->curState = SERVER_UNCONNECTED;
    
    curServer->recoveredSets = 0;
    ++rack->serverNum;
//    printf("current server num:%d\n", rack->serverNum);
}

void initRackServers(struct Rack *racks, int size){
    int idx;
    
    for (idx = 0; idx < size; ++idx) {
        struct Rack *rackPtr = racks + idx;
        rackPtr->servers = talloc(struct Server, SERVER_INCRE_SIZE);
        rackPtr->serverRemainSize = SERVER_INCRE_SIZE;
        rackPtr->serverNum = 0;
    }
}

void initRacksServers(struct rackMap *rMap, const char *path){
    char serverFile[255];
    char serverBuf[255];
    
    int dirSize = getDir(path, serverFile);
    int serverFileLen = strlen(serverFileName);
    
    memcpy((serverFile + dirSize), serverFileName, serverFileLen);
    
    serverFile[dirSize + serverFileLen] = '\0';
    
//    printf("ServerFilePath:%s\n", serverFile);
    
    int fd = open(serverFile, O_RDONLY);
    
    if (fd < 0 ) {
        printf("unable to open %s for read\n", serverFile);
        exit(-1);
    }
    
    rMap->racks = talloc(struct Rack, RACK_INCRE_SIZE);
    rMap->rackRemainSize = RACK_INCRE_SIZE;
    initRackServers(rMap->racks, RACK_INCRE_SIZE);
    
    int lineSize;
    while ((lineSize = readOneLine(fd, serverBuf)) != 0) {
        struct Server server;
        getServerFromStr(&server, serverBuf);
//        printServerInfo(&server);
        
        if (server.rackId >= rMap->rackRemainSize) {
            struct Rack *newRacksPtr = (struct Rack *)realloc(rMap->racks, sizeof(struct Rack) * (rMap->rackRemainSize + RACK_INCRE_SIZE));
            if (newRacksPtr != NULL) {
                rMap->racks = newRacksPtr;
            }else{
                printf("unable to realloc mem\n");
            }
            initRackServers((rMap->racks + rMap->rackRemainSize), RACK_INCRE_SIZE);
            rMap->rackRemainSize = rMap->rackRemainSize + RACK_INCRE_SIZE;
        }
        
        addServerToRack(&server, (rMap->racks + server.rackId));
        
        if (rMap->rackNum <= server.rackId) {
            rMap->rackNum = server.rackId+1;
        }
    }
    
    close(fd);
    
    printRackAndServer(rMap);
}

struct rackMap *initRMap(const char *path){
    struct rackMap *rMap = talloc(struct rackMap, 1);
    
    rMap->rackNum = 0;
    rMap->racks = NULL;
    
    initRacksServers(rMap, path);
    
    return rMap;
}

Server_t *locateServer(rackMap_t* rMap, const char *IPaddr){
    int addrLen = (int)strlen(IPaddr);
    
    int rIdx, sIdx;
    
    for (rIdx = 0; rIdx < rMap->rackNum; ++rIdx) {
        
        Rack_t *rackPtr = rMap->racks + rIdx;
        for (sIdx = 0; sIdx < rackPtr->serverNum; ++sIdx) {
            Server_t *serverPtr = rackPtr->servers + sIdx;
            if (addrLen == serverPtr->IPSize && (strncmp(IPaddr, serverPtr->serverIP, addrLen) == 0)) {
                return serverPtr;
            }
        }
    }
    
    return NULL;
}