#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <string>
#include <iostream>
using namespace std;


// send content, assuming other side already
// knows type. will send length
// followed by content
void sendcontent(int sockfd, const char* buf) {

    uint32_t len = strlen(buf);
    uint32_t bytesleft = len;
    uint32_t total = 0;
    int32_t sent;

    uint32_t nwlen = htonl(len); //length in big endian
    if( send(sockfd, &nwlen, sizeof(nwlen), 0) < 0 ) {
        perror("could not send message length"); 
        exit(1);
    }

    while(total < len) {
        // cout<<"sending"<<endl;
        if((sent = send(sockfd, buf+total, bytesleft, 0)) < 0) {
            perror("uhhh, it just randomly stopped sending");
            exit(1);
        }
        total += uint32_t(sent);
        bytesleft -= sent;
    }
}

// receive content, assuming I already
// knows type. will receive length
// followed by content
string recvcontent(int sockfd) {
    string s;
    uint32_t buflen = 256;
    char buf[buflen];
    int32_t recvlen;
    uint32_t total = 0;

    uint32_t nwlen; //length in big endian
    if(recv(sockfd, &nwlen, sizeof(nwlen), 0) != sizeof(nwlen)) {
        perror("could not receive length properly");
        exit(1);
    }
    uint32_t desired = ntohl(nwlen);
    while(total != desired) {
        // cout<<"desired"<<desired<<endl;
        // cout<<"receiving"<<endl;
        uint32_t attemptlen = buflen-1;
        uint32_t left = desired - total;
        if(left <= 0) { perror("receiving negative length"); break;}
        if(uint32_t(left) <= buflen-1) {attemptlen = left;}

        if ((recvlen = recv(sockfd, buf, attemptlen, 0)) < 0) {
            perror("uhhh, I didn't receive right length"); 
            exit(1);
        }
        total += uint32_t(recvlen);
        // cout<<"received: "<<recvlen<<endl<<"still has: "<<total<<endl;
        buf[recvlen] = 0;
        s += string(buf);
    }
    // cout<<"s: "<<s<<endl;
    return s;
}
