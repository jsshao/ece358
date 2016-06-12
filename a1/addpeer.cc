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
#include <sstream>
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
Peer initPeerFromAddrPort(const char* addr, const char* port);

void updatePeersInfo(const char* addr, const char* port, vector<Peer> &peers);  // called by new peer to get all the other peers
void handleGetPeers(int sockfd, vector<Peer> &peers);   // requested by a new peer to get peers and update system
void addNewPeer(int sockfd, vector<Peer> &peers);       // requested by peer to add a new peer


void removePeer(int sockfd);
void addcontent(int sockfd, Peer &me);              // add content naively to self

void killcontent(int sockfd, uint32_t key, Peer &me); // actually removing content
void removecontent(int sockfd, vector<Peer> &peers);// remove content on request by a client
void removecontent_f(int sockfd, Peer &me);         // remove content on request by a peer

void checkoutcontent(int sockfd, uint32_t key, Peer &me); // actually looking up content
void lookupcontent(int sockfd, vector<Peer> &peers);// lookup content on request by a client
void lookupcontent_f(int sockfd, Peer &me);         // remove content on request by a peer


class Peer {
    private:
        sockaddr_in socket_addr;
        int sockfd;
        int load;
        unordered_map<uint32_t, string> content; 

    public:
        Peer(sockaddr_in socket_addr, int sockfd) {
            this->socket_addr = socket_addr;
            this->sockfd = sockfd;
            this->load = 0;
        }

        Peer(sockaddr_in socket_addr) {
            this->socket_addr = socket_addr;
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

        string get(uint32_t key) {
            return content[key];
        }

        void put(uint32_t key, string value) {
            content[key] = value;
            load++;
        }

        void del(uint32_t key) {
            content.erase(key);
            load--;
        }

        bool has(uint32_t key) {
            return (content.count(key) > 0); 
        }

        string getAddressPort() {
            stringstream ss;
            ss << inet_ntoa(this->socket_addr.sin_addr) << ":" << ntohs(this->socket_addr.sin_port);
            return ss.str();
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

void printPeers(vector<Peer> & peers) {
    for (Peer &p : peers) {
        cout << p.getAddressPort() << endl;
    }
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
        cout << "trying to join p2p" << endl;
        updatePeersInfo(argv[1], argv[2], peers);
        cout << "done joining p2p" << endl;
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
            case GET_PEERS:
                cout << "Notifying all peers ..." << peers.size() << endl;
                handleGetPeers(connectedsock, peers);
                cout << "Done notifying all peers. peers: " << peers.size() << endl;
                break;
            case GET_LOAD:
                break;
            case ADD_NEW_PEER:
                addNewPeer(connectedsock, peers);
                cout << "adding peer " << peers[peers.size() - 1].getAddressPort() << "..." << endl;
                break;
        }
        printPeers(peers);
    }

    REMOVE_PEER:

    //printf("Child %d shutting down...\n", getpid());
    //if(shutdown(connectedsock, SHUT_RDWR) < 0) {
        //perror("attempt at shuting down connection failed"); 
        //exit(1);
    //}
    return 0;
}

// =================================================================================================
// Helpers 
// =================================================================================================

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

Peer initPeerFromAddrPort(const char* addr, const char* port) {
    struct sockaddr_in server;
    bzero(&server, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    if(!inet_aton(addr, &(server.sin_addr))) {
        perror("invalid server-ip"); 
        exit(1);
    }
    server.sin_port = htons(atoi(port));
    return Peer(server);
}

void splitString(const string& s, char delim, vector<string>& ret) {
    int i = 0;
    unsigned long pos = s.find(delim);
    while (pos != string::npos) {
        ret.push_back(s.substr(i, pos-i));
        i = ++pos;
        pos = s.find(delim, pos);
    }
    if (pos == string::npos)
        ret.push_back(s.substr(i, s.length()));
}

// =================================================================================================
// Handling new peer creation  
// =================================================================================================

void updatePeersInfo(const char* addr, const char* port, vector<Peer> &peers) {
    // called by a new peer trying to join a p2p network which contains addr:port
    // will make a request to addr:port which returns a list of all existing peers (and make them add the new peer)

    Peer peer = initPeerFromAddrPort(addr, port);
    int peerfd = createPeerConnection(peer.getSocketAddr());

    char type = GET_PEERS;

    if( send(peerfd, &type, 1, 0) < 0 ) {
        perror("could not send getPeerInfo request from peer to peer");
        exit(1);
    }

    sendcontent(peerfd, peers[0].getAddressPort().c_str());

    char success;
    if( recv(peerfd, &success, 1, 0) < 0 ) {
        perror("could not get getPeerInfo confirmation"); 
        exit(1);
    }

    if(shutdown(peerfd, SHUT_RDWR) < 0) {
        perror("attempt at shuting down connection failed"); 
        exit(1);
    }

    if(success == SUCCESS) {
        string peersString = recvcontent(peerfd);
        cout << peersString << endl;

        vector<string> peerStrings;
        splitString(peersString, ' ', peerStrings);
        for (string newPeerString: peerStrings) {
            vector<string> addrport;
            splitString(newPeerString, ':', addrport);

            const char* addr = addrport[0].c_str();
            const char* port = addrport[1].c_str();

            peers.push_back(initPeerFromAddrPort(addr, port));
        }
    }
}

void handleGetPeers(int sockfd, vector<Peer> &peers) {
    // handles when a new peer joins the network and requests to get a list of all peers
    // does 2 things:
    // 1) returns a list of all peers in the network delimited by ' '
    // 2) sends a request to each existing peer to add this new peer to their vector
    string newPeer = recvcontent(sockfd);
    stringstream ret;

    bool allSuccess = true;
    for(uint32_t i=1; i<peers.size(); i++) {
        char type = ADD_NEW_PEER;
        int peerfd = createPeerConnection(peers[i].getSocketAddr());
        if( send(peerfd, &type, 1, 0) < 0 ) {
            perror("could not send addNewPeer request from peer to peer");
            exit(1);
        }

        sendcontent(peerfd, newPeer.c_str());

        char success;
        if( recv(peerfd, &success, 1, 0) < 0 ) {
            perror("could not get peer addNewPeer confirmation"); 
            exit(1);
        }

        if(shutdown(peerfd, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }
        allSuccess = allSuccess && success == SUCCESS;
        ret << peers[i].getAddressPort() << " ";
    }

    cout << "adding peer..." << newPeer << endl;
    vector<string> addrport;
    splitString(newPeer, ':', addrport);
    peers.push_back(initPeerFromAddrPort(addrport[0].c_str(), addrport[1].c_str()));
    ret << peers[0].getAddressPort();

    if(allSuccess) {
        cout << "all peers added new peer successfully!" << endl;
        char success = SUCCESS;
        if(send(sockfd, &success, 1, 0) < 0) {
            perror("could not send client handleGetPeers confirmation"); 
            exit(1);
        }

        sendcontent(sockfd, ret.str().c_str());
    }
}

void addNewPeer(int sockfd, vector<Peer> &peers) {
    // add new peer to the peer array 
    string newPeer = recvcontent(sockfd);
    cout << "adding new peer..." << newPeer << endl;
    vector<string> addrport;
    splitString(newPeer, ':', addrport);

    const char* addr = addrport[0].c_str();
    const char* port = addrport[1].c_str();

    peers.push_back(initPeerFromAddrPort(addr, port));

    char success = SUCCESS;
    if(send(sockfd, &success, 1, 0) < 0) {
        perror("could not send client remove content confirmation"); 
        exit(1);
    }
}


// =================================================================================================
// Other stuff 
// =================================================================================================

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
    uint32_t key = str_hash(value) * uint32_t(rand());

    me.put(key, value);

    uint32_t nkey = htonl(key);
    if(send(sockfd, &nkey, sizeof(nkey), 0) != sizeof(nkey)) {
        perror("could not send add content key back");
        exit(1);
    }
}

void killcontent(int sockfd, uint32_t key, Peer &me) {
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
    uint32_t nkey;
    if(recv(sockfd, &nkey, sizeof(nkey), 0) < 0) {
        perror("could not receive key"); 
        exit(1);
    }
    uint32_t key = ntohl(nkey);
    killcontent(sockfd, key, me);
}

void removecontent(int sockfd, vector<Peer> &peers) {
    uint32_t nkey;
    if(recv(sockfd, &nkey, sizeof(nkey), 0) < 0) {
        perror("could not receive key"); 
        exit(1);
    }
    uint32_t key = ntohl(nkey);
    if(peers[0].has(key)) {
        killcontent(sockfd, key, peers[0]);
        return;
    }

    for(uint32_t i=1; i<peers.size(); i++) {

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

void checkoutcontent(int sockfd, uint32_t key, Peer &me) {
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
    uint32_t nkey;
    if(recv(sockfd, &nkey, sizeof(nkey), 0) < 0) {
        perror("could not receive key"); 
        exit(1);
    }
    uint32_t key = ntohl(nkey);
    if(peers[0].has(key)) {
        checkoutcontent(sockfd, key, peers[0]);
        return;
    }

    for(uint32_t i=1; i<peers.size(); i++) {

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
    uint32_t nkey;
    if(recv(sockfd, &nkey, sizeof(nkey), 0) < 0) {
        perror("could not receive key"); 
        exit(1);
    }
    uint32_t key = ntohl(nkey);
    checkoutcontent(sockfd, key, me);
}

void redistribute(vector<Peer> &peers) {
    // Total load
    int sum = 0;
    int sizes [peers.size()];

    sizes[0] = peers[0].getLoad();
    for (uint32_t i = 1; i < peers.size(); i++) {
        
    }
}
