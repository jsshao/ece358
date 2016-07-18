#include <cstdlib>
#include <string>
#include <iostream>
#include "rcs.h"
#include "ucp.h"
using namespace std;

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "net_util.h"

#define BUFLEN 500
#define NUMSOCKETS 5000

struct RcsSocket {
    bool inUse;
    int ucpSocket;
    struct sockaddr_in local;
    struct sockaddr_in remote;

    RcsSocket() {
        inUse = false;
    }
};

struct RcsSocket rcsSockets[NUMSOCKETS];

int rcsSocket() 
{
    for(int i=0; i<NUMSOCKETS; i++) {
        RcsSocket &rs = rcsSockets[i];
        if(!rs.inUse) {
            int sockfd = ucpSocket();
            if(sockfd < 0)
                return -1;

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
        if(ucpGetSockName(rs.ucpSocket, addr) == 0) {
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
    string first = "first";
    string second = "second";
    string third = "third";
    char buf[BUFLEN];
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        for(int i=0; i<NUMSOCKETS; i++) {
            RcsSocket &newRs = rcsSockets[i];
            if(!newRs.inUse) {

                //need valid handshake
                while(true) {
                    int len = ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, addr);
                    buf[len] = '\0';


                    if(first == buf) {
                        break; 
                    } else {
                        cout<<"accept first wtf: "<<buf<<endl;
                        exit(1);
                    }
                }

                newRs.inUse = true;
                newRs.ucpSocket = ucpSocket();
                newRs.remote = *addr;
                newRs.local = rs.local;
                newRs.local.sin_port = 0;
                ucpBind(newRs.ucpSocket, &(newRs.local));
                ucpGetSockName(newRs.ucpSocket, &(newRs.local));

                // send new socket info
                cout<<"server port: "<<ntohs(newRs.local.sin_port)<<endl;
                printf("server client info %s %u\n", inet_ntoa(newRs.remote.sin_addr), ntohs(newRs.remote.sin_port));
                ucpSendTo(newRs.ucpSocket, second.c_str(), second.length(), &(newRs.remote));

                // acknowledgement
                cout<<"sent, waiting for acknowlegement"<<endl;
                int len = ucpRecvFrom(newRs.ucpSocket, buf, BUFLEN-1, addr);
                buf[len] = '\0';
                if(third != buf) {
                    cout<<"accept first wtf: "<<buf<<endl;
                    exit(1);
                }

                return i;
            }
        }
    }
    return -1;
}

int rcsConnect(int sockfd, const struct sockaddr_in *addr)
{
    string first = "first";
    string second = "second";
    string third = "third";
    char buf[BUFLEN];
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        rs.remote = *addr;

        //need valid handshake
        printf("true client info %s %u\n", inet_ntoa(rs.local.sin_addr), ntohs(rs.local.sin_port));
        int len = ucpSendTo(rs.ucpSocket, first.c_str(), first.length(), &(rs.remote));
        len = ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, &(rs.remote));
        buf[len] = '\0';
        if(second != buf) {
            cout<<"connect wtf: "<<buf<<endl;
            exit(1);
        }
        cout<<"received second"<<endl;
        cout<<"server port: "<<ntohs(rs.remote.sin_port)<<endl;
        len = ucpSendTo(rs.ucpSocket, third.c_str(), third.length(), &(rs.remote));

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
