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
// knows type. will send 4 bytes of length
// followed by content
void sendcontent(int sockfd, char* buf) {

    size_t len = strlen(buf);
    size_t bytesleft = len;
    ssize_t total = 0;
    ssize_t sent;

    uint32_t nwlen = htonl(len); //length in big endian
    if( send(sockfd, &nwlen, sizeof(uint32_t), 0) < 0 ) {
        perror("could not send message length"); 
        exit(1);
    }

    while(total < len) {
        cout<<"sending"<<endl;
        if((sent = send(sockfd, buf+total, bytesleft, 0)) < 0) {
            perror("uhhh, it just randomly stopped sending");
            exit(1);
        }
        total += sent;
        bytesleft -= sent;
    }
}

// receive content, assuming I already
// knows type. will receive 4 bytes of length
// followed by content
string recvcontent(int sockfd) {
    string s;
    size_t buflen = 256;
    char buf[buflen];
    ssize_t recvlen;
    ssize_t total = 0;

    uint32_t nwlen; //length in big endian
    if(recv(sockfd, &nwlen, sizeof(uint32_t), 0) != sizeof(uint32_t)) {
        perror("could not receive length properly");
        exit(1);
    }
    ssize_t desired = ntohl(nwlen);
    while(total != desired) {
        cout<<"desired"<<desired<<endl;
        cout<<"receiving"<<endl;
        if ((recvlen = recv(sockfd, buf, buflen-1, 0)) < 0) {
            perror("uhhh, I didn't receive right length"); 
            exit(1);
        }
        total += recvlen;
        cout<<"received: "<<recvlen<<endl<<"still has: "<<total<<endl;
        buf[recvlen] = 0;
        s += string(buf);
    }
    cout<<"s: "<<s<<endl;
    return s;
}
