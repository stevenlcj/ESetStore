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

int create_TCP_sockFd();
int create_epollfd();
int create_bind_listen(int port);

#endif /* TaskNetUtilities_h */
