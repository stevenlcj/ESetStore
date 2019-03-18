//
//  rackMap.h
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __EStoreMeta__rackMap__
#define __EStoreMeta__rackMap__

#include <stdio.h>
#include <sys/time.h>

typedef enum server_state{
    
    SERVER_UNCONNECTED = 0,
    SERVER_CONNECTED = 1,
    SERVER_INIT = 2,
    SERVER_ON = 3,
    SERVER_DOWN = 4,
    SERVER_RECOVERED = 5
    
}server_state_t;

typedef struct Server{
    char serverIP[16]; //support IPv4 Only
    int IPSize;
    int serverPort;
    
    int rackId;
    int serverId;
    
    int groupId;
    int groupRowId;
    int groupColId;
    
    server_state_t curState;
    struct server *next;
    
    struct timeval failedTime;
    struct timeval startRecoverTime;
    struct timeval recoveredTime;
    
    int recoveringSets;
    int recoveredSets;
}Server_t;

typedef struct Rack{
    int rackId;
    char *rackName;

    struct Server *servers;
    int serverNum;
    int serverRemainSize;

    struct rack *next;
}Rack_t;

typedef struct rackMap{
    int rackNum;
    int rackRemainSize;
    struct Rack *racks;
}rackMap_t;

struct rackMap *initRMap(const char *path);

Server_t *locateServer(rackMap_t* rMap, const char *IPaddr);
#endif /* defined(__EStoreMeta__rackMap__) */
