// style guide: Geotechnical Software Services
// Version 4.9, January 2011
// http://geosoft.no/development/cppstyle.html

#include <ctime>
#include <cstring>
#include <string>
#include "rcs.h"
#include "ucp.h"
using namespace std;

#define MAXSOCKETS 5000
#define SPAM_COUNT 5
#define UCP_TIMEOUT 1

#define BUFLEN 500
#define BYTES_CHECKSUM 4
#define BYTES_SEQ 4
#define BYTES_TYPE 1
#define MAX_MESSAGE_SIZE BUFLEN - BYTES_CHECKSUM - 2*BYTES_SEQ - BYTES_TYPE - 1


#define MESSAGE 0
#define ACK 1
#define FIRST 2
#define SECOND 3
#define THIRD 4
#define KILL 5


struct RcsSocket {
    bool inUse;
    bool bound;
    bool connected;
    int32_t pktSeq; //sequence number of next packet to send
    int32_t ackSeq; //sequence number of next packet expecting to receive
    int32_t ucpSocket;
    struct sockaddr_in local;
    struct sockaddr_in remote;

    string recvMsg;

    RcsSocket() {
        reset();
    }
    void reset() {
        inUse = false;
        bound = false;
        connected = false;
        pktSeq = 0;
        ackSeq = 0;
        recvMsg = "";
    }
};

struct RcsSocket rcsSockets[MAXSOCKETS];


// create a 32 bit checksum from string
int32_t computeCheckSum(const char* buf, int32_t len) {
   int32_t hash = 0;
   int32_t i = 0;
   for(; i < len; i++) {
       hash += buf[i];
       hash += (hash << 10);
       hash ^= (hash >> 6);
   }
   hash += (hash << 3);
   hash ^= (hash >> 11);
   hash += (hash << 15);
   return hash;
}

// check if a packet is corrupt by checking against checksum
bool isCorrupt(string pkt) {
    if(pkt.length() < BYTES_CHECKSUM)
        return true;
    int32_t checksum = computeCheckSum(pkt.c_str()+BYTES_CHECKSUM, pkt.length()-BYTES_CHECKSUM);
    int32_t sentChecksum;
    memcpy(&sentChecksum, pkt.c_str(), BYTES_CHECKSUM);
    return checksum  != sentChecksum;
}

// create and return a checksumed string message
// PACKET IS IN THE FORM [ CHECK_SUM | IS_ACK | PKT SEQ | ACK SEQ | DATA ]
string makePkt(int32_t pktSeq, int32_t ackSeq, char* buf, int32_t len, int32_t type = MESSAGE) {
    int32_t totalLen = BYTES_CHECKSUM + 2*BYTES_SEQ + BYTES_TYPE + len;
    char pktBuffer[totalLen];
    
    memset(pktBuffer, 0, totalLen);         // initialize packet to all 0s
    memcpy(&pktBuffer[BYTES_CHECKSUM + 2*BYTES_SEQ + BYTES_TYPE], buf, len);      // set the data
    memcpy(&pktBuffer[BYTES_CHECKSUM + BYTES_SEQ + BYTES_TYPE], (char*) &ackSeq, BYTES_SEQ);      // set the data
    memcpy(&pktBuffer[BYTES_CHECKSUM + BYTES_TYPE], (char*) &pktSeq, BYTES_SEQ);        // set the sequence #
    memcpy(&pktBuffer[BYTES_CHECKSUM], (char*) &type, BYTES_TYPE);              // set the acknowledgement byte
    
    int32_t checksum = computeCheckSum(pktBuffer, totalLen);                    // compute checksum
    memcpy(pktBuffer, (char*) &checksum, BYTES_CHECKSUM);                   // set checksum
    return string(pktBuffer, totalLen);
}

string makeTypePkt(int32_t type) {
    char empty[0];
    return makePkt(-1, -1, empty, 0, type);
}

string makeAckPkt(int32_t ackSeq) {
    char empty[0];
    return makePkt(-1, ackSeq, empty, 0, ACK);
}

string fetchPkt(int32_t sockfd, struct sockaddr_in desiredRemote, bool checkAddress = true) {
    struct sockaddr_in addr;
    char buf[BUFLEN];
    int32_t len = ucpRecvFrom(sockfd, buf, BUFLEN-1, &addr);

    // len < 0 handles timeouts. just return empty string
    if(len <= 0 )
        return "";

    if( checkAddress && (addr.sin_addr.s_addr != desiredRemote.sin_addr.s_addr 
             || addr.sin_port != desiredRemote.sin_port))
        return "";

    string pkt(buf, len);
    return pkt;
}

int getType(string pkt) {
    int32_t type = (int32_t)pkt.c_str()[BYTES_CHECKSUM];
    return type;
}

int32_t getPktSeq(string pkt) {
    int32_t pktSeq;
    memcpy((char*)&pktSeq, &pkt.c_str()[BYTES_CHECKSUM + BYTES_TYPE], BYTES_SEQ);
    return pktSeq;
}

int32_t getAckSeq(string pkt) {
    int32_t ackSeq;
    memcpy((char*)&ackSeq, &pkt.c_str()[BYTES_CHECKSUM + BYTES_TYPE + BYTES_SEQ], BYTES_SEQ);
    return ackSeq;

}

string getMessage(string pkt) {
    return pkt.substr(BYTES_CHECKSUM + BYTES_TYPE + 2*BYTES_SEQ);
}




// return a new rcs socket
int32_t rcsSocket() {
    for(int32_t i=0; i<MAXSOCKETS; i++) {
        RcsSocket &rs = rcsSockets[i];
        if(!rs.inUse) {
            int32_t sockfd = ucpSocket();
            if(sockfd < 0)
                return -1;

            rs.ucpSocket = sockfd;
            rs.inUse = true;
            return i;
        }
    }

    errno = ENFILE;
    return -1;
}

// bind rcs socket to address
int32_t rcsBind(int32_t sockfd, struct sockaddr_in *addr) {
    if( !(0 <= sockfd && sockfd < MAXSOCKETS && rcsSockets[sockfd].inUse) ) {
        errno = EBADF;
        return -1;
    }

    RcsSocket &rs = rcsSockets[sockfd];
    int ret =  ucpBind(rs.ucpSocket, addr);
    if(ret == 0)
        rs.bound = true;

    return ret;
}

// store socket's binded address into addr structure
int32_t rcsGetSockName(int32_t sockfd, struct sockaddr_in *addr) {
    if( !(0 <= sockfd && sockfd < MAXSOCKETS && rcsSockets[sockfd].inUse) ) {
        errno = EBADF;
        return -1;
    }

    RcsSocket &rs = rcsSockets[sockfd];
    int ret = ucpGetSockName(rs.ucpSocket, addr);
    if(ret == 0) {
        rs.local = *addr;
    }

    return ret;
}

int32_t rcsListen(int32_t sockfd) {
    if( !(0 <= sockfd && sockfd < MAXSOCKETS && rcsSockets[sockfd].inUse) ) {
        errno = EBADF;
        return -1;
    }

    RcsSocket &rs = rcsSockets[sockfd];
    if(!rs.bound) {
        errno = EDESTADDRREQ; 
        return -1;
    }
    if(rs.connected) {
        errno = EINVAL; 
        return -1;
    }

    return ucpSetSockRecvTimeout(rs.ucpSocket, UCP_TIMEOUT);
}

int32_t rcsConnect(int32_t sockfd, const struct sockaddr_in *addr) {
    if( !(0 <= sockfd && sockfd < MAXSOCKETS && rcsSockets[sockfd].inUse) ) {
        errno = EBADF;
        return -1;
    }

    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.connected) {
        errno = EISCONN;
        return -1;
    }
    if ( ucpSetSockRecvTimeout(rs.ucpSocket, UCP_TIMEOUT) != 0 ) {
        return -1;
    }

    string first = makeTypePkt(FIRST);
    string second;
    string third = makeTypePkt(THIRD);

    time_t start,end;
    time(&start);
    do {
        ucpSendTo(rs.ucpSocket, first.c_str(), first.length(), addr);
        char buf[BUFLEN];
        int32_t len = ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, &(rs.remote));

        // if no valid return message, check for timeout
        if(len <= 0) {
            time(&end);
            if(end-start > 6) {
                errno = ECONNREFUSED; 
                return -1;
            }
            continue;
        }
        second = string(buf, len);

    } while(isCorrupt(second) || getType(second) != SECOND);

    for(int32_t i=0; i<SPAM_COUNT; i++)
        ucpSendTo(rs.ucpSocket, third.c_str(), third.length(), &(rs.remote));

    rs.connected = true;
    return 0;
}

int32_t rcsAccept(int32_t sockfd, struct sockaddr_in *addr) {
    if( !(0 <= sockfd && sockfd < MAXSOCKETS && rcsSockets[sockfd].inUse) ) {
        errno = EBADF;
        return -1;
    }

    string first;
    string second = makeTypePkt(SECOND);
    string third;
    RcsSocket &rs = rcsSockets[sockfd];
    for(int32_t i=0; i<MAXSOCKETS; i++) {
        RcsSocket &newRs = rcsSockets[i];
        if(!newRs.inUse) {
            int socket = ucpSocket();
            if( socket < 0 ) {
                return socket; 
            }

            newRs.inUse = true;
            newRs.bound = true;
            newRs.connected = true;
            newRs.ucpSocket = socket;
            newRs.local = rs.local;
            newRs.local.sin_port = 0;

            if( ucpBind(newRs.ucpSocket, &(newRs.local)) != 0 ||
              ucpGetSockName(newRs.ucpSocket, &(newRs.local)) != 0 ||
              ucpSetSockRecvTimeout(newRs.ucpSocket, UCP_TIMEOUT) != 0 ) {
                newRs.reset();
                return -1;
            }

            do {
                char buf[BUFLEN];
                int32_t len = ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, addr);
                if(len <= 0)
                    continue;
                first = string(buf, len);
            } while ( isCorrupt(first) || getType(first) != FIRST );

            newRs.remote = *addr;
            do {
                ucpSendTo(newRs.ucpSocket, second.c_str(), second.length(), &(newRs.remote));
                third = fetchPkt(newRs.ucpSocket, newRs.remote);
                if(third == "")
                    continue;
            } while ( isCorrupt(third) || getType(third) != THIRD );

            return i;
        }
    }
    errno = ENFILE;
    return -1;
}


int32_t rcsSend(int32_t sockfd, void *buf, int32_t len) {
    if( !(0 <= sockfd && sockfd < MAXSOCKETS && rcsSockets[sockfd].inUse) ) {
        errno = EBADF;
        return -1;
    }

    RcsSocket &rs = rcsSockets[sockfd];
    if( !rs.connected ) {
        errno = ENOTCONN; 
        return -1;
    }

    int totalLen = 0;
    while(totalLen < len) {

        int msgSize = len - totalLen;
        if ( msgSize > MAX_MESSAGE_SIZE ) {
            msgSize = MAX_MESSAGE_SIZE;
        }

        // send pkt
        string sendPkt = makePkt(rs.pktSeq, rs.ackSeq-1, (char*)buf+totalLen, msgSize);
        ucpSendTo(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));

        // receive ack pkt
        for(;;) {
            string recvPkt = fetchPkt(rs.ucpSocket, rs.remote);

            if( recvPkt == "" || isCorrupt(recvPkt) ) {
                ucpSendTo(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
                continue;
            }

            if(getType(recvPkt) == KILL) {
                rs.reset();
                errno = ECONNRESET;
                return -1;
            }

            if(getType(recvPkt) == THIRD) {
                continue;
            }

            if( getAckSeq(recvPkt) != rs.pktSeq ) {
                ucpSendTo(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
                continue;
            }


            totalLen += msgSize;
            rs.pktSeq += 1;
            rs.pktSeq = rs.pktSeq >= 0 ? rs.pktSeq : 0;
            break;
        }
    }
    return len;
}

int32_t rcsRecv(int32_t sockfd, void *buf, int32_t len) {
    if( !(0 <= sockfd && sockfd < MAXSOCKETS && rcsSockets[sockfd].inUse) ) {
        errno = EBADF;
        return -1;
    }

    RcsSocket &rs = rcsSockets[sockfd];
    if( !rs.connected ) {
        errno = ENOTCONN; 
        return -1;
    }
 
    if( rs.recvMsg == "" ) {
        string ackPkt = makeAckPkt(rs.ackSeq-1);
        for(;;) {
            string recvPkt = fetchPkt(rs.ucpSocket, rs.remote);
            if( recvPkt == "" || isCorrupt(recvPkt) ) {
                ucpSendTo(rs.ucpSocket, ackPkt.c_str(), ackPkt.length(), &(rs.remote));
                continue;
            }

            if(getType(recvPkt) == KILL) {
                rs.reset();
                errno = ECONNRESET;
                return -1;
            }

            if(getType(recvPkt) == THIRD) {
                continue;
            }

            int32_t pktSeq = getPktSeq(recvPkt);
            if( pktSeq != rs.ackSeq ) {
                ucpSendTo(rs.ucpSocket, ackPkt.c_str(), ackPkt.length(), &(rs.remote));
                continue;
            }

            ackPkt = makeAckPkt(rs.ackSeq);
            ucpSendTo(rs.ucpSocket, ackPkt.c_str(), ackPkt.length(), &(rs.remote));
            rs.recvMsg = getMessage(recvPkt);

            rs.ackSeq += 1;
            rs.ackSeq = rs.ackSeq >= 0 ? rs.ackSeq : 0;
            break;
        }
    }

    int recvLen = rs.recvMsg.length() < len ? rs.recvMsg.length() : len;
    string msg = rs.recvMsg.substr(0, recvLen);
    memcpy(buf, msg.c_str(), msg.length());

    if(recvLen == rs.recvMsg.length()) {
        rs.recvMsg = "";
    } else {
        rs.recvMsg = rs.recvMsg.substr(recvLen);
    }
    return recvLen;
}

int32_t rcsClose(int32_t sockfd) {
    if( !(0 <= sockfd && sockfd < MAXSOCKETS && rcsSockets[sockfd].inUse) ) {
        errno = EBADF;
        return -1;
    }

    RcsSocket &rs = rcsSockets[sockfd];
    string killPkt = makeTypePkt(KILL);
    for(int i=0; i<SPAM_COUNT; i++)
        ucpSendTo(rs.ucpSocket, killPkt.c_str(), killPkt.length(), &(rs.remote));

    rs.reset();
    return 0;
}
