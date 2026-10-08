// router.cpp
// Top-level functions to implement the SEB

// Compilation options ( project->build options->preprocessor )
// ASL			ASL messages (else DSP)
// SEB			Build a SEB
// BAUD_1200	Use low baudrate range (1200-38400bps).
// 				**** Nb. include vectboot_slow****   
// 				If not specified, include vectorboot_fast (9600-115200)
// RELEASE		Selected trace enabled (else all)
// LO_POWER		DSP at 50mhz (100mhz if not specified)
//				TODO include vectboot_slow ?
//				TODO only in conj with BAUD_1200 ?

// Optional features:
// TFTP			TFTP client/server
// DHCP			DHCP client only
// DHCP SERVER	DHCP server (also specify DHCP)
// SMTP			SMTP mail server
// SWITCH_FIX	SEBS can stop working on LAN side. Enable bug fixes.
// PAD			X28 PAD interface 

// C++ class models:

//		**	Object class derivation - L2 protocols **
// base class CObj				(declared in router.h)
// derived class CProtocol : public CObj
// derived class CAuth : public CProtocol
// derived class Cchap : public CProtocol
// derived class Cipcp : public CProtocol
// derived class CipV4 : public CProtocol
// derived class Clcp : public CProtocol
// derived class Cmlcp : public CProtocol
// derived class CPap : public CProtocol
//
//							CObj
//							|
//						CProtocol
//							|
//		-------------------------------------------------
//		|		|		|		|		|		|		|
//		CAuth	CChap	Cipcp	CipV4	Clcp	Cmlcp	Cpap


//		**	Object class derivation - L3 protocols **
// base class CObj				(declared in router.h)
// derived class CProtocol_L3 : public CObj
// derived class CTcp : public CProtocol_L3
// derived class Cudp : public CProtocol_L3
// derived class Cicmp : public CProtocol_L3
// derived class Carp : public CProtocol_L3
//
//							CObj
//							|
//						CProtocol_L3
//							|
//				-----------------------------------------
//				|			|				|			|
//				CTcp		Cicmp			Cudp		Carp

//		**	Object class derivation - L4 protocols **
// base class CClient
// derived class CClient : public CObj
// derived class CHTTP : public CClient
// derived class CTelnet : public CClient
// (etc..)
//
//							CObj
//							|
//							CClient
//							|
//		-------------------------------------------------------------
//		|	|		|			|			|		|		|		|
//	CHTTP  CTelnet 	CComandport	CTCPModem	Ctftp	Cdhcp	CPad	Csmtp


#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "udp.h"
#include "arp.h"
#include "ip.h"
#include "icmp.h"
#ifdef DHCP
#include "dhcp.h"
#endif
#include "lan.h"
#include "commandport.h"
#include "utils.h"
#include "tcpmodem.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

#ifdef ROUTER
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\router\ppp\ppplink.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\router\ppp\magic.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\router\ppp\auth.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\router\ppp\pap.h"
//#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\router\ppp\chap.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\router\ppp\ipcp.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\router\ppp\lcp.h"
#endif

#ifndef SEB
CProtocol *ppp_protocols[NUM_PPP_PROTOCOLS];		// Link protocols array

// nb the following 2 buffers MUST be contiguous
unsigned char ppp_header_buf[20];					// buffer for ppp header
unsigned char makeup_buf[PPP_MRU+PPP_HDRLEN];		// PPP asynch <-> synch makeup buffer

addr_pool ip_pool[NUM_PPP_LINKS];					// RAS client IP Address pool
ppp_interface ppp_if[NUM_PPP_LINKS * 2];			// Mirror copies contain new, unapplied config info. See also lanport struct
RAS ras;
extern fsm lcp_fsm[];								// Declared in lcp.cpp
//Cmlcp *mlcp;
Cppp *ppp;
Clcp *lcp;
CMagic *magic;
CFsm *cfsm;
CPap* pap;
//Cchap *chap;
CAuth* auth;
Cipcp *ipcp;
#endif

extern int32 idle_service_time_slow;
extern int32 idle_service_time_fast;
extern int32 idle_service_time_very_slow;
extern int16 low_mem_warning_threshold;
void demand_link(void *arg);
extern int16 idle_arps_sent;

//--------------------------------------------------------------------------
int CRouter::pppmain(bool service_uarts)
// Top level routine for router called from schedule
{
int i = 0;
// Service dte port uart buffers
for( i=HW_DTE0 ; i < (NUM_UARTS-TRACE_INTERFACE) ; i++)
	get_input_uarts(i);

if(service_uarts)
	return SEB_OK;

get_input_lan();

#ifndef SEB	
// SEB has no PPP links
// Service ppp interfaces (to ISDN adapter).
// Nb. The parameter is the PPP interface number
for(i=0; i < NUM_PPP_LINKS; i++)
	{
	manage_wan_link(i);
	get_input_ppp(i);
	}
#endif

int32 currenttime = getTickCount();	// 1ms units
	
//----------------------------------------------------------------//
// The following routines run at timed intervals
//----------------------------------------------------------------//	
if(currenttime > idle_service_time_fast)
	{
	idle_service_time_fast = currenttime + FAST_IDLE_SERVICE_PERIOD_MS;

	// Following code runs at interval FAST_IDLE_SERVICE_PERIOD_MS
	calltimeout();

	// Trace data out
	get_input_uarts(HW_TRACE);
	}

//----------------------------------------------------------------//	
if(currenttime > idle_service_time_slow)
	{
	// Following code runs at interval SLOW_IDLE_SERVICE_PERIOD_MS
	idle_service_time_slow = currenttime + SLOW_IDLE_SERVICE_PERIOD_MS;

	arp->arpcachetimeout();
#ifdef DHCP_SERVER
	dhcp->addresspooltimeout(currenttime);
#endif
	delete_idle_processes(currenttime);

	if(!plan->link_test(msg,false))
		{
		// LAN was disconnected
		OutputDebugString(LOG_NOTICE,LOG_SYSTEM,szLANDisconnected);
		return LAN_NOT_CONNECTED;
		}	
	if(auto_ip_assigned_address)
		{
		return SEB_RESTART;
		}

	#ifdef SWITCH_FIX
	RebootIfIdle();
	#endif
	}
	
//----------------------------------------------------------------//	
// This runs fast until low memory detected, then runs slowly
if(currenttime > idle_service_time_very_slow)
	{
	if(freemem() < low_mem_warning_threshold)
		{
		// TODO decline connections until more memory is free
		sprintf(msg,szLowMem,freemem());
		OutputDebugString(LOG_WARNING,LOG_SYSTEM,(const char*)msg);
		idle_service_time_very_slow = currenttime + VERY_SLOW_IDLE_SERVICE_PERIOD_MS;
		}
	}

#ifndef SEB	// SEB has no PPP links
monitor_incoming_calls();
#endif

return SEB_OK;
}


#ifdef SWITCH_FIX
//--------------------------------------------------------------------------
void CRouter::RebootIfIdle()
// If we receive no LAN frames for a period, reinit LAN chip and send some ARPS.
// If there is still no activity, reboot
{
// Calculate time expired since last frame received on LAN
int32 idletime = getTickCount() - plan->idletimer_ms;

if(idletime < LAN_IDLE_TIME_SENDARPS)
	// No timeout yet.
	return;

// Timer LAN_IDLE_TIME_SENDARPS has expired

if(idle_arps_sent>IDLE_ARP_SEND_LIMIT)
	{
	// Already sent limit of ARPS on idle. 
	if(lan[ACTIVE].idle_reset==1)
		{
		// Timer LAN_IDLE_TIME_REBOOT has expired
		// Give up trying to recover otherwise. Reboot
		serialout(szLanIdleReBoot,HW_DTE0);
		watchDogTimerDisable=1;
		while(1);
		}
		
	// Not time yet to reboot, or reboot is disabled
	return;
	}
		
// Send some ARP requests.
// This may remind the switch of our IP and MAC addresses and may get it 
// to resume sending us frames if it has stopped for some reason
plan->init();	// Resets frame counters but not inactivity timer
delay_ms(200);	// Risky ?
	
// Prefer to send to Gateway as it will respond immediately and reset the idletimer
if(lan[UNSAVED].gateway != 0)
	{
	// ARP the gateway. Should get a response which resets the idle timer
	PostMessage(arp,MSG_ARP_REQUEST,lan[UNSAVED].gateway,lan[UNSAVED].ip_addr,(int16)mac_addr,1);
	PostMessage(arp,MSG_ARP_REQUEST,lan[UNSAVED].gateway,lan[UNSAVED].ip_addr,(int16)mac_addr,500);
	}	
else
	{
	// There is no gateway, ARP ourself. No response expected.
	PostMessage(arp,MSG_ARP_REQUEST,lan[UNSAVED].ip_addr,lan[UNSAVED].ip_addr,(int16)mac_addr,1);
	PostMessage(arp,MSG_ARP_REQUEST,lan[UNSAVED].ip_addr,lan[UNSAVED].ip_addr,(int16)mac_addr,500);
	}

idle_arps_sent++;	
OutputDebugString(LOG_NOTICE,LOG_SYSTEM,szLanIdleReInit);
}
#endif // SWITCH_FIX

//--------------------------------------------------------------------------
bool CRouter::link_test()
{
if(plan->link_test(msg,false))
	{
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,szLANConnected,HW_DTE0);
	return true;
	}
return false;
}

extern struct tagMESSAGE *message;
//--------------------------------------------------------------------------
void CRouter::timeout(tagMESSAGE* messg)
// timeout - Schedule a timeout. Calls the classes OnMessage function when the time expires
// Parameters:
// pObj		- the this pointer of any CObj based class
// MESSAGE 	- structure with timeout period and arguments for OnMessage
// Note that this timeout takes 1ms units
{
struct tagMESSAGE *newMsg, *pMsg, **ppMsg;

// Allocate a timeout
newMsg = new tagMESSAGE;
if(newMsg == NULL)
	{
	OutputDebugString(LOG_ERR,LOG_SYSTEM,szNoMem);
	return;
	}

int32 timenow = getTickCount();			// ms

newMsg->msg = messg->msg;
newMsg->wParam = messg->wParam;
newMsg->lParam = messg->lParam;
newMsg->zParam = messg->zParam;
newMsg->time = timenow + messg->time;
newMsg->pObj = messg->pObj;

pMsg=NULL;
// Find correct place and link it in. (linked list is in time order)
for( ppMsg = &message; pMsg = *ppMsg; ppMsg = &pMsg->pNext )
	{
	if( newMsg->time <= pMsg->time )
		{
		break;
		}
	}
	
newMsg->pNext = pMsg;
*ppMsg = newMsg;
}

//--------------------------------------------------------------------------
bool CRouter::untimeout(CObj* pObj, int16 messg)
// untimeout - Unschedule a timeout.
// *func and *arg must match the timeout
{
bool bRes = false;
struct tagMESSAGE *freeMsg, **ppMsg;

if(messg == MSG_ALL)
	{
	// Remove first message for class pObj, ignoring the MSG_ type
	// Called before a CObj class is deleted
	for( ppMsg = &message; freeMsg = *ppMsg ; ppMsg = &freeMsg->pNext )
		if( freeMsg->pObj == pObj )
			{
			*ppMsg = freeMsg->pNext;
			delete freeMsg;
			bRes = true;
			break;
			}
	}
else
	{
	// Remove first message to this class with matching MSG_ type (lParam)
	for( ppMsg = &message; freeMsg = *ppMsg ; ppMsg = &freeMsg->pNext )
		if(( freeMsg->pObj == pObj ) && (freeMsg->msg == messg) )
			{
			*ppMsg = freeMsg->pNext;
			delete freeMsg;
			bRes = true;
			break;
			}
	}
		
return bRes;
}

//--------------------------------------------------------------------------
CObj* CRouter::untimeout_icmp(int16 messg, int16 zParam, int32* pTimeSent)
// untimeout_icmp - Unschedule a timer with specified MSG_ type and wParam
// Used to delete a ping no-response timer when a ping response is received.
// zParam is the ICMP reply id. A timeout object with matching ID is deleted.
// Returns: Time the timer was set and a pointer to pObj.
{
*pTimeSent = 0;
struct tagMESSAGE *freeMsg, **ppMsg;
CObj* pObj = NULL;

// Remove first message to this class with matching MSG_ type
for( ppMsg = &message; freeMsg = *ppMsg ; ppMsg = &freeMsg->pNext )
	if(	(freeMsg->msg == messg ) &&
		(freeMsg->zParam == zParam))
		{
		// Return timesent so response time can be calculated
		*pTimeSent = freeMsg->wParam;
		pObj = freeMsg->pObj;
			
		*ppMsg = freeMsg->pNext;
		delete freeMsg;
		break;
		}

return pObj;
}

//--------------------------------------------------------------------------
int32 CRouter::untimeout_rtx(CObj* pObj, int32 rx_ack_num)
// untimeout_rtx - Unschedule a retransmission timer
// Unschedules a rtx timer:
//  for the specified objet and
//	containing sequence numbers that are acknowledged
// Returns: Time the retransmission timer was set.
{
int32 timesent = 0;
struct tagMESSAGE *freeMsg, **ppMsg;
bool timer_deleted = false;

next:

// Remove first message to this class with matching MSG_ type (lParam)
for( ppMsg = &message; freeMsg = *ppMsg ; ppMsg = &freeMsg->pNext )
	if(( freeMsg->pObj == pObj ) && 
		(freeMsg->msg == MSG_RETRANSMISSION ) &&
		(freeMsg->lParam <= rx_ack_num))
		{
//sprintf(msg,"untimeout_rtx  seq_num=%lu rx_ack_num=%lu",(freeMsg->lParam + (int32)freeMsg->zParam),rx_ack_num);
//OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);	
		if(rx_ack_num == (freeMsg->lParam + (int32)freeMsg->zParam))
			{
			// Retransmission timer with (tx_seq_num + telnetbytes)== current rx_ack_num. 
			// Return timesent so RTT can be calculated
			timesent = freeMsg->wParam;	
			*ppMsg = freeMsg->pNext;
			delete freeMsg;
			timer_deleted = true;
			return timesent;
			}
			
		*ppMsg = freeMsg->pNext;
		delete freeMsg;
		timer_deleted = true;
		break;
		}

if(timer_deleted)
	{// A retransmission timer was found but for an earlier frame.
	// Keep looking for a timer corresponding exactly to rx_ack_num
	timer_deleted = false;
	goto next;
	}
	
return 0;
}

//--------------------------------------------------------------------------
bool CRouter::calltimeout()
// Calls the OnMessage function of specified object if it is time
// Returns true if a timeout occurred
{
int32 timenow;			// ms
bool bRet=false;
struct tagMESSAGE *pMsg;

while(message != NULL)
	{
	pMsg = message;
	
	timenow = getTickCount();

	if(pMsg->time > timenow)
	    break;		// no, it's not time yet

	// Call timeout routine
	message = pMsg->pNext;
	(pMsg->pObj)->OnMessage(pMsg->msg,pMsg->lParam,pMsg->wParam,pMsg->zParam);
	delete(pMsg);
	bRet=true;
	}
	
return bRet;
}

//--------------------------------------------------------------------------
void CRouter::delete_idle_processes(int32 currenttime)
// Delete unused L3 objects eg dead/closed TCP sessions
// ip_protocols[n]->enabled_flag is used to mark a protocol for deletion.
// ip_protocols[n]->autodelete flag indicates whether protocol should be deleted
// Either:
// Time of last frame received that contained data and idletimer_ms
// The enabled_flag
// .. is used to delete inactive clients with the autodelete flag set
{
CProtocol_L3 *protp;
int i;

for (i = 0;i < MAXL3CLIENTS ; ++i) 
	{
	if(((protp = ip_protocols[i]) != NULL) && (protp->autodelete))
		{
		if((currenttime - protp->time) > protp->idletimer_ms)				
			{
			// Inactive timed-out client
			if((protp->port_local == TELNET_PORT) || (protp->port_local== TRACE_PORT) ||
				 (protp->port_local == modem_config.local_port))
				{

				// A TCP/telnet client. Tell them they are being disconnected.
				protp->output((unsigned char*)szTCPClientTimeout,strlen((const char*)szTCPClientTimeout),ACK,protp->port_remote,HW_LAN);
		
				// Forced shutdown option
				//protp->output(0,0,RST,protp->port_remote,HW_LAN);
				
				// Clean shutdown option
				protp->close();
				// Another close attempt in 5s
				protp->time -= 5000L;
				}
			else
				// Mark for instant deletion
				protp->enabled_flag = false;
			}

		// Clients with deletion flag set
		if(protp->enabled_flag == false)
			{
			// Idle client found (TCP or UDP). Delete it
			// Outstanding timeout objects are deleted in base class destructor
			ip_protocols[i] = NULL;				
			delete(protp);
			}
		}
	}
}

//--------------------------------------------------------------------------
int CRouter::get_input_uarts(unsigned int IF_NUM)
// Copy data from a uart "IF_NUM" buffer to its owner CClient object
// A uart can be owned by either:
// a. Another uart if hw->uart[IF_NUM].owner < NUM_UARTS. The data is copied in hardware.cpp on reception (not implemented)
// b. A CClient object if hw->uart[IF_NUM].owner  >= NUM_UARTS

// This function handles case b.
// Also signal interface state changes in DTR to the owner object
{
CClient *pClient = (CClient*)hw->uart[IF_NUM].pClient;		// Pointer to the CClient object that owns this uart
if( !pClient)
	return 0;												// No CClient owner. The owner is another uart or none at all.

unsigned int owner = hw->uart[IF_NUM].owner;				// Who owns this uart; the trace buffer or another uart
unsigned char *pdata = &packet_buf[TCP_DATA_OFFSET+200];	// Temp buffer
unsigned int bytesread = 0;
unsigned int bytestoread = 0;
unsigned int client_bytes,uart_bytes;

/*
// Signal any RTS status change to the uarts owner
if(hw->uart[IF_NUM].flag_delta_rts)
	{
	pClient->OnTransport(TRANSPORT_DTEPORT,DELTA_RTS | (hw->uart[IF_NUM].flag_rts? RTS_STATE : 0));
	hw->uart[IF_NUM].flag_delta_rts = false;
	}
*/
// Signal any DTR status change to the uarts owner
if(hw->uart[IF_NUM].flag_delta_dtr)
	{
	pClient->OnTransport(TRANSPORT_DTEPORT,DELTA_DTR | (hw->uart[IF_NUM].flag_dtr? DTR_STATE : 0));
	hw->uart[IF_NUM].flag_delta_dtr = false;
	}

// Try to copy as many bytes as client can accept 
client_bytes = pClient->GetTransmitBuffer();
if( client_bytes == 0 )
	return 0;

// Read bytes from uart interface "IF_NUM" into temp buffer "pData"
if(IF_NUM == HW_TRACE)
	{
	// This is the debug trace output uart and it is owned by a CClient object.
	// Note if hw->uart[IF_NUM].pClient==NULL then trace data 
	// goes to uart hw->uart[IF_NUM].owner which will be HW_DTEx, or HW_NONE ... done in hardware.cpp
	
	if(traceout.pause)
		// User has paused the trace output
		return 0;
		
	// the trace buffer is a special case, since it is one way only (trace output)
	while(hw->uart[IF_NUM].uart_buffer_tx_in != hw->uart[IF_NUM].uart_buffer_tx_out)
		{
		pdata[bytesread++] = hw->uart[IF_NUM].uart_buffer_tx[hw->uart[IF_NUM].uart_buffer_tx_out];
		++hw->uart[IF_NUM].uart_buffer_tx_out;
		if (hw->uart[IF_NUM].uart_buffer_tx_out == hw->uart[IF_NUM].tx_buffer_len)
			hw->uart[IF_NUM].uart_buffer_tx_out=0;

		if(bytesread >= client_bytes)
			goto read_buffer_full;
		}
	// Fall through to read_buffer_full
	}
else
	{
	// This is a DTE port uart.
	// Check how many bytes in uart buffer
	uart_bytes = hw->uartInputQueue(IF_NUM);
	if( uart_bytes == 0 )
		return 0;

	// Forward data when either:
	// a. Inter character timeout expired AND (uart buffer < flow control HI threshold). A genuine inter char timeout.
	// b. If (uart buffer > flow control HI threshold). Congestion, send as much as client will accept.
	// TCP Note it is the remotes responsibility to report a sensible window size, using a delayed ACK if necessary.

	// Note on flag_interCharTimeoutRx_expire.
	// While uart input is idle, this flag is continually set to 1
	// After a char is received, it is set to 0 and then 1 after timeout expire
	if(( (hw->uart[IF_NUM].flag_interCharTimeoutRx_expire == 1) &&  (uart_bytes <= (hw->uart[IF_NUM].flowctrl_hi) ) ) ||
		( (uart_bytes >= (hw->uart[IF_NUM].flowctrl_hi) ) ))
		{
		// One of the data forwarding conditions is satisfied.
		// Calculate how many bytes to copy from the uart buffer to the client object
		bytestoread = min(client_bytes,uart_bytes);
/*
		sprintf(msg,"Reading %u bytes( client %u uart %u)\r\n",bytestoread,client_bytes,uart_bytes);
		serialout((const char*)msg,HW_DTE0);
*/
		do
			{
			hw->uartInputChar(IF_NUM,(unsigned int*)&pdata[bytesread++]);
			}while(bytesread < bytestoread);

/*
		sprintf(msg,"UART =%u TCP=%u read %u",uart_bytes,client_bytes,bytesread);
		OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
		// Echo the chars to the serial port
		for(int i=0; i< bytesread; i++)
			{
			serialout((const char*)&pdata[i],0,1);
			hw->uartOutputEmpty(0);
			};	
*/
		}
	}

read_buffer_full:
		
if(bytesread==0)
	return 0;
		
if(owner >= NUM_UARTS)				// If owner < NUM_UARTS the owner is another uart
	{
	// Trace chars
/*
	if((IF_NUM==HW_DTE0) && (traceout.module & LOG_SERIAL))
		{
		sprintf(msg,"<DTE Rx %u> ",bytesread);
		OutputDebugString(LOG_INFO,LOG_SERIAL,(const char*)msg,false);
		printformatstring(msg,0,(char*)pdata,min(bytesread,20),traceout.hexmode,false);
		OutputDebugString(LOG_INFO,LOG_SERIAL,(const char*)msg);
		if( hw->uart[IF_NUM].glitches_removed||
			hw->uart[IF_NUM].false_start_bits||
			hw->uart[IF_NUM].framing_error_count  )
			{
			sprintf(msg,"Serial: %u glitches %u start %u framing",hw->uart[IF_NUM].glitches_removed,hw->uart[IF_NUM].false_start_bits,hw->uart[IF_NUM].framing_error_count);
			hw->uart[IF_NUM].glitches_removed=0;
			hw->uart[IF_NUM].false_start_bits=0;
			hw->uart[IF_NUM].framing_error_count=0;
			OutputDebugString(LOG_INFO,LOG_SERIAL,(const char*)msg);
			}

		}
*/
	// Send DTE port data to the CClient software object pointed to by pClient
	pClient->serial_receive(&pdata,&bytesread);

	// Send clients' response back to the uart
	while(bytesread--)
			hw->uartOutputChar(IF_NUM,*(pdata++));
	}

// Hopefully, serial reception is restarted by now. And won't get a dummy expire
if(bytestoread)
	hw->uart[IF_NUM].flag_interCharTimeoutRx_expire = 0;
return 0;
}

//--------------------------------------------------------------------------
int CRouter::get_input_lan(bool discard_ip)
{
int len = 0;
short prot;
unsigned char *p;

plan->input_lan((char*)packet_buf,&len);

if(len == 0)
	return _IDLE;

//	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"router FRAME");
	
idle_arps_sent=0;		// Not idle

// Skip past target and src mac addresses
#ifdef SEB
// NB 8029 / 8019 Chips Received LAN frames have a 4 byte header prepended by the LAN card
p = &packet_buf[12 + 4];
#else
// Data starts with Ether hardware type field for 91c911 chip
p = &packet_buf[12];
#endif

GETSHORT(prot,p);

switch(prot)
	{
	case ETHERTYPE_ARP:
	arp->input_arp(p,len,0,0,HW_LAN);
	break;

	case ETHERTYPE_IPV4:
	if(!discard_ip)
		// Only interested in ARP frames during startup
		// (for address conflict checking and IP allocation)
		ipV4->input(HW_LAN,p,len);
	break;

	default:
	sprintf(msg,szUnrecognisedProtocol,prot);
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
	}

return _BUSY;		// busy - input received
}

#ifdef ROUTER

//--------------------------------------------------------------------------
int CRouter::monitor_incoming_calls()
// Look for "RING" received on free TA interfaces
// Allocate a ppp interface to the serial interface,
// and mark it as active
{
int IF_NUM;

// For each TA interface...
for(IF_NUM=NUM_DTE_INTERFACES; IF_NUM < (NUM_TA_INTERFACES + NUM_DTE_INTERFACES); IF_NUM++)
	{
	if( hw->uart[IF_NUM].owner == OWNER_NONE )
		{
		// Check for an incoming call.
		if(ppp->serialin(IF_NUM,(unsigned char*)"RING"))
			{
			OutputDebugString(LOG_INFO,LOG_PPP,szIncomingCall);
		
			// Allocate call / TA to a ppp interface
			// TODO based on CLI??
		
			for(IF_NUM=0; IF_NUM<NUM_PPP_LINKS ; IF_NUM++)
				{
				if(ppp_if[IF_NUM].phase == PHASE_DEAD)
					{
					ppp_if[IF_NUM].inactivitytimer = getTickCount();
					ppp_if[IF_NUM].phase = PHASE_INITIALIZE;
					// Allocate serial port to a ppp_if
					// and answer the call
					ppp->OpenComms(IF_NUM,LISTEN);
					return 0;
					}
				}
			// Incoming call but no free ppp interfaces !
			OutputDebugString(LOG_INFO,LOG_PPP,"No free PPP interface for call");		
			return 0;
			}
		}
	}
return 0;
}

//--------------------------------------------------------------------------
int CRouter::get_input_ppp(int unit)
// Service serial interfaces (to ISDN adapter).
{
int len, i;
unsigned char *p;
int16 protocol;
CProtocol *protp;

if(HW_NONE==ppp_if[unit].HW_IF)
	return _IDLE;

if(ppp_if[unit].phase < PHASE_ESTABLISH)
	return _IDLE;

p = packet_buf;	// point to beginning of packet buffer
ppp->input(packet_buf,&len,unit);

if (len < 0)
	return _IDLE;

if (len == 0) 
	{
	OutputDebugString(LOG_NOTICE,LOG_PPP,szCarrierLost);
	lcp->lowerdown(unit);	// serial link is no longer available
    ppp_if[unit].phase = PHASE_DEAD;
	return _IDLE;
	}

if (len < PPP_HDRLEN) 
	{
	sprintf(msg,szShortPacket,len);
	OutputDebugString(LOG_ERR,LOG_PPP,(const char*)msg);
	return _IDLE;
	}

GETSHORT(protocol, p);
len -= PPP_HDRLEN;

// Toss all non-LCP packets unless LCP is OPEN.
if ((protocol != PPP_LCP) && (lcp_fsm[unit].state != OPENED)) 
	{
	sprintf(msg,szRcvdNonLCP,protocol);
	OutputDebugString(LOG_ERR,LOG_PPP,(const char*)msg);
	return _IDLE;
	}

// Until we get past the authentication phase, toss all packets
// except LCP, LQR and authentication packets.
if (ppp_if[unit].phase <= PHASE_AUTHENTICATE
	&& !(protocol == PPP_LCP || protocol == PPP_LQR
    || protocol == PPP_PAP || protocol == PPP_CHAP)) 
	{
	OutputDebugString(LOG_ERR,LOG_PPP,szDiscardingProtocol);
	return _IDLE;
	}
		
// Upcall the proper protocol input routine.
for (i = 0; (protp = (CProtocol*)ppp_protocols[i]) != NULL; ++i) 
	{
	if (protocol == protp->protocol && protp->enabled_flag) 
		{
	    protp->input(unit, p, len);
	    return _BUSY;
		}
	
	// If the first byte of the protocol is ODD, the frame is protocol compressed
	if(protocol & ~0x0100)
		if ((protocol>>8) == (protp->protocol & ~0x8000) && protp->enabled_flag)
			{
			// Protocol compressed frame
			protp->input(unit, p-1, len);
			return _BUSY;	// Don't enter idle;
			}
	}


sprintf(msg,szUnsupportedProtocol,protocol);
OutputDebugString(LOG_ERR,LOG_PPP,(const char*)msg);
lcp->lcp_sprotrej(unit, p - PPP_HDRLEN, len + PPP_HDRLEN);
return _BUSY;	// Don't enter idle
}

//--------------------------------------------------------------------------
void CRouter::init_ppp()
// Called at startup to init each PPP link (unit)
{
/// PPP Interface defaullts
for(int i=0; i < NUM_PPP_LINKS; i++)
	{
	ppp_if[i].HW_IF = HW_NONE;
	ppp_if[i].dialnumber[0] = NULL;
	ppp_if[i].password[0] = NULL;
	ppp_if[i].auth[0] = NULL;

	ppp_if[i].ip_addr = 0;
	ppp_if[i].netmask = 0xffffff00;
	ppp_if[i].subnet = ppp_if[i].ip_addr & ppp_if[i].netmask;
	ppp_if[i].maxinactivitytime = LINK_INACTIVITY_TIMEOUT;
//	ppp_if[i].inactivitytimer = 1000;
//	ppp_if[i].maxestablishtime = LINK_CONNECT_TIME;
	ppp_if[i].phase = PHASE_DEAD;
	ppp_if[i].demand = true;
	ppp_if[i].answer = true;
	ppp_if[i].dirty = false;

	// Copy link interface settings to configuration scratch area
	memcpy(&ppp_if[i + NUM_PPP_LINKS],&ppp_if[i],sizeof(ppp_interface));
	}

// Debug add PPP WAN interface
strcpy((char*)ppp_if[0].dialnumber,(char*)"384020");
strcpy((char*)ppp_if[0].password,(char*)"bbb1234");
strcpy((char*)ppp_if[0].auth,(char*)"bbb1234");

// Initiate periodic ICMP router discovery advertisments
icmp->timeout(MSG_ICMP_ROUTER_ADVERT,10);

magic = new CMagic;
lcp = new Clcp;
cfsm = new CFsm;
pap = new CPap;
auth = new CAuth;
ipcp = new Cipcp;
//chap = new Cchap;
//mlcp = new Cmlcp;
ppp = new Cppp;

// PPP data link layer protocol table.
// NB the order of protocols in this array is important !!
// The last entry must be NULL
ppp_protocols[0]= (CProtocol*)lcp;
ppp_protocols[1]= (CProtocol*)pap;
ppp_protocols[2]= (CProtocol*)ipV4;	// must be before ipcp (protocol number & protocol compression dictates)
ppp_protocols[3]= (CProtocol*)ipcp;
ppp_protocols[4]= NULL;//(CProtocol*)mlcp;
ppp_protocols[5]= NULL;
//ppp_protocols[2]= NULL;//(CProtocol*)chap;

num_np_up = 0;

int i,n,unit;
CProtocol *protp;

for(n=0; n < NUM_PPP_LINKS; n++)
	{	
	// Initialize to the standard option set
	for (i = 0; (protp = (CProtocol*)ppp_protocols[i]) != NULL; ++i)
		{// Initialize to the standard option set
		protp->init(n);
		// Check that the options given are valid and consistent.
		protp->check_options();
		}
	}

for(unit=0; unit < NUM_PPP_LINKS; unit++)
	{
	// Initialise PPP
	ppp->init(unit);

	// Initialize magic number package.
	magic->init();

	// Open the serial port for incoming calls
	if(ppp_if[unit].demand)
		{
		ppp_if[unit].npi[0].protocol = PPP_IP;
		//ppp->sifup(unit);
		//ppp->sifnpmode(unit,PPP_IP,NPMODE_QUEUE);
		}
	}
}

//--------------------------------------------------------------------------
int CRouter::manage_wan_link(int if_num)
// State machine for dial on demand links
// Terminate failed call requests
// Clear idle links
// Answer incoming calls
{
#ifdef WIN32
	int uartNo;
#endif

bool bCloseLink = false;
int HW_IF;
if(HW_NONE == (HW_IF = ppp_if[if_num].HW_IF))
	{
	// This PPP interface is a configuration place holder as
	// no ISDN/DTE port is assigned to it
	return 0;
	}
	
switch(ppp_if[if_num].phase)
	{
	case PHASE_DEAD:
//	nb. case PHASE_DEAD: is handled by function monitor_incoming_calls()
	break;

	case PHASE_INITIALIZE:
	// Waiting for "CONNECT"
	if(ppp->serialin(HW_IF,(unsigned char*)"CONNECT"))
		{
		OutputDebugString(LOG_INFO,LOG_PPP,szCallAnswered);
		
		ppp_if[if_num].phase = PHASE_ESTABLISH;

		lcp->lowerup(if_num);
		lcp->open(if_num);		// Start protocol
		break;
		}

	if(((getTickCount() - ppp_if[if_num].inactivitytimer)/100) > LINK_CONNECT_TIME)
		{// Connect timeout : close the link
		OutputDebugString(LOG_WARNING,LOG_PPP,(const char*)"Initialise phase inactivity timeout");
		bCloseLink = true;
		}

	break;

	case PHASE_AUTHENTICATE:
	case PHASE_ESTABLISH:
	// ISDN up, negotiating LCP and network protocols.
	if(((getTickCount() - ppp_if[if_num].inactivitytimer)/100) > LINK_ESTABLISH_TIME)
		{// LCP negotiation timeout : close the link
		OutputDebugString(LOG_WARNING,LOG_PPP,(const char*)"Link establish phase timeout");
		bCloseLink = true;
		}

	// Check DCD is up. If not close the link
#ifndef WIN32
// Win32. CTA puts DCD up 250ms after "CONNECT" causing ppp->linkup() to report false here
	if(!ppp->linkup(if_num))
		{// ISDN link dropped unexpectedly : close it
		// TODO could re-establish link here.
		OutputDebugString(LOG_WARNING,LOG_PPP,(const char*)"Link dropped in LCP negotiation phase");
		bCloseLink = true;
		}
#endif
	break;


	case PHASE_NETWORK:
	// Link and network protocols up
	if(ppp_if[if_num].inactivitytimer != 0)
		if(((getTickCount() - ppp_if[if_num].inactivitytimer)/100) > ppp_if[if_num].maxinactivitytime)
			{
			// Inactivity timeout : close the link
			OutputDebugString(LOG_NOTICE,LOG_PPP,(const char*)"Network phase inactivity timeout\r\n");
			bCloseLink = true;
			ppp_if[if_num].phase = PHASE_DEAD;
			}

	// Check DCD is up. If not close the link
	#ifdef WIN32
		if(!ppp->linkup(if_num))
	#else
		if(atdec[if_num]->BiosDataInterface.V120_online_state_L3!=10)
	#endif
		{// ISDN link dropped unexpectedly : close it
		// TODO could re-establish link here.
		OutputDebugString(LOG_NOTICE,LOG_PPP,(const char*)"Link dropped in network phase");
		bCloseLink = true;
		}

	break;

	case PHASE_TERMINATE:
	case PHASE_HOLDOFF:
		// Wait period before hanging up (although I don't)
		// Connection terminated by LCP.
		bCloseLink = true;
		break;

	default:
		OutputDebugString(LOG_DEBUG,LOG_PPP,(const char*)"Error unknown PHASE_");
		bCloseLink = true;
	break;
	}

if(bCloseLink)
	{
#ifndef WIN32
	atdec[HW_IF-2]->BiosDataInterface.V120_online_request=-1;
#endif
	ppp_if[if_num].phase = PHASE_DEAD;
	lcp->lowerdown(if_num);
	lcp->close(if_num,"Closing PPP interface");
	ppp->init(if_num);
	//ppp->OpenComms(if_num,LISTEN);		// Closes and re-opens the com port (forces hangup)
	}

return false;
}

//--------------------------------------------------------------------------
void demand_link(void *arg)
// Bring up a PPP demand dial interface (dial,connect, auth)
{
CProtocol *protp;
unsigned int *unit = (unsigned int*)arg;
int i;

if(*unit > (NUM_PPP_LINKS-1)) 
	return;		// Should never get here

if(ppp_if[*unit].phase >= PHASE_INITIALIZE)
	return;		// Link in use

if(ppp_if[*unit].demand == false)
	return;		// Permanent link

// Initialize to the standard option set
for (i = 0; (protp = (CProtocol*)ppp_protocols[i]) != NULL; i++)
	protp->init(*unit);

sprintf(msg,"Bringing up ISDN%d",*unit);
OutputDebugString(LOG_INFO,LOG_PPP,(const char*)msg);

// Reinitialise the PPP interface
//ppp->init(pp_if[*unit].HW_IF);
ppp_if[*unit].phase = PHASE_INITIALIZE;
ppp_if[*unit].inactivitytimer = getTickCount();

//ppp->OpenComms(*unit,DIAL);
}
#endif // SEB


/*
//--------------------------------------------------------------------------
int CRouter::open(int unit,bool neg_multilink, char *endpoint_discriminator,int discriminator_len)
// Negotiate LCP on connected link 
// Multilink may be negotiated for Point to Point links. 
// If multilink is negotiated, a unique endpoint_discriminator is also negotiated with the peer.
// If a link is added to the bundle, it must use the same endpoint_discriminator (rfc1990)
{
extern lcp_options lcp_wantoptions[NUM_PPP_LINKS];		// Options that we want to request
extern lcp_options lcp_allowoptions[NUM_PPP_LINKS];		// Options we allow peer to request
extern lcp_options lcp_gotoptions[NUM_PPP_LINKS];		// Options we allow peer to request
extern lcp_options lcp_hisoptions[NUM_PPP_LINKS];		// Options we allow peer to request

//lcp_wantoptions[unit].neg_multilink = neg_multilink;// todo. this fails !
lcp_allowoptions[unit].neg_multilink = neg_multilink;
if(neg_multilink)
	{// If the discriminator value matches another link, peer will add us to the bundle
	if(unit == 1)
		{
		// MLPPP kludge
		//lcp_wantoptions[1].neg_discriminator = 1;
		//lcp_wantoptions[1].address_len = lcp_hisoptions[0].address_len;
		//memcpy(lcp_wantoptions[1].address,lcp_hisoptions[0].address,lcp_hisoptions[0].address_len);
		}

	//memcpy(lcp_wantoptions[unit].address, endpoint_discriminator,discriminator_len);
		lcp_allowoptions[unit].neg_discriminator = true;

	if(unit == 1)
		{
		lcp_wantoptions[unit].neg_discriminator = true;
		char *p = lcp_gotoptions[0].address;
		char *address = lcp_wantoptions[unit].address;
		char cilong;
		int cilen = lcp_gotoptions[0].address_len ;
		for(int i = 0; i < cilen ;i++)
			{
			GETCHAR(cilong, p);
			PUTCHAR(cilong, address); // address points to wo->address
			}
		}

		//	memcpy(lcp_wantoptions[unit].address,lcp_gotoptions[0].address,20);
	}

return 0;
}
*/


