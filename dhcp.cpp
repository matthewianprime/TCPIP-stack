// dhcp.cpp
// Implementation of RFC2131. See also RFC2132 DHCP options.
// DHCP client and server
// nb. Parts of the DHCP implementation are in udp.cpp

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "udp.h"
#include "dhcp.h"
#include "utils.h"
#include "arp.h"

#ifdef DHCP

int32 IP_LEASE_EXPIRE_TIME =	60 * 60;	// 1 Second units. 1 hour
extern int32 transaction_id;

#ifdef DHCP_SERVER
// Allocated IP addresses stored in a simple array
// nb also use for RAS clients
struct DHCP_ADDRESS_LEASE
// Nb the ip addr is address_pool.first_ip_addr plus the array index
{
//int32 ip_addr;			// Leased IP address
int16 mac_addr[3];			// Unique client ID
int32 time_lease_expires;
int status;					// Offered or leased
};
DHCP_ADDRESS_LEASE dhcp_address[MAX_POOL_LEN];	// Default first entry
#endif

//--------------------------------------------------------------------------
void Cdhcp::OnMessage(int16 messg,int32 lParam,int32 wParam,int16 zParam)
{
switch(messg)
	{
	case MSG_DHCP_REQUEST_ADDR:
	// Act as DHCP client and
	// Request an IP address from a DHCP server
	break;
	}
}

#ifdef DHCP_SERVER
//--------------------------------------------------------------------------
void Cdhcp::addresspooltimeout(int32 currenttime)
// Called periodically to free timed-out ip address leases
// All time values are in seconds
{
int status;

currenttime = currenttime / 1000;		// Convert to seconds

for(int n=0; n < num_pool_addresses; n++)
	{
	status = dhcp_address[n].status;
	if(( status == OFFERED ) || ( status == LEASED ))
		{
		if( dhcp_address[n].time_lease_expires < currenttime )
			{
			if( status == OFFERED )
				{// Offered ip was not requested
				sprintf(msg,szDHCPReleaseOffered,address_pool.first_ip_addr + n);
				OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
				dhcp_address[n].status = AVAIL;
				}
			
			if( status == LEASED )
				{// Lease timed out
				sprintf(msg,szDHCPleasetimeout,address_pool.first_ip_addr + n);
				OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
				dhcp_address[n].status = TIMEDOUT;
				}
			}
		}
	}
}

//--------------------------------------------------------------------------
bool Cdhcp::pool_release_ip_address(int32 ip_addr,int16* mac_addr,int newstatus)
// Mark an address in the pool as free
// Use mac addr param if supplied, ELSE
// use ip addr param to find array entry
{
unsigned int n;

if(mac_addr != NULL)
	{
	// Find a previous or current ip assigned to this mac address
	for(n=0; n < num_pool_addresses; n++)
		{
		if(( dhcp_address[n].mac_addr[0] == mac_addr[0] ) &&
			( dhcp_address[n].mac_addr[1] == mac_addr[1] ) &&
			( dhcp_address[n].mac_addr[2] == mac_addr[2] ))
			{
			// Found requesters mac addr
			dhcp_address[n].status = newstatus;
			// Release the lease on this ip address
			return true;
			}
		}
	}
	
n = ip_addr - address_pool.first_ip_addr;
if(n >= num_pool_addresses)
	return false;

// Don't release a declined ip address
if(dhcp_address[n].status != DECLINED)
	dhcp_address[n].status = newstatus;

return true;
}

//--------------------------------------------------------------------------
int32 Cdhcp::pool_get_ip_address(int32 ip_addr,int16* mac_addr,int newstatus)
// Return an ip address from the ip address pool
// rfc 2131 para 4.3
// if mac_addr is specified return the matching ip, if not reallocated ELSE
// if ip_addr is specified, return that address if available ELSE
// issue a free address from the pool ELSE
// issue a timed out address from the pool ELSE
// return 0 if none available
{
unsigned int n;

if(mac_addr != NULL)
	{
	// Find a previous or current ip assigned to this mac address
	for(n=0; n < num_pool_addresses; n++)
		{
		if(( dhcp_address[n].mac_addr[0] == mac_addr[0] ) &&
			( dhcp_address[n].mac_addr[1] == mac_addr[1] ) &&
			( dhcp_address[n].mac_addr[2] == mac_addr[2] ))
			{
			// Found requesters mac addr
			if(dhcp_address[n].status != DECLINED)
				// Don't issue a declined address !
				{
				// ReIssue the lease on this ip address
				ip_addr = address_pool.first_ip_addr + n;
				goto done;
				}
			}
		}
	}

// No matching mac in pool.....

// Try to offer the requested ip address	
if(ip_addr > 0)
	{
	// Range check requested ip address
	n = ip_addr - address_pool.first_ip_addr;
	if(n >= num_pool_addresses)
		goto issue_any_pool_addr;

	if((dhcp_address[n].status == AVAIL) || (dhcp_address[n].status == TIMEDOUT))
		{
		ip_addr = address_pool.first_ip_addr + n;
		goto done;
		}
	}

issue_any_pool_addr:
// Cannot allocate requested ip address........

// Issue any pool address that is available
for(n=0; n < num_pool_addresses; n++)
	{
	if(dhcp_address[n].status == AVAIL)	
		{
		// We have a lease on this ip address
		ip_addr = address_pool.first_ip_addr + n;
		goto done;
		}
	}

// Issue any pool address that is timed out
for(n=0; n < num_pool_addresses; n++)
	{
	if(dhcp_address[n].status == TIMEDOUT)	
		{
		// We have a lease on this ip address
		ip_addr = address_pool.first_ip_addr + n;
		goto done;
		}
	}
	
return 0;

done:
dhcp_address[n].mac_addr[0] = mac_addr[0];
dhcp_address[n].mac_addr[1] = mac_addr[1];
dhcp_address[n].mac_addr[2] = mac_addr[2];
if(newstatus == OFFERED)
	// Offered status persists for 60 seconds
	dhcp_address[n].time_lease_expires = ( getTickCount() / 1000 ) + 60;
if(newstatus == LEASED)
	// Lease status persists for :-
	dhcp_address[n].time_lease_expires = IP_LEASE_EXPIRE_TIME + (getTickCount() / 1000);

dhcp_address[n].status = newstatus;

return ip_addr;
}
#endif

//--------------------------------------------------------------------------
void Cdhcp::init(int client,CProtocol_L3* pUDP,bool bAutoDelete)
// One time init, the DHCP instance is static
{
name = szDHCP;
outbuf = &packet_buf[UDP_DATA_OFFSET];
transport = pUDP;
autodelete = bAutoDelete;				// Delete instance when quit flag is set
quit = false;
//client_state = DHCP_INIT;

if(client)
	{// Init DHCP client
	return;
	}

#ifdef DHCP_SERVER
// Init DHCP server
// Initialise the ip address pool. Could just set to 0's
for(int n = 0;n < MAX_POOL_LEN;n++)
	{
	dhcp_address[n].mac_addr[0] = 0;
	dhcp_address[n].mac_addr[1] = 0;
	dhcp_address[n].mac_addr[2] = 0;
	dhcp_address[n].time_lease_expires = 0;
	dhcp_address[n].status = AVAIL;
	}

num_pool_addresses = 1 + address_pool.last_ip_addr - address_pool.first_ip_addr;
#endif

// Values from Inside TCP/IP book
T3_duration = IP_LEASE_EXPIRE_TIME;
T1_renewal = T3_duration / 2;
T2_rebinding = T3_duration /8 * 7;
}

//--------------------------------------------------------------------------
int Cdhcp::receive(unsigned char **rxdata,unsigned int *len,unsigned int)
// Process a received DHCP frame
{
int resp = ACKNOWLEDGE;
unsigned char *data = *rxdata;
unsigned char *p = data;
unsigned char* pudp;
int op,type,frametype;
int32 allocated_ip = 0;							// Will get from a cache
int32 requested_ip = 0;
int32 rcvd_xid;
int16 sender_ha[3];
unsigned char length,optionlen;

GETCHAR(op,p);									// BOOTP op code
p = data + DHCP_CHADDR_FIELD;					// Point at clients mac addr field of the UDP frame.
GETMAC(sender_ha,p);							// Get their MAC addr, for our response

p = data + DHCP_OPTIONSFIELD + DHCP_MAGIC_COOKIE_LEN;// Point at options field of the UDP frame.

requested_ip = 0;
frametype = 0;
optionlen = 0;

// Parse the options field
do
	{
	GETCHAR(type,p);							// Option type field
	GETCHAR(length,p);							// Length field

	switch(type)
		{
		case 0xff:
		case 0:
		// End of options or parsing error
		break;

		case 0x32:
		// Requested ip addr
		GETLONG(requested_ip,p);				// They want this ip addr
		break;
		
		case 53:
		// DHCP frame type
		GETCHAR(frametype,p);					//
		break;
				
		default:
		// Some other option
		INCPTR(length,p);						// Skip option
		break;
		}

//	sprintf(msg,"DHCP option 0x%X len %u",type,length);
//	OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);	
	}while((type != 0xff ) && (type != 0 ) && ((optionlen += length) <= *len));

switch(frametype)
	{
	case DHCP_OFFER:
	// We must be acting as a DHCP client, This is the response to our DHCPREQUEST
	// NB, before accepting the offer, ARP for offered IP to check it is unique.
	// ( If it is not, send DHCPDECLINE )
	resp = NORESPONSE;
	if(client_state != DHCP_SELECTING)
		break;

	// Get the ip address and netmask..
	p = data;
	// Verify this is a response to our request...
	INCPTR(4,p);
	GETLONG(rcvd_xid,p);
	if( rcvd_xid != transaction_id)
		{// Not ours..
		sprintf(msg,szDHCPxid_nomatch,rcvd_xid ,transaction_id);
		OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
		break;
		}
		
	INCPTR(8,p);
	GETLONG(allocated_ip,p);

	// Look for recognised options..
	p = data + DHCP_OPTIONSFIELD+DHCP_MAGIC_COOKIE_LEN;// Point at options field of the UDP frame.
	pudp=p;
	
	do
	{
	GETCHAR(type,p);	
	GETCHAR(length,p);							// Length field
	switch(type)
		{
		case 0:
		// Pad option
		break;
		case 255:
		// End option
		break;
		case 0x36:
		// DHCP server IP
		GETLONG(lan[UNSAVED].dhcp_server,p);
		sprintf(msg,szDHCPserver_ip,lan[UNSAVED].dhcp_server);
		OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
		break;
		
		default:
		INCPTR(length,p);						// Skip option
		break;
		}
	}while((type!= 255) && (( p-pudp) < *len));
	
	// If another offer is received, these settings are overwritten.
	// The last offer is accepted at the expiry of the discovery phase timer.
	// DISCOVERED IP addr is ok, ask for the lease, expect ACK response
	client_state = DHCP_REQUESTING;
	
	// Accept the offer immediately. (Win98 does this)
	// Want from server:ip, mask,gateway
	msg[0]=0x3;msg[1]=1;msg[2]=6;msg[3]=0;
	((Cudp*)transport)->output_dhcp(DHCP_REQUEST,lan[UNSAVED].dhcp_server,allocated_ip,(char*)msg);

	lan[ACTIVE].ip_addr = allocated_ip;
	// Set a retry timer incase DHCP server does not respond
	PostMessage(this,MSG_TIMER,lan[UNSAVED].dhcp_server,lan[UNSAVED].ip_addr,0,2000L);

	sprintf(msg,szDHCPoffer,allocated_ip,lan[UNSAVED].dhcp_server);
	break;
	
#ifdef DHCP_SERVER
	case DHCP_DISCOVER:
	allocated_ip = pool_get_ip_address(requested_ip,sender_ha,OFFERED);
	sprintf(msg,szDHCPdiscover,requested_ip,allocated_ip);
	
	if(allocated_ip == 0)
		{
		sprintf(msg,szDHCPnopool,requested_ip);
		resp = NORESPONSE;
		break;
		}
		
	OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);

	// Copy the response over the request
	p = data;
	PUTCHAR(2,p);						// BOOTREPLY

	INCPTR(9,p);						// Skip to FLAGS field. 
	PUTCHAR(0,p);						// Flags
	INCPTR(5,p);						// Skip to YIADDR field. 
	PUTLONG(allocated_ip,p);
	PUTLONG(0L,p);						// Our ip addr (Microsoft send 0 here)
	PUTLONG(0L,p);						// ip addr of router running relay agent (?)

	p = data + DHCP_OPTIONSFIELD+DHCP_MAGIC_COOKIE_LEN;// Point at options field of the UDP frame.

	PUTCHAR(53,p);						// Frame type option
	PUTCHAR(1,p);
	if(allocated_ip != 0)
		// Offer an ip addr
		PUTCHAR(DHCP_OFFER,p)
	else
		// Can't allocate any ip addr
		PUTCHAR(DHCP_NAK,p);
			
	PUTCHAR(1,p);						// Subnet mask option
	PUTCHAR(4,p);
	PUTLONG(lan[ACTIVE].netmask,p);

	PUTCHAR(0x3A,p);					// Renewal time option
	PUTCHAR(4,p);
	PUTLONG(0x1fa40L,p);

	PUTCHAR(0x3B,p);					// Rebinding time option
	PUTCHAR(4,p);
	PUTLONG(0x375F0L,p);

	PUTCHAR(0x33,p);					// Lease time option
	PUTCHAR(4,p);
	PUTLONG(0x3f480L,p);

	PUTCHAR(0x36,p);					// Server ip option
	PUTCHAR(4,p);
	PUTLONG(lan[ACTIVE].ip_addr,p);

	PUTCHAR(255,p);						// End options

	// Send as broadcast
	transport->dest_addr = IP_ADDR_BROADCAST;
	break;

	case DHCP_REQUEST:
	allocated_ip = pool_get_ip_address(requested_ip,sender_ha,LEASED);
	sprintf(msg,szDHCPrequest,requested_ip,allocated_ip);
	OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);

	// Copy the response over the request
	p = data;
	PUTCHAR(2,p);						// BOOTREPLY

	INCPTR(9,p);						// Skip to FLAGS field. 
	PUTCHAR(0,p);						// Flags
	INCPTR(5,p);						// Skip to YIADDR field. 
	PUTLONG(allocated_ip,p);
	PUTLONG(0L,p);						// Our ip addr (Microsoft send 0 here)
	PUTLONG(0L,p);						// ip addr of router running relay agent (?)

	p = data + DHCP_OPTIONSFIELD+DHCP_MAGIC_COOKIE_LEN;// Point at options field of the UDP frame.

	PUTCHAR(53,p);						// Frame type option
	PUTCHAR(1,p);	
	if(allocated_ip != 0)
		// Offer an ip addr
		PUTCHAR(DHCP_ACK,p)
	else
		// Can't allocate any ip addr
		PUTCHAR(DHCP_NAK,p);

	PUTCHAR(1,p);						// Subnet mask option
	PUTCHAR(4,p);
	PUTLONG(lan[ACTIVE].netmask,p);

	PUTCHAR(0x3A,p);					// Renewal time option
	PUTCHAR(4,p);
	PUTLONG(T1_renewal,p);

	PUTCHAR(0x3B,p);					// Rebinding time option
	PUTCHAR(4,p);
	PUTLONG(T2_rebinding,p);

	PUTCHAR(0x33,p);					// Lease time option
	PUTCHAR(4,p);
	PUTLONG(T3_duration,p);

	PUTCHAR(0x36,p);					// Server ip option
	PUTCHAR(4,p);
	PUTLONG(lan[ACTIVE].ip_addr,p);

	PUTCHAR(255,p);						// End options

	// Send as broadcast
	transport->dest_addr = IP_ADDR_BROADCAST;
	break;
	
	case DHCP_DECLINE:
	// Client doesn't want this address. It may be a duplicate !
	p = data + 28;								// Point at chaddr, client ip addr
	GETMAC(sender_ha,p);
	pool_release_ip_address(0,sender_ha,DECLINED);
	sprintf(msg,szDHCPdecline,sender_ha);
	OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
	break;
#endif // DHCP_SERVER

	case DHCP_ACK:
	resp = NORESPONSE;

	if(!((client_state == DHCP_REQUESTING)||(client_state == DHCP_RENEWING)))
		break;

	// ACK to our DHCP client
	OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)szDHCPack);

	p = data;
	// Verify this is a response to our request...
	INCPTR(4,p);
	GETLONG(rcvd_xid,p);
	if( rcvd_xid != transaction_id)
		{// Not ours..
		sprintf(msg,szDHCPxid_nomatch,rcvd_xid ,transaction_id);
		OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
		break;
		}

	// Get the ip address..
	INCPTR(8,p);
	GETLONG(lan[UNSAVED].ip_addr,p);

	// Look for recognised options..
	p = data + DHCP_OPTIONSFIELD+DHCP_MAGIC_COOKIE_LEN;// Point at options field of the UDP frame.
	pudp=p;
	
	do
	{
	GETCHAR(type,p);	
	GETCHAR(length,p);							// Length field
	
	switch(type)
		{
		case 0:
		// Pad option
		break;
		
		case 255:
		// End option
		break;
						
		case 1:
		// Subnet mask
		GETLONG(lan[UNSAVED].netmask,p);
		sprintf(msg,szDHCPnetmask,lan[UNSAVED].netmask);
		OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
		break;

		case 3:
		// Gateway option
		GETLONG(lan[UNSAVED].gateway,p);
		sprintf(msg,szDHCPgateway,lan[UNSAVED].gateway);
		OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
		break;

		case 0x38:
		// TCP keepalive
		INCPTR(length,p);						// Skip option
		sprintf(msg,"TCP keepalive option. Length=%u",length);
		OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
		break;
		
		case 0x33:
		// IP lease period (in seconds)
		GETLONG(lan[UNSAVED].dhcp_leasetime,p);
		if(lan[UNSAVED].dhcp_leasetime == 0XFFFFFFFF )
			// Infinite lease
			lan[UNSAVED].dhcp_leasetime = 0;
			
		sprintf(msg,szDHCPlease_time,lan[UNSAVED].dhcp_leasetime);
		OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
		lan[UNSAVED].dhcp_leasetime = lan[UNSAVED].dhcp_leasetime * 1000; // convert to ms
		break;

		case 0x36:
		// DHCP server IP
		GETLONG(lan[UNSAVED].dhcp_server,p);
		sprintf(msg,szDHCPserver_ip,lan[UNSAVED].dhcp_server);
		OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
		break;
		
		default:
		INCPTR(length,p);						// Skip option
		break;
		}
	}while((type!= 255) && (( p-pudp) < *len));
	
	lan[UNSAVED].subnet = lan[UNSAVED].ip_addr & lan[UNSAVED].netmask;
	
	// Make these settings active
	memcpy(&lan[ACTIVE],&lan[UNSAVED],sizeof(lanport));	
	
	sprintf(msg,szDHCPaccept,lan[UNSAVED].ip_addr,lan[UNSAVED].dhcp_server);
	OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
	
	// Delete any previous lease renewal timer(s)
	router->untimeout((CObj*)ip_protocols[1],MSG_DHCP_RENEW_ADDR);
	router->untimeout((CObj*)ip_protocols[1],MSG_DHCP_T2_TIMER);
	router->untimeout((CObj*)ip_protocols[1],MSG_DHCP_T3_TIMER);
	if(lan[UNSAVED].dhcp_leasetime != 0)
		{
		// Set time for next lease renewal (T1)
		PostMessage((CObj*)ip_protocols[1],MSG_DHCP_RENEW_ADDR,lan[ACTIVE].dhcp_server,lan[ACTIVE].ip_addr,0,lan[ACTIVE].dhcp_leasetime/2);
		// Set T2 DHCP server unavailable timer
		PostMessage((CObj*)ip_protocols[1],MSG_DHCP_T2_TIMER,0,0,0,lan[ACTIVE].dhcp_leasetime/7 * 8);
		// Set T3 end of lease timer
		PostMessage((CObj*)ip_protocols[1],MSG_DHCP_T3_TIMER,0,0,0,lan[ACTIVE].dhcp_leasetime);
		}
		
	client_state = DHCP_BOUND;
	break;

	case DHCP_NAK:
	resp = NORESPONSE;
	if((client_state == DHCP_REQUESTING)||(client_state == DHCP_RENEWING))
		client_state = DHCP_INIT;	
//	OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)"DHCP received NAK");
	break;

	case DHCP_RELEASE:
	resp = NORESPONSE;
	p = data + 28;								// Point at chaddr, client ip addr
	GETMAC(sender_ha,p);
#ifdef DHCP_SERVER
	pool_release_ip_address(0,sender_ha);
	sprintf(msg,szDHCPrelease,sender_ha);
	OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
#endif	
	client_state = DHCP_INIT;
	break;

	default:
	resp = NORESPONSE;
//	sprintf(msg,"DHCP unknown type %u",frametype);
//	OutputDebugString(LOG_INFO,LOG_DHCP,(const char*)msg);
	break;
	}

return resp;
}

#endif // DHCP
