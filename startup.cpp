// startup.cpp
// Class Cinit is created to initialise the unit and load defaults or saved settings
// The Class is deleted once startup is complete.

#include "router.h"
#include "pppd.h"
#include "startup.h"
#include "protocols.h"
#include "tcp.h"
#include "udp.h"
#include "arp.h"
#include "ip.h"
#include "icmp.h"
#include "lan.h"
#include "commandport.h"
#include "utils.h"
#include "tcpmodem.h"
#include "pad.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\router\ppp\ppplink.h"
#include "ffs.h"
#ifdef GALAXY
#include "galaxy.h"
#endif
#ifdef DHCP
#include "dhcp.h"
#endif
#ifdef SMTP
#include "smtp.h"
extern Csmtp *smtp;
#endif

// Default configuration settings
int16 MAC_BROADCAST[3]		={0XFFFF,0XFFFF,0XFFFF};									// Ethernet broadcast addr
int16 MAC_ALLZEROS[3]		={0,0,0};	// ARP request placeholder addr
int16 mac_addr[3];			// Our mac address

int32 idle_service_time_slow = SLOW_IDLE_SERVICE_PERIOD_MS;	// Idle task execution timer
int32 idle_service_time_fast = FAST_IDLE_SERVICE_PERIOD_MS; // Idle task execution timer
int32 idle_service_time_very_slow = VERY_SLOW_IDLE_SERVICE_PERIOD_MS; // Idle task execution timer
int16 low_mem_warning_threshold =  UART_BUFLEN + 100 ;			// Space for TCPmodem always needed
int16 tcp_transient_port_number	=	1024;		// Port number to use for outgoing TCP calls.Increments per call

int dirty;										// New settings made, not applied
int auto_ip_assigned_address;
int system_arp_cache_entries;					// Any static entries beyond this index in the cache are saved
int16 idle_arps_sent;							// Inactive LAN reset system retry counter

Cffs *ffs;										// File system
CTelnet *telnet;
CipV4 *ipV4;
Carp *arp;
Clan *plan;
Cicmp *icmp;
Cdhcp *dhcp;
CClient *shell;									// The command interface on DTE1

#ifdef router
Cppp ppp;
#endif
extern CRouter *router;

CProtocol_L3 *ip_protocols[MAXL3CLIENTS];		// Client protocols array TCP,ICMP etc
unsigned char packet_buf[PPP_MRU+PPP_HDRLEN+TCP_DATA_OFFSET];	// buffer for outgoing packets
unsigned char ifstr[MAXLEN1];
unsigned char ownerstr[MAXLEN1];
unsigned char baudstr[MAXLEN1];
unsigned char flowstr[MAXLEN1];
unsigned char *msg;								// for sprintf

ADDRESS_POOL address_pool;
int num_pool_addresses;

tagmodem_config modem_config;					// AT decoder settings
tagmail_config mail_config;						// email settings
tag_traceout traceout;							// Trace preferences
lanport lan[2];									// 2 Lan interface structures, ACTIVE and UNSAVED
unsigned char profile[MAXLEN2];

#ifndef SEB
tag_rtable rtable[RTABLE_SIZE];					// The routing table. Each entry = 4 32bit integers
#endif

struct tagMESSAGE *message;						// First entry in callout linked-list must be 0
#define DHCP_WAIT_TIMER 4000
//--------------------------------------------------------------------------
int CInit::init(int mode)
// Call once to initialise everything.
// mode : INIT_ALL or INIT_LAN
{
bool lan_settings_loaded=true;
auto_ip_assigned_address = 0;
dirty = 0;

int16 *pGateway_Mac = NULL;
int16 gateway_mac[3] = {0xffff,0xffff,0xffff};
int32 end_time;
Cudp* udp;

if(mode == INIT_LAN)
	goto initlan;

idle_arps_sent=0;
lan_settings_loaded = false;
message = NULL;

hw->uartOutputFlush(HW_DTE0);
hw->uartInputFlush(HW_DTE0);

msg = new unsigned char[TRACE_BUFLEN];			// Buffer for sprintf

// DHCP address pool
num_pool_addresses = 0;
address_pool.first_ip_addr = 0;
address_pool.last_ip_addr = 0;
address_pool.dhcp_enabled = false;

// Default trace attributes
#ifdef RELEASE
traceout.module = LOG_ALL - LOG_ARP;// - LOG_BUG - LOG_IP - LOG_SERIAL - LOG_ICMP - LOG_TCP;
#else
traceout.module = LOG_ALL - LOG_ARP;// - LOG_IP;
#endif

traceout.level = LOG_DEBUG;//LOG_DEFAULT;			// Default level LOG_DEBUG;(all)
traceout.suppress=false;
traceout.pause = false;
traceout.overrun=false;
traceout.hexmode=false;

arp = new Carp;
plan = new Clan;
ipV4 = new CipV4;
icmp = new Cicmp;

#ifdef SMTP
smtp = NULL;
//smtp = new(Csmtp);
//smtp->init(HW_NONE,NULL,false);
#endif

ffs = new Cffs;
ffs->init();									// Init file system

ipV4->init(0);
udp = (Cudp*)ip_protocols[1];

#ifdef DHCP
dhcp = new Cdhcp;
dhcp->init(1,udp,false);
#endif

#ifndef SEB
//Init RAS defaults
ras.enableras = true;							// Permit incoming dial-up networking (RAS / DUN)conection
ras.PermitLANAccess = true;						// Permit RAS client to access LAN (or ony the router itself)
#endif

// Install factory default settings
restore_defaults();

char pFn[20];	
pFn[0] = NULL;
// Remove deleted flash files
ffs->driveclean();

// Restore LAN settings from file LAN, overwriting factory settings
if(ffs->open(MODE_OPENEXISTING,szLAN,szFILETYPE_SYSTEM))
	{
	ffs->restore_from_flash(SAVE_RESTORE_LAN);
	ffs->close();
	lan_settings_loaded = true;
	}
	
// Get startup profile name and set to factory defaults (except LAN)
ffs->get_bootfile_name(pFn);

// Restore saved settings from startup profile, overwriting factory settings
// If this fails, flash is faulty etc
if(ffs->open(MODE_OPENEXISTING,pFn,szFILETYPE_PROFILE))
	{
	ffs->restore_from_flash(SAVE_RESTORE_ALL);
	ffs->close();
	strcpy((char*)profile,pFn);		// Save the active profile name for reference
	}

// Create the shell (user interface on DTE1) object
switch(modem_config.shell)
	{
	case PROFILE_AT:
	shell = (CClient*)new CTCPModem;		
	break;

#ifdef PAD
	case PROFILE_PAD:
	shell = (CClient*)new CPad;
	break;
#endif
#ifdef GALAXY
	case PROFILE_GALAXY:
	shell = (CClient*)new CGalaxy;
	break;
#endif

	default:
	case PROFILE_TELNET:
	shell = (CClient*)new CCommandPort;
	break;
	}

shell->init(HW_DTE0,NULL,true);				// Shell is user interface on DTE1. A static object ( because autodelete==false )
shell->autodelete = false;

// Watchdog timer enable. Needed for "ATY" command
asm( " stm #(0x0c2f),@tcr " );  	// reserved(15-12)=0,soft(11)=1,free(10)=1,psc(9-6)=0,trb(5)=1,tss(4)=0,tddr(3-0)=0xf "start timer"

// Default DTE1=command port. DTE1 can also receive trace data ( below )
hw->uart[HW_DTE0].owner =  OWNER_SHELL;
hw->uart[HW_DTE0].pClient = (taguart::CClient*)shell;

// Discard trace output
hw->uart[HW_TRACE].owner = OWNER_NONE;
hw->uart[HW_TRACE].pClient = NULL;
// Send trace output to DTE1
//hw->uart[HW_TRACE].owner = HW_DTE0;
//hw->uart[HW_TRACE].pClient = (taguart::CClient*)shell;

if(! ffs->GetMac( mac_addr))
	{
	// Failed to load MAC from flash. MAC has never been set as it cannot be erased (only in pmboot)
		
	// Ask user for MAC address now.
	// Only the command interface is operable until both a MAC and IP addrs are entered and saved.
	serialout(szFactorySettings,HW_DTE0);
	serialout(szEnterMAC,HW_DTE0);
	return STARTUP_NEED_MAC;
	}
	
if(lan_settings_loaded == false)
	{
	// Failed to load LAN profile from flash. 
	// SEB may have been reprogrammed
#ifdef AUTO_IP
	lan[UNSAVED].ip_addr = 0;
#elif DHCP
	lan[UNSAVED].ip_addr = 0;
#else
	// No automatic IP address assignment available - 
	// user must assign address via serial port
	// Ask user for IP address now.
	// Only the command interface is operable until both a MAC and IP addrs are entered and saved.
	serialout(szFactorySettings,HW_DTE0);
	serialout(szEnterIP,HW_DTE0);
	return STARTUP_NEED_IP;
#endif
	}
	
initlan:
// We have an IP and MAC address.
// Init LAN chip. Sets the MAC address in the chip
plan->init();
plan->configure_leds(lan[UNSAVED].led_mode);

bool bRes;

// Check LAN media is connected and report status if not
// Nb. ** This section only works from a delay after startup with CNet switch **
end_time = getTickCount() + LAN_CONNECT_WAIT_TIMER;
do
	{
	bRes = plan->link_test(msg,false);
	delay_ms(100);
	}while(!bRes && (end_time > getTickCount()));
if(!bRes)
	{
	// Tell user to connnect the cable
	serialout((char*)msg,HW_DTE0);
//	serialout(szEthNotConnected2,HW_DTE0);
	return LAN_NOT_CONNECTED;
	}

// Init the routing table
#ifndef SEB
OutputDebugString(LOG_NOTICE,LOG_SYSTEM,szInitRtable);
for(int n=0; n < RTABLE_SIZE ; n++)
	rtable[n].dest_net = 0;							// Marks entry as unused
#endif

#ifdef DHCP
// DHCP option
if(lan[UNSAVED].ip_addr == 0)
	// Use DHCP to obtain our IP address
	{
	arp->addtocache(IP_ADDR_BROADCAST,MAC_BROADCAST,ARP_DYNAMIC,false);

	end_time = getTickCount() + DHCP_WAIT_TIMER;

	// Broadcast a DHCPDISCOVER frame
	PostMessage(udp,MSG_DHCP_DISCOVER_ADDR,0,0,0,1);
	
	do{
		router->calltimeout();						// Manage message queue
		router->get_input_lan();					// Looks for responses. Sets our IP address to 0 if a response is received
		}while((lan[UNSAVED].ip_addr == 0) && (end_time > getTickCount()));

	while(router->untimeout((CObj*)udp, MSG_ALL));	// Remove ARP messages from the queue

	if(lan[UNSAVED].ip_addr == 0)
		{
		// DHCP failed
		OutputDebugString(LOG_ERR,LOG_SYSTEM,szDHCPfail);
		
		// Need ip addr
		return STARTUP_DHCP_FAIL;
		}
	// Note: lan[UNSAVED].dhcp_server should be non-0
	}
#endif	// DHCP

// Before activating the LAN interface, check if our IP address is already in use by another station
// We do this if the ip address was assigned by a dhcp server too, and before accepting the address.
arp->init(0);
memcpy(&lan[ACTIVE],&lan[UNSAVED],sizeof(lanport));

#ifdef AUTO_IP
// A proprietary arp-type DHCP system.
if(lan[UNSAVED].ip_addr == 0)
	{
	// IP address 0 set and DHCP is off
	// We could receive an IP via our MAC address
	serialout(szIpNotAssigned,HW_DTE0);
	return STARTUP_NEED_IP; 
	}
#endif

if(lan[UNSAVED].gateway != 0)
	{
	if(ArpFor(lan[UNSAVED].gateway,GATEWAY_WAIT_TIMER))
		{
		// Save the gateway mac addr as ARP cache will be flushed and entries re-added
		if(pGateway_Mac = arp->isincache(lan[UNSAVED].gateway,NULL))
			{
			gateway_mac[0] = *pGateway_Mac++;
			gateway_mac[1] = *pGateway_Mac++;
			gateway_mac[2] = *pGateway_Mac;
			}
		}
	else
		{
		// Gateway did not respond in time
		sprintf(msg,szGatewayNotFound,lan[UNSAVED].gateway);
		serialout((const char*)msg,HW_DTE0);
		}
	}

if(ArpFor(lan[UNSAVED].ip_addr,DUPLICATE_IP_RESPONSE_TIMER))
	{
	// Duplicate IP on the LAN. Tell the user
	serialout(szDuplicate_IP,HW_DTE0);

#ifdef DHCP
	if(lan[UNSAVED].dhcp_server != 0)
		{
		// Our IP address was obtained using DHCP.
		// It is a duplicate so send DHCPDECLINE to the DHCP server
		PostMessage(udp,MSG_DHCP_DECLINE_ADDR,lan[UNSAVED].dhcp_server,lan[UNSAVED].ip_addr,0,0);
		while(router->calltimeout());
		// We have no ip address ! Need to restart.
		}
#endif

	// V1.94. New action is to continue regardless.
	}


// The above procedures may have introduced cache entries if other stations were sending ARP.
// Flush the ARP cache
// Flush ARP messages from the queue
while(router->untimeout((CObj*)udp, MSG_ALL));
arp->init(0);

// Add ARP cache the entries in order of frequency of use.
// For efficiency, we want the most commonly used addresses to be the first entries in the cache.
// Gateway ip addr and MAC
if(lan[UNSAVED].gateway != 0)
	{
	// If the gateway has not responded yet, this is a placeholder
	// in the ARP cache.
	arp->addtocache(lan[UNSAVED].gateway,gateway_mac,ARP_STATIC,false);
	system_arp_cache_entries++;
	}

/*
// Add static (no-timeout) entries for our ip, gateway and broadcast ip, dhcp server
arp->addtocache(lan[UNSAVED].ip_addr, mac_addr,ARP_STATIC,false);
system_arp_cache_entries++;
arp->addtocache(lan[UNSAVED].ip_addr | LAN_IP_ALLSTATIONS,MAC_BROADCAST,ARP_STATIC,false);
system_arp_cache_entries++;
arp->addtocache(IP_ADDR_MULTICAST,MAC_BROADCAST,ARP_STATIC,false);
system_arp_cache_entries++;
arp->addtocache(IP_ADDR_BROADCAST,MAC_BROADCAST,ARP_STATIC,false);
system_arp_cache_entries++;
*/

#ifndef SEB
// Add a default route for our LAN interface
addroute(lan[UNSAVED].ip_addr & lan[UNSAVED].netmask,lan[UNSAVED].netmask,lan[UNSAVED].gateway,HW_LAN);

// Add a default route for the broadcast addr on our LAN interface
addroute(lan[UNSAVED].ip_addr | LAN_IP_ALLSTATIONS,IP_ADDR_BROADCAST,0L,HW_LAN);
	
// And a route for the LAN multicast address
// TODO is this correct or should I use IP_ADDR_MULTICAST ?
addroute(IP_ADDR_MULTICAST,0xffffffff,0L,HW_LAN);
#endif

// Copy LAN temporary interface settings to active
memcpy(&lan[ACTIVE],&lan[UNSAVED],sizeof(lanport));

#ifdef DHCP
if(lan[UNSAVED].dhcp_server != 0)
	{
	// Set time for next lease renewal (T1)
	PostMessage((CObj*)ip_protocols[1],MSG_DHCP_RENEW_ADDR,lan[ACTIVE].dhcp_server,lan[ACTIVE].ip_addr,0,lan[ACTIVE].dhcp_leasetime/2);
	// Set T2 DHCP server unavailable timer
	PostMessage((CObj*)ip_protocols[1],MSG_DHCP_T2_TIMER,0,0,0,lan[ACTIVE].dhcp_leasetime/7 * 8);
	// Set T3 end of lease timer
	PostMessage((CObj*)ip_protocols[1],MSG_DHCP_T3_TIMER,0,0,0,lan[ACTIVE].dhcp_leasetime);
	}
#endif

#ifndef SEB
// Initiate router advertisments. These are self perpetuating.
PostMessage(icmp,MSG_ICMP_ROUTER_ADVERT,0,0,0,(int32)ICMP_ROUTER_ADVERT_PERIOD);

OutputDebugString(LOG_NOTICE,LOG_SYSTEM,szInitPPP);
//ppp->init_ppp(); ??
ppp->init(0);
#endif

return STARTUP_OK;
}

//-----------------------------------------------
bool CInit::ArpFor(int32 dest_ip,int32 waittime)
// Send ARPs for dest_ip until timer expires or response received
{
int32 arp_retry_timer;

waittime = getTickCount() + waittime;
do
	{
	// Send ARPs for dest_ip until it responds or timer expires
	arp->output_arp(dest_ip,NULL,ARP_REQUEST,lan[UNSAVED].ip_addr,mac_addr);

	arp_retry_timer = getTickCount() + ARP_RESEND_TIME;

	do	{
		// Loop waiting for a response to the ARP request (appx 1 second)
		router->get_input_lan(true);					

		if(arp->isincache(dest_ip,NULL))
			return true;

		}while(arp_retry_timer > getTickCount());

	// No response to ARP, retry until timer expires
	}while(waittime > getTickCount());

return false;
}


/*
//--------------------------------------------------------------------------
int CInit::options_from_file()
// Run startup configuration script using commandport
{
//cmdport->use_scratchpad = 0;
#ifndef WIN32

// TI implementation
int16 data,n;
int16 ch16;
unsigned char *p = packet_buf;
unsigned int len=0;
n=0;

do
	{
	// nb TI string representations use 16 bit memory storage. 
	// The upper 8 bits must be 0's for some library string functions to work !
	
	data = FlsRead( script_flash_address + n++ );
	if((data == 0xffff) || ( data == 0 ))
		break;
	
	ch16 = data & 0xff;	// UN-Set top 8 bits
	*p++=ch16;

	ch16 = (data >> 8) & 0xff;

	if((ch16 == '\r') || (ch16 == '\n'))
		{
		*p++='\x0D';
		*p++='\0';
		len = p - packet_buf;
		p = packet_buf;

		cmdport->receive(&p,&len,0);
		if(len)
			{
			p[len] = '\0';
			OutputDebugString(LOG_NOTICE,LOG_SYSTEM,p);
			}
		p = packet_buf;
		len = 0;
		}
	else 
		*p++=ch16 & 0xff;	// UnSet top 18 bits

	
	}while(len < (PPP_MRU/2));	// Don't exceed array bounds !

#endif
cmdport->use_scratchpad = 1;
return 1;
}


*/


