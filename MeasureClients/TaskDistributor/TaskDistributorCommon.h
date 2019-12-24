//
//  TaskDistributorCommon.h
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/16.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#ifndef TaskDistributorCommon_h
#define TaskDistributorCommon_h


#define talloc(type,num) (type *)malloc(sizeof(type)*num)

#define MAX_CONN_EVENTS 128
#define DEFAULT_ACCEPT_TIME_OUT_IN_MLSEC 100000

#define DEFAULT_MESSAGE_BUFFER_SIZE 1024

#endif /* Header_h */
