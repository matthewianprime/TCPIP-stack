// router.h
// Prototypes for procedures local to this file.
#include "strings.h"
typedef unsigned int int16;
typedef unsigned long int32;

#define MSG_ALL					0
#define MSG_TIMER				1
#define MSG_RETRANSMISSION		2
#define MSG_ECHO_REQUEST		3
#define MSG_ECHO_REPLY			4
#define MSG_ARP_REQUEST			5
#define MSG_ARP_REPLY			6
#define MSG_DHCP_DISCOVER_ADDR	7
#define MSG_DHCP_REQUEST_ADDR	8
#define MSG_DHCP_RENEW_ADDR		9
#define MSG_DHCP_ACK_ADDR		10
#define MSG_DHCP_DECLINE_ADDR	11
#define MSG_DHCP_T2_TIMER		12
#define MSG_DHCP_T3_TIMER		13
#define MSG_ICMP_ROUTER_ADVERT	14
#define MSG_KEEPALIVE			15
#define MSG_ARP_PERSIST			16

// Shell / profile constants.These define:
// The default command shell to load ( config.shell )
// Shell defaults are set by calling ATFactoryReset( PROFILE_x)
#define	PROFILE_NONE			0
#define PROFILE_AT				1
#define PROFILE_PAD				2
#define PROFILE_TELNET			3
#define	PROFILE_PRI				4
#define	PROFILE_ELSTER			5
#define	PROFILE_ADPRO			6
#define PROFILE_PRINTER			7
#define PROFILE_EASE			8
#define PROFILE_GALAXY			9
#define PROFILE_DCU				10
#define PROFILE_2217			11

#define STARTUP_OK					0
#define STARTUP_LAN_NOT_CONNECTED	1
#define STARTUP_NEED_MAC			2
#define STARTUP_NEED_IP				3
#define STARTUP_DUPLICATE_IP		4
#define STARTUP_DHCP_FAIL			5

// returns from pppmain and init
#define SEB_OK					0
#define LAN_NOT_CONNECTED		1
#define SEB_RESTART				2

class CObj
// Base class for objects requiring a message handler
{
public:
CObj();
~CObj();
// Routine receiving messages
virtual void OnMessage(int16 msg, int32 lParam,int32 wParam,int16 zParam=0);
// Send a message to specified object
virtual void PostMessage(CObj* pObj,int16 msg,int32 lParam,int32 wParam,int16 zParam,int32 time);
virtual void PeekMessage(int32,int32);

void timeout(int32 lParam,int32 time);				// Set a callback timer running
bool untimeout(int32 messg, CObj* pObj=0);			// Remove all callbacks of specified MSG_ type
};

struct tagMESSAGE
{
int16		msg;					// MSG_ constant. Defines which function is called.
int32 		lParam;					// Param for OnMessage
int32		wParam;					// Param for OnMessage
int16		zParam;					// Param for OnMessage
int32		time;					// Time to callback
CObj		*pObj;					// Points to class receiving callback
struct 		tagMESSAGE	*pNext;		// Next message to be sent
};

extern tagMESSAGE* message;

// Copy int16 mac address to character array[6]
#define PUTMAC(pmac, cp) { \
	*(cp++) = 0x00ff & *(pmac);\
	*(cp++) = 0x00ff & *(pmac)>> 8;\
	*(cp++) = 0x00ff & *(pmac + 1);\
	*(cp++) = 0x00ff & *(pmac + 1)>> 8;\
	*(cp++) = 0x00ff & *(pmac + 2);\
	*(cp++) = 0x00ff & *(pmac + 2)>> 8;}

// Copy character array[6] to int16[3] mac address
// Reverse the byte order.
#define GETMAC(pmac, cp){ \
	*(pmac) = *( cp++  );\
	*(pmac) += *(cp++) << 8;\
	*(pmac + 1) = *( cp++ );\
	*(pmac + 1) += *(cp++) << 8;\
	*(pmac + 2) = *( cp++ );\
	*(pmac + 2) += *(cp++) << 8;}
	
#undef  FALSE
#define FALSE	0
#undef  TRUE
#define TRUE	1
#undef  NULL
#define NULL	0

const int MAXLEN 			= 10;
const int MAXLEN1 			= 20;
const int MAXLEN2 			= 30; 
const int UART_BUFLEN		= 1024;						// Must be 2^n
const int TRACE_BUFLEN		= 256;						// Debug trace buffer
const int UART_FULL_THRESHOLD = UART_BUFLEN *8/10;		// Threshold to read UART buffer

extern unsigned char *msg;								// for sprintf
extern unsigned char ipstr[MAXLEN2];					// for ip_ntoa,ip2ascii etc
extern unsigned char ifstr[MAXLEN1];
extern unsigned char baudstr[MAXLEN1];
extern unsigned char flowstr[MAXLEN1];
extern unsigned char ownerstr[MAXLEN1];
extern unsigned char macstr[MAXLEN1];

#ifdef SEB
const int NUM_DTE_INTERFACES =	1;	// Serial ports
const int NUM_TA_INTERFACES =	0;	// Physical links, ISDN or X21
const int TRACE_INTERFACE 	=	1;	// Trace / stdout buffer
const int NUM_X21_INTERFACES =	0;	// Physical links, ISDN or X21
const int NUM_LAN_INTERFACES =	1;	// Physical links, ISDN or X21
const int NUM_UARTS = NUM_DTE_INTERFACES + NUM_TA_INTERFACES + TRACE_INTERFACE;	// Serial ports
#else
const int NUM_DTE_INTERFACES =	2;	// Serial ports
const int NUM_TA_INTERFACES =	2;	// Physical links, ISDN or X21
const int TRACE_INTERFACE 	=	1;	// Trace / stdout buffer
const int NUM_X21_INTERFACES =	1;	// Physical links, ISDN or X21
const int NUM_LAN_INTERFACES =	1;	// Physical links, ISDN or X21
const int NUM_UARTS = NUM_DTE_INTERFACES + NUM_TA_INTERFACES + TRACE_INTERFACE;	// Serial ports
#endif

// Hardware interface "HW" constants in struct hw_interface.
// These define the type of interface

// Also used to describe the routing interface
#define HW_CLASS_DTE	100							// A DTE port
#define HW_DTE0			0							// 0 First DTE port
#define HW_DTE1			1							// 1 Second DTE port
#define HW_TA			NUM_DTE_INTERFACES			// 2 First AT decoder #1
#define HW_TRACE		HW_TA + NUM_TA_INTERFACES	// 4 Like a TA or DTE port. Debug output buffer
#define HW_X21			HW_TRACE + TRACE_INTERFACE	// 5 X21 port
#define HW_LAN			HW_X21 + NUM_X21_INTERFACES	// 6 LAN

#define HW_ISDN			NUM_UARTS + 99	// A request for either AT decoder 1 or 2
#define HW_NONE			0xffff	// Not allocated / disconnected / INVALID_HANDLE_VALUE
#define HW_ALL			0xfffe	// Used only as a wildcard

// Hardware interface "owner" constants in struct hw_interface.
// These are further qualified in the structure by either:
//	a:/ A "this" object pointer (sw) in pClient
//  b:/ An buffer array offset (hw) in 
#define OWNER_UART		NUM_UARTS + 0	// Value not used,use HW_ instead
#define OWNER_DTE		NUM_UARTS + 1	// Hardware.Used only as a calling param "any DTE port"
#define OWNER_TA		NUM_UARTS + 2	// Hardware.Used only as a calling param "any TA"
#define OWNER_PPP		NUM_UARTS + 3	// Software object
#define OWNER_SW_OBJECT	NUM_UARTS + 4	// Special case. PPP polls its UART buffers
#define OWNER_TCPPORT	NUM_UARTS + 5	// Software object
#define OWNER_UDPPORT	NUM_UARTS + 6	// Software object
#define OWNER_SHELL		NUM_UARTS + 7	// Software object console
#define OWNER_NONE		NUM_UARTS + 8	// Avail

#define TRACE_BUFFER_SEND_THRESHOLD_MS		100		// Todo use % of buffer size
#define FAST_IDLE_SERVICE_PERIOD_MS			10		// How often to service trace out buffer
#define SLOW_IDLE_SERVICE_PERIOD_MS			200		// How often to service timeouts
#define VERY_SLOW_IDLE_SERVICE_PERIOD_MS	60000	// Memory low warning

class CRouter:public CObj
{
public:
void timeout(tagMESSAGE* message);
bool untimeout(CObj* pObj,int16 mssg);
bool calltimeout();
int32 untimeout_rtx(CObj* pObj, int32 rx_ack_num);
CObj* untimeout_icmp(int16 messg, int16 zParam, int32* pTimeSent);
void RebootIfIdle();

int pppmain(bool);
int get_input_lan(bool discard_ip=false);
int get_input_uarts(unsigned int IF_NUM);
bool link_test();

private:
void init_ppp();
int get_input_ppp(int);
void init_defaults();
int open(int unit,bool neg_multilink, char *endpoint_discriminator,int discriminator_len);
int manage_wan_link(int unit);
void delete_idle_processes(int32);
int restore_data();
int monitor_incoming_calls();
};

