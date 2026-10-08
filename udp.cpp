// udp.cpp
// udp transport protocol implementation
// Also includes DHCP functions output_dhcp(..) and OnMessage()

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "udp.h"
#include "tftp.h"
#ifdef DHCP
#include "dhcp.h"
#endif
#include "utils.h"
#ifdef AUTO_IP
#include "auto_ip.h"
#endif

int32 transaction_id;

//--------------------------------------------------------------------------
void Cudp::init(int local_port)
{
protocol =  IP_UDP;
name = szUDP;
port_local = local_port;				// Our port number
port_remote = 0;
state = STATE_NA;

enabled_flag = true;
autodelete = true;						// Set false if system should not delete this instance
pClient = NULL;

rtx_buf = NULL;							// The retransmission circular buffer. Unused MUST BE NULL in this class!

#ifdef DHCP
dhcp->client_state = DHCP_INIT;			// Our DHCP client state variable
#endif
time = getTickCount();					// Data inactivity timer
idletimer_ms = UDP_IDLE_TIMEOUT_MSECS;	// Default idle timeout value. Override in pClient->init() if required
}

//--------------------------------------------------------------------------
void Cudp::close()
// Purpose:
// Clean shut down of the UDP connection
{
// Simply mark this object for deletion
enabled_flag = false;
}

//--------------------------------------------------------------------------
unsigned int Cudp::input(void *datagram,int len,int32 rcvd_src_addr,int32 ip_dest_addr,int unit)
{
int16 cksum;
volatile int16 udp_len;
int16 rcvd_local_port;
int16 rcvd_remote_port;
int16 tmp_cksum;
int iRes;

unsigned char *p = (unsigned char*)datagram;
unsigned char *pUDP_data;

GETSHORT(rcvd_remote_port,p);
GETSHORT(rcvd_local_port,p);

// Is the frame for this UDP port number ?
// If port_local=0 this is a new UDP instance created for a UDP
// protocol that changes its port number on the fly like TFTP.
if((rcvd_local_port != port_local) && (port_local != 0))
	{
	// Datagram not for this UDP instance. (If port_local==0 this is a new instance )
//	sprintf(msg,"UDP port %u ignoring port %u",port_local,rcvd_local_port);
//	OutputDebugString(LOG_INFO,LOG_UDP,(const char*)msg);
	return false;
	}

GETSHORT(udp_len,p);

if((udp_len < UDP_HEADERLEN) || (udp_len > len))
	{// short frame
	OutputDebugString(LOG_INFO,LOG_UDP,szUDPbadframesize);
	return true;
	}

// Udp data length, excluding UDP header
if(udp_len > UDP_HEADERLEN)
	{
	time = getTickCount();							// Refresh UDP no-data inactivity timer
	if(udp_len > TCP_MAX_WINDOW)
		{
		// Frame too big error. Give up now.
		close();
		OutputDebugString(LOG_INFO,LOG_UDP,szUDPframetoobig);
		return true;
		}
	}

//sprintf(msg,"UDP rx %u port %u our port %u",udp_bytes,rcvd_local_port,port_local);
//OutputDebugString(LOG_INFO,LOG_UDP,(const char*)msg);

// Save the clients details. 
port_remote = rcvd_remote_port;						// Remotes port number
dest_addr = rcvd_src_addr;							// Their ip addr

GETSHORT(cksum,p);
// pUDP_data points at UDP data....
pUDP_data = p;

if(cksum !=0)
	{
	// 0 checksum means "checksums not used"
	// Checksum verification  :
	// Make up the UDP pseudoheader - 12 bytes. Can be positioned before the UDP data bytes.
	p = (unsigned char*)datagram - UDP_PSEUDOHEADERLEN;
	PUTLONG(rcvd_src_addr,p);				// Their ip addr
	PUTLONG(ip_dest_addr,p)					// Frame dest addr (our ip or broadcast addr)
	PUTCHAR(0,p);							// Reserved
	PUTCHAR(IP_UDP,p);						// Protocol = UDP
	PUTSHORT(udp_len,p);					// Length. 

	tmp_cksum  = CHECKSUM((unsigned char*)datagram - UDP_PSEUDOHEADERLEN,UDP_PSEUDOHEADERLEN + udp_len);

	if(tmp_cksum != 0)
		{// UDP FCS error
		sprintf(msg,szUDPfcserr,rcvd_src_addr,ip_dest_addr,cksum,tmp_cksum);
		OutputDebugString(LOG_INFO,LOG_UDP,(const char*)msg);
		return true;
		}
	}

if(pClient ==  NULL)
	{
	// First frame received by this new UDP instance.
	// Create client based on UDP port number
	pClient = CreateClient(rcvd_local_port);
	if(!pClient)
		{// Client create fail.
		enabled_flag=false;					// Delete this instance
		return true;
		}
	else
		// We are handling the datagram - set port number
		// Note it remains at port 0 ("accept any") if we do not handle this datagram
		port_local = rcvd_local_port;
	}
	
	
// Nb. Ctftp will change "port_local" from TFTP listener port 69
// to a new random portnumber when it gets the initial RRQ or WRQ
udp_len = udp_len - UDP_HEADERLEN;
iRes = pClient->receive(&pUDP_data,(int16*)&udp_len);

switch(iRes)
	{
	case NACKNOWLEDGE:
	case TERMINATE:
	// ACK last frame and mark this object for deletion
	enabled_flag = false;

	case ACKNOWLEDGE:
	output(pUDP_data,udp_len,NULL,0,unit);
	break;
	
	case ACK_DELAYED:
	// Send an ACK after 25 milliseconds
	PostMessage(this,MSG_TIMER,DELAYED_ACK,udp_len,0,25);
	break;
	
	case NORESPONSE:
#ifdef DHCP
	// Set a retry timer incase no DHCP server responds
	PostMessage(this,MSG_TIMER,0,0,0,10000L);
#endif
	break;
	}
	
return true;
}

//--------------------------------------------------------------------------
void Cudp::output(unsigned char* udp_data, int udp_len, char, int32,int unit,bool)
{
// Need to prepend headers to this frame as it passes down the stack.
// Calculate the header rooom needed
int UDP_OFFSET = UDP_HEADERLEN;

if(unit == HW_LAN)
//				| LAN (12)		| IP (max=20) | UDP (8) |
	UDP_OFFSET += LAN_HDRLEN;

//else
//				| PPP (max=4)	| IP (max=20) | UDP (8) |
//	UDP_OFFSET += PPP_HDRLEN + MP_HEADERLEN + 2;

unsigned char* outp= &packet_buf[UDP_OFFSET];	// UDP header structure

// Generate a pseudoheader to be prepended to UDP frame.
// The pseudoheader is used only for checksum calculation then discarded
unsigned char* p = &packet_buf[UDP_OFFSET - UDP_PSEUDOHEADERLEN];
PUTLONG(lan[ACTIVE].ip_addr,p);					// Source IP addr - our ip or broadcast addr
PUTLONG(dest_addr,p);							// Remote IP addr
PUTCHAR(0,p);									// Reserved
PUTCHAR(IP_UDP,p);								// Protocol = UDP
PUTSHORT((unsigned int)UDP_HEADERLEN + udp_len,p);	// Length of UDP datagram not including pseudoheader

// Generate UDP header following pseudo header. ( at &packet_buf[UDP_OFFSET] )
PUTSHORT(port_local,p);							// Our port no.
PUTSHORT(port_remote,p);						// Their port no.
PUTSHORT(udp_len + UDP_HEADERLEN,p);			// UDP frame length
PUTSHORT(0,p);									// Placeholder for checksum

if(udp_len)
	memmove(p,udp_data,udp_len);					// Append UDP data to UDP header

unsigned int cksum  = CHECKSUM(&packet_buf[UDP_OFFSET - UDP_PSEUDOHEADERLEN]	\
						,UDP_HEADERLEN + udp_len + UDP_PSEUDOHEADERLEN);
						
DECPTR(2,p);									// Point at checksum
PUTSHORT(cksum,p);

sprintf(msg,szTx_UDP,udp_len,dest_addr);
OutputDebugString(LOG_INFO,LOG_UDP,(const char*)msg);
output_ip(ip_id++,IP_UDP,dest_addr,outp,UDP_HEADERLEN + udp_len ,unit);
}

//--------------------------------------------------------------------------
CClient* Cudp::CreateClient(int16 portnumber)
// Create a CClient object to handle the connection data based on the UDP port number.
// Called when UDP session opened.
{
CClient* udpClient = NULL;
int HW_CLASS = HW_NONE;
bool bAutoDelete = true;						// Client is automatically deleted when pClient->quit = true

if(pClient)
	{
	// There should not be a client so if this happens is I delete it.
	if(pClient->autodelete)
		{
		delete(pClient);
		pClient=NULL;
		}
	else
		// A static client object (Not an auto delete client)
		return pClient;
	}
	
switch(portnumber)
	{
#ifdef AUTO_IP
	case DSP_DISCOVER_PORT:
		// Response to our client DISCOVER request
		udpClient = new Cauto_ip;
		bAutoDelete = false;					// This code will be called once when the first DHCP frame is received.
		idletimer_ms = NO_IDLE_TIMEOUT;				// Don't want this UDP transport to time out (must be present to service DHCP client)
		break;
#endif
#ifdef DHCP
	case UDP_DHCP_CLIENT:
		// Response to our client DISCOVER request
		udpClient = dhcp;						// Use the static instance of dhcp.
		bAutoDelete = false;					// This code will be called once when the first DHCP frame is received.
		idletimer_ms = NO_IDLE_TIMEOUT;				// Don't want this UDP transport to time out (must be present to service DHCP client)
		break;

	case UDP_DHCP_LISTEN:
		// A client wants to talk to our DHCP server
		//udpClient = (CudpClient*) new Cdhcp;
		udpClient = dhcp;						// Use the static instance of dhcp.
		bAutoDelete = false;					// This code will be called once when the first DHCP frame is received.
		idletimer_ms = NO_IDLE_TIMEOUT;			// Don't want this UDP transport to time out (must be present to service DHCP server)
		break;
#endif

#ifdef TFTP
	case UDP_TFTP:
		udpClient = new Ctftp;
		HW_CLASS = HW_TRACE;					// Select trace port  ??????
		break;
#endif
	}

if(udpClient == NULL)
	{
	sprintf(msg,szUDPFailCreateClient,portnumber);
	OutputDebugString(LOG_INFO,LOG_UDP,(const char*)msg);	
	pClient=NULL;
	return NULL;								// Create fail
	}

// Client instance created
pClient = udpClient;
((CClient*)pClient)->init(HW_CLASS,this,bAutoDelete);

if(((CClient*)pClient)->quit)
	{
	// Client refuses connection
	sprintf(msg,szUDPFailCreateClient,portnumber);
	OutputDebugString(LOG_DEBUG,LOG_UDP,(const char*)msg);
	return pClient;
	}

sprintf(msg,szUDPCreatedClient,pClient->name,portnumber);
OutputDebugString(LOG_INFO,LOG_UDP,(const char*)msg);
return pClient;
}

//--------------------------------------------------------------------------
void Cudp::OnMessage(int16 messg, int32 server_ip_addr,int32 our_ip_addr,int16 zParam)
// A timer has expired
// Implements the client DHCP state machine
{
unsigned char *p= &packet_buf[UDP_DATA_OFFSET+20];
unsigned char *pUDP_data = p;

switch(messg)
	{
	case MSG_TIMER:
	// Something timed out
#ifdef TFTP
	if(server_ip_addr==DELAYED_ACK)
		{
		// our_ip_addr is the blocknumber to acknowledge

//		sprintf(msg,"TFTP DELAYED ACK");
//		OutputDebugString(LOG_INFO,LOG_UDP,(const char*)msg);

		PUTSHORT(TFTP_ACK,p);
		PUTSHORT((int16)our_ip_addr,p);
		output(pUDP_data,4,NULL,0,HW_LAN);
		}
#endif // TFTP

#ifdef DHCP
	switch(dhcp->client_state)
		{
		case DHCP_RENEWING:
		// The DHCP server which provided the lease is not responding.
		OutputDebugString(LOG_INFO,LOG_DHCP,szDHCPrenewingnoresp);
		// Keep attempting to renew until T2 expires ( == lease time * 0.875 )
		PostMessage(this,MSG_DHCP_RENEW_ADDR,server_ip_addr,our_ip_addr,0,4000L);
		break;

		case DHCP_REQUESTING:
		// The DHCP server is not responding ACK to our REQUEST.
		OutputDebugString(LOG_INFO,LOG_DHCP,szDHCPrequestingnoresp);
		dhcp->client_state = DHCP_INIT;
		// Try broadcasting for any DHCP server
		PostMessage(this,MSG_DHCP_DISCOVER_ADDR,0,0,0,1);
		break;

		case DHCP_SELECTING:
		if(lan[UNSAVED].ip_addr != 0)
			{
			// An ip addr was offered. Accept it
			PostMessage(this,MSG_DHCP_REQUEST_ADDR,0,0,0,10000L);
			break;
			}

		OutputDebugString(LOG_INFO,LOG_DHCP,szDHCPservernoresp);
		// Keep trying forever
		PostMessage(this,MSG_DHCP_DISCOVER_ADDR,0,0,0,1);
		break;

		default:
		break;
		}
	break;

	case MSG_DHCP_DISCOVER_ADDR:
	// Initial dhcp request at boot, or  recovery if primary DHCP server fails
	OutputDebugString(LOG_INFO,LOG_DHCP,szDHCPClientRequest);
	dhcp->client_state = DHCP_SELECTING;

	// Want from server:ip, mask,gateway,TCP keepalive period
	msg[0]=0x3;msg[1]=1;msg[2]=6;msg[3]=38;msg[4]=0;

	output_dhcp(DHCP_DISCOVER,IP_ADDR_BROADCAST,0,(char*)msg);
	
	// Set a retry timer incase no DHCP server responds
	PostMessage(this,MSG_TIMER,0,0,0,10000L);
	break;

	case MSG_DHCP_REQUEST_ADDR:
	// DISCOVERED IP addr is ok, ask for the lease, expect ACK response
	dhcp->client_state = DHCP_REQUESTING;
	
	// Want from server:ip, mask,gateway
	msg[0]=0x3;msg[1]=1;msg[2]=6;msg[3]=38;msg[4]=0;
	output_dhcp(DHCP_REQUEST,lan[UNSAVED].dhcp_server,lan[UNSAVED].ip_addr,(char*)msg);
	
	// Set a retry timer incase DHCP server does not respond
	PostMessage(this,MSG_TIMER,lan[UNSAVED].dhcp_server,lan[UNSAVED].ip_addr,0,2000L);
	break;

//	case MSG_DHCP_T1_TIMER:
	case MSG_DHCP_RENEW_ADDR:
	// Request lease renewal
	OutputDebugString(LOG_INFO,LOG_DHCP,szDHCPrenewing);
	dhcp->client_state = DHCP_RENEWING;
	output_dhcp(DHCP_REQUEST,server_ip_addr,our_ip_addr);
	// Set a retry timer incase the DHCP server does not respond ACK
	PostMessage(this,MSG_TIMER,server_ip_addr,our_ip_addr,0,5000L);
	break;

	case MSG_DHCP_DECLINE_ADDR:
	// IP addr is not ok, decline the lease
	dhcp->client_state = DHCP_INIT;	
	output_dhcp(DHCP_DECLINE,server_ip_addr,our_ip_addr);
	break;
	
	case MSG_DHCP_T2_TIMER:
	if(dhcp->client_state == DHCP_RENEWING)
		// Original server not responding.
		// Broadcast for any DHCP server
		PostMessage(this,MSG_DHCP_DISCOVER_ADDR,0,0,0,1);	
	break;

	case MSG_DHCP_T3_TIMER:
	// Lease expired.
	// Discard any existing DHCP settings
	OutputDebugString(LOG_WARNING,LOG_SYSTEM,szDHCPexpire);
	lan[UNSAVED].ip_addr = 0;
	lan[UNSAVED].dhcp_leasetime = 0;
	lan[UNSAVED].dhcp_server = 0;					// Used as a flag meaning "ip obtained by DHCP"
	lan[ACTIVE].ip_addr = 0;
	lan[ACTIVE].dhcp_leasetime = 0;
	lan[ACTIVE].dhcp_server = 0;					// Used as a flag meaning "ip obtained by DHCP"

	// Kill all DHCP timers. Will periodically broadcast a DISCOVER until a server is found
	while(router->untimeout((CObj*)this, MSG_ALL));
	PostMessage(this,MSG_DHCP_DISCOVER_ADDR,0,0,0,1);
#endif //DHCP
	break;
	}
}

#ifdef DHCP
//--------------------------------------------------------------------------
void Cudp::output_dhcp(int frametype,int32 server_ip_addr,int32 our_ip_addr,char* Param_Req_List)
// Send a DHCP frame
{
unsigned char* udp_data = &packet_buf[UDP_HEADERLEN + UDP_PSEUDOHEADERLEN + LAN_HDRLEN];
unsigned char* p = udp_data;
int udp_len;
int16 flags=0;

dest_addr = 0xffffffff;//server_ip_addr;			// Servers ip, or broadcast

port_local = UDP_DHCP_CLIENT;		// We are DHCP client
port_remote = UDP_DHCP_LISTEN;		// DHCP server
transaction_id = getTickCount();	// Unique transaction ID
udp_len = 300;

PUTCHAR(0x01,p);					// Operation = BOOTREQUEST
PUTCHAR(0x01,p);					// Htype=10m Ethernet
PUTCHAR(0x06,p);					// HLEN=6
PUTCHAR(0x0,p);						// hops
PUTLONG(transaction_id,p);			// Unique request ID (XID)
PUTSHORT(0x1,p);					// Secs since trying to boot
if(server_ip_addr == 0xffffffff)
	flags = 0x8000;					// Broadcast message
PUTSHORT(flags,p);					// (flags)

PUTLONG(0L,p);						// (yiaddr)
PUTLONG(0L,p);						// (yiaddr)
PUTLONG(0L,p);						// (yiaddr)
PUTLONG(0L,p);						// (yiaddr)
PUTMAC(mac_addr,p);					// chaddr

memset(p,0,udp_len);				// Fill sname, file, options with 0's

p = udp_data + DHCP_OPTIONSFIELD;	// Point at options field of the UDP frame.
PUTLONG(0x63825363,p);				// Magic cookie

PUTCHAR(0x35,p);					// DHCP type field 1 (53)
PUTCHAR(0x01,p);					// DHCP type field 2
PUTCHAR(frametype,p);				// 1 = DHCP discover, 4=DECLINE,5=ACK

PUTCHAR(0x3d,p);					// Type = Client ID
PUTCHAR(0x07,p);					// Length
PUTCHAR(0x01,p);					// Ether type (?)
PUTMAC(mac_addr,p);
	
if(our_ip_addr != 0)
	{
	PUTCHAR(0x32,p);					// Type = requested ip
	PUTCHAR(0x04,p);					// Length
	PUTLONG(our_ip_addr,p);				// Server ip
	}
		
if(server_ip_addr != 0xffffffff)
	{
	PUTCHAR(0x36,p);					// Type = server ip
	PUTCHAR(0x04,p);					// Length
	PUTLONG(server_ip_addr,p);			// Server ip
	}
	
PUTCHAR(0x0c,p);						// HOSTNAME option
int len=strlen((const char*)lan[UNSAVED].hostname);
PUTCHAR(len+1,p);
strcpy((char*)p,(const char*)lan[UNSAVED].hostname);
INCPTR(len,p);
PUTCHAR(0,p);							// Null terminate string ( as Win98 )

if(Param_Req_List != NULL)
	{
	len = strlen(Param_Req_List);
	PUTCHAR(0x37,p);
	PUTCHAR(len,p);
	strcpy((char*)p,Param_Req_List);
	INCPTR(len,p);
	}

PUTCHAR(0xff,p);					// Options end
udp_len = p - udp_data;

output(udp_data,udp_len, NULL,0,HW_LAN);
}
#endif
