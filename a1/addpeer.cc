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
#include <time.h>
#include <functional>
#include "constants.h"
using namespace std;

#define MSG_REMOVE 49

class Peer;


extern int pickServerIPAddr(struct in_addr *srv_ip);
extern int mybind(int sockfd, struct sockaddr_in *addr);
extern string recvcontent(int sockfd);
extern void sendcontent(int sockfd, const char* buf);

void redistribute(vector<Peer> &peers);

int createPeerConnection(struct sockaddr_in server);
void removePeer(int sockfd);
void addcontent(int sockfd, Peer &me);              // add content naively to self

void killcontent(int sockfd, size_t key, Peer &me); // actually removing content
void removecontent(int sockfd, vector<Peer> &peers);// remove content on request by a client
void removecontent_f(int sockfd, Peer &me);         // remove content on request by a peer

void checkoutcontent(int sockfd, size_t key, Peer &me); // actually looking up content
void lookupcontent(int sockfd, vector<Peer> &peers);// lookup content on request by a client
void lookupcontent_f(int sockfd, Peer &me);         // remove content on request by a peer

class Peer {
    private:
        sockaddr_in socket_addr;
        int sockfd;
        int load;
        unordered_map<size_t, string> content; 

    public:
        Peer(sockaddr_in socket_addr, int sockfd) {
            this->socket_addr = socket_addr;
            this->sockfd = sockfd;
            this->load = 0;
        }

        sockaddr_in getSocketAddr() {
            return socket_addr;
        }

        int getSockfd() {
            return sockfd;
        }

        int getLoad() {
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

        bool has(size_t key) {
            return (content.count(key) > 0); 
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

    srand(time(NULL));

    // Initialize current peer
    vector<Peer> peers;
    peers.push_back(initialize());

    // Update existing peers
    if (argc == 3) {
        // TBD
        cout << argv[2] << argv[1] << endl;
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
        if (recv(connectedsock, &type, 1, 0) < 0) {
            perror("recv"); 
            exit(1);
        }

        cout<<"Main Loop: message type :"<<int(type)<<endl;

        switch(type) {
            case MSG_REMOVE:
                removePeer(connectedsock);
                goto REMOVE_PEER;
            case ADD: //smart, redistribute
                addcontent(connectedsock, peers[0]);
                redistribute(peers);
                break;
            case REMOVE: //smart, redistribute
                removecontent(connectedsock, peers);
                redistribute(peers);
                break;
            case REMOVE_F: //dummy
                removecontent_f(connectedsock, peers[0]); 
                break;
            case LOOKUP: //smart redistribute
                lookupcontent(connectedsock, peers);
                redistribute(peers);
                break;
            case LOOKUP_F:
                lookupcontent_f(connectedsock, peers[0]);
                break;
        }
    }

    REMOVE_PEER:

    //printf("Child %d shutting down...\n", getpid());
    //if(shutdown(connectedsock, SHUT_RDWR) < 0) {
        //perror("attempt at shuting down connection failed"); 
        //exit(1);
    //}
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

void removePeer(int sockfd) {
    char success = 'y';
    // NEED TO REDISTRIBUTE HERE
    //
    if (send(sockfd, &success, 1, 0) < 0) {
        perror("could not send remove peer confirmation"); 
        exit(1);
    }
    return;
}

void addcontent(int sockfd, Peer &me) {

    hash<string> str_hash;

    string value = recvcontent(sockfd);
    size_t key = str_hash(value) * size_t(rand());

    me.put(key, value);

    if( send(sockfd, &key, sizeof(key), 0) < 0 ) {
        perror("could not send add content success");
        exit(1);
    }
}

void killcontent(int sockfd, size_t key, Peer &me) {
    char success = FAILURE;
    if(me.has(key)) {
        success = SUCCESS;
        me.del(key);
        cout<<"content killed"<<endl;
    }
    if( send(sockfd, &success, 1, 0) < 0 ) {
        perror("could not send force remove content confirmation"); 
        exit(1);
    }
}

void removecontent_f(int sockfd, Peer &me) {
    size_t key;
    if(recv(sockfd, &key, sizeof(key), 0) < 0) {
        perror("could not receive key"); 
        exit(1);
    }
    killcontent(sockfd, key, me);
}

void removecontent(int sockfd, vector<Peer> &peers) {
    size_t key;
    if(recv(sockfd, &key, sizeof(key), 0) < 0) {
        perror("could not receive key"); 
        exit(1);
    }
    if(peers[0].has(key)) {
        killcontent(sockfd, key, peers[0]);
        return;
    }

    for(size_t i=1; i<peers.size(); i++) {

        char type = REMOVE_F;
        int peerfd = createPeerConnection(peers[i].getSocketAddr());
        if( send(peerfd, &type, 1, 0) < 0 ) {
            perror("could not send kill content request from peer to peer");
            exit(1);
        }

        char success;
        if( recv(peerfd, &success, 1, 0) < 0 ) {
            perror("could not get peer force delete confirmation"); 
            exit(1);
        }

        if(shutdown(peerfd, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }

        if(success == SUCCESS) {
            if(send(sockfd, &success, 1, 0) < 0) {
                perror("could not send client remove content confirmation"); 
                exit(1);
            }
            return;
        }
    }

    char success = FAILURE;
    if(send(sockfd, &success, 1, 0) < 0) {
        perror("could not send client remove content confirmation"); 
        exit(1);
    }
}

void checkoutcontent(int sockfd, size_t key, Peer &me) {
    char success = FAILURE;
    if(me.has(key)) {
        success = SUCCESS;
    }
    if( send(sockfd, &success, 1, 0) < 0 ) {
        perror("could not send force lookup content confirmation"); 
        exit(1);
    }
    if(success == SUCCESS){
        sendcontent(sockfd, me.get(key).c_str());
    }
}
void lookupcontent(int sockfd, vector<Peer> &peers) {
    size_t key;
    if(recv(sockfd, &key, sizeof(key), 0) < 0) {
        perror("could not receive key"); 
        exit(1);
    }
    if(peers[0].has(key)) {
        checkoutcontent(sockfd, key, peers[0]);
        return;
    }

    for(size_t i=1; i<peers.size(); i++) {

        char type = LOOKUP_F;
        int peerfd = createPeerConnection(peers[i].getSocketAddr());
        if( send(peerfd, &type, 1, 0) < 0 ) {
            perror("could not send lookup content request from peer to peer");
            exit(1);
        }

        char success;
        if( recv(peerfd, &success, 1, 0) < 0 ) {
            perror("could not get peer lookup content confirmation"); 
            exit(1);
        }

        if(shutdown(peerfd, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }

        if(success == SUCCESS) {
            if(send(sockfd, &success, 1, 0) < 0) {
                perror("could not send client lookup content confirmation"); 
                exit(1);
            }
            string content = recvcontent(peerfd);
            sendcontent(sockfd, content.c_str());
            return;
        }
    }

    char success = FAILURE;
    if(send(sockfd, &success, 1, 0) < 0) {
        perror("could not send client remove content confirmation"); 
        exit(1);
    }
}
void lookupcontent_f(int sockfd, Peer &me) {
    size_t key;
    if(recv(sockfd, &key, sizeof(key), 0) < 0) {
        perror("could not receive key"); 
        exit(1);
    }
    checkoutcontent(sockfd, key, me);
}

void redistribute(vector<Peer> &peers) {
    // Total load
    int sum = 0;
    for (size_t i = 0; i < peers.size(); i++) {
        sum += peers[i].getLoad();
    }
}
