#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "rcs.h"
#include "ucp.h"


#define BUFLEN 500
#define SOCKETS 5000

struct RcsSocket {
    bool inUse;
    int ucpSocket;
    struct sockaddr_in local;
    struct sockaddr_in remote;

    RcsSocket() {
        inUse = false;
    }
};

struct RcsSocket rcsSockets[5000];

int rcsSocket() 
{
    for(int i=0; i<100; i++) {
        RcsSocket &rs = rcsSockets[i];
        if(!rs.inUse) {
            int sockfd = ucpSocket();
            if(sockfd < 0)
                return -1;

            int optval = 1;
            setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

            rs.ucpSocket = sockfd;
            rs.inUse = true;
            return i;
        }
    }
    return -1;
}

int rcsBind(int sockfd, struct sockaddr_in *addr) 
{
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        return ucpBind(rs.ucpSocket, addr);
    }
    return -1;
}

int rcsGetSockName(int sockfd, struct sockaddr_in *addr) 
{
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        if(ucpGetSockName(sockfd, addr) == 0) {
            rs.local = *addr;
            return 0;
        }
    }
    return -1;
}

int rcsListen(int sockfd)
{
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        return 0;
    }
    return -1;
}

int rcsAccept(int sockfd, struct sockaddr_in *addr)
{
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        for(int i=0; i<100; i++) {
            RcsSocket &newRs = rcsSockets[i];
            if(!newRs.inUse) {
                //need valid handshake
                char buf[BUFLEN];
                ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, addr);
                newRs = rs;
                newRs.remote = *addr;
                return i;
            }
        }
    }
    return -1;
}

int rcsConnect(int sockfd, const struct sockaddr_in *addr)
{
    const char* buf = "remoteect";
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        rs.remote = *addr;

        //need valid handshake
        int len = ucpSendTo(rs.ucpSocket, buf, strlen(buf), addr);
        if(len >= 0)
            return 0;
    }
    return -1;
}

int rcsRecv(int sockfd, void *buf, int len)
{
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        return ucpRecvFrom(rs.ucpSocket, buf, len, &(rs.remote));
    }
    return -1;
}

int rcsSend(int sockfd, void *buf, int len)
{
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        return ucpSendTo(rs.ucpSocket, buf, len, &(rs.remote));
    }
    return -1;
}

int rcsClose(int sockfd)
{
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        rs.inUse = false;
        return 0;
    }
    return -1;
}
