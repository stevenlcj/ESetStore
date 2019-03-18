//
//  ECNetHandle.h
//  ECClient
//
//  Created by Liu Chengjian on 17/9/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __ECClient__ECNetHandle__
#define __ECClient__ECNetHandle__

#include <stdio.h>
#include "ECClientEngine.h"

int create_TCP_sockFd();
int connectMetaEngine(ECClientEngine_t *clientEnginePtr);
int createAFileToMeta(ECClientEngine_t *clientEnginePtr, const char *FileName);
ssize_t writeCmdToSock(int sockFd, char *cmdStr, size_t size);
void closeSockFd(int sockFd);

size_t recvMetaReply(int sockFd, char *buf, size_t bufSize);

int create_epollfd();
int make_non_blocking_sock(int sfd);

int update_event(int efd, uint32_t opFlag, int fd, void *arg);
int add_event(int efd, uint32_t opFlag, int fd, void *arg);
int delete_event(int efd, int fd);

#endif /* defined(__ECClient__ECNetHandle__) */
