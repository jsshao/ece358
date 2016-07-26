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
    
    memset(pktBuffer, 0, totalLen);			// initialize packet to all 0s
    memcpy(&pktBuffer[BYTES_CHECKSUM + BYTES_SEQ + BYTES_TYPE], buf, len);		// set the data
    memcpy(&pktBuffer[BYTES_CHECKSUM + BYTES_TYPE], (char*) &seqNum, BYTES_SEQ);		// set the sequence #
    memcpy(&pktBuffer[BYTES_CHECKSUM], (char*) &type, BYTES_TYPE);				// set the acknowledgement byte
    
    int32_t checksum = computeCheckSum(pktBuffer, totalLen);					// compute checksum
    memcpy(pktBuffer, (char*) &checksum, BYTES_CHECKSUM);					// set checksum
	return string(pktBuffer, totalLen);
}

string makeAckPkt(int32_t seqNum) {
	char empty[0];
	return makePkt(seqNum, empty, 0, 1);
}

string fetchPkt(int32_t sockfd, struct sockaddr_in desiredRemote) {
	struct sockaddr_in addr;
	char buf[BUFLEN];
	int32_t len = ucpRecvFrom(sockfd, buf, BUFLEN-1, &addr);

	// len < 0 handles timeouts. just return empty string
	if(len < 0 || (addr.sin_addr.s_addr != desiredRemote.sin_addr.s_addr)
			|| (addr.sin_port != desiredRemote.sin_port)) {
		/*if(len >= 0) */
            /*cout<<"somehow the connection addresses are not matching"<<endl;*/

		return "";
	}
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

int32_t rcsAccept(int32_t sockfd, struct sockaddr_in *addr)
{
	if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
		return -1;

    string first = "first";
    string second = "second";
    string third = "third";
    char buf[BUFLEN];
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
                    int32_t len = ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, addr);
                    buf[len] = '\0';
				} while ( first != buf );
				newRs.remote = *addr;

				cout<<endl<<"server port: "<<ntohs(newRs.local.sin_port)<<endl;
				printf("server client info %s %u\n", inet_ntoa(newRs.remote.sin_addr), ntohs(newRs.remote.sin_port));
				do {
					// second handshake: send new socket info
					ucpSendTo(newRs.ucpSocket, second.c_str(), second.length(), &(newRs.remote));

					// recieve third hanshake
					cout<<"sent, waiting for acknowlegement"<<endl;
					int32_t len = ucpRecvFrom(newRs.ucpSocket, buf, BUFLEN-1, addr);
					buf[len] = '\0';
				} while ( third != buf );
                    cout<<"got acknowledgement"<<endl<<endl<<endl;

                return i;
            }
        }
    }
    return -1;
}

int32_t rcsConnect(int32_t sockfd, const struct sockaddr_in *addr)
{
	if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
		return -1;

    string first = "first";
    string second = "second";
    string third = "third";
    char buf[BUFLEN];
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
    	if ( ucpSetSockRecvTimeout(rs.ucpSocket, 1000) != 0 ) {
    		return -1;
    	}
        rs.remote = *addr;

        //need valid handshake
        printf("true client info %s %u\n", inet_ntoa(rs.local.sin_addr), ntohs(rs.local.sin_port));
		do {
			int32_t len = ucpSendTo(rs.ucpSocket, first.c_str(), first.length(), &(rs.remote));
			len = ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, &(rs.remote));
			buf[len] = '\0';
		} while(second != buf);
        cout<<"received second"<<endl;
        cout<<"server port: "<<ntohs(rs.remote.sin_port)<<endl<<endl<<endl;

		// this needs to be improved plox pretty plox oh my pretty plox
		// jason jason jason jason jason
		for(int32_t i=0; i<5; i++) {
			ucpSendTo(rs.ucpSocket, third.c_str(), third.length(), &(rs.remote));
		}

        return 0;

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
			if( isCorrupt(recvPkt) ) {
				ucpSendTo(rs.ucpSocket, ackPkt.c_str(), ackPkt.length(), &(rs.remote));
				continue;
			}

			int32_t seqNum = getSeqNum(recvPkt);
			if( seqNum != rs.seqNum ) {
				ucpSendTo(rs.ucpSocket, ackPkt.c_str(), ackPkt.length(), &(rs.remote));
				continue;
			}

			ackPkt = makeAckPkt(rs.seqNum);
			ucpSendTo(rs.ucpSocket, ackPkt.c_str(), ackPkt.length(), &(rs.remote));
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

int32_t rcsSend(int32_t sockfd, void *buf, int32_t len)
{
	///////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////
	len = len<BUFLEN-1 ? len : BUFLEN-1;
	///////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////
	if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
		return -1;

    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {

		// send pkt
		string sendPkt = makePkt(rs.seqNum, (char*)buf, len);
		int32_t sentLen = ucpSendTo(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
		while(sentLen != sendPkt.length()) {
			sentLen = ucpSendTo(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
		}

		// receive ack pkt
		for(;;) {
			string recvPkt = fetchPkt(rs.ucpSocket, rs.remote);

			if( recvPkt == "" || isCorrupt(recvPkt) ) {
				ucpSendTo(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
				continue;
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
				ucpSendTo(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
				continue;
			}

			rs.seqNum += 1;
			rs.seqNum = rs.seqNum >= 0 ? rs.seqNum : 0;

			return sentLen;
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
        rs.inUse = false;
		rs.seqNum = 0;
        return 0;
    }
    return -1;
}
