//
//  ecNetHandle.h
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef __EStoreMeta__ecNetHandle__
#define __EStoreMeta__ecNetHandle__

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

//#include "ECThreadHandle.h"
void accept_client_conns(ECMetaServer_t *ecMetaServer);

int create_epollfd();
int create_bind_listen (int port);
int make_non_blocking_sock(int sfd);

void start_wait_conn(ECMetaServer_t *ecMetaServer);

int update_event(int efd, uint32_t opFlag, int fd, void *arg);
int add_event(int efd, uint32_t opFlag, int fd, void *arg);

int delete_event(int efd, int fd);

//void start_epoll_ClientConn(struct ClientThread *clientThread);
#endif /* defined(__EStoreMeta__ecNetHandle__) */
