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

int update_event(int efd, uint32_t opFlag, int fd, void *arg);
int add_event(int efd, uint32_t opFlag, int fd, void *arg);
int delete_event(int efd, int fd);

#endif /* TaskNetUtilities_h */
