#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "rcs.h"
#include "ucp.h"


#define BUFLEN 500
#define SOCKETS 5000

struct RcsSocket {
	int ucpSocket;
	struct sockaddr_in addr;
	struct sockaddr_in conn;
};

struct RcsSocket* rcsSockets[5000] = {0};

int rcsSocket() 
{
	for(int i=0; i<100; i++) {
		if(rcsSockets[i] == NULL) {
			int sockfd = ucpSocket();
			if(sockfd < 0)
				return -1;
			rcsSockets[i] = malloc(sizeof(struct RcsSocket));
			rcsSockets[i]->ucpSocket = sockfd;
			return i;
		}
	}
	return -1;
}

int rcsBind(int sockfd, struct sockaddr_in *addr) 
{
	if(rcsSockets[sockfd] != NULL) {
		if(ucpBind(rcsSockets[sockfd]->ucpSocket, addr) == 0) {
			rcsSockets[sockfd]->addr = *addr;
			return 0;
		}
	}
	return -1;
}

int rcsGetSockName(int sockfd, struct sockaddr_in *addr) 
{
	if(rcsSockets[sockfd] != NULL) {
		*addr =  rcsSockets[sockfd]->addr;
		return 0;
	}
	return -1;
}

int rcsListen(int sockfd)
{
	if(rcsSockets[sockfd] != NULL) {
		return 0;
	}
	return -1;
}

int rcsAccept(int sockfd, struct sockaddr_in *addr)
{
	if(rcsSockets[sockfd] != NULL) {
		for(int i=0; i<100; i++) {
			if(rcsSockets[i] == NULL) {
				char buf[BUFLEN];

				ucpRecvFrom(rcsSockets[sockfd]->ucpSocket, buf, BUFLEN-1, addr);
				rcsSockets[i] = malloc(sizeof(struct RcsSocket));
				*rcsSockets[i] = *rcsSockets[sockfd];
				rcsSockets[i]->conn = *addr;
				return i;
			}
		}
	}
	return -1;
}

int rcsConnect(int sockfd, const struct sockaddr_in *addr)
{
	const char* buf = "connect";
	if(rcsSockets[sockfd] != NULL) {
		rcsSockets[sockfd]->conn = *addr;
		int len = ucpSendTo(rcsSockets[sockfd]->ucpSocket, buf, strlen(buf), addr);
		if(len >= 0)
			return 0;
	}
	return -1;
}

int rcsRecv(int sockfd, void *buf, int len)
{
	if(rcsSockets[sockfd] != NULL) {
		printf("%s", ucpSocket);
		return ucpRecvFrom(rcsSockets[sockfd]->ucpSocket, buf, len, &(rcsSockets[sockfd]->conn));
	}
	return -1;
}

int rcsSend(int sockfd, void *buf, int len)
{
	if(rcsSockets[sockfd] != NULL) {
		return ucpSendTo(rcsSockets[sockfd]->ucpSocket, buf, len, &(rcsSockets[sockfd]->conn));
	}
	return -1;
}

int rcsClose(int sockfd)
{
	if(rcsSockets[sockfd] != NULL) {
		free(rcsSockets[sockfd]);
		rcsSockets[sockfd] = NULL;
	}
	return -1;
}
