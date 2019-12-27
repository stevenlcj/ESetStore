//
//  TaskRunnerCommon.h
//  TaskRunner
//
//  Created by Lau SZ on 2019/12/26.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#ifndef TaskRunnerCommon_h
#define TaskRunnerCommon_h

#define talloc(type,num) (type *)malloc(sizeof(type)*num)
#define DEFAULT_MESSAGE_BUFFER_SIZE 1024
#define DEFAULT_ACCEPT_TIME_OUT_IN_MLSEC 100000

#define MAX_CONN_EVENTS 128


#endif /* TaskRunnerCommon_h */
