// CTCPModem.h

// AT decoder state variables for save/restore.
#define HI	1
#define LO	0

// state variables
#define LINESTATE_OFFLINE		0
#define LINESTATE_DIAL_WAIT		1
#define LINESTATE_ANSWER_WAIT	2
#define LINESTATE_ONLINE		3
#define LINESTATE_ONLINE_CMDS	4

#define STATE_LOOK_FOR_A		1
#define STATE_LOOK_FOR_T		2
#define STATE_LOOK_FOR_ENDLINE	3
#define STATE_ENDLINE			4

#define OFFLINE					0
#define ONLINE					1

#define NOT_NEGOTIATED_2217		0
#define NEGOTIATED_SEND_2217	1
#define NEGOTIATED_RECEIVE_2217	2
#define NEGOTIATED_2217			3

// AT command buffer
#define MAX_AT_CMDLEN	30

#define SE					240	// (F0) End of subnegotiation parameters
#define	SB					250	// (FA) Start of subnegotiated options
#define WILL				251	// (FB) Sender wants to enable option
#define	WONT				252 // (FC) Sender wants to disable option
#define DO					253 // (FD) Sender wants receiver to enable option
#define	DONT				254 // (FE) Sender wants receiver to disable option

// rfc2217 constants
#define SIGNATURE				0
#define	SET_BAUDRATE			1
#define	SET_DATASIZE			2
#define	SET_PARITY				3
#define	SET_STOPSIZE			4
#define	SET_CONTROL				5
#define	NOTIFY_LINESTATE		6
#define	NOTIFY_MODEMSTATE		7
#define	FLOWCONTROL_SUSPEND		8
#define	FLOWCONTROL_RESUME		9
#define	SET_LINESTATE_MASK		10
#define	SET_MODEMSTATE_MASK		11
#define	PURGE_DATA				12

#define FLOW_REQUEST_IN			13
#define FLOW_REQUEST			0
#define FLOW_NONE_IN			14
#define FLOW_NONE				1
#define FLOW_XON_IN				15
#define FLOW_XON				2
#define FLOW_RTS_IN				16
#define FLOW_RTS				3
#define DTR_REQUEST				7
#define DTR_ON					8
#define DTR_OFF					9
#define RTS_REQUEST				10
#define RTS_ON					11
#define RTS_OFF					12
						
#define COM_PORT_OPTION			44 // (2C)

#define OPTION_CHAR(command,cVal,p) { \
PUTCHAR(IAC, p);\
PUTCHAR(SB, p);\
PUTCHAR(COM_PORT_OPTION,p );\
PUTCHAR(command,p );\
PUTCHAR(cVal,p );\
PUTCHAR(IAC,p );\
PUTCHAR(SE,p );\
}

#define OPTION_LONG(command,lVal,p) { \
PUTCHAR(IAC, p);\
PUTCHAR(SB, p);\
PUTCHAR(COM_PORT_OPTION,p );\
PUTCHAR(command,p );\
PUTLONG((int32)lVal,p );\
PUTCHAR(IAC,p );\
PUTCHAR(SE,p );\
}


class CTCPModem : public CClient
{
public:
enum RESPONSE_TYPE
	{
	RESPONSE_NONE				=0,
	RESPONSE_OK,
	RESPONSE_RING,
	RESPONSE_ERROR,
	RESPONSE_CONNECT,
	RESPONSE_NOCARRIER,
	RESPONSE_IDENTITY,
	RESPONSE_ECHO,
	RESPONSE_DISPLAY_LAN_SETTINGS,
	RESPONSE_DISPLAY_AT_SETTINGS,
	RESPONSE_DISPLAY_ARP_TABLE,
	RESPONSE_DISPLAY_OBJECTS,
	RESPONSE_DISPLAY_BUILDINFO,
	RESPONSE_DISPLAY_FREEMEM,
	RESPONSE_DISPLAY_LAN,
	RESPONSE_NO_DIALTONE,
	RESPONSE_ABORTED,
	RESPONSE_NUMBER,
	RESPONSE_MSG
	}m_nresponse;

struct _tagResponseTable
	{
	enum RESPONSE_TYPE	response;
	const char			*numeric;
	const char			*verbose;
	};


void init(int,CProtocol_L3*,bool);
int receive(unsigned char **data,unsigned int *len,unsigned int window);
int serial_receive(unsigned char **data,unsigned int *len,unsigned int window);
int16 GetTransmitBuffer();
int16 GetReceiveBuffer();
int atoi2(char* str,int* numparsed);
int ParseCOM_PORTOptions(unsigned char *data,unsigned int *len,unsigned char **response);

int transport_state;
int linestate;						// On/offline
unsigned char *response_buf;		// AT cmd responses copied here
int ring_count;

int OnTransport(int,int);			// Overrides CClient function
void OnMessage(int16 msg,int32 lParam,int32 wParam,int16 zParam=0);
int parse(unsigned char *data);

private:

unsigned char commandline[MAX_AT_CMDLEN+1];	// AT commands, and buffer for line data whilst dialing
int TelnetOptions(unsigned char **rxdata,unsigned int *plen);
const char* AT_response(RESPONSE_TYPE response,bool tracelog=true);
RESPONSE_TYPE init_dial(unsigned char *data,int32 ip_addr=0);
int dial(int32 ip_addr);
void answer();

int ResponseTableLen;

int response_number;				// Numeric element of command response. eg S-reg valus
int state;							// Command parser state
int rfc2217_state;					// Either NOT_NEGOTIATED_2217 or NEGOTIATED_2217
int32 dial_ip_addr;					// Dial number store
int16 dial_port_number;				// Remote port number for outgoing call

char modemstate_flags;
char linestate_flags;

RESPONSE_TYPE response;				// AT command response
int pos;							// Command line parsing position
int plus_count;
};



