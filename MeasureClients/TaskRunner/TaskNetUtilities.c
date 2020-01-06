//
//  TaskNetUtilities.c
//  TaskDistributor
//
//  Created by Lau SZ on 2019/12/20.
//  Copyright © 2019 Shenzhen Technology University. All rights reserved.
//

#include "TaskNetUtilities.h"

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <strings.h>

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

int create_epollfd(){
    return epoll_create(10);
}

int create_TCP_sockFd(){
    return socket(AF_INET, SOCK_STREAM, 0);
}


int create_bind_listen(int port)
{
    int listenfd;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if(listenfd < 0)
    {
        printf("socket error\n");
        return -1;
    }
    
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    
    if(bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        printf("bind %d error\n", port);
        return -1;
    }
    
    listen(listenfd, 128);
    
    make_non_blocking_sock(listenfd);
    
    return listenfd;
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

int connectToIPPort(char *IPAddr, int port){
    int sockFd = create_TCP_sockFd();
    
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(IPAddr);
    
    if (serv_addr.sin_addr.s_addr == INADDR_NONE) {
        printf("Error on convert server IP \n");
        printf("server IP:%s\n", IPAddr);
        close(sockFd);
        return -1;
    }

    serv_addr.sin_port = htons(port);
    
    if (connect(sockFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Connect error to server");
        printf("server IP:%s\n", IPAddr);
        close(sockFd);

        return -1;
    }
    
    make_non_blocking_sock(sockFd);

    return sockFd;
}

ssize_t readSockData(int sockFd, char *buf, size_t bufSize){
    ssize_t totalReadSize = 0;
    ssize_t curRecvdSize = 0;
    
    do{
        curRecvdSize = recv(sockFd, buf + totalReadSize, bufSize - totalReadSize, 0);
        
        if (curRecvdSize > 0) {
            totalReadSize = totalReadSize + curRecvdSize;
        }
    }while(curRecvdSize > 0);
    
    if (totalReadSize == 0) {
        totalReadSize = curRecvdSize;
    }
    
    return totalReadSize;
}

