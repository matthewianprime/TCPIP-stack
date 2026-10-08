// arp.cpp
// Send/receive ARP frames, and the ARP cache

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "arp.h"
#include "lan.h"
#include "utils.h"

ARP_CACHE arp_cache[ARP_CACHELEN];	// The ARP cache
int deletearpcacheentry(int32 ip_addr);


//--------------------------------------------------------------------------
void Carp::init(int unit)
{
protocol = ETHERTYPE_ARP;		// 0x806 Ethertype for ARP
autodelete=false;				// Flag to system not to delete arp instance
int i;

for(i=0; i < ARP_CACHELEN; i++)
	{
	arp_cache[i].ip_addr = 0L;
	arp_cache[i].mac_addr[0] = 0;
	arp_cache[i].mac_addr[1] = 0;
	arp_cache[i].mac_addr[2] = 0;
	arp_cache[i].type = ARP_DYNAMIC;
	arp_cache[i].time = 0xffffffffL;
	};
}

//--------------------------------------------------------------------------
void Carp::OnMessage(int16 messg,int32 lParam,int32 wParam,int16 zParam)
{
switch(messg)
	{
	case MSG_ARP_REQUEST:
	output_arp(lParam,NULL,ARP_REQUEST,wParam,(int16*)zParam);
	break;

	case MSG_ARP_REPLY:
	break;
	}
}

//--------------------------------------------------------------------------
void Carp::output_arp(int32 their_ip,int16 *their_mac_addr,int operation,int32 our_ip,int16 *our_mac_addr)
{
msg[0]=0;
if(operation==ARP_REQUEST)
	sprintf(msg,szARPtxrequest,their_ip);
else if(operation==ARP_REPLY)
	sprintf(msg,szARPtxreply,their_ip);

OutputDebugString(LOG_INFO,LOG_ARP,(char*)msg);

unsigned char *p = &packet_buf[LAN_HDRLEN];
PUTSHORT(0x0001,p);					// hw type
PUTSHORT(0x0800,p);					// Protocol = ip
PUTSHORT(0x0604,p);					// Hlen and Plen
PUTSHORT(operation,p);				// 1 = ARP request, 2 = reply

if(our_mac_addr != NULL)
	PUTMAC(our_mac_addr,p)			// Supplied MAC addr
else
	PUTMAC(mac_addr,p);	// Our MAC addr

if( our_ip == 0)
	PUTLONG(lan[ACTIVE].ip_addr,p)	// Insert our IP addr unless specified otherwise
else
	// Proxy ARP for our RAS client
	PUTLONG(our_ip,p);				// Not our IP addr. Could be RAS client

if(their_mac_addr == NULL)
	their_mac_addr = MAC_BROADCAST;		// No mac param supplied. Use Broadcast mac addr

if(operation == ARP_REQUEST)
	PUTMAC(MAC_ALLZEROS,p)			// ARP request : use mac addr 000000
else
	PUTMAC(their_mac_addr,p);		// ARP reply

PUTLONG(their_ip,p);				// IP addr to resolve if this is a request

memset(p,0,22);						// Zero pad frame. (Ethernet min frame size = 60 bytes)

if(!plan->output_lan(&packet_buf[LAN_HDRLEN],44,ETHERTYPE_ARP,mac_addr,their_mac_addr,their_ip))
	serialout(szEthNotConnected,HW_DTE0);
}

//--------------------------------------------------------------------------
bool Carp::input_arp(unsigned char *inp_buf,int len,int16 ip_id,int32 ip_src_addr,int unit)
// ARP frame received. request or response.
{
int32 target_ip,their_ip;
unsigned char* p;
int16 hardware_type, proto_type;
int16 their_mac_addr[3];
int16 target_mac_addr[3];
int16 operation;
p = inp_buf;

GETSHORT(hardware_type,p);
if(hardware_type != 1)
	// We only support Ethernet hardware
	return true;

GETSHORT(proto_type,p);
if(proto_type != 0x800) // IP
	// We only support IP protocol
	return true;

INCPTR(2,p);				// Skip Hlen and Plen fields
GETSHORT(operation,p);		// 1 = ARP request, 2 = reply
GETMAC(their_mac_addr,p);	// Get sender MAC
GETLONG(their_ip,p);		// Get sender IP
GETMAC(target_mac_addr,p);	// Get our MAC ( Required for ARP auto IP allocation )
GETLONG(target_ip,p);		// IP addr for which MAC addr is required

// We have the senders IP and MAC address, add them to the ARP cache
// SEB - only add to cache if we are the target (RAM constraint)
// Router - add all ARPs to our cache (promiscuous collection)

#ifdef SEB
if(target_ip != lan[ACTIVE].ip_addr )
	return true;
#endif

msg[0]=0;
if(operation==ARP_REQUEST)
	sprintf(msg,szARPrxrequest,their_ip);
else if(operation==ARP_REPLY)
	sprintf(msg,szARPrxreply,their_ip);
OutputDebugString(LOG_INFO,LOG_ARP,(char*)msg);

// Add the ARP request or reply to cache
// Nb We reply to arp requests even if our cache is full
addtocache(their_ip,their_mac_addr);

if(operation != ARP_REQUEST )
	// This is (probably) an ARP response frame.(there are other types too)
	return true;

#ifdef RAS
if((target_ip < lan[ACTIVE].ip_addr ) ||
   (target_ip > ip_pool[NUM_PPP_LINKS-1].ip_addr))
	// LAN is not the target address.
	// RAS clients are not the target address
	// Could add proxy ARP here
	return true;
#endif

// Now respond to sender. Swap src and dest mac and ip addresses
output_arp(their_ip,their_mac_addr,ARP_REPLY,target_ip);
return true;
}

//--------------------------------------------------------------------------
int deletearpcacheentry(int32 ip_addr)
{
int i;
for(i=0; i < ARP_CACHELEN; i++)
	{
	if(arp_cache[i].ip_addr == ip_addr)
		{
		// Remove hosts arp cache entry.
		arp_cache[i].ip_addr = 0L;
		arp_cache[i].mac_addr[0] = 0;
		arp_cache[i].mac_addr[1] = 0;
		arp_cache[i].mac_addr[2] = 0;
		arp_cache[i].time = 0L;
		
		sprintf(msg,szARPremove,ip_addr);
		OutputDebugString(LOG_INFO,LOG_ARP,(const char*)msg);
		return true;
		}
	}
return false;
}
/*
//--------------------------------------------------------------------------
int16* Carp::isincache(int32 ip_addr,int16* mac_addr)
// Is ip_addr in the arp cache ?
// Returns 
// 1/ If ip_addr on our subnet: mac address ptr
// 2/ If ip_addr not on our subnet but a gateway exists: gateway mac address ptr
// 3/ If ip_addr not on our subnet and no gateway exists: NULL
// **Nb. If ip_addr is found, and a mac is supplied,**
//     the mac and last-used time are both updated
{
int i;

if((ip_addr == IP_ADDR_MULTICAST)||(ip_addr == IP_ADDR_BROADCAST))
	return MAC_BROADCAST;

// Is ip_addr on our subnet ?
if((ip_addr & lan[ACTIVE].netmask) != lan[ACTIVE].subnet)
	{
	// IP not on our subnet, send frame via default gateway
	// if one exists
	if( lan[ACTIVE].gateway == 0 )
		{
		// No gateway is specified, ip not on our subnet.
		// Can't deliver to this ip address
		sprintf(msg,szARPUndeliverable,ip_addr);
		OutputDebugString(LOG_DEBUG,LOG_ARP,(const char*)msg);
		return NULL;
		}
	else
		{
		// Return the mac addr for the default gateway
		sprintf(msg,szARPUsingGateway,ip_addr);
		OutputDebugString(LOG_DEBUG,LOG_ARP,(const char*)msg);
		return arp_cache[0].mac_addr;
		}
	}

// Search the arp cache for ip_addr
for(i=0; i < ARP_CACHELEN; i++)
	{
	if(arp_cache[i].ip_addr == ip_addr)
		{
		// ip addr cache entry found. 
		// If a mac address was supplied, update the entry.
		if(mac_addr != NULL)
			{
			arp_cache[i].mac_addr[0] = mac_addr[0];
			arp_cache[i].mac_addr[1] = mac_addr[1];
			arp_cache[i].mac_addr[2] = mac_addr[2];
			// Update timeout
			arp_cache[i].time = getTickCount() / 1000;
			}			

//		sprintf(msg,"ARP found ip %0a MAC %m",ip_addr,arp_cache[i].mac_addr);
//		OutputDebugString(LOG_DEBUG,LOG_ARP,(const char*)msg);
		return arp_cache[i].mac_addr;
		}	
	}

// ip_addr not found in arp cache
sprintf(msg,szARPnotInCache,ip_addr);
OutputDebugString(LOG_DEBUG,LOG_ARP,(const char*)msg);
return NULL;
}
*/
//--------------------------------------------------------------------------
int16* Carp::isincache(int32 ip,int16* mac,int *cache_location,bool update_time)
// Returns 
// 1/ mac address pointer or NULL
// 2/ Index of cache location if param cache_location is supplied
{
int i;
int32 ip_addr_param = ip;

if((ip & lan[ACTIVE].netmask) != lan[ACTIVE].subnet)
	{
	// IP not on our subnet. Send via default gateway.
	if((ip != 0XFFFFFFFFL/*IP_ADDR_BROADCAST*/)&&(ip != 0xe0000002L/*IP_ADDR_MULTICAST*/))
		{
		ip = lan[ACTIVE].gateway;
		if( ip == 0 )
			{
			// No gateway is specified
			sprintf(msg,szARPUndeliverable,ip_addr_param);
			OutputDebugString(LOG_DEBUG,LOG_ARP,(char*)msg);
			return NULL;
			}
		else
			{
			// IP is not on our subnet, send frame via default gateway
			// Fall through to return the mac addr for the default gateway
			sprintf(msg,szARPUsingGateway,ip_addr_param);
			OutputDebugString(LOG_DEBUG,LOG_ARP,(char*)msg);
			}
		}
	}
	
for(i=0; i < ARP_CACHELEN; i++)
	{
	if(arp_cache[i].ip_addr == ip)
		{
		// Update hw address if supplied
		if(mac != NULL)
			{
			arp_cache[i].mac_addr[0] = mac[0];
			arp_cache[i].mac_addr[1] = mac[1];
			arp_cache[i].mac_addr[2] = mac[2];
			}			

		// Matching cache location found. Update received time
		if(update_time)
			arp_cache[i].time = GBL_count_1ms / 1000;
		
//		sprintf(msg,"ARP found ip %0a MAC %m time %lu"/*szARPfound*/,ip,arp_cache[i].mac_addr,arp_cache[i].time);
//		OutputDebugString(LOG_DEBUG,LOG_ARP,(char*)msg);
		
		if(cache_location)
			*cache_location = i;
		return arp_cache[i].mac_addr;
		}	
	}

return NULL;
}

//--------------------------------------------------------------------------
void Carp::addtocache(int32 add_ip_addr,int16 *add_mac_addr,int type,bool checkexist)
// Add or update an arp cache entry
// ip_addr is in host format
// mac_addr points to < int16 mac address[3] >
// optional type param is ARP_STATIC or ARP_DYNAMIC
{
int cache_location = 0;

// If the entry already exists in the cache, this returns its location
// This is to avoid duplicating entries
//if(checkexist)
	isincache(add_ip_addr,add_mac_addr,&cache_location);

for(; cache_location < ARP_CACHELEN;)
	{
	if((arp_cache[cache_location].ip_addr == 0L) || (arp_cache[cache_location].ip_addr == add_ip_addr))
		// Empty cache location found
		// Or entry is already in cache - in this case, update it
		{
		arp_cache[cache_location].ip_addr = add_ip_addr;
		if(add_mac_addr != NULL)
			{
			arp_cache[cache_location].mac_addr[0] = add_mac_addr[0];
			arp_cache[cache_location].mac_addr[1] = add_mac_addr[1];
			arp_cache[cache_location].mac_addr[2] = add_mac_addr[2];
			}
		
		// Don't convert static to dynamic entries,but dynamic entries can be converted to static
		if(arp_cache[cache_location].type == ARP_DYNAMIC)
			arp_cache[cache_location].type = type;

		arp_cache[cache_location].time = GBL_count_1ms / 1000;

		sprintf(msg,szARPadd,add_ip_addr,add_mac_addr);
		OutputDebugString(LOG_INFO,LOG_ARP,(char*)msg);
		return;
		}
		
	cache_location++;
	}

sprintf(msg,szARPcachefull);
OutputDebugString(LOG_WARNING,LOG_ARP,(char*)msg);
}
/*
//--------------------------------------------------------------------------
bool Carp::addtocache(int32 add_ip_addr,int16 *add_mac_addr,int type,bool checkexist)
// Add or update an arp cache entry
// Remove ARP_STALE flag for new entries
// mac_addr points to < int16 mac address[3] >
// optional type param is ARP_STATIC or ARP_DYNAMIC
// Returns:
// true if entry added, false if cache full
{
int cache_location = 0;

// Is ip_addr in the cache already ?
for(; cache_location < ARP_CACHELEN;cache_location++)
	{
	if(arp_cache[cache_location].ip_addr ==  add_ip_addr)
		break;
	};

if(cache_location==ARP_CACHELEN)
	// ip_addr is not in the cache
	cache_location = 0;

for(; cache_location < ARP_CACHELEN;)
	{
	if((arp_cache[cache_location].ip_addr == 0L) || (arp_cache[cache_location].ip_addr == ip_addr))
		// Empty cache location found
		// Or entry is already in cache - in this case, update it
		{
		arp_cache[cache_location].ip_addr = ip_addr;
		if(mac_addr != NULL)
			{
			arp_cache[cache_location].mac_addr[0] = mac_addr[0];
			arp_cache[cache_location].mac_addr[1] = mac_addr[1];
			arp_cache[cache_location].mac_addr[2] = mac_addr[2];
			}
		
		// Clear ARP_STALE flag
		arp_cache[cache_location].type &= ARP_RESET_STALE;

		// Don't convert static to dynamic entries,but dynamic entries can be converted to static
		if(arp_cache[cache_location].type & ARP_DYNAMIC)
			arp_cache[cache_location].type = type;

		arp_cache[cache_location].time = getTickCount() / 1000;

//		sprintf(msg,"ARP add %0a at %lu",ip_addr,arp_cache[cache_location].time);
		sprintf(msg,szARPadd,ip_addr,mac_addr);
		OutputDebugString(LOG_INFO,LOG_ARP,(const char*)msg);
		return true;
		}
		
	cache_location++;
	}

sprintf(msg,szARPcachefull,ip_addr);
OutputDebugString(LOG_WARNING,LOG_ARP,(const char*)msg);
return false;
}
*/
//--------------------------------------------------------------------------
void Carp::arpcachetimeout()// **Called frequently**** from idle routine. 
// arp_cache[i].time is the time added in 1 second units
// Remove or refresh ageing ARP_DYNAMIC entries in the ARP cache
// Update ageing ARP_STATIC cache entries
// If the host is in session with SEB, send an Arp request to refresh the entry

// PRE 1.95:
// *** Entries 0-2 never time out ***.
// arp_cache[0] = Router IP gateway (only present if a gateway is specified)
// arp_cache[1] = Router IP addr
// arp_cache[2] = Allstations broadcast addr  (x.x.x.255 for class C LAN )
// arp_cache[3] = Multicast address ( 224.0.0.2 )
// POST 1.95
// arp_cache[0] = Router IP gateway (only present if a gateway is specified)
// POST 1.96 timenow is 32 bit value.
// Pre 1.96 did not timeout entries added after 18 hours. See version.cpp for details
{
int i,n;
int32 timenow = getTickCount()/1000;

for(i=0; i < ARP_CACHELEN; i++)
	{
	if(arp_cache[i].ip_addr == 0L)
		// No entry at this location in cache
		continue;
	
	if((arp_cache[i].time + ARP_CACHE_TIMEOUT) >= timenow)
		// Cache entry not yet timed out
		continue;

	if(arp_cache[i].type & ARP_STATIC)
		{
		sprintf(msg,szARPstale,arp_cache[i].ip_addr);
		OutputDebugString(LOG_INFO,LOG_ARP,(const char*)msg);

		// Send an Arp to refresh the cache entry
		output_arp(arp_cache[i].ip_addr,MAC_BROADCAST,ARP_REQUEST,lan[ACTIVE].ip_addr,mac_addr);
		// Will attempt another refresh on expiry
		arp_cache[i].time = timenow;
		// Flag for "not updated"
		arp_cache[i].type |= ARP_STALE;
		continue;
		}
		
	// A dynamic cache entry is timed out
	// Is the ip address connected to us ?
	for(n=0; n < MAXL3CLIENTS; n++)
		{
		if((ip_protocols[n] != NULL) &&
			 (ip_protocols[n]->their_ip == arp_cache[i].ip_addr))
			{
			// SEB is connected to this ip.
			// Send an Arp to refresh the cache entry
			if(!(arp_cache[i].type & ARP_STALE))
				{
				// Send the Arp, and mark entry as "stale"
				sprintf(msg,szARPstale,arp_cache[i].ip_addr);
				OutputDebugString(LOG_INFO,LOG_ARP,(const char*)msg);
				arp_cache[i].type |= ARP_STALE;
				output_arp(arp_cache[i].ip_addr,MAC_BROADCAST,ARP_REQUEST,lan[ACTIVE].ip_addr,mac_addr);
				return;
				}

			if((arp_cache[i].time + ARP_CACHE_TIMEOUT+3) > timenow)
				// Still waiting 3 secs for response to Arp refresh
				return;

			// No response to Arp refresh. Fall thru to delete entry
			sprintf(msg,szARPnotResponding,arp_cache[i].ip_addr);
			OutputDebugString(LOG_INFO,LOG_ARP,(const char*)msg);
			goto delete_cache_entry;
			}
		}

	delete_cache_entry:
	// Only delete dynamic entries
	sprintf(msg,szARPtimeout,arp_cache[i].ip_addr);
	OutputDebugString(LOG_INFO,LOG_ARP,(const char*)msg);
	// Remove hosts arp cache entry.
	arp_cache[i].ip_addr = 0L;
	arp_cache[i].mac_addr[0] = 0;
	arp_cache[i].mac_addr[1] = 0;
	arp_cache[i].mac_addr[2] = 0;
	arp_cache[i].time = 0;
	arp_cache[i].type = ARP_DYNAMIC;
	}
	
return;
}
