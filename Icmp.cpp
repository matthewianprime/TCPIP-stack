// icmp.cpp
// PING and periodic advertisment of the unit as a router

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "icmp.h"
#include "ip.h"
#include "utils.h"

//--------------------------------------------------------------------------
void Cicmp::init(int unused)
{
protocol =  IP_ICMP;				// Protocol number
name = szICMP;						// Protocol name
enabled_flag = true;
autodelete=true;					// Set false if system not to delete this icmp instance
state = STATE_NA;
pClient = NULL;
icmp_seq_num = 0x950c;				// Any number will do !
time = 0xffffffffL;					// Don't want to time out. This instance persists always.
rtx_buf = NULL;						// The retransmission circular buffer. Unused MUST BE NULL in this class!
}

//--------------------------------------------------------------------------
unsigned int Cicmp::input(void *datagram,int len,int32 ip_src_addr,int32 ip_dest_addr,int unit)
// Process a received ICMP frame
{
unsigned char *p = (unsigned char*)datagram;
unsigned int request_id,seq_num;
int16 cksum;
CObj* pObj=NULL;
int32 timesent;
	
switch((char)*p)					// Switch on Type field
	{
	case ICMP_ECHO_REQUEST:			// Echo request
	
	// Make up an echo reply frame using the echo request data.
	PUTCHAR(ICMP_ECHO_REPLY,p);		// Point to Type field
	INCPTR(1,p);					// Point to checksum
	PUTSHORT(0,p);					// Init cksum to 0 for calculation
	DECPTR(2,p);					// Point to checksum
	cksum = CHECKSUM((unsigned char*)datagram,len);
	PUTSHORT(cksum,p);
	
	sprintf(msg,szIcmp_request,ip_src_addr);
	OutputDebugString(LOG_DEBUG,LOG_ICMP,(const char*)msg);

	// Send ICMP reply
	// NB frame is still in in packet_buf at this point 
	output_ip(0,IP_ICMP,ip_src_addr,(unsigned char*)datagram,len,unit);
	break;

	case ICMP_ECHO_REPLY: // Echo reply to our request
	INCPTR(4,p);
	GETSHORT(request_id,p);	
	GETSHORT(seq_num,p);	

	sprintf(msg,szIcmp_reply_id,request_id,seq_num);
	OutputDebugString(LOG_DEBUG,LOG_ICMP,(const char*)msg);

	// If we sent the request, there is a timeout object containing:
	// A pointer to the sender, time sent.
	// Delete the timeout object, which also acts as a no response timer.
	pObj = router->untimeout_icmp(MSG_ECHO_REQUEST,request_id,&timesent);
	
	if((timesent) && (pObj != NULL))
		// Inform object that initiated the ping that there is a reply
		PostMessage(pObj,MSG_ECHO_REPLY,ip_src_addr,getTickCount(),request_id,1L);	

	break;

	case ICMP_ROUTERSELECTION:
	// A LAN host wants to discover routers. Respond with ICMP_ROUTER_ADVERT frame
	// TODO  test this. Untested.
	output(NULL,ICMP_ROUTER_ADVERT,0,ip_src_addr,0);
	break;

	default:
	break;
	}
return true;
}

//--------------------------------------------------------------------------
void Cicmp::output(unsigned char* data, int type, char code, int32 ip_addr,int request_id,bool)
// Send an ICMP frame
// ICMP header general format
// |  Type(8)  |  Code(8)  |    Checksum(16)   |
{
int len, datalen;

// Get the interface on which to send the frame
int unit;

#ifdef SEB
unit = HW_LAN;
#else
// Nb. We send Arp to our own address at startup
// If this is now the case frame must go out LAN interface
if(ip_addr != lan[ACTIVE].ip_addr)
	{
	unit  = ipV4->select_interface(ip_addr);
	if((unit < HW_DTE0) || (unit > HW_LAN))
		// Destination unreachable	
		return;
	}
#endif

len = ICMP_HDRLEN;

unsigned char *p,*p2;
if(unit == HW_LAN)
	p = packet_buf + LAN_HDRLEN + IP_HEADERLEN;
else
	p = packet_buf + IP_HEADERLEN + 2;

p2 = p;									// Store ICMP frame start
int16 cksum=0;

// These bytes are common to all ICMP  frames
PUTCHAR(type,p);				// Type
PUTCHAR(code,p);				// Code
PUTSHORT(0,p);					// Cksum. Init to 0

sprintf(msg,szIcmp_send,type,code);
OutputDebugString(LOG_DEBUG,LOG_ICMP,(const char*)msg);

// Put frame-type specific data
switch(type)
	{
	case ICMP_DEST_UNREACHABLE:
	case ICMP_ROUTERSELECTION:
	case ICMP_TTL:
		PUTLONG(0L,p);				// Unused field

		// Copy IP header + 64 bits of datagram into ICMP message
		datalen = 64+IP_HEADERLEN;
		// memmove handles overlapped src and dest areas.
		memmove((void*)p,data,datalen);
		len +=datalen;
		break;

	case ICMP_ROUTER_ADVERT:
		// TODO check the spec and test this !
		// NB this frame is sent to Multicast mac addr
		PUTCHAR(1,p);					// No of addresses. Ours only
		PUTCHAR(2,p);					// Address entry size. Always 2 I think.
		PUTSHORT(0xffff,p);				// Lifetime. Seconds this address is valid.TODO Guess value - is ok ?

		PUTLONG(ip_addr,p);				// Router address
		PUTLONG(1L,p);					// Preference level = 1

		len += 8;						// 8 bytes for address & pref. level
		break;

	case ICMP_ECHO_REQUEST:
		PUTSHORT(request_id,p);			// IP ID.
		PUTSHORT(icmp_seq_num++,p);		// ICMP Sequence number

		datalen = strlen((const char*)szPingData);
		memcpy(p,szPingData,datalen);	// Data, free text field.
		len += datalen;
		break;
	}

p = p2;
cksum = CHECKSUM(p,len);
INCPTR(2,p);							// Point at cksum
PUTSHORT(cksum,p);

// select_interface returns 1 more than the unit number if ISDN
if(unit != HW_LAN)
	unit = unit-1;

output_ip(0xb400,IP_ICMP,ip_addr,p2,len,unit);
}

//--------------------------------------------------------------------------
void Cicmp::OnMessage(int16 messg, int32 lParam,int32 wParam,int16)
// A timer has expired
{
if(wParam == NULL)
	return;

switch(messg)
	{
	case MSG_ICMP_ROUTER_ADVERT:
	// Send an ICMP router advertisment frame to tell LAN clients we're here
	output(NULL,ICMP_ROUTER_ADVERT,0,IP_ADDR_MULTICAST,0);
	
	// Schedule the next router advertisment...
	PostMessage(this,MSG_ICMP_ROUTER_ADVERT,0,0,0,(int32)ICMP_ROUTER_ADVERT_PERIOD);
	break;

	case MSG_ECHO_REQUEST:
	// We're here because someone wants a ping sent
	output((unsigned char*)szPingData,ICMP_ECHO_REQUEST,0,lParam,(int)wParam);
	break;

	default:
	// Should never get here !
	break;
	}
}
