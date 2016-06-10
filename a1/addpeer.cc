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
extern string recvcontent(int sockfd);
extern void sendcontent(int sockfd, char* buf);
int createPeerConnection();
void addcontent(int sockfd, Peer &me);
void removecontent(int sockfd, Peer &me);

class Peer {
    private:
        sockaddr_in socket_addr;
        int sockfd;
        long load;
        unordered_map<size_t, string> content; 

    public:
        Peer(sockaddr_in socket_addr, int sockfd) {
            this->socket_addr = socket_addr;
            this->sockfd = sockfd;
            this->load = 0;
        }

        sockaddr_in getSocketId() {
            return socket_addr;
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
            load++;
        }
        void del(size_t key) {
            content.erase(key);
            load--;
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
        exit(1);
    }

    // Initialize current peer
    vector<Peer> peers;
    peers.push_back(initialize());

    // Update existing peers
    if (argc == 3) {
         
    }

    //if (fork()) {
        //// Parent just prints socket address and returns to user
        //return 0;
    //}

    // Handle requests
    for (;;) {
        if (listen(peers[0].getSockfd(), 0) < 0) {
            perror("listen"); 
            exit(1);
        }

        int connectedsock;
        struct sockaddr_in client;
        socklen_t alen = sizeof(struct sockaddr_in);
        if ((connectedsock = accept(peers[0].getSockfd(), (struct sockaddr *)&client, &alen)) < 0) {
            perror("accept"); 
            exit(1);
        }

        printf("Connection accepted from %s %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        /*****************************************
         *
         * change to recv only one char here,
         * since tcp doesn't preserve message boundary
         *
         * ***************************************/

        char type;
        ssize_t recvlen;
        if ((recvlen = recv(connectedsock, &type, 1, 0)) < 0) {
            perror("recv"); 
            exit(1);
        }

        cout<<"Main Loop: message type :"<<int(type)<<endl;

        switch(type) {
            case 2:
                addcontent(connectedsock, peers[0]);
            //case 3:
            //case 'a':
        }

        printf("Child %d shutting down...\n", getpid());
        if(shutdown(connectedsock, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }

        break;
    }
    return 0;
}

int createPeerConnection(struct sockaddr_in server) {
    // create and bind a socket
    int sockfd = -1;
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("could not create socket"); 
        exit(1);
    }
    struct sockaddr_in client;
    bzero(&client, sizeof(struct sockaddr_in));
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = htonl(INADDR_ANY);
    client.sin_port = 0; // Let OS choose.
    if(mybind(sockfd, &client) < 0) {
        perror("could not bind socket"); 
        exit(1);
    }
    // fill client with actual socket address
    socklen_t alen = sizeof(struct sockaddr_in);
    if(getsockname(sockfd, (struct sockaddr *)&client, &alen) < 0) {
        perror("getsockname"); 
        exit(1);
    }

    //printf("client associated with %s %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
    // printf("Trying to connect to %s %d...\n", inet_ntoa(server.sin_addr), ntohs(server.sin_port));

    if(connect(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
        printf("Error: no such peer\n"); 
        perror("");
        exit(1);
    }

    return sockfd;
}

void addcontent(int sockfd, Peer &me) {
    cout<<"case 2"<<endl;
    //auto a = std::chrono::system_clock::now();
    //time_t b = std::chrono::system_clock::to_time_t(a);
    hash<string> str_hash;

    string value = recvcontent(sockfd);
    size_t key = str_hash(value);

    cout<<key<<value<<endl;

    me.put(key, value);
}

//void removecontent(int sockfd, Peer &me) {
    //hash<string> str_hash;

    //uint32_t nwlen; //length in big endian
    //if(recv(sockfd, &nwlen, sizeof(uint32_t), 0) != sizeof(uint32_t)) {
        //perror("could not receive length properly");
        //exit(1);
    //}
    //size_t key = ntohl(nwlen);

    //me.del(key);
//}
