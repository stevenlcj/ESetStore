//
//  aiori-ESetStore.c
//  IORWork
//
//  Created by Lau SZ on 2020/2/1.
//  Copyright © 2020 Shenzhen Technology University. All rights reserved.
//

/*
 * This file implements the abstract I/O interface for ESetStore Client API.
 */


#include <stdio.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>                /* strnstr() */

#include <errno.h>
#include <assert.h>

#include "ior.h"
#include "aiori.h"
#include "iordef.h"

#include "ECClientEngine.h"

#define DEFAULT_BUF_SIZE (1024 * 1024)

static ECClientEngine_t *clientEngine = NULL;

/**************************** P R O T O T Y P E S *****************************/
static void *ESetStore_Create(char *, IOR_param_t *);
static void *ESetStore_Open(char *, IOR_param_t *);
static IOR_offset_t ESetStore_Xfer(int, void *, IOR_size_t *,
                               IOR_offset_t, IOR_param_t *);
static void ESetStore_Close(void *, IOR_param_t *);
static void ESetStore_Delete(char *, IOR_param_t *);
static void ESetStore_Fsync(void *, IOR_param_t *);
static IOR_offset_t ESetStore_GetFileSize(IOR_param_t *, MPI_Comm, char *);
static int ESetStore_StatFS(const char *, ior_aiori_statfs_t *, IOR_param_t *);
static int ESetStore_MkDir(const char *, mode_t, IOR_param_t *);
static int ESetStore_RmDir(const char *, IOR_param_t *);
static int ESetStore_Access(const char *, int, IOR_param_t *);
static int ESetStore_Stat(const char *, struct stat *, IOR_param_t *);
static option_help * ESetStore_options();

/************************** D E C L A R A T I O N S ***************************/
ior_aiori_t esetstore_aiori = {
    .name = "ESetStore",
    .name_legacy = NULL,
    .create = ESetStore_Create,
    .open = ESetStore_Open,
    .xfer = ESetStore_Xfer,
    .close = ESetStore_Close,
    .delete = ESetStore_Delete,
    .get_version = aiori_get_version,
    .fsync = ESetStore_Fsync,
    .get_file_size = ESetStore_GetFileSize,
    .statfs = ESetStore_StatFS,
    .access = ESetStore_Access,
    .stat = ESetStore_Stat,
};

void connectToMetaServer(){
    if (clientEngine == NULL) {
        char *IPAddrStr=getenv("ESetStoreMetaIP");
        char *PortStr=getenv("ESetStoreMetaPort");
        int port;
        
        if (PortStr == NULL) {
            port = 20001;
        }else{
            port = atoi(PortStr);
        }
        
        if (IPAddrStr != NULL) {
            clientEngine = createECClientEngine(IPAddrStr, port);
        }else{
            clientEngine = createECClientEngine("192.168.0.201", port);
        }
    }
}

void finalizeClient(){
    if (clientEngine != NULL) {
        deallocECClientEngine(clientEngine);
        clientEngine = NULL;
    }
}

static void *ESetStore_Create(char *testFileName, IOR_param_t * param){
    connectToMetaServer();
    int *fd;
    fd = (int *)malloc(sizeof(int));

    *fd = createFile(clientEngine, testFileName);
    return ((void *)fd);

}

static void *ESetStore_Open(char *testFileName, IOR_param_t * param){
    connectToMetaServer();
    int *fd;
    fd = (int *)malloc(sizeof(int));
    
    *fd = openFile(clientEngine, testFileName);
    
    return ((void *)fd);

}

static IOR_offset_t ESetStore_Xfer(int access, void *fd, IOR_size_t * buffer,
                                   IOR_offset_t length, IOR_param_t * param)
{
    int ret;
    int fileFd = (int *)fd;
    
    if (access == WRITE)
    {
        writeFile(clientEngine, fileFd, buffer, length);
    }else{
        readFile(clientEngine, fileFd, buffer, length);
    }
    return length;
}

static void ESetStore_Close(void *fd, IOR_param_t * param)
{
    int fileFd = (int *)fd;
    closeFile(clientEngine,  fileFd);
    finalizeClient();
    return;
}

static void ESetStore_Delete(char *testFileName, IOR_param_t * param){
    conectToMetaServer();
    deleteFile(clientEngine, testFileName);
    finalizeClient();
}

static void ESetStore_Fsync(void *fd, IOR_param_t * param)
{
    return;
}

static IOR_offset_t ESetStore_GetFileSize(IOR_param_t * test, MPI_Comm testComm, char *testFileName){
    connectToMetaServer();
    int fd = openFile(clientEngine, testFileName);
    IOR_offset_t fileSize = (IOR_offset_t) getFileSize(clientEngine, fd);
    closeFile(clientEngine, fd);
    finalizeClient();
    return fileSize;
}

static int ESetStore_StatFS(const char *oid, ior_aiori_statfs_t *stat_buf,
                            IOR_param_t *param){
    WARN("statfs not supported in ESetStore backend!");
    return -1;
}

static int ESetStore_MkDir(const char *oid, mode_t mode, IOR_param_t *param)
{
    WARN("mkdir not supported in ESetStore backend!");
    return -1;
}

static int ESetStore_RmDir(const char *oid, IOR_param_t *param)
{
    WARN("rmdir not supported in ESetStore backend!");
    return -1;
}

static int ESetStore_Access(const char *oid, int mode, IOR_param_t *param){
//    int ret = -1;
//    connectToMetaServer();
//    int fd = openFile(clientEngine, testFileName);
//    if (fd > 0) {
//        ret = 0;
//        closeFile(clientEngine, fd);
//    }
    
    return 0;
}

static int ESetStore_Stat(const char *, struct stat *, IOR_param_t *){
    WARN("stat not supported in ESetStore backend!");
    return -1;
}
