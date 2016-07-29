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
#include <iostream>

#include "net_util.h"
using namespace std;

int main(int argc, char *argv[]) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a;

    memset(&a, 0, sizeof(struct sockaddr_in));
    a.sin_family = AF_INET;
    a.sin_port = 0;
    if((a.sin_addr.s_addr = getPublicIPAddr()) == 0) {
    fprintf(stderr, "Could not get public ip address. Exiting...\n");
    exit(0);
    }

    if(mybind(s, &a) < 0) {
    fprintf(stderr, "mybind() failed. Exiting...\n");
    exit(0);
    }

    printf("%s %u\n", inet_ntoa(a.sin_addr), ntohs(a.sin_port));
    
    if(listen(s, 0) < 0) {
        perror("listen"); exit(0);
    }

    printf("LISTENING\n");

    memset(&a, 0, sizeof(struct sockaddr_in));
    socklen_t alen = sizeof(struct sockaddr_in);
    int asock;

    if ((asock = accept(s, (struct sockaddr *)&a, &alen)) < 0) {
        perror("ACCEPT\n");
    }

    int len = 10;
    for (;;) {
        char buf[len];
        cout << "WAITING... \n";
        if (recv(asock, buf, len, 0) < 0) {
            perror("recv error"); 
            exit(1);
        }

        char bufcpy[len];
        memcpy(bufcpy, buf, len);
        cout << "REPLYING... " << bufcpy << endl;
        if(send(asock, bufcpy, len, 0) < 0) {
            perror("send"); exit(1);
        }
    }

    close(asock);
    return 0;
}
