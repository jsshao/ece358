#include <iostream>
#include <vector>
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

using namespace std;

extern int pickServerIPAddr(struct in_addr *srv_ip);

struct Peer {
    string ip;
    int port;
};

int main(int argc, char* argv[]) {
    if (argc != 1 && argc != 3) {
        cerr << "Invalid Input" << endl;
        return -1;
    }

    // Pick port to new peer
    struct in_addr srvip;
    if (pickServerIPAddr(&srvip) < 0) {
        fprintf(stderr, "pickServerIPAddr() returned error.\n");
        return -1;
    }

    int sockfd = -1;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket"); 
        return -1;
    }

    struct sockaddr_in server;
    bzero(&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    memcpy(&(server.sin_addr), &srvip, sizeof(struct in_addr)); // From above
    server.sin_port = 0; // Allow OS to pick port

    if(bind(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
        perror("bind"); 
        return -1;
    }

    socklen_t alen = sizeof(struct sockaddr_in);
    if(getsockname(sockfd, (struct sockaddr *)&server, &alen) < 0) {
        perror("getsockname"); 
        return -1;
    }

    cout << inet_ntoa(server.sin_addr) << " " << ntohs(server.sin_port) << endl;

    vector<Peer> peers;
    // First peer
    if (argc == 1) {

    } 
    // Update existing peers
    else {

    }

    return 0;
}
