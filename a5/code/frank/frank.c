/* Mahesh V. Tripunitara
 * University of Waterloo
 * tcp-client.c -- takes as cmd line args a server ip & port.
 * After establishing a connection, reads from stdin till eof
 * and sends everything it reads to the server. sleep()'s
 * occasionally in the midst of reading & sending.
 */

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
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
 #include <string>
using namespace std;

unsigned int getrand() {
    int f = open("/dev/urandom", O_RDONLY);
    if(f < 0) {
    perror("open(/dev/urandom)"); return 0;
    }

    unsigned int ret;
    read(f, &ret, sizeof(unsigned int));
    close(f);
    return ret;
}

void gen_random(char *s, const int len) {
    char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[getrand() % (sizeof(alphanum) - 1)];
    }

    s[len-1] = 0;
}

int main(int argc, char *argv[]) {
    if(argc < 3) {
        printf("usage: %s <server-ip> <server-port>\n", argv[0]);
        exit(0);
    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a;

    memset(&a, 0, sizeof(struct sockaddr_in));
    a.sin_family = AF_INET;
    a.sin_port = 0;
    a.sin_addr.s_addr = INADDR_ANY;

    if(bind(s, (const struct sockaddr *)(&a), sizeof(struct sockaddr_in)) < 0) {
        perror("bind"); exit(1);
    }

    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)(atoi(argv[2])));
    if(inet_aton(argv[1], &(a.sin_addr)) < 0) {
        fprintf(stderr, "inet_aton(%s) failed.\n", argv[1]);
        exit(1);
    }

    if(connect(s, (const struct sockaddr *)(&a), sizeof(struct sockaddr_in)) < 0) {
        perror("connect"); exit(1);
    }

    int it = 0;
    int len = 10;
    while (it < 1000) {
        char buf[len];
        printf("%d iteration\n", it);
        gen_random(buf, len);
        if(send(s, buf, len, 0) < 0) {
            perror("send"); exit(1);
        }

        char received_buf[len];
        int recvlen = 0;
        if((recvlen = recv(s, received_buf, len, 0)) < 0) {
            perror("RECEIVE ERROR\n");
            exit(1);
        }

        cout << buf << endl;
        cout << received_buf << " with len " << recvlen << endl;
        if (strcmp(received_buf, buf) != 0) {
            printf("sent string %s\n and got %s\n", buf, received_buf);
            exit(1);
        }

        it += 1;
    }

    printf("SUCCESSFULLY VERIFIED %d STRINGS\n", it);

    shutdown(s, SHUT_RDWR);
    close(s);

    return 0;
}