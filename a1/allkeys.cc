#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <string>
#include "constants.h"
using namespace std;

extern int mybind(int sockfd, struct sockaddr_in *addr);

int main(int argc, char *argv[]) {
    // grab server ip and port

    struct sockaddr_in server;
    bzero(&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    if(!inet_aton(argv[1], &(server.sin_addr))) {
        perror("invalid server-ip"); 
        return -1;
    }
    server.sin_port = htons(atoi(argv[2]));

    // create and bind a socket
    int sockfd = -1;
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("could not create socket"); 
        return -1;
    }
    struct sockaddr_in client;
    bzero(&client, sizeof(struct sockaddr_in));
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = htonl(INADDR_ANY);
    client.sin_port = 0; // Let OS choose.
    if(mybind(sockfd, &client) < 0) {
        perror("could not bind socket"); 
        return -1;
    }
    // fill client with actual socket address
    socklen_t alen = sizeof(struct sockaddr_in);
    if(getsockname(sockfd, (struct sockaddr *)&client, &alen) < 0) {
        perror("getsockname"); 
        return -1;
    }
    //printf("client associated with %s %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
    // printf("Trying to connect to %s %d...\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    if(connect(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
        printf("Error: no such peer\n"); 
        return -1;
    }

    printf("Connection established with peer.\n");


    //type
    char sendd = 'a';

    if(send(sockfd, &sendd, 1, 0) != 1) {
        perror("Failed to send type");
        return -1;
    }

    stringstream buffer;
    char lc = ',';
    while (true) {
        char c;
        if(recv(sockfd, &c, 1, 0) != 1) {
            perror("Failed to get lookup confirmation");
            return -1;
        }
        buffer << c;
        if (c == '0' && lc == ',') {
            break;
        }
        lc = c;
    }

    cout << "the keys are " << buffer.str() << endl;

    if(shutdown(sockfd, SHUT_RDWR) < 0) {
        perror("Could not shut down connection"); 
        return -1;
    }
    if(close(sockfd) < 0) {
        perror("close(peerfd)"); 
    }

    return 0;
}
