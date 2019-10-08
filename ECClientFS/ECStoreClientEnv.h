//
//  ECStoreClientEnv.h
//  ECClientFS
//
//  Created by Liu Chengjian on 18/4/4.
//  Copyright (c) 2018 csliu. All rights reserved.
//

#ifndef __ECStoreClientEnv_Header__
#define __ECStoreClientEnv_Header__

const char cmdPut[] = "-put\0";
const char cmdGet[] = "-get\0";
const char cmdRm[] = "-rm\0";
const char ECStoreHome[] = "ESetSTORE_HOME\0";

const char ECStoreMetaServer_IP[] = "MetaServer_IPADDR\0";
const char ECStoreMetaServer_Port[] = "MetaServer_Port\0";
const char cfgFileSuffix[20] = "/cfg/MetaServer";
#endif
