//
//  TaskNetUtilities.h
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/20.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#ifndef TaskNetUtilities_h
#define TaskNetUtilities_h

#include <stdio.h>
#include <stdint.h>


int create_TCP_sockFd();
int create_epollfd();
int create_bind_listen(int port);

int connectToIPPort(char *IPAddr, int port);

int add_event(int efd, uint32_t opFlag, int fd, void *arg);

ssize_t readSockData(int sockFd, char *buf, size_t bufSize);

#endif /* TaskNetUtilities_h */
