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

struct RcsSocket {
    bool inUse;
	int seqNum; // fuck it, we're only going to deal with positive seqNum, negative seqNum iff handshake residue
    int ucpSocket;
    struct sockaddr_in local;
    struct sockaddr_in remote;

    RcsSocket() {
        inUse = false;
		int seqNum = 0;
    }
};

struct RcsSocket rcsSockets[MAXSOCKETS];

/************** these are dummy implementations, please actually implement these ****************/

// create and return a checksumed string message
string makePkt(int seqNum, char* buf, int len) {
	string s(buf, len);
	cout<<"make pkt: "<<to_string(seqNum)<<endl;
	return to_string(seqNum)+s;
}

string makeAckPkt(int seqNum) {
	cout<<"make ack: "<<to_string(seqNum)+"ACK"<<endl;
	return to_string(seqNum)+"ACK";
}

// make sure to handle ucpRecvFrom timeout, maybe just return empty string?
string fetchPkt(int sockfd, struct sockaddr_in desiredRemote) {
	struct sockaddr_in addr;
	char buf[BUFLEN];
	int len = ucpRecvFrom(sockfd, buf, BUFLEN-1, &addr);
	if(len < 0 || (addr.sin_addr.s_addr != desiredRemote.sin_addr.s_addr)
			|| (addr.sin_port != desiredRemote.sin_port)) {
		if(len >= 0) cout<<"somehow the connection addresses are not matching"<<endl;
		return "";
	}
	string s(buf, len);
	return s;
}

bool isCorrupt(string s) {
	return false;
}

bool isAck(string s) {
	return s.substr(1) == "ACK";
}

int getSeqNum(string s) {
	cout<<"get seq num: "<<s[0]-'0'<<"     from string:"<<s<<endl;
	return s[0]-'0';
}

string getMessage(string s) {
	return s.substr(1);
}

/***********************************************************************************************/

int rcsSocket() 
{
    for(int i=0; i<MAXSOCKETS; i++) {
        RcsSocket &rs = rcsSockets[i];
        if(!rs.inUse) {
            int sockfd = ucpSocket();
            if(sockfd < 0)
                return -1;

            rs.ucpSocket = sockfd;
            rs.inUse = true;
            return i;
        }
    }
    return -1;
}

int rcsBind(int sockfd, struct sockaddr_in *addr) 
{
	if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
		return -1;

    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        return ucpBind(rs.ucpSocket, addr);
    }
    return -1;
}

int rcsGetSockName(int sockfd, struct sockaddr_in *addr) 
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

int rcsListen(int sockfd)
{
	if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
		return -1;

    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
		return ucpSetSockRecvTimeout(rs.ucpSocket, 1000);
    }
    return -1;
}

int rcsAccept(int sockfd, struct sockaddr_in *addr)
{
	if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
		return -1;

    string first = "first";
    string second = "second";
    string third = "third";
    char buf[BUFLEN];
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        for(int i=0; i<MAXSOCKETS; i++) {
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
                    int len = ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, addr);
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
					int len = ucpRecvFrom(newRs.ucpSocket, buf, BUFLEN-1, addr);
					buf[len] = '\0';
				} while ( third != buf );
				cout<<"got acknowledgement"<<endl<<endl<<endl;

                return i;
            }
        }
    }
    return -1;
}

int rcsConnect(int sockfd, const struct sockaddr_in *addr)
{
	if( !(0 <= sockfd && sockfd <= MAXSOCKETS) )
		return -1;

    string first = "first";
    string second = "second";
    string third = "third";
    char buf[BUFLEN];
    RcsSocket &rs = rcsSockets[sockfd];
    if(rs.inUse) {
        rs.remote = *addr;

        //need valid handshake
        printf("true client info %s %u\n", inet_ntoa(rs.local.sin_addr), ntohs(rs.local.sin_port));
		do {
			int len = ucpSendTo(rs.ucpSocket, first.c_str(), first.length(), &(rs.remote));
			len = ucpRecvFrom(rs.ucpSocket, buf, BUFLEN-1, &(rs.remote));
			buf[len] = '\0';
		} while(second != buf);
        cout<<"received second"<<endl;
        cout<<"server port: "<<ntohs(rs.remote.sin_port)<<endl<<endl<<endl;

		// this needs to be improved plox pretty plox oh my pretty plox
		// jason jason jason jason jason
		for(int i=0; i<5; i++) {
			ucpSendTo(rs.ucpSocket, third.c_str(), third.length(), &(rs.remote));
		}

        return 0;

    }
    return -1;
}

int rcsRecv(int sockfd, void *buf, int len)
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

			int seqNum = getSeqNum(recvPkt);
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

			return msg.length();
		}
    }
    return -1;
}

int rcsSend(int sockfd, void *buf, int len)
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
		int sentLen = ucpSendTo(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
		while(sentLen != sendPkt.length()) {
			sentLen = ucpSendTo(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
		}

		// receive ack pkt
		for(;;) {
			string recvPkt = fetchPkt(rs.ucpSocket, rs.remote);
			cout<<"client: "<<recvPkt<<endl;
			if( recvPkt == "" || isCorrupt(recvPkt) ) {
				cout<<"wololol"<<endl;
				ucpSendTo(rs.ucpSocket, sendPkt.c_str(), sendPkt.length(), &(rs.remote));
				continue;
			}

			bool ack = isAck(recvPkt);
			int seqNum = getSeqNum(recvPkt);
			
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

int rcsClose(int sockfd)
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
