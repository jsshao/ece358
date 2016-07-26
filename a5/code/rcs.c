#include <cstdlib>
#include <string>
#include <iostream>
#include "rcs.h"
#include "ucp.h"
using namespace std;

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
#include "net_util.h"

#define BUFLEN 500
#define MAXSOCKETS 5000
#define SPAM 20

#define BYTES_CHECKSUM 4
#define BYTES_SEQ 4
#define BYTES_TYPE 1

#define MESSAGE 0
#define ACK 1
#define FIRST 2
#define SECOND 3
#define THIRD 4
#define KILL 5


struct RcsSocket {
    bool inUse;
    int32_t seqNum;
    int32_t ucpSocket;
    struct sockaddr_in local;
    struct sockaddr_in remote;

    RcsSocket() {
        inUse = false;
        seqNum = 0;
    }
};

struct RcsSocket rcsSockets[MAXSOCKETS];

/************** these are dummy implementations, please actually implement these ****************/

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
bool isCorrupt(string s) {
    cout<<"check corrupt"<<s<<endl;
    int32_t checksum = computeCheckSum(s.c_str()+BYTES_CHECKSUM, s.length()-BYTES_CHECKSUM);
    int32_t sentChecksum;
    memcpy(&sentChecksum, s.c_str(), BYTES_CHECKSUM);
    /*cout << "CHECKSUM IS " << checksum << endl;*/
    /*cout << "CHECKSUM IS SUPPOSE TO BE" << sentChecksum << endl;*/
    /*cout << "equal" << (checksum == sentChecksum) <<endl;*/
    return checksum  != sentChecksum;
}
// create and return a checksumed string message
// PACKET IS IN THE FORM [ CHECK_SUM | IS_ACK | SEQUENCE | DATA ]
string makePkt(int32_t seqNum, char* buf, int32_t len, int32_t type = MESSAGE) {
    int32_t totalLen = BYTES_CHECKSUM + BYTES_SEQ + BYTES_TYPE + len;
    char pktBuffer[totalLen];
    
    memset(pktBuffer, 0, totalLen);         // initialize packet to all 0s
    memcpy(&pktBuffer[BYTES_CHECKSUM + BYTES_SEQ + BYTES_TYPE], buf, len);      // set the data
    memcpy(&pktBuffer[BYTES_CHECKSUM + BYTES_TYPE], (char*) &seqNum, BYTES_SEQ);        // set the sequence #
    memcpy(&pktBuffer[BYTES_CHECKSUM], (char*) &type, BYTES_TYPE);              // set the acknowledgement byte
    
    int32_t checksum = computeCheckSum(pktBuffer, totalLen);                    // compute checksum
    memcpy(pktBuffer, (char*) &checksum, BYTES_CHECKSUM);                   // set checksum
    return string(pktBuffer, totalLen);
}

string makeTypePkt(int32_t seqNum, int32_t type) {
    char empty[0];
    return makePkt(seqNum, empty, 0, type);
}

string makeAckPkt(int32_t seqNum) {
    return makeTypePkt(seqNum, ACK);
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

    string s(buf, len);
    return s;
}

int getType(string s) {
    int32_t type = (int32_t)s.c_str()[BYTES_CHECKSUM];
    return type;
}

bool isAck(string s) {
    return getType(s) == ACK;
}

int32_t getSeqNum(string s) {
    int32_t seqNum;

    memcpy((char*)&seqNum, &s.c_str()[BYTES_CHECKSUM + BYTES_TYPE], BYTES_SEQ);
    return seqNum;
}

string getMessage(string s) {
    return s.substr(BYTES_CHECKSUM + BYTES_TYPE + BYTES_SEQ);
}


void ucpRetrySend(int socket, const void* buf, int len, const struct sockaddr_in *addr) {
    int sentLen = ucpSendTo(socket, buf, len, addr);
    /*while( sentLen != ucpSendTo(socket, buf, len, addr) )*/
        /*sentLen = ucpSendTo(socket, buf, len, addr);*/
}

/***********************************************************************************************/

int32_t rcsSocket() 
{
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
    return -1;
}

int32_t rcsBind(int32_t sockfd, struct sockaddr_in *addr) 
{
    if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
        return -1;

    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        return ucpBind(rs.ucpSocket, addr);
    }
    return -1;
}

int32_t rcsGetSockName(int32_t sockfd, struct sockaddr_in *addr) 
{
    if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
        return -1;

    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        if(ucpGetSockName(rs.ucpSocket, addr) == 0) {
            rs.local = *addr;
            return 0;
        }
    }
    return -1;
}

int32_t rcsListen(int32_t sockfd)
{
    if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
        return -1;

    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        return ucpSetSockRecvTimeout(rs.ucpSocket, 1000);
    }
    return -1;
}

int32_t rcsConnect(int32_t sockfd, const struct sockaddr_in *addr)
{
    if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
        return -1;

    string first = makeTypePkt(-1, FIRST);
    string second;
    string third = makeTypePkt(-1, THIRD);
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        if ( ucpSetSockRecvTimeout(rs.ucpSocket, 1000) != 0 ) {
            return -1;
        }

        //need valid handshake
        do {
            ucpRetrySend(rs.ucpSocket, first.c_str(), first.length(), addr);
            cout<<"sent"<<endl;
            char buf[BUFLEN];
            int32_t len = ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, &(rs.remote));
            if(len <= 0)
                continue;
            second = string(buf, len);
        } while(getType(second) != SECOND);
        cout<<"received second"<<endl;
        printf("server info %s %u\n", inet_ntoa(rs.remote.sin_addr), ntohs(rs.remote.sin_port));

        for(int32_t i=0; i<SPAM; i++) {
            ucpRetrySend(rs.ucpSocket, third.c_str(), third.length(), &(rs.remote));
        }

        return 0;

    }
    return -1;
}

int32_t rcsAccept(int32_t sockfd, struct sockaddr_in *addr)
{
    if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
        return -1;

    string first;
    string second = makeTypePkt(-1, SECOND);
    string third;
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        for(int32_t i=0; i<MAXSOCKETS; i++) {
            RcsSocket &newRs = rcsSockets[i];
            if(!newRs.inUse) {

                newRs.inUse = true;
                newRs.ucpSocket = ucpSocket();
                newRs.local = rs.local;
                newRs.local.sin_port = 0;
                ucpBind(newRs.ucpSocket, &(newRs.local));
                ucpGetSockName(newRs.ucpSocket, &(newRs.local));

                //first handshake
                do {
                    char buf[BUFLEN];
                    int32_t len = ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, addr);
                    if(len <= 0)
                        continue;
                    first = string(buf, len);
                    cout<<"got package"<<endl;
                    cout<<"package type"<<getType(first)<<endl;
                    cout<<"desired first"<<FIRST<<endl;
                } while ( getType(first) != FIRST );
                cout<<"escape 1"<<endl;
                newRs.remote = *addr;

                cout<<endl<<"server port: "<<ntohs(newRs.local.sin_port)<<endl;
                printf("server client info %s %u\n", inet_ntoa(newRs.remote.sin_addr), ntohs(newRs.remote.sin_port));
                do {
                    cout<<"waiting"<<endl;
                    ucpRetrySend(newRs.ucpSocket, second.c_str(), second.length(), &(newRs.remote));
                    third = fetchPkt(newRs.ucpSocket, newRs.remote);
                    if(third != "") {
                        cout<<"type"<<getType(third)<<endl;
                        cout<<"desired"<<THIRD<<endl;
                    }
                } while ( getType(third) != THIRD );
                cout<<"got acknowledgement"<<endl<<endl<<endl;

                cout<<"------------- connection established ------------"<<endl;

                return i;
            }
        }
    }
    return -1;
}


int32_t rcsSend(int32_t sockfd, void *buf, int32_t len)
{
    int sentLen = len<BUFLEN-1 ? len : BUFLEN-1;
    if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
        return -1;

    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {

        // send pkt
        string sendPkt = makePkt(rs.seqNum, (char*)buf, sentLen);
        ucpRetrySend(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));

        // receive ack pkt
        for(;;) {
            string recvPkt = fetchPkt(rs.ucpSocket, rs.remote);

            if( recvPkt == "" || isCorrupt(recvPkt) ) {
                ucpRetrySend(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
                continue;
            }

            if(getType(recvPkt) == KILL) {
                rs.inUse = false;
                rs.seqNum = 0;
                return -1;
            }

            bool ack = isAck(recvPkt);
            int32_t seqNum = getSeqNum(recvPkt);
            
            cout<<"ack: "<<ack<<endl;
            cout<<"seq: "<<seqNum<<endl;
            cout<<"exp: "<<rs.seqNum<<endl;
            if( seqNum < 0 ) {
                //residue of initial handshake
                continue;
            }

            if( (seqNum != rs.seqNum) || !ack ) {
                ucpRetrySend(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
                continue;
            }

            rs.seqNum += 1;
            rs.seqNum = rs.seqNum >= 0 ? rs.seqNum : 0;

            return sentLen;
        }
    }
    return -1;
}

int32_t rcsRecv(int32_t sockfd, void *buf, int32_t len)
{
    if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
        return -1;

    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        string ackPkt = makeAckPkt(rs.seqNum-1);
        for(;;) {
            string recvPkt = fetchPkt(rs.ucpSocket, rs.remote);
            if( recvPkt == "" || isCorrupt(recvPkt) ) {
                ucpRetrySend(rs.ucpSocket, ackPkt.c_str(), ackPkt.length(), &(rs.remote));
                continue;
            }

            if(getType(recvPkt) == KILL) {
                rs.inUse = false;
                rs.seqNum = 0;
                return -1;
            }

            int32_t seqNum = getSeqNum(recvPkt);
            if( seqNum != rs.seqNum ) {
                ucpRetrySend(rs.ucpSocket, ackPkt.c_str(), ackPkt.length(), &(rs.remote));
                continue;
            }

            ackPkt = makeAckPkt(rs.seqNum);
            ucpRetrySend(rs.ucpSocket, ackPkt.c_str(), ackPkt.length(), &(rs.remote));
            string msg = getMessage(recvPkt);
            memcpy(buf, msg.c_str(), msg.length()+1);

            rs.seqNum += 1;
            rs.seqNum = rs.seqNum >= 0 ? rs.seqNum : 0;

            cout << "GOT MESSAGE SUCCESSFULLY: " << msg << endl;
            return msg.length();
        }
    }
    return -1;
}

int32_t rcsClose(int32_t sockfd)
{
    if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
        return -1;

    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        string killPkt = makeTypePkt(-1, FIRST);
        for(int i=0; i<SPAM; i++)
            ucpRetrySend(rs.ucpSocket, killPkt.c_str(), killPkt.length(), &(rs.remote));

        rs.inUse = false;
        rs.seqNum = 0;
        return 0;
    }
    return -1;
}
