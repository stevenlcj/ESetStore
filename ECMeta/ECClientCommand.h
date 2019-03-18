//
//  ECClientCommand.h
//  ECMeta
//
//  Created by Liu Chengjian on 17/9/5.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#ifndef ECMeta_ECClientCommand_h
#define ECMeta_ECClientCommand_h

typedef enum {
    ECClientCommand_CREATE = 0x01,
    ECClientCommand_OPEN,
    ECClientCommand_READ,
    ECClientCommand_WRITE,
    ECClientCommand_APPEND,
    ECClientCommand_WRITEOVER,
    ECClientCommand_DELETE,
    ECClientCommand_CLOSING,
    ECClientCommand_UNKNOWN
}ECClientCommand_t;

ECClientCommand_t parseClientCmd(char *parseBuf,size_t *cmdSize);
#endif
