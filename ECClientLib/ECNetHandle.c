//
//  ECNetHandle.c
//  ECClient
//
//  Created by Liu Chengjian on 17/9/4.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include "ECNetHandle.h"
#include "ECClientCommon.h"

#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <strings.h>

#define DEFAULT_ACCEPT_TIME_OUT_IN_SEC 1000

int create_epollfd(){
    return epoll_create(10);
}

int create_TCP_sockFd(){
    return socket(AF_INET, SOCK_STREAM, 0);
}

int connectMetaEngine(ECClientEngine_t *clientEnginePtr){
    clientEnginePtr->metaSockFd = create_TCP_sockFd();
    
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(clientEnginePtr->metaIP);
    serv_addr.sin_port = htons(clientEnginePtr->metaPort);
    
    if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
        close(clientEnginePtr->metaSockFd);
        printf("Error on convert meta IP addr\n");
        printf("meta server IP:%s\n", clientEnginePtr->metaIP);
        return -1;
    }
    
    if(connect(clientEnginePtr->metaSockFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1){
        close(clientEnginePtr->metaSockFd);
        printf("meta server IP:%s port:%d\n", clientEnginePtr->metaIP, clientEnginePtr->metaPort);
        perror("Connect error to meta server");
        return -1;
    }

    return 0;
}

ssize_t writeCmdToSock(int sockFd, char *cmdStr, size_t size){
    //printf("Write:%s to sock\n", cmdStr);
    ssize_t writeSize = write(sockFd, (void *)cmdStr, size);
    
    return writeSize;
}

size_t recvMetaReply(int sockFd, char *buf, size_t bufSize){
    size_t recvdSize = 0, recvFlag = 1;
    ssize_t recvSize = 0;
    
    do{
        recvSize = recv(sockFd, buf + recvdSize, (bufSize - recvdSize), 0);

        if (recvSize <= 0) {
            perror("recvSize == 0");
            return recvSize;
        }
        
        size_t idx;
        for (idx = recvdSize; idx < (recvdSize + recvSize); ++idx) {
            if (buf[idx] == '\0') {
                return ((size_t)recvSize + recvdSize);
            }
        }
        
        recvdSize = recvdSize + (size_t)recvSize;
        
        if (recvdSize == bufSize) {
            return bufSize;
        }
        
    }while(recvFlag == 1);
    
    return 0;
}

int make_non_blocking_sock(int sfd)
{
    int flags, s;
    
    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror ("fcntl");
        return -1;
    }
    
    flags |= O_NONBLOCK;
    s = fcntl (sfd, F_SETFL, flags);
    if (s == -1)
    {
        perror ("fcntl");
        return -1;
    }
    
    return 0;
}


void closeSockFd(int sockFd){
    close(sockFd);
}

int update_event(int efd, uint32_t opFlag, int fd, void *arg){
    struct epoll_event eEvent;
    eEvent.events = opFlag;
    if (arg == NULL) {
        eEvent.data.fd = fd;
    }else{
        eEvent.data.ptr = arg;
    }
    
    int status = epoll_ctl(efd, EPOLL_CTL_MOD,fd,&eEvent);
    
    return status;
}

int add_event(int efd, uint32_t opFlag, int fd, void *arg){
    struct epoll_event eEvent;
    eEvent.events = opFlag;
    
    if (arg == NULL) {
        eEvent.data.fd = fd;
    }else{
        eEvent.data.ptr = arg;
    }
    
    int status = epoll_ctl(efd, EPOLL_CTL_ADD,fd,&eEvent);
    
    return status;
}

int delete_event(int efd, int fd){
    int status = epoll_ctl(efd, EPOLL_CTL_DEL,fd,NULL);
    
    return status;
}
