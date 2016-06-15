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
#include <fcntl.h>
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

void redistribute(vector<Peer> &peers, bool includeSelf);

int createPeerConnection(struct sockaddr_in server);
Peer initPeerFromAddrPort(const char* addr, const char* port);

void updatePeersInfo(const char* addr, const char* port, vector<Peer> &peers);  // called by new peer to get all the other peers
void handleGetPeers(int sockfd, vector<Peer> &peers);   // requested by a new peer to get peers and update system
void addNewPeer(int sockfd, vector<Peer> &peers);       // requested by peer to add a new peer
void eraseRemovedPeer(int sockfd, vector<Peer> &peers);

void handleGetLoad(int sockfd, Peer& me);
void popRandom(int connectedsock, Peer &me);
void allkeys(int sockfd, Peer& me);

void removePeer(int sockfd, vector<Peer>& peers);
void addcontent(int sockfd, Peer &me, vector<Peer> & peers, bool isTransfer);     // add content naively to self

char killcontent(uint32_t key, Peer &me); // actually removing content
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

        void printKeyValues() {
            cout << this->getAddressPort() << ": Printing Key Values of peer" << endl;
            for (unordered_map<uint32_t, string>::iterator kv = content.begin(); kv != content.end(); ++kv ) {
                cout << kv->first << ": " << kv->second << endl;
            }
        }

        string getKeys() {
            if (load == 0) {
                return "0";
            }

            stringstream ss;
            for (unordered_map<uint32_t, string>::iterator kv = content.begin(); kv != content.end(); ++kv ) {
                ss << kv->first << ",";
            }
            ss << "0";

            return ss.str();
        }

        uint32_t getFirstKey() {
            for (unordered_map<uint32_t, string>::iterator kv = content.begin(); kv != content.end(); ++kv ) {
                return kv->first;
            }
            return -1;   // shouldnt happen
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

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    cout << inet_ntoa(server.sin_addr) << " " << ntohs(server.sin_port) << endl;
    return Peer(server, sockfd);
}

void printPeers(vector<Peer> & peers) {
    cout << peers[0].getAddressPort() << ": Printing peers of peer" << endl;
    for (uint32_t i = 0; i < peers.size(); i++) {
        cout << peers[i].getAddressPort() << endl;
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
        updatePeersInfo(argv[1], argv[2], peers);
        redistribute(peers, true);
    }

    if (fork()) {
        // Parent just prints socket address and returns to user
        return 0;
    }

    // Handle requests
    for (;;) {
        // printPeers(peers);
        // peers[0].printKeyValues();
        // cout << "LISTENING!" << endl;
        if (listen(peers[0].getSockfd(), 0) < 0) {
            perror("listen"); 
            exit(1);
        }
        // cout << "STOP LISTENING" << endl;

        int connectedsock;
        struct sockaddr_in client;
        socklen_t alen = sizeof(struct sockaddr_in);
        // cout << "ACCEPTING" << endl;
        if ((connectedsock = accept(peers[0].getSockfd(), (struct sockaddr *)&client, &alen)) < 0) {
            perror("accept"); 
            exit(1);
        }

        // printf("Connection accepted from %s %d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

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

        // cout<<"Main Loop for: " << peers[0].getAddressPort() << " message type :"<<int(type)<<endl;

        switch(type) {
            case ALL_KEYS:
                allkeys(connectedsock, peers[0]);
                break;
            case MSG_REMOVE:    // redistribute
                removePeer(connectedsock, peers);
                goto REMOVE_PEER;
            case ADD: //smart, redistribute based on flag
                addcontent(connectedsock, peers[0], peers, false);
                break;
            case REMOVE: //smart, redistribute after removal
                removecontent(connectedsock, peers);
                break;
            case REMOVE_F: //dummy, does not redistribute
                removecontent_f(connectedsock, peers[0]); 
                break;
            case LOOKUP:
                lookupcontent(connectedsock, peers);
                break;
            case LOOKUP_F:
                lookupcontent_f(connectedsock, peers[0]);
                break;
            case GET_PEERS:
                handleGetPeers(connectedsock, peers);
                break;
            case GET_LOAD:
                handleGetLoad(connectedsock, peers[0]);
                break;
            case ADD_NEW_PEER:  // adds a peer to the vector
                addNewPeer(connectedsock, peers);
                break;
            case ERASE_REMOVED_PEER:  // removes a peer from the vector
                eraseRemovedPeer(connectedsock, peers);
                break;
            case POP_RANDOM:    // used by redistribution
                popRandom(connectedsock, peers[0]);
                break;
            case RECEIVE_TRANSFER_CONTENT:  // used by redistribution
                addcontent(connectedsock, peers[0], peers, true);
                break;
        }
        if(shutdown(connectedsock, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }
    }

    REMOVE_PEER:

    // printf("Child %d shutting down...\n", getpid());

    if(shutdown(peers[0].getSockfd(), SHUT_RDWR) < 0) {
        perror("attempt at shuting down connection failed"); 
        exit(1);
    }
    if(close(peers[0].getSockfd()) < 0) {
        perror("close(child, sockfd)"); 
    }
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

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
       perror("setsockopt(SO_REUSEADDR) failed");

    fd_set set;
    FD_ZERO(&set);
    FD_SET(sockfd, &set);
    struct timeval tv;
    tv.tv_sec = 4;             /* 4 second timeout */
    tv.tv_usec = 0;

    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    
    // cout << "BEGIN CONNECT" << endl;
    if(connect(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {

        if ( errno != EINPROGRESS ) {
            printf("Error: no such peer\n"); 
            perror("");
            exit(1);
        }
    }
    // cout << "SELECTING" << endl;
    if (select(sockfd+1, NULL, &set, NULL, &tv) == 1) {
        int so_error;
        socklen_t len = sizeof so_error;
        // cout << "ERROR CHECK" << endl;
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len);

        if (so_error == 0) {
            long arg = fcntl(sockfd, F_GETFL, NULL); 
            arg &= (~O_NONBLOCK); 
            fcntl(sockfd, F_SETFL, arg); 
            // cout << "END CONNECT" << endl;
            return sockfd;
        }
    }
    // cout << "ERROR CONENCT< RETRYING" << endl;
    close(sockfd);
    return createPeerConnection(server);
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
// Handling peer creation / removal
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

    if(success == SUCCESS) {
        string peersString = recvcontent(peerfd);

        vector<string> peerStrings;
        splitString(peersString, ' ', peerStrings);
        for (uint32_t i = 0; i < peerStrings.size(); i++) {
            string newPeerString = peerStrings[i];
            vector<string> addrport;
            splitString(newPeerString, ':', addrport);

            const char* addr = addrport[0].c_str();
            const char* port = addrport[1].c_str();

            peers.push_back(initPeerFromAddrPort(addr, port));
        }
    }
    if(shutdown(peerfd, SHUT_RDWR) < 0) {
        perror("attempt at shuting down connection failed"); 
        exit(1);
    }
    if(close(peerfd) < 0) {
        perror("close(peerfd)"); 
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
        if(close(peerfd) < 0) {
            perror("close(peerfd)"); 
        }
        allSuccess = allSuccess && success == SUCCESS;
        ret << peers[i].getAddressPort() << " ";
    }

    vector<string> addrport;
    splitString(newPeer, ':', addrport);
    peers.push_back(initPeerFromAddrPort(addrport[0].c_str(), addrport[1].c_str()));
    ret << peers[0].getAddressPort();

    if(allSuccess) {
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

void eraseRemovedPeer(int sockfd, vector<Peer> &peers) {
    string removedPeer = recvcontent(sockfd);
    uint32_t removeIndex = -1;

    for (uint32_t i = 1; i < peers.size(); i++) {
        if (peers[i].getAddressPort().compare(removedPeer) == 0) {
            removeIndex = i;
            break;
        }
    }

    if (removeIndex > 0) {
        peers.erase(peers.begin() + removeIndex);
    }

    char success = SUCCESS;
    if(send(sockfd, &success, 1, 0) < 0) {
        perror("could not send client remove content confirmation"); 
        exit(1);
    }
}

// =================================================================================================
// Redistribution & Synchronizing
// =================================================================================================

string encodeKeyValue(uint32_t key, const string& value) {
    stringstream res;
    res << key << "," << value;
    return res.str();
}

void decodeKeyValue(const string& encoded, uint32_t& key, string& value) {
    unsigned long pos = encoded.find(",");
    key = stoul(encoded.substr(0, pos));
    value = encoded.substr(pos + 1, encoded.length());
}

void popRandom(int sockfd, Peer & me) {    
    uint32_t key = me.getFirstKey();
    string keyvalue = encodeKeyValue(key, me.get(key));
    me.del(key);
    sendcontent(sockfd, keyvalue.c_str());
}

void transferContent(Peer &from, Peer &to, Peer & me) {
    string contentKeyValue;
    if (&me != &from) {
        char type = POP_RANDOM;    
        int fromfd = createPeerConnection(from.getSocketAddr());

        if( send(fromfd, &type, 1, 0) < 0 ) {
            perror("could not send send transferContent request from peer to peer");
            exit(1);
        }

        contentKeyValue = recvcontent(fromfd);

        if(shutdown(fromfd, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }
        if(close(fromfd) < 0) {
            perror("close(peerfd)"); 
        }
    } else {
        uint32_t key = me.getFirstKey();
        contentKeyValue = encodeKeyValue(key, me.get(key));
        me.del(key);
    }

    if (&me != &to) {
        char type = RECEIVE_TRANSFER_CONTENT;
        int tofd = createPeerConnection(to.getSocketAddr());

        if( send(tofd, &type, 1, 0) < 0 ) {
            perror("could not send send transfercontent request from peer to peer");
            exit(1);
        }

        sendcontent(tofd, contentKeyValue.c_str());

        char success = SUCCESS;
        if( recv(tofd, &success, 1, 0) < 0 ) {
            perror("could not get transfer content confirmation"); 
            exit(1);
        }

        if(shutdown(tofd, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }
        if(close(tofd) < 0) {
            perror("close(peerfd)"); 
        }
    } else {
        uint32_t key;
        string value;
        decodeKeyValue(contentKeyValue, key, value);
        me.put(key, value);
    }
}

void redistribute(vector<Peer> &peers, bool includeSelf) {
    // cout << "REDISTRIBUTING" << endl;
    // Total load
    uint32_t sum = 0;
    uint32_t sizes [peers.size()];

    sizes[0] = peers[0].getLoad();

    for(size_t i = 1; i< peers.size(); i++) {
        // cout << "GETTING LOAD" << endl;
        char type = GET_LOAD;
        int peerfd = createPeerConnection(peers[i].getSocketAddr());

        if( send(peerfd, &type, 1, 0) < 0 ) {
            perror("could not send send getLoad request from peer to peer");
            exit(1);
        }

        uint32_t load;
        if( recv(peerfd, &load, sizeof(load), 0) < 0 ) {
            perror("could not get getLoad confirmation"); 
            exit(1);
        }
        load = ntohl(load);

        if(shutdown(peerfd, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }
        if(close(peerfd) < 0) {
            perror("close(peerfd)"); 
        }

        sizes[i] = load;
        sum += load;
    }

    uint32_t startingPeer = includeSelf ? 0 : 1;
    while (true) {
        int maxPeer = startingPeer, minPeer = startingPeer;
        for (uint32_t i = 1; i < peers.size(); i++) {
            if (sizes[i] > sizes[maxPeer]) maxPeer = i;
            else if (sizes[i] < sizes[minPeer]) minPeer = i;
        }
        if (sizes[maxPeer] - sizes[minPeer] <= 1) {
            break;
        }
        transferContent(peers[maxPeer], peers[minPeer], peers[0]);
        sizes[maxPeer] -= 1;
        sizes[minPeer] += 1;
    }
}

void handleGetLoad(int sockfd, Peer& me) {
    uint32_t size = htonl(me.getLoad());

    if( send(sockfd, &size, sizeof(size), 0) < 0 ) {
        perror("could not send add content success");
        exit(1);
    }
}

// =================================================================================================
// Other stuff 
// =================================================================================================

void allkeys(int sockfd, Peer& me) {
    string ret = me.getKeys();
    if( send(sockfd, ret.c_str(), strlen(ret.c_str()), 0) < 0 ) {
        perror("could not send all keys success");
        exit(1);
    }
}

void removePeer(int sockfd, vector<Peer>& peers) {
    // cout << "Removing peer " << endl;
    for(uint32_t i=1; i<peers.size(); i++) {
        // cout << "Telling peer " << i << " to remove me from their vector" << endl;
        char type = ERASE_REMOVED_PEER;
        int peerfd = createPeerConnection(peers[i].getSocketAddr());

        if(send(peerfd, &type, 1, 0) < 0 ) {
            perror("could not send eraseRemovedPeer request from peer to peer");
            exit(1);
        }
        // cout << "SENT TYPE" << endl;
        sendcontent(peerfd, peers[0].getAddressPort().c_str());
        // cout << "SENT CONTENT" << endl;
        char success;
        if( recv(peerfd, &success, 1, 0) < 0 ) {
            perror("could not get peer force delete confirmation"); 
            exit(1);
        }

        if(shutdown(peerfd, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }
        if(close(peerfd) < 0) {
            perror("close(peerfd)"); 
        }
    }

    while (peers[0].getLoad() > 0 && peers.size() > 1) {
        // cout << "Sending all my content to my neighbour peer. Left: " << peers[0].getLoad() << endl;
        int peerfd = createPeerConnection(peers[1].getSocketAddr());
        char type = RECEIVE_TRANSFER_CONTENT;

        if(send(peerfd, &type, 1, 0) < 0 ) {
            perror("could not send transferContent request from peer to peer");
            exit(1);
        }

        popRandom(peerfd, peers[0]);        // sends a random key from me to peerfd

        char success;
        if( recv(peerfd, &success, 1, 0) < 0 ) {
            perror("could not get transferContent confirmation"); 
            exit(1);
        }

        if(shutdown(peerfd, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }
        if(close(peerfd) < 0) {
            perror("close(peerfd)"); 
        }
    }

    if (peers.size() > 1) {
        redistribute(peers, false);
    }

    char success = 'y';
    // cout << "Notifying removepeer success" << endl;
    if (send(sockfd, &success, 1, 0) < 0) {
        perror("could not send remove peer confirmation"); 
        exit(1);
    }
    // cout << "DONE" << endl;
    return;
}

void addcontent(int sockfd, Peer &me, vector<Peer> & peers, bool isTransfer) {
    uint32_t key;
    string value;

    if (isTransfer) {
        string keyvalue = recvcontent(sockfd);
        decodeKeyValue(keyvalue, key, value);
    } else {
        hash<string> str_hash;
        value = recvcontent(sockfd);
        key = str_hash(value) * uint32_t(rand());
    }

    me.put(key, value);

    if (!isTransfer) {
        redistribute(peers, true);
        uint32_t nkey = htonl(key);
        if(send(sockfd, &nkey, sizeof(nkey), 0) != sizeof(nkey)) {
            perror("could not send add content key back");
            exit(1);
        }
    } else {
        char success = SUCCESS;
        if(send(sockfd, &success, 1, 0) < 0) {
            perror("could not send transfer content confirmation"); 
            exit(1);
        }
    }
}

char killcontent(uint32_t key, Peer &me) {
    char success = FAILURE;
    if(me.has(key)) {
        success = SUCCESS;
        me.del(key);
        // cout<<"content killed"<<endl;
    }
    return success;
}

void removecontent_f(int sockfd, Peer &me) {
    uint32_t nkey;
    if(recv(sockfd, &nkey, sizeof(nkey), 0) < 0) {
        perror("could not receive key"); 
        exit(1);
    }
    uint32_t key = ntohl(nkey);
    char success = killcontent(key, me);
    if( send(sockfd, &success, 1, 0) < 0 ) {
        perror("could not send force remove content confirmation"); 
        exit(1);
    }
}

void removecontent(int sockfd, vector<Peer> &peers) {
    uint32_t nkey;
    if(recv(sockfd, &nkey, sizeof(nkey), 0) < 0) {
        perror("could not receive key"); 
        exit(1);
    }
    uint32_t key = ntohl(nkey);
    if(peers[0].has(key)) {
        char success = killcontent(key, peers[0]);
        redistribute(peers, true);
        if( send(sockfd, &success, 1, 0) < 0 ) {
            perror("could not send force remove content confirmation"); 
            exit(1);
        }
        return;
    }

    for(uint32_t i=1; i<peers.size(); i++) {

        char type = REMOVE_F;
        int peerfd = createPeerConnection(peers[i].getSocketAddr());
        if( send(peerfd, &type, 1, 0) < 0 ) {
            perror("could not send kill content request from peer to peer");
            exit(1);
        }

        if(send(peerfd, &nkey, sizeof(nkey), 0) != sizeof(nkey)) {
            perror("could not send add content key back");
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
        if(close(peerfd) < 0) {
            perror("close(child, sockfd)"); 
        }

        if(success == SUCCESS) {
            redistribute(peers, true);
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

    for(uint32_t i=1; i<peers.size(); i++)  {
        // cout << "looking at peer for content " << peers[i].getAddressPort() << endl;
        char type = LOOKUP_F;
        int peerfd = createPeerConnection(peers[i].getSocketAddr());
        if( send(peerfd, &type, 1, 0) < 0 ) {
            perror("could not send lookup content request from peer to peer");
            exit(1);
        }
        // cout << "SENT type" << endl;

        if(send(peerfd, &nkey, sizeof(nkey), 0) != sizeof(nkey)) {
            perror("could not send add content key back");
            exit(1);
        }
        // cout << "SENT KEY" << endl;

        char success;
        if( recv(peerfd, &success, 1, 0) < 0 ) {
            perror("could not get peer lookup content confirmation"); 
            exit(1);
        }

        if(success == SUCCESS) {
            // cout << "found content " << endl;
            if(send(sockfd, &success, 1, 0) < 0) {
                perror("could not send client lookup content confirmation"); 
                exit(1);
            }
            string content = recvcontent(peerfd);
            if(shutdown(peerfd, SHUT_RDWR) < 0) {
                perror("attempt at shuting down connection failed"); 
                exit(1);
            }
            if(close(peerfd) < 0) {
                perror("close(peerfd)"); 
            }
            sendcontent(sockfd, content.c_str());
            return;
        }

        if(shutdown(peerfd, SHUT_RDWR) < 0) {
            perror("attempt at shuting down connection failed"); 
            exit(1);
        }
        if(close(peerfd) < 0) {
            perror("close(peerfd)"); 
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
