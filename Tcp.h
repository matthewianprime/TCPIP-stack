// tcp.h
// Defines classes :

// class CProtocol
// class CAuth : public CProtocol
// class Cchap : public CProtocol
// class Cipcp : public CProtocol
// class CipV4 : public CProtocol
// class Clcp : public CProtocol
// class Cmlcp : public CProtocol
// class CPap : public CProtocol

//					Object class derivation - L2 protocols
//
//							CObj
//							|
//						CProtocol
//							|
//		-------------------------------------------------
//		|		|		|		|		|		|		|
//		CAuth	CChap	Cipcp	CipV4	Clcp	Cmlcp	Cpap


// class CProtocol_L3				// Base class TCP & UDP
// class CTcp : public CProtocol_L3
// class Cudp : public CProtocol_L3	// (see udp.h)
// class Cicmp : public CProtocol_L3
// class Carp : public CProtocol_L3

//					Object class derivation - L3 protocols

//							CObj
//							|
//						CProtocol_L3
//							|
//				-----------------------------------------
//				|			|				|			|
//				CTcp		Cicmp			Cudp		Carp


// class CClient						// Base class for dynamicaly created "Layer 4" application-specific clients

//					Object class derivation - L4 protocols
//
//							CObj
//							|
//							CClient
//							|
//		-------------------------------------------------------------
//		|	|		|			|			|		|		|		|
//	CHTTP  CTelnet 	CComandport	CTCPModem	Ctftp	Cdhcp	CPad	Csmtp

// IP datagram protocol field values
#define IP_TCP				6		// TCP protocol number
#define IP_UDP				17		// UDP protocol number
#define IP_ICMP				1

#define IAC					255		// The command escape char in a telnet data stream

#define TCP_PSEUDOHEADERLEN 12		// bytes
#define TCP_MAX_WINDOW		1460	// Bytes. Max chars we receive in a frame
#define TCP_RESEND_LIMIT	7		// Max resends before closing TCP
#define TCP_DELAYED_ACK_BUFFER_THRESHOLD	500 // If there is less TCP rcv buffer then this, the ACK is delayed until there is more buffer space

// tcpstate numbers
// Unsynchronised states per rfc0793 (Cannot send data states....)
#define SYN_SENT		1
#define LISTEN			2
#define SYN_RECEIVED	3

// Synchronised states per rfc0793
#define ESTABLISHED		4
#define DELAYED_ACK		5			// Special case for TCP flow control, not in TCP state machiine
// End can send data states
#define CLOSE_WAIT		6
#define CLOSING_TCP		7
#define LAST_ACK		8
#define TIME_WAIT		9
#define FIN_WAIT1		10
#define FIN_WAIT2		11
#define STATE_NA		12			// Dummy state for non-TCP protocols

// TCP retransmission boundary times
#define UBOUND_MS		20 * 1000	// Max time without ACK before resending frame
#define LBOUND_MS		600			// Min time without ACK before resending frame

// TCP output buffers
#define TCP_BUFLEN		1024		// TCP retransmission buffer size.

// Notification messages from TCP/UDP/Matrix to client, calling param for OnTransport()
enum CLIENT_TRANSPORT_MESSAGES{
// First parameter
	TRANSPORT_CLOSE		=1,
	TRANSPORT_CALL,
	TRANSPORT_OPEN,
	TRANSPORT_DTEPORT
	};
// Second parameter if first parameter is TRANSPORT_DTEPORT.
// Values are OR'd together
#define	DELTA_DTR		0x01
#define	DTR_STATE		0x02
#define	DELTA_RTS		0x04
#define	RTS_STATE		0x08

// Responses for L3 Client receive function
enum TCP_CLIENT_RESPONSES{
	TERMINATE		=0,
	ACKNOWLEDGE,
	NORESPONSE,
	NACKNOWLEDGE,
	ACK_DELAYED
	};

class CClient:public CObj
// Base class for dynamicaly created "Layer 4" application-specific clients
// This class allows inter-class communication between derived classes
// using my stream API. It holds pointers, and entry / exit points
{
public:
CClient();
~CClient();
virtual void init(int,CProtocol_L3* pTransport=NULL,bool bAutoDelete=true);
virtual int serial_receive(unsigned char **data,unsigned int *len,unsigned int window=TCP_MAX_WINDOW);
virtual int receive(unsigned char **data,unsigned int *len,unsigned int window=TCP_MAX_WINDOW);
virtual int OnTransport(int,int);	// Overridable for derived classes
virtual int16 GetTransmitBuffer();	// Returns how much data client can accept from uart
virtual int16 GetReceiveBuffer();	// Returns how much data client can accept from LAN
unsigned char *outbuf;				// Buffer for output TCP or UDP data
const char *name;					// Text name of protocol
const char *szBanner; 				// Welcome message
bool autodelete;					// Flag to system to delete instance
CProtocol_L3 *transport;			// Pointer to UDP or TCP transport layer / parent object
int password_entered;				// Has user entered password ?
int MRU;							// Max chars we can rcv from remote in a TCP frame
int HW_IF;							// The DTE/TA/trace port we are using
bool quit;							// Quit TCP or UDP session flag
int32 dest_addr;					// Ip address of client
};

// Definition of base class for applications using TCP transport protocol Class CTcp
class CProtocol_L3:public CObj			// Base class for IP protocols (TCP & UDP)
{
public:
CProtocol_L3();
~CProtocol_L3();			// Destructor required for delete function
int state;					// state machine control var.
const char *name;			// Text name of protocol
char protocol;				// IP protocol number 
unsigned int HW_IF;			// Interface on which we are being accessed
bool enabled_flag;
bool autodelete;			// Flag system to automatically delete idle instances
unsigned char *rtx_buf;						// The retransmission circular buffer. Created at same time as client

// Initialisation proc
virtual void init(int local_port);
// Shut down transport
virtual void close();
// Process a received packet 
virtual unsigned int input(void *datagram,int len,int32 their_ip,int32 ip_dest_addr,int unit);
// Send a packet
virtual void output(unsigned char* data, int datalen, char code, int32 remote_port_or_ipaddr,int unit,bool retransmission=false);
// Return how much transmission buffer is available for sending
virtual int16 GetTransmitBuffer();
// Return how much buffer is available for receiving
virtual int16 GetReceiveBuffer();

// Create a client application to handle transport data
virtual CClient* CreateClient(int16 portnumber);

int16 MRU;				// Set based on client. If client is serial port, don't exceed the serial buffer size.
int16 MSS;				// Remote's max segment size
CClient* pClient;		// Client application using this transport layer

// IP frame params needed for pseudo header
int32 their_ip;			// Source ip address of frame.
int32 dest_addr;		// Dest ip address of frame. (our address)
int ip_id;				// IP frame id field

// TCP frame params
int16 port_remote;		// *Incoming* Datagram "source port" number. ie Remote port number
int16 port_local;		// *Incoming* Datagram "destiation port". ie Our port number
int32 time;				// Time of last frame received. Used to delete inactive TCP clients
int32 idletimer_ms;		// Max idle time before object is deleted
};

class CTcp : public CProtocol_L3
{
public:
void OnMessage(int16 msg,int32 lParam,int32 wParam,int16 zParam=0);
void init(int local_port);
unsigned int input(void *datagram,int len,int32 their_ip,int32 ip_dest_addr,int unit);
void output(unsigned char* telnet_data, int telnet_len, char code, int32 unused,int unit=HW_LAN,bool retransmission=false);
void close();
int16 GetTransmitBuffer();

int32 highest_tx_seq_num;
int32 tx_ack_num;
int32 tx_seq_num;
int32 rx_ack_num;
int32 rx_seq_num;
int	retransmission_buffer_out(unsigned char *telnet_data,int telnet_len);

int16 rtx_buf_in;								// Retransmission buffer in ptr
int16 rtx_buf_out;								// Retransmission buffer out ptr
int16 rtx_buf_size;							// Retransmission buffer size

int resends;									// Consecutive resends
int total_resends;								// Total this connection

CClient *CreateClient(int16 portnumber);

// TCP  retransmission, lost packet control vars.
//See inside TCP/IP p.504
int32 RTT_MS;									// Round trip time
int32 SRTT_MS;									// Smoothed Round Trip Time
int32 RTO_MS;									// Retransmission Time Out
int32 BACKOFF_TIMEOUT_MS;						// Exponential growth resend backoff time
int32 ShortestRTT_MS;							// Shortest round trip time this connection
int16 window;									// Remotes receive Window size

int32 keepalive_ms;								// TCP keepalive timer

private:
void state_machine_timeout();
void rtx_timeout(int32 lParam);
int	retransmission_buffer_in(unsigned char *telnet_data,int telnet_len);
void retransmission_buffer_free(int32 telnet_len);	// Remote ACK's reception of "telnet_len" bytes
int BytesInBuffer();
};

