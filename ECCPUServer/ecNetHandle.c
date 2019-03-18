//
//  ecNetHandle.c
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <strings.h>

#include "ecNetHandle.h"
#include "ecCommon.h"
#include "ClientHandle.h"



#define MAX_CONN_EVENTS MAX_EVENTS
#define DEFAULT_ACCEPT_TIME_OUT_IN_SEC 1000

int delete_event(int efd, int fd);

void accept_client_conn(ECBlockServerEngine_t*blockServerEnginePtr, int fd){
//    struct connection *conn = new_connection();
    struct sockaddr_in remoteAddr;
    socklen_t nAddrlen = sizeof(remoteAddr);
    
    int sockFd = accept(fd, (struct sockaddr *)&remoteAddr, &nAddrlen);
    printf("Accept client conn fd:%d\n", sockFd);
    ECClient_t *clientPtr = newECClient(sockFd);
    
    addComingECClient(blockServerEnginePtr->ecClientMgr, clientPtr);
}

//void accept_server_conn(int fd){
//    
//}

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

void start_wait_conn(ECBlockServerEngine_t *ecBlockServerEnginePtr){
    int idx, eventsNum;
    
//    struct epoll_event clientEvent, serverEvent;
    struct epoll_event* events;

    int status = add_event(ecBlockServerEnginePtr->eClientfd, EPOLLIN | EPOLLET, ecBlockServerEnginePtr->clientFd, NULL);
    if(status == -1)
    {
        perror("epoll_ctl clientEvent");
        return;
    }

    events = talloc(struct epoll_event, MAX_CONN_EVENTS);
    
    if (events == NULL) {
        perror("unable to alloc memory for events");
        return;
    }
    
    printf("start to wait events\n");
    
    while (ecBlockServerEnginePtr->exitFlag == 0) {
        eventsNum = epoll_wait(ecBlockServerEnginePtr->eClientfd, events, MAX_EVENTS,DEFAULT_ACCEPT_TIME_OUT_IN_SEC);
        
        if (eventsNum  <= 0) {
//            printf("Time out\n");
            continue;
        }
        
        for (idx = 0 ; idx < eventsNum; ++idx) {
            if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)||
               (!(events[idx].events & EPOLLIN)))
            {
                fprintf(stderr,"epoll error\n");
                if (ecBlockServerEnginePtr->clientFd == events[idx].data.fd) {
                    delete_event(ecBlockServerEnginePtr->eClientfd, events[idx].data.fd);
                    perror("error on the client fd, have to close\n");
                }
                
                close(events[idx].data.fd);
                break;
            }else if(ecBlockServerEnginePtr->clientFd == events[idx].data.fd){
                /*Notification on the client*/
                accept_client_conn(ecBlockServerEnginePtr, ecBlockServerEnginePtr->clientFd);
            }

        }
    }
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

void dry_pipe_msg(int sockFd){
    ssize_t readSize;
    char readBuf[1024];
    while((readSize =  read(sockFd, readBuf, 10)) > 0 ){
    }
}
