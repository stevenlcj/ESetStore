//
//  ecNetHandle.c
//  EStoreMeta
//
//  Created by Liu Chengjian on 17/4/24.
//  Copyright (c) 2017年 csliu. All rights reserved.
//

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <strings.h>
#include <arpa/inet.h>

#include "ECMetaEngine.h"
#include "ecNetHandle.h"
#include "ecCommon.h"
#include "ECClientManagement.h"
#include "ClientConnection.h"

#define DEFAULT_ACCEPT_TIME_OUT_IN_SEC 1000

int accept_client_conn(ECMetaServer_t *ecMetaServer){
//    struct connection *conn = new_connection();
    struct sockaddr_in remoteAddr;
    socklen_t nAddrlen = sizeof(remoteAddr);

    int clientFd = accept(ecMetaServer->clientFd, (struct sockaddr *)&remoteAddr, &nAddrlen);
    
    if(clientFd <= 0)
    {
        return -1;
    }
    
    make_non_blocking_sock(clientFd);

    printf("Accept sock:%d from client\n", clientFd);
    
    ClientConnection_t *clientConn = createClientConn(clientFd);
    //clientConn->logFd = createLogFd(clientFd);
    
    size_t minConnThreadIdx = 0, minConnNum = CLIENT_CONN_SIZE,idx;
    for (idx = 0; idx < ecMetaServer->ecClientThreadMgr->clientThreadsNum; ++idx) {
        ECClientThread_t *clientThreadPtr = ecMetaServer->ecClientThreadMgr->clientThreads + idx;
        if (minConnNum > clientThreadPtr->clientConnNum) {
            minConnNum = clientThreadPtr->clientConnNum;
            minConnThreadIdx = idx;
        }
    }
    
    ECClientThread_t *ecClientThreadPtr = ecMetaServer->ecClientThreadMgr->clientThreads + minConnThreadIdx;
    addConnection(ecClientThreadPtr, clientConn);
    
    return clientFd;
}

void accept_client_conns(ECMetaServer_t *ecMetaServer){
    while (accept_client_conn(ecMetaServer) > 0) {
    }
    
    return;
}

int accept_server_conn(ECMetaServer_t *ecMetaServer){
    struct sockaddr_in remoteAddr;
    socklen_t nAddrlen = sizeof(remoteAddr);

    int serverFd = accept(ecMetaServer->chunkFd, (struct sockaddr *)&remoteAddr, &nAddrlen);
    
    if(serverFd <= 0)
    {
        printf("accept error for server!");
        return -1;
    }
    
    
    char *IP = inet_ntoa(remoteAddr.sin_addr);
    
    Server_t *serverPtr = locateServer(ecMetaServer->rMap, IP);
    
    if (serverPtr == NULL) {
        printf("Unrecorded Server Connected, close it\n");
        close(serverFd);
        return -1;
    }
    
    printf("Server from rack:%d, server id:%d connected\n",serverPtr->rackId, serverPtr->serverId);
    
    if (serverPtr->curState == SERVER_UNCONNECTED) {
        serverPtr->curState = SERVER_CONNECTED;
        
        BlockServer_t *blockServer = createBlockServer(serverFd);
        make_non_blocking_sock(serverFd);

        if (blockServer == NULL) {
            return -1;
        }
        
        blockServer->serverId = serverPtr->serverId;
        blockServer->rackId = serverPtr->rackId;
        
        addNewBlockServer(ecMetaServer->blockServerMgr, blockServer);
    }

    return 0;
}

int create_epollfd(){
    return epoll_create(10);
}

int create_bind_listen (int port)
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

void start_wait_conn(ECMetaServer_t *ecMetaServer){
    int idx, eventsNum;
    
//    struct epoll_event clientEvent, serverEvent;
    struct epoll_event* events;

//    clientEvent.data.fd = ecMetaServer->clientFd;
//    clientEvent.events = EPOLLIN | EPOLLET;
//    
//    serverEvent.data.fd = ecMetaServer->chunkFd;
//    serverEvent.events = EPOLLIN | EPOLLET;
//
//    int status = epoll_ctl(ecMetaServer->efd, EPOLL_CTL_ADD,ecMetaServer->clientFd,&clientEvent);
    int status = add_event(ecMetaServer->efd, EPOLLIN | EPOLLET, ecMetaServer->clientFd, NULL);
    if(status ==-1)
    {
        perror("epoll_ctl clientEvent");
        return;
    }
    
    status = add_event(ecMetaServer->efd, EPOLLIN | EPOLLET,ecMetaServer->chunkFd, NULL);
    
    if(status == -1)
    {
        perror("epoll_ctl serverEvent");
        return;
    }

    events = talloc(struct epoll_event, MAX_CONN_EVENTS);
    
    if (events == NULL) {
        perror("unable to alloc memory for events");
        return;

    }
    
//    printf("start to wait events\n");
    
    while (ecMetaServer->exitFlag == 0) {
        eventsNum = epoll_wait(ecMetaServer->efd, events, MAX_CONN_EVENTS,DEFAULT_ACCEPT_TIME_OUT_IN_SEC);
        
        if (eventsNum  <= 0) {
//            printf("Time out\n");
            continue;
        }
        
//        printf("Event with amount:%d happened\n",eventsNum);
        
        for (idx = 0 ; idx < eventsNum; ++idx) {
            if((events[idx].events & EPOLLERR)||
               (events[idx].events & EPOLLHUP)||
               (!(events[idx].events & EPOLLIN)))
            {
                fprintf(stderr,"epoll error\n");
                if (ecMetaServer->clientFd == events[idx].data.fd) {
//                    epoll_ctl(ecMetaServer->efd, EPOLL_CTL_DEL, NULL);
                    delete_event(ecMetaServer->efd, events[idx].data.fd);
                    perror("error on the client fd, have to close\n");
                }
                
                if (ecMetaServer->chunkFd == events[idx].data.fd) {
                    delete_event(ecMetaServer->efd, events[idx].data.fd);
                    perror("error on the chunk fd, have to close\n");
                }
                
                close(events[idx].data.fd);
                continue;
            }else if(ecMetaServer->clientFd == events[idx].data.fd){
                /*Notification on the client*/
                //printf("Client connection comed \n");
                accept_client_conns(ecMetaServer);
            }else if(ecMetaServer->chunkFd == events[idx].data.fd){
                /*Notification on the chunk*/
                printf("Block server connection comed\n");
                accept_server_conn(ecMetaServer);
            }else{
                printf("Unexpected error\n");
            }

        }
    }
}

//void start_epoll_ClientConn(struct ClientThread *clientThread){
//    if (clientThread->handleConnNum == 0) {
//        return;
//    }
//    
//    while (clientThread->pendingWaitMsgConn != NULL) {
//        struct connection *conn = clientThread->pendingWaitMsgConn;
//        clientThread->pendingWaitMsgConn = clientThread->pendingWaitMsgConn->next;
//        
//        add_event(clientThread->efd, EPOLLIN | EPOLLET, conn->fd);
//        
//        conn->next = clientThread->waitingMsgConn;
//        clientThread->waitingMsgConn = conn;
//    }
//    
//    while (clientThread->waitingWriteConn != NULL) {
//        struct connection *conn = clientThread->waitingWriteConn;
//    }
//    
//    int idx;
//    int eventsNum = epoll_wait(clientThread->efd,clientThread->events,MAX_EVENTS,DEFAULT_TIMEOUT_IN_Millisecond);
//    
//    for (idx = 0; idx < eventsNum; ++idx) {
//        if(events[i].events & EPOLLIN){
//            
//        }else if (events[i].events&EPOLLOUT){
//            
//        }
//    }
//}

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