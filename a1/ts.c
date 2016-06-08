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

/* Based on the code from your TA, Meng Yang */

extern int pickServerIPAddr(struct in_addr *srv_ip);

int main(int argc, char *argv[]) {
    int nconnections = 0;
    if(argc < 2) { nconnections = 1; }
    else { nconnections = atoi(argv[1]); }

    if(nconnections <= 0) {
	fprintf(stderr, "nconnections <= 0. Exiting...\n");
	return -1;
    }

    struct in_addr srvip;
    if(pickServerIPAddr(&srvip) < 0) {
	fprintf(stderr, "pickServerIPAddr() returned error.\n");
	exit(-1);
    }

    int sockfd = -1;
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	perror("socket"); return -1;
    }

    struct sockaddr_in server;
    bzero(&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    memcpy(&(server.sin_addr), &srvip, sizeof(struct in_addr)); // From above
    server.sin_port = 0; // Allow OS to pick port

    if(bind(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
	perror("bind"); return -1;
    }

    socklen_t alen = sizeof(struct sockaddr_in);
    if(getsockname(sockfd, (struct sockaddr *)&server, &alen) < 0) {
	perror("getsockname"); return -1;
    }

    printf("server starting at %s %d, will accept() %d connection(s)\n",
	inet_ntoa(server.sin_addr), ntohs(server.sin_port),
	nconnections);

    for(int i = 0; i < nconnections; i++) {
	if(listen(sockfd, 0) < 0) {
	    perror("listen"); return -1;
	}

	int connectedsock;
	struct sockaddr_in client;
	alen = sizeof(struct sockaddr_in);
	if((connectedsock = accept(sockfd, (struct sockaddr *)&client, &alen)) < 0) {
	    perror("accept"); return -1;
	}

	printf("Connection accepted from %s %d\n",
	       inet_ntoa(client.sin_addr), ntohs(client.sin_port));

	if(fork()) {
	    // Parent -- go back to listen()-ing
	    if(close(connectedsock) < 0) {
		perror("close(parent, connectedsock)"); 
		// Keep going
	    }

	    continue;
	}
	else {
	    // Child -- accept()
	    if(close(sockfd) < 0) {
		perror("close(child, sockfd)"); 
		// Keep going
	    }

	    size_t buflen = 10;
	    char buf[buflen];
	    ssize_t recvlen;
	    if((recvlen = recv(connectedsock, buf, buflen-1, 0)) < 0) {
		perror("recv"); return -1;
	    }

	    buf[recvlen] = 0; // ensure null-terminated string
	    printf("Child %d received the following %d-length string: %s\n",
		    getpid(), (int)recvlen, buf);

	    strcpy(buf, "bye");
	    printf("Child %d sending %s\n", getpid(), buf);
	    if(send(connectedsock, buf, strlen(buf), 0) < 0) {
		perror("send"); return -1;
	    }

	    printf("Child %d shutting down...\n", getpid());
	    if(shutdown(connectedsock, SHUT_RDWR) < 0) {
		perror("shutdown, child"); return -1;
	    }

	    return 0;
	    //Child is done
	}
    }

    // Only parent gets here. Not a child.
    printf("Parent shutting down...\n");
    if(shutdown(sockfd, SHUT_RDWR) < 0) {
	perror("shutdown, child"); return -1;
    }
    return 0;
}
