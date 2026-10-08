// ip.cpp
// IP base level transport protocol

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#ifdef ROUTER
#include "C:\DataDir\Adsp21xx\CTA\VC5410A\pmRun\router\ppp\ppplink.h"
#endif
#include "udp.h"
#include "ip.h"
#include "icmp.h"
#include "dhcp.h"
//#include "mlcp.h"
#include "arp.h"
#include "utils.h"
#include "lan.h"

int addroute(int32 ip_addr,int32 ip_mask,int32 gateway,int HW_IF);
int deleteroute(int32 ip_addr);
int16 ip_id = 500;								// Unique ID for each frame. Any number is OK

//--------------------------------------------------------------------------
void CipV4::init(int)
{
protocol =  PPP_IP;
name = szIP; 
enabled_flag=true;


for(int n=0;n < MAXL3CLIENTS;n++)
	ip_protocols[n] = NULL;

// ip_protocols is an array of link protocols. 
// All other entries are added and removed dynamically

// Need one IP/ICMP entry at startup for router advertisment.
ip_protocols[0] = (CProtocol_L3*)icmp;
ip_protocols[0]->init(false);
ip_protocols[0]->autodelete = false;			// Don't delete on idle - static instance

#ifdef DHCP
// Need a default entry for UDP/DHCP
ip_protocols[1] = (CProtocol_L3*)new Cudp;		// UDP protocol
ip_protocols[1]->init(UDP_DHCP_LISTEN);			// DHCP server listen port. Static dhcp instance will be used
ip_protocols[1]->autodelete = false;			// Don't delete on idle - static instance
#endif
}

// route() return values. Should not clash with HW_IF constants !
#define ROUTE_FRAME_ERR				NUM_UARTS + 91
#define ROUTE_FRAME_ROUTED			NUM_UARTS + 92
#define ROUTE_FRAME_DISCARDED		NUM_UARTS + 93
#define ROUTE_LOCAL_ADDR			NUM_UARTS + 94	// For router
#define ROUTE_RAS_ADDR				NUM_UARTS + 95	// For dial-in client
#define ROUTE_FRAME_DISCARDED_CLI	NUM_UARTS + 96  // No access to command i/f from unknown ip addr
#define ROUTE_BROADCAST_ADDR		NUM_UARTS + 97	// Received broadcast IP addr
#define ROUTE_MULTICAST_ADDR		NUM_UARTS + 98	// Received broadcast IP addr

//--------------------------------------------------------------------------
void CipV4::input(int HW_IF,unsigned char *in_buf,int len)
// IP frame received on interface HW_IF (See interface definitions in router.h)
{
#ifdef NAT
nat->input(HW_IF,*in_buf,len);
#endif

// Route sends the frame to the interface dictated by the routing table, 
// Or returns ROUTE_LOCAL_ADDR, meaning the frame is addressed to the router itself
switch(route(in_buf,len,HW_IF))
	{
	case ROUTE_FRAME_DISCARDED_CLI:
	OutputDebugString(LOG_INFO,LOG_SYSTEM,szIPCallBar );
	case ROUTE_FRAME_ERR:			// Bad frame
	case ROUTE_FRAME_ROUTED:		// Frame has been forwarded
	case ROUTE_FRAME_DISCARDED:
		return;
	case ROUTE_RAS_ADDR:
		return;
		
//	case ROUTE_BROADCAST_ADDR:
//	case ROUTE_MULTICAST_ADDR:
//	case ROUTE_LOCAL_ADDR:			// Fall through
//		break;
	default:
		break;
	};

// IP datagram is either for this router or the broadcast IP addr. Pass to correct protocol stack
unsigned char* p;
int ip_total_len;
int32 src_addr,dest_addr;
char protocol;
int created_new_protocol=0;

// Extract some vars from the IP header for higher layer protocols.
p = in_buf + 2;
//INCPTR(2,p);				// Skip version and TOS
GETSHORT(ip_total_len,p);
//INCPTR(2,p);				// Skip id
//INCPTR(3,p);				// Skip offset and TTL
INCPTR(5,p);				// 
GETCHAR(protocol,p);		// TCP or UDP protocols are handled
INCPTR(2,p);				// Skip cksum
GETLONG(src_addr,p);		// Their IP addr
GETLONG(dest_addr,p);		// Our IP addr, or maybe a broadcast

// Find the upper layer protocol (client) that owns this frame, or attempt to create one.
try_again:
CProtocol_L3 *protp;
for (int i = 0;i < MAXL3CLIENTS ; ++i) 
	{
	if((protp = ip_protocols[i]) != NULL)
		if ((protocol == protp->protocol) && protp->enabled_flag) 
			{
		    if(protp->input((void*)p,ip_total_len-IP_HEADERLEN,src_addr,dest_addr,HW_IF))
				{
				traceout.suppress = false;	// If this was trace data (eg TCP ACK) the trace was suppressed
				return;
				}
			}
	}

// No existing client protocol accepts the datagram.
// This must be a new client session so create another protocol instance
// to handle it.
if(created_new_protocol++ == 0)
	{
	// Only attempt to create the protocol once !
	if(CreateTransport(protocol,HW_IF,0,0,src_addr))
		goto try_again;
	}
}

//--------------------------------------------------------------------------
void output_ip(int16 unused,char protocol,int32 dest_ip_addr,unsigned char *outp_buf, unsigned int datalen, unsigned int HW_IF)
// Build an IP frame and send it via interface HW_IF
// Add IP header to frame.Inside TCP/IP p.266
{
// Where to assemble the datagram. Leave room for LAN/ML/PPP headers to be prepended
unsigned char *p,*pcksum;
int16 *pdest_mac;
int16 cksum;

if(HW_IF == HW_LAN)
	p = packet_buf + LAN_HDRLEN;
else
	p = packet_buf;					

unsigned char *outp = p;

// Copy upper layer protocol data (outp_buf) into datagram (outp) if necessary
if(outp_buf != outp + IP_HEADERLEN)
	memmove(outp + IP_HEADERLEN, outp_buf, datalen);

// Prepend IP header to frame
PUTCHAR(0x45,outp);								// Protocol 0x45 = IP, v4
PUTCHAR(0,outp);
PUTSHORT(datalen + IP_HEADERLEN,outp);			// Don't include the PPP header in the length field
PUTSHORT(ip_id++,outp);							// unique number for each frame (global var)
PUTSHORT(0x4000,outp);							// Allow fragmentation. TODO add to MP
PUTCHAR(0x80,outp);								// TTL
PUTCHAR(protocol,outp);							// IP Protocol eg TCP, UDP
pcksum = outp;
PUTSHORT(0,outp);								// Reserved for header checksum. Should be 0 for FCS calculation !
PUTLONG(lan[ACTIVE].ip_addr,outp)				// Our IP
PUTLONG(dest_ip_addr,outp);						// Their ip

// Place header options here if any.NB adjust IP_HEADERLEN to match !!

cksum = CHECKSUM(p,IP_HEADERLEN);
PUTSHORT(cksum,pcksum);			// Header checksum

if(LOG_IP & traceout.module)
	{
	sprintf(msg,szTx_IP,datalen + IP_HEADERLEN,dest_ip_addr,lan[ACTIVE].ip_addr);
	OutputDebugString(LOG_DEBUG,LOG_IP,(const char*)msg);
	}

if(HW_IF == HW_LAN)
	{
	pdest_mac = NULL;
	if((dest_ip_addr == IP_ADDR_MULTICAST) ||
		(dest_ip_addr == IP_ADDR_BROADCAST))
		pdest_mac = MAC_BROADCAST;

	plan->output_lan(p,datalen + IP_HEADERLEN,ETHERTYPE_IPV4,mac_addr,pdest_mac,dest_ip_addr);
	return;
	}

#ifndef SEB
//if(ppp_if[HW_IF].multilink)
//	mlcp->out(p,datalen + IP_HEADERLEN, PPP_IP,HW_IF);
	ppp->output_ppp(HW_IF, p, datalen + IP_HEADERLEN, PPP_IP, true);
#endif

sprintf(msg,szTx_IP_no_HW_IF,dest_ip_addr);
OutputDebugString(LOG_DEBUG,LOG_IP,(const char*)msg);
}

//--------------------------------------------------------------------------
int CipV4::CreateTransport(char protocol,int HW_IF,int tcp_port,CClient* ptrClient,int32 their_ip)
// Purpose: Create a transport protocol and add to ip_protocols[n] arry.
// Returns: reference to class or 0 if create failed
// Nb check memory before calling new as exception handling is not supported
{
int n;

// Find a free location in the protocols array
for(n=1; n < MAXL3CLIENTS;n++)
	{
	if(ip_protocols[n] == NULL)
		break;
	}

int16 free_mem = freemem();
if(free_mem < ( UART_BUFLEN + 100 ))			// Space for TCPmodem instance
	{
	// Unable to create protocol. Out of memory
	sprintf(msg,szNotCreated,protocol);
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
	return NULL;
	}
	
// There *can be* a limit to the number of protocol entries we support..
if(n >= MAXL3CLIENTS)
	{
	sprintf(msg,szConnectLimit,their_ip,protocol);
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
	return NULL;
	}

// Create the protocol, based on the protocol field extracted from the IP frame
switch(protocol)
	{
	case IP_TCP:
	if(free_mem > (sizeof(CTcp) + 100))
	   	ip_protocols[n] = (CProtocol_L3*)new CTcp;		// TCP protocol
	break;

#ifdef TFTP
	case IP_UDP:
	if(free_mem > (sizeof(Cudp) + 100))
		ip_protocols[n] = (CProtocol_L3*)new Cudp;		// UDP protocol
	break;
#endif

	case IP_ICMP:
	// Ping. Only want one instance of ICMP as there is no higher layer protocol supported (yet...)
	// It's created at ip.init().
	OutputDebugString(LOG_ERR,LOG_IP,(const char*)szERRCreating);
	return NULL;

	default:
	// Unsupported protocol
	return NULL;
	}

if(ip_protocols[n] == NULL)
	{
	// Unable to create protocol. Probably out of memory
	sprintf(msg,szNotCreated,protocol);
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
	return NULL;
	}
	
ip_protocols[n]->their_ip=their_ip;							// Our IP Required for TCP/UDP pseudoheader generation
ip_protocols[n]->time = getTickCount();						// Set before calling init()
ip_protocols[n]->init(tcp_port);							// Client port number
ip_protocols[n]->HW_IF = HW_IF;
ip_protocols[n]->pClient = ptrClient;						// If non-NULL, A Client is creating TCP transport. Outgoing call required

if(ptrClient != NULL)
	sprintf(msg,"Created protocol %s port %u for %s",ip_protocols[n]->name,tcp_port,ptrClient->name);
else
	sprintf(msg,"Created protocol %s",ip_protocols[n]->name);
OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);

return n;													// Return the index number
}

//--------------------------------------------------------------------------
int CipV4::route(unsigned char *in_buf, int16 len, int IF_NUM)
// Purpose: 
// Verify integrity of IP frame
// From the destination IP address, determine the destination interface on which to forward the frame
// Parameters:
// IF_NUM is the interface on which the frame was received
// Returns:
// A ROUTE_ constant indicating how the frame was handled
{
// Extract some vars from the IP header for higher layer protocols.
unsigned char *p = in_buf;
char version;
int32 src_addr, dest_addr;
int16 ip_total_len;
char ttl;

// Extract some vars from the IP header
// nb This header is reused
p = in_buf;
GETCHAR(version,p);
INCPTR(1,p);				// Skip TOS field
GETSHORT(ip_total_len,p);
INCPTR(4,p);				// Skip id, offset fields
GETCHAR(ttl,p);
//GETSHORT(cksum,p);		// TODO verify checksum ? Probably not...
//PUTSHORT(0,P);			// 
//INCPTR(1,p);				// Skip cksum and protocol fields
INCPTR(3,p);				// Skip cksum and protocol fields
GETLONG(src_addr,p);
GETLONG(dest_addr,p);


// Don't trace rcvd frames to our own ip. 
// Generates trace data to syslog client !
//sprintf(msg,szRx_IP,ip_total_len,src_addr,dest_addr);
//OutputDebugString(LOG_DEBUG,LOG_IP,(const char*)msg);


if((version & 0xf0) != 0x40)
	{// Not IP Version 4 frame. Discard
	// TODO support ipV6 routing
//	headerlen = ( version & 0x0f ) * 4;
	sprintf(msg,szDiscardingFrame,version & 0x0f,src_addr);
	OutputDebugString(LOG_NOTICE,LOG_IP,(const char*)msg);
	return ROUTE_FRAME_ERR;
	}

if(ip_total_len > len)
	{// Bad length. Discard , inform sender with ICMP
	icmp->output(in_buf,ICMP_PARAM_PROBLEM,2,src_addr,0);
	sprintf(msg,szDiscardingFrame_len,src_addr,dest_addr);
	OutputDebugString(LOG_NOTICE,LOG_IP,(const char*)msg);
	return ROUTE_FRAME_ERR;
	}

if(ttl-- == 0)
	{// TTL expired. Discard, inform sender with ICMP
	icmp->output(in_buf,ICMP_TTL,0,src_addr,0);
	
	sprintf(msg,szDiscardingFrame_TTL,src_addr,dest_addr);
	OutputDebugString(LOG_NOTICE,LOG_IP,(const char*)msg);
	return ROUTE_FRAME_DISCARDED;
	}

// If we get here the IP frame is considered to be OK
#ifdef SEB

if(dest_addr == lan[ACTIVE].ip_addr)
	{
	// This datagram is addressed to the SEB. Pass on to matching higher layer protocol
	// First check the "CLI"
	if((src_addr >= lan[ACTIVE].from_ip_addr1 ) && (src_addr <= lan[ACTIVE].to_ip_addr1))
	{
//	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"ROUTE_LOCAL_ADDR 1");
		return ROUTE_LOCAL_ADDR;
}
	if((src_addr >= lan[ACTIVE].from_ip_addr2 ) && (src_addr <= lan[ACTIVE].to_ip_addr2))
	{
//	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"ROUTE_LOCAL_ADDR 2");
		return ROUTE_LOCAL_ADDR;
}		
	return ROUTE_FRAME_DISCARDED_CLI;		
	}

// Trace IP frames NOT to the SEB ( or CLI discarded frames )
//sprintf(msg,szRx_IP,ip_total_len,src_addr,dest_addr);
//OutputDebugString(LOG_DEBUG,LOG_IP,(const char*)msg);

	
if( dest_addr == IP_ADDR_BROADCAST )
	{
//	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"IP BROADCAST");
	return ROUTE_BROADCAST_ADDR;
	}
#define MULTICAST_MASK 0x000000FFL
if( dest_addr == (lan[ACTIVE].ip_addr | MULTICAST_MASK ))
	{
//	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"IP MULTICAST1");
	return ROUTE_MULTICAST_ADDR;
	}

if( (lan[ACTIVE].ip_addr == 0) &&
	((dest_addr & MULTICAST_MASK ) == MULTICAST_MASK))
	{
//	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"IP MULTICAST2");
	return ROUTE_MULTICAST_ADDR;
	}

//OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"DISCARDED");
return ROUTE_FRAME_DISCARDED;
}

#else

int PPP_IF;
int16 cksum;
int headerlen = IP_HEADERLEN;			// For ipV4 packets

sprintf(msg,szIP_info1,src_addr,dest_addr);
OutputDebugString(LOG_DEBUG,LOG_IP,(const char*)msg);

// Perform the actual routing function
IF_NUM = select_interface(dest_addr);

sprintf(msg,szIPselectedInterface,HW_2ascii(IF_NUM));
OutputDebugString(LOG_INFO,LOG_IP,(const char*)msg);

// Recalc header checksum
// TODO only needed if frame is to be forwarded
// TODO could simply "adjust" checksum since I only decrement TTL
in_buf[10] = 0;	
in_buf[11] = 0;	
cksum = CHECKSUM(in_buf,headerlen);
in_buf[11] = 0x00ff & cksum;			// Header checksum
in_buf[10] = 0x00ff & (cksum >> 8);		// Header checksum

// Send the frame
switch(IF_NUM)
	{
	case HW_LAN:
		// Will need mac address to send frame to LAN host.
		// Use dest_ip param with ARP if not in our ARP cache.
		plan->output_lan(in_buf,ip_total_len,ETHERTYPE_IPV4,lan[ACTIVE].mac_addr,NULL,dest_addr);
		return ROUTE_FRAME_ROUTED; // datagram routed;

	case ROUTE_LOCAL_ADDR:
		// Frame is for this router
		return ROUTE_LOCAL_ADDR;

	case HW_NONE:
		// Not in routing table.Discard and inform sender with ICMP
		icmp->output(in_buf,ICMP_DEST_UNREACHABLE,NET_UNREACHABLE,src_addr,0);
		sprintf(msg,szDiscardingFrame_destn,src_addr,dest_addr);
		OutputDebugString(LOG_NOTICE,LOG_IP,(const char*)msg);
		return ROUTE_FRAME_DISCARDED;

	default:
		// RAS case
		PPP_IF = 0;/*IF_NUM;*/
//		sprintf(msg,szIP_sendingPPP,PPP_IF,HW_2ascii(ppp_if[PPP_IF].HW_IF));
//		OutputDebugString(LOG_DEBUG,LOG_IP,(const char*)msg);
		ppp->output_ppp(PPP_IF, in_buf, ip_total_len, PPP_IP, true);
		return ROUTE_FRAME_ROUTED; // datagram routed;
	}
}
#endif

#ifdef SEB
int addroute(int32 ip_addr,int32 ip_mask,int32 gateway,int HW_IF)
{return 0;}
int deleteroute(int32 ip_addr)
{return 0;}
#else
//--------------------------------------------------------------------------
int addroute(int32 ip_addr,int32 ip_mask,int32 gateway,int HW_IF)
// Add a route to the routing table
{
int n;
bool bAdded = false;

if(!((ip_addr & ip_mask) == ip_addr) && (HW_IF >= HW_DTE0) && (HW_IF <= HW_LAN))
	return 0;

// Check for conflicting routes
for(n=0; n < RTABLE_SIZE; n++)
	{
	if(rtable[n].dest_net == ip_addr)
		// Duplicate route !
		return 0;
	}

// Add the route to the routing table
for(n=0; n < RTABLE_SIZE; n++)
	{
	if(rtable[n].dest_net == 0)
		{
		// Available entry found
		rtable[n].dest_net = ip_addr;
		rtable[n].dest_mask = ip_mask;
		rtable[n].gateway = gateway;
		rtable[n].if_num = HW_IF;
		bAdded=true;
		break;
		}
	}

if(bAdded)
	{
	// The "route number" (table entry no.) 
	sprintf(msg,szRouteAdded,n,ip_addr);
	OutputDebugString(LOG_INFO,LOG_IP,(const char*)msg);
	}

return 1;
}

//--------------------------------------------------------------------------
int deleteroute(int32 ip_addr)
// Delete a route from the routing table
{
int numdeleted =0;

for(int n=0; n < RTABLE_SIZE; n++)
	{
	if(rtable[n].dest_net == ip_addr)
		{
		// Specified entry found
		rtable[n].dest_net = 0;
		numdeleted++;
		break;
		}
	}

if(numdeleted)
	{
	sprintf(msg,szRouteDeleted,numdeleted);
	OutputDebugString(LOG_DEBUG,LOG_IP,(const char*)msg);
	}

return numdeleted;
}

//--------------------------------------------------------------------------
int CipV4::select_interface(int32 dest_addr)
// Purpose:
// The Routing function
// ** Bring up an ISDN demand dial interface if necessary **
// Returns:
// The destination interface (HW_xxx) for a given IP address
// ... or ROUTE_LOCAL_ADDR if dest_addr is the routers own LAN address
{
int IF_NUM = HW_NONE;	// Destination interface for frame
int n=0;

// Is the destination address on our subnet ?
if((dest_addr & lan[ACTIVE].netmask) == (lan[ACTIVE].ip_addr & lan[ACTIVE].netmask))
	{
	// Yes.
	// Routers address
	if(dest_addr == lan[ACTIVE].ip_addr)
		// This datagram is addressed to the router. Pass on to matching higher layer protocol
		// TODO extend for NAT
		return ROUTE_LOCAL_ADDR;

	// This datagram is NOT addressed to the router

	// Is the IP address for a Dial-in client ?
	// Nb. This relies on a continuous range of IP addrs in the pool.
	if((dest_addr >= ip_pool[0].ip_addr) &&
		(dest_addr <= ip_pool[NUM_PPP_LINKS-1].ip_addr))
			{
			// Fall thru to find the RAS interface in the 
			// routing table.
			// TODO what ? Skip entry 0 which would match the RAS client 
			sprintf(msg,szRasClientIP,dest_addr);
			OutputDebugString(LOG_DEBUG,LOG_IP,(const char*)msg);
			n=1;
			goto search;
			}
	}

n=0;		// Search the routing table starting from entry 0

search:

// Search the routing table.
// Look for the destination net in the routing table.
// Bring up ISDN demand dial interface if necessary
for(; n < RTABLE_SIZE ;n++)
	{
	if((dest_addr & rtable[n].dest_mask) == rtable[n].dest_net)
		{
		// Found a route in the routing table
		// dest_if is a HW_ port number:
		// A. HW_LAN -> route via LAN
		// B. >HW_LAN -> use interface ppp_if[dest_if]
		
		IF_NUM = rtable[n].if_num;
		
		sprintf(msg,szIPFoundAddress,dest_addr,n);
		OutputDebugString(LOG_DEBUG,LOG_IP,(const char*)msg);
		
		return IF_NUM;
		}
	}

return IF_NUM;
}

#endif // SEB

#pragma CODE_SECTION("sectionFastCode");
//--------------------------------------------------------------------------
int16 CHECKSUM(unsigned char *dgram,int len)
// CRC16 calc. Add bytes in pairs to a 32-bit accumulator.
// Texas:
// Typecasting a char string to int16 produces pointer to an
// int16 with the top 8 bits set to 0.
// Consequently this is a character operation not an int16 operation
// Note ~ NOT operator reverses the state of each bit.
{
int32 sum=0;
unsigned char *cptr = (unsigned char*)dgram;
int16 chsum16 = 0;

while(len >1)
	{
	chsum16 = (*cptr++);
	chsum16 <<= 8;
	chsum16 += (*cptr++);

	sum += (int32)chsum16;
	len -= 2;
	}
	
if(len == 1)				
	{
	chsum16 = (*cptr);
	sum += (int32)chsum16 << 8;
	}

chsum16 = (int16) (sum>>16);
sum = sum & 0xffffL;
sum += (int32)chsum16;			// If sum was 0xffff it will exceed a 16 bit value. Next 2 lines deal with this correctly

chsum16 = (int16) (sum>>16);
sum += (int32)chsum16;

chsum16=(int16)sum;
chsum16=~chsum16;
return chsum16;
}

/*
// Code to debug checksum calculation failure
// Tests all possible checksum values.
char t[42] ={0x0a,0x00,0x00,0x03,	\
			0x0a,0x00,0x00,0x01,	\
			0x00,0x06,0x00,0x1e,	\
			0x07,0xd0,0x07,0xd0,	\
			0x00,0x00,0x35,0x6f,	\
			0x00,0xd2,0xa8,0xdc,	\
			0x50,0x18,0x04,0x00,	\
			0x00,0x00,0x00,0x00,	\
			0x69,0x00,0x6e,0x00,	\
			0x74,0x00,0x00,0x00,	\
			0x61,0x00};

char *cksum = &t[28];
char *data_lo = &t[0];
char *data_hi = &t[1];
char ch_data_lo=0;
char ch_data_hi=0;

for(ch_data_hi=0;ch_data_hi <= 255;ch_data_hi++)
{
ch_data_lo = NULL;
*(data_lo) = NULL;
*(data_hi) = ch_data_hi;
	
for(ch_data_lo=0;ch_data_lo <= 255;ch_data_lo++)
	{
	// Calculate checksum for test frame
	int16 iRes = CHECKSUM((unsigned char*)t,42);

	// Insert the checksum into the test frame.
	// Running CHECKSUM on the resullting frame should yield "0"
	*(cksum+1) = (char)iRes & 0xff;
	*(cksum) = (char(iRes >>8)); 
	iRes = CHECKSUM((unsigned char*)t,42);

	if(iRes != 0)
		{
		sprintf(msg,"\r\niRes = 0x%04X\r\n",iRes);
		serialout((char*)msg,HW_DTE0);
		hexdump((unsigned char*)t,42,HW_DTE0);
		goto end;
		}

	*cksum = NULL;
	*(cksum+1) = NULL;
	
	*(data_lo) = ch_data_lo;
	}
}
end:
*/

