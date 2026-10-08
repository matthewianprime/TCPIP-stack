// protocols.cpp
// Implements Classes CProtocol_L3 and CProtocol
// CProtocol_L3 is the base class for IP, UDP, ICMP
// CProtocol is the base class for LCP, PAP, CHAP etc (Abstraction of the 'C' protent structure used in BSD)

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "utils.h"

// Constructor and destructor
CProtocol_L3::CProtocol_L3(){}
CProtocol_L3::~CProtocol_L3()
{
if((pClient->autodelete==0)||(pClient==NULL))
	{
	// Not deleting a client.
	sprintf(msg,szDeletingProtocol,name);
	OutputDebugString(LOG_INFO,LOG_SYSTEM,(char*)msg);
	}

// Delete all outstanding messages in the queue for this object
while(router->untimeout(this,MSG_ALL));

if(rtx_buf)
	{
	delete [] rtx_buf;
	rtx_buf = NULL;
	}
	
if(pClient)
	{
	pClient->OnTransport(TRANSPORT_CLOSE,0);		// Client must release resources eg ownership of trace

	if(pClient->autodelete)
		{
		// Delete all outstanding messages in the queue for the client
		while(router->untimeout(pClient,MSG_ALL));

		// Delete this TCP or UDP instances' client protocol
		sprintf(msg,"Deleting protocol %s and client %s",name,pClient->name);
		OutputDebugString(LOG_INFO,LOG_SYSTEM,(char*)msg);
		delete(pClient);
		}
		
	pClient->transport=NULL;					// Do last. Client may call close()
	}
}

// Initialisation proc
void CProtocol_L3::init(int local_port){}
// Process a received packet 
unsigned int CProtocol_L3::input(void *datagram,int len,int32 ip_src_addr,int32 ip_dest_addr,int unit){return 0;}
// Send a packet
void CProtocol_L3::output(unsigned char* data, int datalen, char code, int32 remote_port_or_ipaddr,int unit,bool retransmission){}
// Shut down transport
void CProtocol_L3::close(){}
// Return how much transmission buffer is available
int16 CProtocol_L3::GetTransmitBuffer(){return 0;}
// Return how much reception buffer is available
int16 CProtocol_L3::GetReceiveBuffer(){return TCP_MAX_WINDOW;}
// Create layer 4 application client
CClient* CProtocol_L3::CreateClient(int16 portnumber){return NULL;}

// Initialisation proc
void CProtocol::init(int){}
// Process a received packet 
void CProtocol::input(int, unsigned char*,int){}
// Process a received protocol-reject 
void CProtocol::protrej(int){}
// Lower layer has come up 
void CProtocol::lowerup(int){}
// Lower layer has gone down 
void CProtocol::lowerdown(int){}
// Open the protocol 
void CProtocol::open (int unit){}
// Close the protocol 
void CProtocol::close (int unit, const char *reason){}
// Process a received data packet 
void CProtocol::datainput (int unit, unsigned char *pkt, int len){}
// Check requested options, assign defaults 
void CProtocol::check_options(void){}
// Configure interface for demand-dial 
int CProtocol::demand_conf(int unit){return 0;}
// Say whether to bring up link for this pkt 
int CProtocol::active_pkt(unsigned char *pkt, int len){return 0;}

void CProtocol::resetci (fsm *){return;}									// Reset our CI
int  CProtocol::cilen (fsm *){return 0;}									// Return length of our CI
void CProtocol::addci (fsm *, unsigned char *, int *){return;}				// Add our CI to pkt
int  CProtocol::ackci (fsm *, unsigned char *, int){return 0;}				// Peer ack'd our CI
int  CProtocol::nakci (fsm *, unsigned char *, int){return 0;}				// Peer nak'd our CI
int  CProtocol::rejci (fsm *, unsigned char *, int){return 0;}				// Peer rej'd our CI
int  CProtocol::reqci (fsm *, unsigned char *, int *, int){return 0;}		// Rcv peer CI
void CProtocol::up (fsm *){return;}											// We're UP
void CProtocol::down (fsm *){return;}										// We're DOWN
void CProtocol::starting (fsm *){return;}									// We need lower layer up
void CProtocol::finished (fsm *){return;}									// We need lower layer down
int  CProtocol::extcode (fsm *, int, int, unsigned char *, int){return 0;}
void CProtocol::rprotrej (fsm *, unsigned char *, int){return;}

