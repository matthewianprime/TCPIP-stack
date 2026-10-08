// pppd.h - global declarations.

#define LAN_MRU	1500				// Max size for a LAN frame.
#define FLASH_AUTOSTART_BOOT    (0X0048FF80L)

extern bool output_ppp(int PPP_IF, unsigned char* outp_buf,unsigned int len,unsigned int protocol, bool PPPasync);
extern int16 mac_addr[];

#include <string.h>
#include <stdlib.h>		// TI atoi, atol
#define HI	1
#define LO	0

// state variables
#define LINESTATE_OFFLINE		0
#define LINESTATE_DIAL_WAIT		1
#define LINESTATE_ANSWER_WAIT	2
#define LINESTATE_ONLINE		3
#define LINESTATE_ONLINE_CMDS	4

#define OFFLINE					0
#define ONLINE					1

#define CH_CR				0x0d
#define CH_LF				0x0a
#define CH_DLE				0x10		// ^P
#define CH_DEL				0x08
#define CH_CTRLX			0x18
#define CH_CTRLR			0x12
#define PARITY_MASK			0x7f

// String lengths
#define NUM_PPP_PROTOCOLS 	10
#define NUM_LAN_LINKS		1
const int NUM_PPP_LINKS 	= 5;				// Logical PPP link entities

#ifdef SEB
const int RTABLE_SIZE 		= 5;
#else
const int RTABLE_SIZE 		= 20;
#endif

#define IP				0x21
#define IP_HEADERLEN	20
#define TELNET_PORT		23
#define HTTP_PORT		80
#define TA_PORT			1000
#define TA_PORT1		1001
#define TA_PORT2		1002
#define MODEM_PORT		2000
#define MODEM_PORT1		2001
#define MODEM_PORT2		2002
#define PRINTER_PORT	9100
#define SMTP_PORT		25
#define POP3_PORT		110
#define TRACE_PORT		84		// ctf port Common Trace Facility
#define DSP_DISCOVER_PORT	5050
#define ECHO_ON			0
#define ECHO_OFF		1
#define ECHO_STAR		2	// Echo '*' if secure information

// Forward class declarations....
class Chw;
class CInit;
class Clcp;
class CFsm;
class CPap;
class CMagic;
class CRouter;
class CAuth;
class Cppp;
class Cmlcp;
class CProtocol;
class CProtocol_L3;
class Clan;
class Carp;
class Cicmp;
class Cdhcp;
class CipV4;
class CCommandPort;
class CTelnet;
class CTCPModem;
class Cmatrix;
class CClient;
class Cffs;

extern Cffs *ffs;
extern Chw *hw;
extern Clcp *lcp;
extern CFsm *cfsm;
extern CMagic *magic;
extern CAuth *auth;
extern CPap *pap;
extern Cppp *ppp;
extern Cmlcp *mlcp;
extern Clan *plan;
extern Carp *arp;
extern Cicmp *icmp;
extern Cdhcp* dhcp;
extern CipV4 *ipV4;
extern CRouter *router;
extern CClient *shell;
extern Cmatrix *matrix;

extern int dirty;							// New LAN settings made but not applied flag
extern int auto_ip_assigned_address;		// Set when auto ip receives a new IP address
extern bool discard_ip_frames;				// Set when router is initialised
extern int get_input_lan(int unit);

typedef struct npioctl
{
int protocol;
int mode;
}npioctl;


typedef struct ppp_interface
// PPP interface attributes - independent of physical hardware
{
// State handling vars
int phase;				// The state the link is at
int HW_IF;				// hardware_interface[n] number "n" (ISDN / X21 etc)
int dirty;				// Flags a config change

int32 ip_addr;			// This links IP address
int32 netmask;			// Remote network netmask
int32 subnet;			// Remote subnet

int32 inactivitytimer;	// Time of last good PPP frame received
int32 maxinactivitytime;	// Inactive time before link will be reset
int32 maxestablishtime;	// Time for link to connect
int demand;				// Link allows/support demand dial connection
int answer;				// Auto answer incoming call

unsigned char auth[MAXLEN1];					// Username
unsigned char password[MAXLEN1];				// Password
unsigned char dialnumber[MAXLEN1];


// PPP default params that are negotiated on a connection basis and not saved
int16 mru;
int16 mtu;
int16 peer_mru;
int address_len;
int32 xmit_accm;
int32 rcv_accm;
int32 peer_ip_addr;
int aCompress;
int pCompress;
int vjCompress;
int multilink;
int useChap;

unsigned char peer_address[MAXLEN1];
struct npioctl npi[NUM_PPP_PROTOCOLS];
}ppp_interface;
extern ppp_interface ppp_if[];

typedef struct RAS
{
int32 enableras;
int32 PermitLANAccess;
}RAS;
extern RAS ras;

// Routing table if_num definitions
#define IP_LAN			5	// LAN hardware
#define PPP_WAN			1	// PPP WAN hardware.
#define LOOPBACK		3	// ??future use

struct tag_rtable
{
int32 dest_net;				// Subnet for remote network
int32 dest_mask;			// Netmask of remote net
int32 gateway;				// Next hop router. 0 = directly connected
int16 if_num;				// See definitions above (WAN,LAN....)
};
extern tag_rtable rtable[];


#define MAXINT 0xffffffff

//**********************************************************************
//*********************** Debug trace struct / constants  *************
//**********************************************************************

// Trace output attributes global structure.Only one needed.
struct tag_traceout
{
int32 	module;		// LOG_LAN -> LOG_NONE constant
int		level;		// Severity level
int		suppress;	// Switch on while trace message is output
int 	overrun;	// Trace buffer overflow flag
int 	pause;		// Trace user paused trace
int 	hexmode;	// Hex data display flag
};extern tag_traceout traceout;

// traceout.level constants
//#define LOG_NONE		0			// No errors
#define LOG_ERR			1			// Baaad errors
#define LOG_WARNING		2
#define LOG_NOTICE		3	
#define LOG_INFO		4			// Minor things
#define LOG_DEBUG		5			// Programmer stuff / all
#define LOG_DEFAULT		LOG_INFO	// Default logging level

// traceout.module constants. LOG_DEBUG used for non-category stuff
#define LOG_LAN			1
#define LOG_PPP			2
#define LOG_ARP			4
#define LOG_LCP			8	// and ipcp
#define LOG_AUTH		16
#define LOG_TCP			32
#define LOG_IP			64
#define LOG_SYSTEM		128
#define LOG_HTML		256
#define LOG_TCPMODEM	512
#define LOG_ICMP		1024
#define LOG_DHCP		2048
#define LOG_UDP			4096
#define LOG_BUG			8192
#define LOG_SERIAL		16384			// Undocumented serial port trace
#define LOG_ALL			32767		// Does not include LOG_BUG
#define LOG_NONE		0

//**********************************************************************


#define LINK_INACTIVITY_TIMEOUT	3000		// Drop inactive WAN link timer.300 = 30s
#define LINK_CONNECT_TIME		100			// Wait for CONNECT timer. 100ms units
#define LINK_ESTABLISH_TIME		200			// LCP negotiation time limit
#define IDLE_SERVICE_PERIOD_MS 	500			// Idle tasks
#define LAN_IDLE_TIME_SENDARPS	60000*30	// Reinitialise LAN after 30 Minutes idle
#define IDLE_ARP_SEND_LIMIT		24			// Reboot after repeating arp on idle count

#define NO_IDLE_TIMEOUT  		0xffffffffL					// No idle timeout constant
#define UDP_IDLE_TIMEOUT_MSECS  30L*1000L			// UDP session idle timeout. 30 Secs
#define TCP_IDLE_TIMEOUT_MSECS  20L*60L*1000L		// TCP session idle timeout. 20MINS
#define TCP_MODEM_IDLE_TIMEOUT_MSECS  5L*60L*1000L	// TCP session idle timeout. 5MINS
//#define _BUSY					1
#define _IDLE					0

extern CProtocol *ppp_protocols[];			// Link layer protocols array
extern CProtocol_L3 *ip_protocols[];		// TCP, UDP etc
#ifdef SEB
#define MAXL3CLIENTS	5					// Maximum ip client protocols. One is always ARP, one DHCP (Option)
#else
#define MAXL3CLIENTS	6					// 
#endif

// Ethertype protocol values - in ethernet frame header
#define ETHERTYPE_ARP	0x806
#define ETHERTYPE_IPV4	0x800
#define ETHERTYPE_RARP	0x8035

// The basic PPP frame.
#define MP_HEADERLEN	4					// Max length. 8 bytes. Can be 6 bytes by negotiation
#define PPP_HDRLEN		4					// octets for standard ppp header
#define PPP_FCSLEN		2					// octets for FCS
#define PPP_MRU			1500				// default MRU = max length of info field
#define TCP_DATA_OFFSET	220					// Transmitted TCP frames made up at this offset in packet_buf
#define UDP_DATA_OFFSET 220					// Transmitted UDP frames made up at this offset in packet_buf

// Significant octet values.
#define	PPP_ALLSTATIONS	0x00ff				// All-Stations broadcast address
#define	PPP_UI			0x0003				// Unnumbered Information
#define	PPP_FLAG		0x007e				// Flag Sequence
#define	PPP_ESCAPE		0x007d				// Asynchronous Control Escape
#define	PPP_TRANS		0x0020				// Asynchronous transparency modifier

#define PPP_IP			0x21				// Internet Protocol
#define PPP_AT			0x29				// AppleTalk Protocol
#define PPP_IPX			0x2b				// IPX protocol
#define	PPP_VJC_COMP	0x2d				// VJ compressed TCP
#define	PPP_VJC_UNCOMP	0x2f				// VJ uncompressed TCP
#define PPP_MP			0x3d				// Multilink protocol
#define PPP_IPV6		0x57				// Internet Protocol Version 6
#define PPP_COMPFRAG	0xfb				// fragment compressed below bundle
#define PPP_COMP		0xfd				// compressed packet
#define PPP_IPCP		0x8021				// IP Control Protocol
#define PPP_ATCP		0x8029				// AppleTalk Control Protocol
#define PPP_IPXCP		0x802b				// IPX Control Protocol
#define PPP_IPV6CP		0x8057				// IPv6 Control Protocol
#define PPP_CCPFRAG		0x80fb				// CCP at link level (below MP bundle)
#define PPP_CCP			0x80fd				// Compression Control Protocol
#define PPP_LCP			0xc021				// Link Control Protocol
#define PPP_PAP			0xc023				// Password Authentication Protocol
#define PPP_LQR			0xc025				// Link Quality Report protocol
#define PPP_CHAP		0xc223				// Cryptographic Handshake Auth. Protocol
#define PPP_CBCP		0xc029				// Callback Control Protocol

// Lengths of lcp configuration options.
#define CILEN_VOID		2
#define CILEN_COMPRESS	4					// min length for compression protocol opt.
#define CILEN_VJ		6					// length for RFC1332 Van-Jacobson opt.
#define CILEN_ADDR		6					// new-style single address option 
#define CILEN_ADDRS		10					// old-style dual address option 

// Values for FCS calculations.
#define PPP_INITFCS	0xffff	// Initial FCS value
#define PPP_GOODFCS	0xf0b8	// Good final FCS value
#define PPP_FCS(fcs, c)	(((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])

// Limits.
#define MAXWORDLEN		1024				// max length of word in file (incl null) 
#define MAXARGS			1					// max # args to a command 
#define MAXNAMELEN		256					// max length of hostname or name for auth 
#define MAXSECRETLEN	256					// max length of password or secret 

// TCP header flags
#define SYN 0x02
#define FIN 0x01
#define RST 0x04
#define PSH 0x08
#define ACK 0x10
#define URG 0x20

#define TCP_HLEN			5				// 32-bit words. May be extended if there are options
#define DATAOFFSET			TCP_HLEN
#define TCP_HEADERLEN		TCP_HLEN * 4	// Bytes
#define DEFAULT_TCP_SEGMENT_SIZE	1460	// My guess at a default value.

// Indexes for configuration data arrays (lanport, modem_config, call_bar)
#define ACTIVE			0
#define UNSAVED			1

// Values for npioctl.mode
#define NPMODE_NONE		0
#define NPMODE_QUEUE	1
#define NPMODE_DISCARD	2
#define NPMODE_PASS		3
#define NPMODE_ERROR	4

typedef struct tagmodem_config
{
int	echoflag;
int	verboseflag;
int quietflag;
int dcdflag;
int	dtrflag;
int	S0_answer_rings;
int S2_escape_char;
int S42_dialabort;
int ATX;
int autobaud_flag;
int baudrate_flag;
int character_time_uS;							// Time to send 1 char at current baudrate
int parity_flag;
int flowcontrol_flag;
int16 remote_port;
int16 local_port;
int16 shell;									// Interface for serial port
int32 dtrdialnumber;
bool com_enable_rfc2217;
char at_cmd[20];								// Default AT commands
int32 tcp_idletimer;							// Time out idle TCP sessions timer
}tagmodem_config;
extern tagmodem_config modem_config;						// AT decoder settings

typedef struct tagmail_config
{
int	echoflag;
int16 smtp_remote_port;
int16 smtp_local_port;
//int16 pop3_remote_port;
//int16 pop3_local_port;
}tagmail_config;
extern tagmail_config mail_config;						// AT decoder settings

typedef struct lanport
// Properties of the LAN interface
{
int32 ip_addr;							// Routers IP address
int32 netmask;							// Routers netmask
int32 subnet;							// netmask & ip_addr
int32 gateway;
int32 dhcp_leasetime;					// Duration of ip address lease if we are DHCP client
int32 dhcp_server;						// DHCP server IP
int32 from_ip_addr1;
int32 to_ip_addr1;
int32 from_ip_addr2;
int32 to_ip_addr2;
int32 tcp_idletimer;					// Time out idle TCP Telnet/Trace port sessions
int16 led_mode;
unsigned permitTFTP:	1;
unsigned idle_reset:	1;
unsigned spare:			14;
unsigned char if_name[MAXLEN];			// en0 for ethernet port
unsigned char hostname[MAXLEN2];		// Our hostname
unsigned char password[MAXLEN2];		// Security pass
unsigned char username[MAXLEN2];		// Security hostname
}lanport;
extern lanport lan[];					// Have 2 of these, current and unsaved new settings

extern unsigned char profile[];			// Name of our default profile

typedef struct ADDRESS_POOL
{
int32 first_ip_addr;
int32 last_ip_addr;
bool dhcp_enabled;
}ADDRESS_POOL;
extern ADDRESS_POOL address_pool;
extern int num_pool_addresses;

#define MAX_POOL_LEN	10
extern int32 IP_LEASE_EXPIRE_TIME;	// DHCP lease timeout period in 1 Second units.

typedef struct addr_pool
{
int32 ip_addr;
int ip_addr_avail;
}addr_pool;

extern addr_pool ip_pool[NUM_PPP_LINKS];// Address pool for dial-in clients

// Each FSM is described by an fsm structure and fsm callbacks.
typedef struct fsm {
    int unit;					// Interface unit number
    int protocol;				// Data Link Layer Protocol field value
    int state;					// State
    int flags;					// Contains option bits
    unsigned char id;			// Current id
    unsigned char reqid;		// Current request id
    unsigned char seen_ack;		// Have received valid Ack/Nak/Rej to Req
    int timeouttime;			// Timeout time in milliseconds
    int maxconfreqtransmits;	// Maximum Configure-Request transmissions
    int retransmits;			// Number of retransmissions left
    int maxtermtransmits;		// Maximum Terminate-Request transmissions
    int nakloops;				// Number of nak loops since last ack
    int maxnakloops;			// Maximum number of nak loops tolerated
	CProtocol *callbacks;		// Class pointer to calling routine, for callbacks
	int retransmit;				// mp added. Was member of fsm_callbacks
	int reqci;					// mp added. Was member of fsm_callbacks

	unsigned char *term_reason;	// Reason for closing protocol
    int term_reason_len;		// Length of term_reason
} fsm;


extern int	num_np_up;			// Network protocols active in LCP

extern "C"
{
extern int addroute(int32 ip_addr,int32 ip_mask,int32 gateway,int if_unit);
extern int deleteroute(int32 ip_addr);
extern void output_ip(int16 id,char protocol,int32 dest_ip_addr,unsigned char *outp_buf, unsigned int datalen, unsigned int unit);
extern int deletearpcacheentry(int32 ip_addr);
extern int16 CHECKSUM(unsigned char *dgram,int len);	// Texas
extern void hardware_shutdown();
extern void FlsAutoStart(unsigned long);
}

extern int32 getTickCount();
extern int Flash_save(char sector,int channel,unsigned int *pData, int len);
extern int Flash_save_bytearray(char sector,int offset,unsigned int *pData, int len);
extern int Flash_load(char sector,int channel,unsigned int *pData,int len);
extern int Flash_load_bytearray(char sector,int channel,unsigned int *pData,int len);
extern bool flash_erase(char);

#define SAVE_RESTORE_LAN	0x01
#define SAVE_RESTORE_MAIL	0x02
#define SAVE_RESTORE_AT		0x04
#define SAVE_RESTORE_SYSTEM	0x08
#define SAVE_RESTORE_ALL	0x0C		// Nb Does NOT include LAN or mail

extern void fsm_sdata(fsm *f, unsigned char code, unsigned char id, unsigned char *data, int datalen);

extern unsigned char ppp_header_buf[MAXLEN1];			// buffer for ppp header
extern unsigned char packet_buf[PPP_MRU+PPP_HDRLEN+TCP_DATA_OFFSET];	// buffer for iP packetS

#ifdef ROUTER
extern unsigned char makeup_buf[PPP_MRU + PPP_HDRLEN];	// PPP asynch/synch conversion buffer
#endif

extern int icmp_arg;
typedef struct icmp_header
{
	char type;			// IP version (4 bits) and Header length in 32 bit words(4 bits)
	char code;			// Delivery required
	int16 checksum;		// Datagram length including header in 8-bit bytes.
	int16 id;			// Datagram id
	int16 seq_num;		// Fragmentation
	char data;
} icmp_header;

extern unsigned long GBL_count_1ms;

#define LAN_HDRLEN			14				// 2 mac addresses + 2 protocol bytes
#define ARP_HDRLEN			20				// Bytes

#define ICMP_ROUTER_ADVERT_PERIOD 8*60*1000	// Router advert message interval

#define LAN_CONNECT_WAIT_TIMER		4000	// Time before reporting "LAN not connected" at startup
#define GATEWAY_WAIT_TIMER			10000	// 50 seconds to wait for reply. ARP at startup checking gateway
#define DUPLICATE_IP_RESPONSE_TIMER	2000	// Startup discovering own IP on LAN timer

extern int16 MAC_BROADCAST[3];					// MAC broadcast address
extern int16 MAC_ALLZEROS[3];				// MAC all zeros address
#define DEFAULT_IP_ADDR		0xffffffff		// Unconfigured Ethernet IP address value
#define DEFAULT_IP_NETMASK	0xffffff00		// Our Ethernet netmask
#define IP_ADDR_MULTICAST	0xe0000002		// 0xe0000002 = 224.0.0.2
#define IP_ADDR_BROADCAST	0xffffffff		// 0xffffffff = 255.255.255.255
#define IP_ADDR_LOOPBACK	0x7f000001		// 0x7f000001 = 127.0.0.1
#define LAN_IP_ALLSTATIONS	0x000000ff		// OR with ip addr to calculate our multicast addr

extern int system_arp_cache_entries;		// Any static entries beyond this index in the cache are saved
#define ARP_REQUEST			1
#define ARP_REPLY			2
#define ARP_DYNAMIC			1
#define ARP_STATIC			2
#define ARP_STALE			4
#define ARP_RESET_STALE		0xfb

#define ARP_RESEND_TIME		700				// ARP resend if no reply
#define ARP_CACHE_TIMEOUT	5 * 60			// 5 minutes to time out cache entries
#define ARP_RESPONSE_TIMER	1000			// If no ARP response in 1s assume there will never be one
#ifdef SEB
#define ARP_CACHELEN		20
#else
#define ARP_CACHELEN		30
#endif

typedef struct ARP_CACHE
{
int32 ip_addr;		// IP address
int16 mac_addr[3];	// 6 byte ethernet MAC address. MSByte = [0]
int16 type;			// ARP_DYNAMIC or ARP_STATIC
int32 time;			// Last used. **SECONDS NOT MS**
}ARP_CACHE;

extern ARP_CACHE arp_cache[];

// Values for phase in main.cpp.
#define PHASE_DEAD			0
#define PHASE_INITIALIZE	1
#define PHASE_ESTABLISH		2
#define PHASE_AUTHENTICATE	3
#define PHASE_CALLBACK		4
#define PHASE_NETWORK		5
#define PHASE_TERMINATE		6
#define PHASE_HOLDOFF		7

// Inline versions of get/put char/short/long.
// Pointer is advanced; we assume that both arguments
// are lvalues and will already be in registers.
// cp MUST be unsigned char *.

// Swap the byte order of a short
#define SWAPSHORT(s) {short x; \
	(x) = ((s)<< 8)&0xff00; \
	(x) |= ((s)>> 8)&0x00ff; \
	(s) = x;} 

#define GETCHAR(c, cp) { \
	(c) = (unsigned char)*(cp)++; \
}
#define PUTCHAR(c, cp) { \
	*(cp)++ = (unsigned char) (c); \
}

#define GETLONG(l, cp) { \
	(l) = (unsigned char)*(cp)++ << 8; \
	(l) |= (unsigned char)*(cp)++; (l) <<= 8; \
	(l) |= (unsigned char)*(cp)++; (l) <<= 8; \
	(l) |= (unsigned char)*(cp)++; \
}

#ifdef WIN32
// Microsoft
// int - 16-bit.
#define SWAPINT(i) {short h,l; \
	(h) = (int)(i) >> 16; \
	(l) = (int)(i); \
	SWAPSHORT(h);\
	SWAPSHORT(l);\
	(i) =  (int)h;\
	(i) += (int)(l << 16);} 

#define PUTLONG(l, cp) { \
	*(cp)++ = (unsigned char) ((l) >> 24); \
	*(cp)++ = (unsigned char) ((l) >> 16); \
	*(cp)++ = (unsigned char) ((l) >> 8); \
	*(cp)++ = (unsigned char) (l);}

#define PUTSHORT(s, cp) { \
	*(cp)++ = (unsigned char) ((s) >> 8); \
	*(cp)++ = (unsigned char) (s); \
}

#define GETSHORT(s, cp) { \
	(s) = (unsigned char)*(cp)++ << 8; \
	(s) |= (unsigned char)*(cp)++; \
}
#else
// Texas
// TI int is 16 bit, long is 32-bit
#define PUTSHORT(s, cp) { \
	*(cp)++ = (unsigned char) (((s) >> 8) & 0xff); \
	*(cp)++ = (unsigned char) (s & 0xff); \
}

#define GETSHORT(s, cp) { \
	(s) = (unsigned char)*(cp)++ <<8; \
	(s) |= (unsigned char)*(cp)++; \
}

// long - 32-bit, TI
#define SWAPINT(i) {unsigned long h,l; \
	(h) = (unsigned long)(i) >> 16; \
	(l) = (unsigned long)(i); \
	SWAPSHORT(h);\
	SWAPSHORT(l);\
	(i) =  (unsigned long)h;\
	(i) += (unsigned long)(l << 16);} 

#define PUTLONG(l,cp) { \
	*(cp)++ = (int32)(0XFF &((l) >> 24)); \
	*(cp)++ = (int32)(0XFF &((l) >> 16)); \
	*(cp)++ = (int32)(0XFF &((l) >> 8)); \
	*(cp)++ = (int32)(0XFF & (l));}

#endif

#define INCPTR(n, cp)	((cp) += (n))
#define DECPTR(n, cp)	((cp) -= (n))

// System dependent definitions for user-level 4.3BSD UNIX implementation.
//#define DEMUXPROTREJ(u, p)	demuxprotrej(u, p)
#define BCOPY(s, d, l)	memmove(d, s, l)
#define BZERO(s, n)		memset(s, 0, n)
#define EXIT(u)			quit()

// MAKEHEADER - Add Header fields to a packet.
#define MAKEHEADER(p, t) { \
PUTCHAR(PPP_ALLSTATIONS, p); \
PUTCHAR(PPP_UI, p); \
PUTSHORT(t, p); }
