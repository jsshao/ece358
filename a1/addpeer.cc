#include <iostream>
#include <vector>
#include <unordered_map>
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
#include <functional>
//#include <chrono>
using namespace std;

class Peer;

extern int pickServerIPAddr(struct in_addr *srv_ip);
extern int mybind(int sockfd, struct sockaddr_in *addr);
void addcontent_f(int sockfd, char *buf, Peer &me);
//void removecontent_f(int sockfd, char *buf);
void addcontent(int sockfd, char *buf, vector<Peer> &peers);
//void removecontent(int sockfd, char *buf, vector<Peer> &peers);

class Peer {
    private:
        sockaddr_in socket;
        int sockfd;
        long load;
        unordered_map<size_t, string> content; 

    public:
        Peer(sockaddr_in socket, int sockfd) {
            this->socket = socket;
            this->sockfd = sockfd;
            this->load = 0;
        }

        sockaddr_in getSocket() {
            return socket;
        }

        int getSockfd() {
            return sockfd;
        }

        long getLoad() {
            return load;
        }

        string get(size_t key) {
            return content[key];
        }

        void put(size_t key, string value) {
            content[key] = value;
        }
};

Peer initialize() {
    // Pick port to new peer
    struct in_addr srvip;
    if (pickServerIPAddr(&srvip) < 0) {
        fprintf(stderr, "pickServerIPAddr() returned error.\n");
        exit(-1);
    }

    int sockfd = -1;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket"); 
        exit(-1);
    }

    struct sockaddr_in server;
    bzero(&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    memcpy(&(server.sin_addr), &srvip, sizeof(struct in_addr)); // From above
    server.sin_port = 0; // Allow OS to pick port

    if (mybind(sockfd, &server) < 0) {
        perror("bind"); 
        exit(-1);
    }

    socklen_t alen = sizeof(struct sockaddr_in);
    if (getsockname(sockfd, (struct sockaddr *)&server, &alen) < 0) {
        perror("getsockname"); 
        exit(-1);
    }

    cout << inet_ntoa(server.sin_addr) << " " << ntohs(server.sin_port) << endl;
    return Peer(server, sockfd);
}


int main(int argc, char* argv[]) {
    if (argc != 1 && argc != 3) {
        cerr << "Invalid Input" << endl;
        return -1;
    }

    // Initialize current peer
    vector<Peer> peers;
    peers.push_back(initialize());

    // Update existing peers
    if (argc == 3) {
         
    }

    if (fork()) {
        // Parent just prints socket address and returns to user
        return 0;
    }

    // Handle requests
    for (;;) {
        if (listen(peers[0].getSockfd(), 0) < 0) {
            perror("listen"); 
            return -1;
        }

        int connectedsock;
        struct sockaddr_in client;
        socklen_t alen = sizeof(struct sockaddr_in);
        if ((connectedsock = accept(peers[0].getSockfd(), (struct sockaddr *)&client, &alen)) < 0) {
            perror("accept"); 
            return -1;
        }

        printf("Connection accepted from %s %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        size_t buflen = 256;
        char buf[buflen];
        ssize_t recvlen;
        if ((recvlen = recv(connectedsock, buf, buflen-1, 0)) < 0) {
            perror("recv"); 
            return -1;
        }
        buf[recvlen] = 0; // ensure null-terminated string
        printf("Child %d received the following %d-length string: %s\n",
            getpid(), (int)recvlen, buf);

        switch(buf[0]) {
            //case 2:
                //addcontent_f(peers[0]);
            //case 21:
                //addcontent(connectedsock, buf, peers);
        }




        printf("Child %d shutting down...\n", getpid());
        if(shutdown(connectedsock, SHUT_RDWR) < 0) {
            perror("shutdown, child"); 
            return -1;
        }

        break;
    }
    return 0;
}

void addcontent_f(char *buf, Peer me) {
    //auto a = std::chrono::system_clock::now();
    //time_t b = std::chrono::system_clock::to_time_t(a);
    //return static_cast<long>(b);
    hash<string> str_hash;
    size_t key = str_hash(buf);
    me.put(key, buf);
    //me.setLoad(me.getLoad()+1);
}


void addcontent(int sockfd, char *buf, vector<Peer> &peers) {
    //long my_load = me.getLoad();
    //bool exception = False;
    //size_t selected_peer = 0;
    //for( size_t i = 1; i<peers.size(); i++ ) {
        //long load = peers[i].getLoad();
        //if( load < my_load ) {
            ////peers[i].setLoad(load+1);
            //selected_peer = i;
            ////send(peers)
            //break;
        //}
        //if( load > my_load ) {
            ////addcontent_f(buf, peers[0]);
            //break;
        //}
    //}


    //for( size_t i = 1; i<peers.size(); i++ ) {
        
    //}


    ////if(send(connectedsock, buf, strlen(buf), 0) < 0) {
        ////perror("send"); 
        ////return -1;
    ////}
}
//void removecontent(int sockfd, char *buf) {
//}
