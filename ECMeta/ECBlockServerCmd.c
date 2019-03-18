//
//  ECBlockServerCmd.c
//  ECMeta
//
//  Created by Liu Chengjian on 17/11/23.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECBlockServerCmd.h"
#include "ECRecoveryManagement.h"
#include "ESetPlacement.h"
#include "rackMap.h"
#include "ECMetaEngine.h"
#include "ecUtils.h"
#include "ecCommon.h"

#include "BlockServer.h"

#include <string.h>

const char recoveryCmdHeader[] = "RecoveryNodes\r\n\0";
const char recoveryOverCmdHeader[] = "EndRecoveryNodes\r\n\0";
const char recoveryAbortCmdHeader[] = "AbortRecoveryNodes\r\n\0";
const char recoveryBlockGroupCmdHeader[] = "RecoveryBlockGroup\r\n\0";
const char recoveryBlockGroupIdHeader[] = "BlockGroupId:\0";
const char recoveryBlockIdHeader[] = "BlockId:\0";
const char recoveryBlockSizeHeader[] = "BlockSize:\0";

const char ESetIdxHeader[] = "ESetIdx:\0";

const char sufixStr[] = "\r\n\0";


const char deleteBlockCmd[] = "DeleteBlock\0";
const char serverBlockIdStr[] = "BlockId:\0";

/**
 Cmd:
 RecoveryBlockGroup\r\n
 ESetIdx:__\r\n
 BlockGroupId:__\r\n
 BlockId:__\r\n
 BlockSize:__\r\n
 ...
 BlockId:__\r\n
 BlockSize:__\r\n
 ... Currently, assume each block's size is 64 MB. Will update in the future to accomodate different size
 */
size_t writeRecoveryBlockGroupCmd(void *listPtr, void* blockGroup, void *blockServerPtr){
    FailedESetsList_t *eSetListPtr =  (FailedESetsList_t *)listPtr;
    ECBlockGroup_t *blockGpPtr = (ECBlockGroup_t *)blockGroup;
    
    int idx;
    int bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    
    size_t writeBackOffset = 0, curWriteSize = 0;

    char bufToWrite[bufSize];
    char tempBuf[bufSize];
    
    writeToBuf(bufToWrite, (char *)recoveryBlockGroupCmdHeader, &curWriteSize, &writeBackOffset);
    
    writeToBuf(bufToWrite, (char *) ESetIdxHeader,  &curWriteSize, &writeBackOffset);
    int_to_str(eSetListPtr->failedESets->esetId, tempBuf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    
    writeToBuf(bufToWrite, (char *)recoveryBlockGroupIdHeader, &curWriteSize, &writeBackOffset);
    uint64_to_str(blockGpPtr->blockGroupId, tempBuf, bufSize);
    writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    
    for (idx = 0; idx < eSetListPtr->eSetNodesNum; ++idx) {
        ESetNode_t *nodePtr = eSetListPtr->eSetNodes + idx;
        ECBlock_t *blockPtr = blockGpPtr->blocks + nodePtr->row;
        
        writeToBuf(bufToWrite, (char *)recoveryBlockIdHeader, &curWriteSize, &writeBackOffset);
        uint64_to_str(blockPtr->blockId, tempBuf, bufSize);
        writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
        writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);

        writeToBuf(bufToWrite, (char *)recoveryBlockSizeHeader, &curWriteSize, &writeBackOffset);
        uint64_to_str(blockPtr->blockSize, tempBuf, bufSize);
        writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
        writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    }
    
    for (idx = 0; idx < eSetListPtr->failedNodesNum; ++idx) {
        int nodeIdx = *(eSetListPtr->failedIdx + idx);
        ECBlock_t *blockPtr = blockGpPtr->blocks + nodeIdx;
        
        writeToBuf(bufToWrite, (char *)recoveryBlockIdHeader, &curWriteSize, &writeBackOffset);
        uint64_to_str(blockPtr->blockId, tempBuf, bufSize);
        writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
        writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
        
        writeToBuf(bufToWrite, (char *)recoveryBlockSizeHeader, &curWriteSize, &writeBackOffset);
        uint64_to_str(blockPtr->blockSize, tempBuf, bufSize);
        writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
        writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    }
    
    bufToWrite[writeBackOffset] = '\0';
    writeBackOffset = writeBackOffset + 1;
    addWriteMsgToBlockserver((BlockServer_t *) blockServerPtr, bufToWrite, writeBackOffset);
    
    return writeBackOffset;
}

/**
 Cmd:
 RecoveryNodes\r\n
 ESetIdx:__\r\n
 StripeK:__\r\n
 StripeM:__\r\n
 StripeW:__\r\n
 matrix:....\r\n
 SourceNodeSize:__\r\n
 SourceNodeIdx:__\r\n
 IP:__\r\n
 Port:__\r\n
 ...
 TargetNodeSize:__\r\n
 TargetNodeIdx:__\r\n
 ...
 TargetNodeIdx:__\r\n\0
 */
size_t writeRecoveryCmd(void *listPtr, void *enginePtr, void *blockServerPtr){
    ECMetaServer_t *metaServerEnginePtr = (ECMetaServer_t *)enginePtr;
    FailedESetsList_t *eSetListPtr =  (FailedESetsList_t *)listPtr;
    
    size_t writeBackOffset = 0, curWriteSize = 0;
    int idx;
    int bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    char tempBuf[bufSize];
    char bufToWrite[bufSize];
    
    char stripeKHeader[] = "stripeK:\0";
    char stripeMHeader[] = "stripeM:\0";
    char stripeWHeader[] = "stripeW:\0";
    char matrixHeader[] = "matrix:\0";
    char sourceNodeSizeHeader[] = "SourceNodesSize:\0";
    char sourceNodeIdxHeader[] = "SourceNodeIdx:\0";
    char TargetNodeSizeHeader[] = "TargetNodesSize:\0";
    char TargetNodeIdxHeader[] = "TargetNodeIdx:\0";
    char IPStr[] = "IP:\0";
    char PortStr[] = "Port:\0";
    char space = ' ';

    //RecoveryNodes\r\n
    writeToBuf(bufToWrite, (char *)recoveryCmdHeader, &curWriteSize, &writeBackOffset);
    
    //ESetIdx:__\r\n
    writeToBuf(bufToWrite, (char *)ESetIdxHeader, &curWriteSize, &writeBackOffset);
    int eSetIdx = eSetListPtr->failedESets->esetId;
    int_to_str(eSetIdx, tempBuf, bufSize);
    writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    
    //StripeK:
    writeToBuf(bufToWrite, stripeKHeader, &curWriteSize, &writeBackOffset);
    int_to_str(metaServerEnginePtr->stripeK, tempBuf, bufSize);
    writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    
    //StripeM:
    writeToBuf(bufToWrite, stripeMHeader, &curWriteSize, &writeBackOffset);
    
    int_to_str(metaServerEnginePtr->stripeM, tempBuf, bufSize);
    writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);

    //StripeW:
    writeToBuf(bufToWrite, stripeWHeader, &curWriteSize, &writeBackOffset);
    int_to_str(metaServerEnginePtr->stripeW, tempBuf, bufSize);
    writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    
    writeToBuf(bufToWrite, matrixHeader, &curWriteSize, &writeBackOffset);
    
    //Bitmatrix:
    size_t matrixSize = (metaServerEnginePtr->stripeK * metaServerEnginePtr->stripeM);
    for (idx = 0; idx < matrixSize; ++idx) {
        int value = *(metaServerEnginePtr->matrix + idx);
        int_to_str(value, tempBuf, DEFAUT_MESSGAE_READ_WRITE_BUF);
        writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
        memcpy((bufToWrite + writeBackOffset), &space, 1);
        writeBackOffset = writeBackOffset + 1;
    }
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);

    writeToBuf(bufToWrite, sourceNodeSizeHeader, &curWriteSize, &writeBackOffset);
    int_to_str(eSetListPtr->eSetNodesNum, tempBuf, bufSize);
    writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    
    for (idx = 0; idx < eSetListPtr->eSetNodesNum; ++idx) {
        ESetNode_t *curNodePtr = eSetListPtr->eSetNodes + idx;
         writeToBuf(bufToWrite, sourceNodeIdxHeader, &curWriteSize, &writeBackOffset);
        int_to_str(curNodePtr->row, tempBuf, bufSize);
        writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
        writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
        
        Rack_t *curRack = metaServerEnginePtr->rMap->racks + curNodePtr->rackId;
        Server_t *curServer = curRack->servers + curNodePtr->serverIdInRack;;
        
        writeToBuf(bufToWrite, IPStr, &curWriteSize, &writeBackOffset);
        
        memcpy((bufToWrite + writeBackOffset), curServer->serverIP, curServer->IPSize);
        writeBackOffset = writeBackOffset + curServer->IPSize;
        writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
        
        writeToBuf(bufToWrite, PortStr, &curWriteSize, &writeBackOffset);
        int_to_str(curServer->serverPort, tempBuf, bufSize);
        writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
        writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    }
    
    int targetNodeSize = eSetListPtr->failedNodesNum;
    
//    for (idx = 0; idx < eSetListPtr->failedESets->size; ++idx) {
//        ESetNode_t *curNodePtr = eSetListPtr->failedESets->setNodes + idx;
//        
//        if (curNodePtr->failedFlag == 1 && curNodePtr->recoveredFlag != 1) {
//            ++targetNodeSize;
//        }
//    }
    
    writeToBuf(bufToWrite, TargetNodeSizeHeader, &curWriteSize, &writeBackOffset);
    int_to_str(targetNodeSize, tempBuf, bufSize);
    writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    
    for (idx = 0 ; idx < targetNodeSize; ++idx) {
        ESetNode_t *curNodePtr = eSetListPtr->failedESets->setNodes + *(eSetListPtr->failedIdx+idx);
        
        if (curNodePtr->failedFlag == 1 && curNodePtr->recoveredFlag != 1) {
            writeToBuf(bufToWrite, TargetNodeIdxHeader, &curWriteSize, &writeBackOffset);
            int_to_str(curNodePtr->row, tempBuf, bufSize);
            writeToBuf(bufToWrite, tempBuf, &curWriteSize, &writeBackOffset);
            writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
        }else{
            printf("Error: node is not in failed state\n");
        }
    }
    
    bufToWrite[writeBackOffset] = '\0';
    writeBackOffset = writeBackOffset + 1;
    
//    printf("***********writeRecoveryCmd******\n%s\n",bufToWrite);
    addWriteMsgToBlockserver((BlockServer_t *) blockServerPtr, bufToWrite, writeBackOffset);
    
    return writeBackOffset;
}

size_t writeRecoveryOverCmd(void *listPtr, void *blockServerPtr){
    FailedESetsList_t *eSetListPtr =  (FailedESetsList_t *)listPtr;

    int bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    
    size_t writeBackOffset = 0, curWriteSize = 0;
    
    char bufToWrite[bufSize];
    char tempBuf[bufSize];

    char ESetIdxHeader[] = "ESetIdx:\0";
    
    writeToBuf(bufToWrite, (char *)recoveryOverCmdHeader, &curWriteSize, &writeBackOffset);

    writeToBuf(bufToWrite, (char *)ESetIdxHeader, &curWriteSize, &writeBackOffset);
    int_to_str(eSetListPtr->failedESets->esetId, tempBuf, DEFAUT_MESSGAE_READ_WRITE_BUF);
    writeToBuf(bufToWrite, (char *)tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);
    
//    char *cmd = talloc(char, (writeBackOffset + 1));
//    memcpy(cmd, bufToWrite, writeBackOffset);
//    
//    *(cmd + writeBackOffset) = '\0';
    
//    printf("formRecoveryOverCmd**********:%s\n",cmd);
    
    *(bufToWrite + writeBackOffset) = '\0';
    writeBackOffset = writeBackOffset + 1;
    
    addWriteMsgToBlockserver((BlockServer_t *) blockServerPtr, bufToWrite, writeBackOffset);
    
    return writeBackOffset;
}

// DeleteBlock/r/nBlockId:__/r/n/0
size_t writeDeleteBlockCmd(void *blockPtr, void *blockServerPtr){
    ECBlock_t *curBlockPtr = (ECBlock_t *)blockPtr;

    int bufSize = DEFAUT_MESSGAE_READ_WRITE_BUF;
    
    size_t writeBackOffset = 0, curWriteSize = 0;
    
    char bufToWrite[bufSize];
    char tempBuf[bufSize];

    writeToBuf(bufToWrite, (char *)deleteBlockCmd, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);

    writeToBuf(bufToWrite, (char *)serverBlockIdStr, &curWriteSize, &writeBackOffset);
    uint64_to_str(curBlockPtr->blockId, tempBuf, bufSize);
    writeToBuf(bufToWrite, (char *)tempBuf, &curWriteSize, &writeBackOffset);
    writeToBuf(bufToWrite, (char *)sufixStr, &curWriteSize, &writeBackOffset);

    *(bufToWrite + writeBackOffset) = '\0';
    writeBackOffset = writeBackOffset + 1;
    
    printf("writeDeleteBlockCmd***:%s\n",bufToWrite);
    addWriteMsgToBlockserver((BlockServer_t *) blockServerPtr, bufToWrite, writeBackOffset);
    
    return writeBackOffset;
}











