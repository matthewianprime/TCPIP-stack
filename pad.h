// CTCPModem.h

// AT decoder state variables for save/restore.
#define HI	1
#define LO	0
#define PAD_ECHO_ON		1
#define PAD_ECHO_OFF	0
#ifdef BAUD_1200
	#define INTERCHAR_TIMEOUT_CALIBRATION	30
#else
	#define INTERCHAR_TIMEOUT_CALIBRATION	60
#endif
#define PAD_COMMANDLINE_BUFLEN	45

typedef struct tagpad_X3params
{
int16 P0_Paknet_protocol;			//Proprietary parameter to Paknet
int16 P1_DLE_escape;
int16 P2_echo;
int16 P3_fwd_chars;
int16 P4_datafwd;
int16 P5_flow;
int16 P6_suppress_service;
int16 P7_break;
int16 P8_delivery;
int16 P9_cr_pad;
int16 P10_linefold;
int16 P11_baudrate;
int16 P12_flowcontrol;
int16 P13_insertLF;
int16 P14_lfpadding;
int16 P15_edit;
int16 P16_ch_delete;
int16 P17_ch_bufferdelete;
int16 P18_ch_bufferdisplay;
}tagpad_X3params;

// state variables
#define LINESTATE_OFFLINE		0
#define LINESTATE_DIAL_WAIT		1
#define LINESTATE_ANSWER_WAIT	2
#define LINESTATE_ONLINE		3
#define LINESTATE_ONLINE_CMDS	4

#define OFFLINE					0
#define ONLINE					1

// PAD command buffer
#define MAX_AT_CMDLEN	30

class CPad : public CClient
{
tagpad_X3params X3params;

public:

enum RESPONSE_TYPE
	{
	RESPONSE_NONE = 0,
	RESPONSE_PROMPT,
	RESPONSE_ERROR,
	RESPONSE_PAR,
	RESPONSE_RING,
	RESPONSE_LINEDROP_LOCAL,
	RESPONSE_LINEDROP_REMOTE,
	RESPONSE_COM,
	RESPONSE_NO_DIALTONE,
	RESPONSE_NO_CARRIER,
	RESPONSE_HELP,
	RESPONSE_STAT,
	RESPONSE_DISPLAY_BUILDINFO,
	RESPONSE_DTEPORT,
	RESPONSE_DISPLAY_LAN_SETTINGS,
	RESPONSE_DISPLAY_ARP_CACHE,
	RESPONSE_DISPLAY_FREEMEM,
	RESPONSE_NULL
	}m_nresponse;

	struct _tagResponseTable
	{
	enum RESPONSE_TYPE	response;
	const char			*verbose;
	};


void init(int,CProtocol_L3*,bool);
int receive(unsigned char **data,unsigned int *len,unsigned int window);
int serial_receive(unsigned char **data,unsigned int *len,unsigned int window);
int16 GetTransmitBuffer();
int16 GetReceiveBuffer();
bool DoCommandLine(unsigned char* CurrentLine,unsigned int len);
int ParseCommand(unsigned char* CurrentLine,unsigned int len);

int transport_state;
int linestate;						// On/offline
unsigned char *response_buf;		// AT cmd responses copied here
//int ring_count;

int OnTransport(int,int);			// Overrides CClient function
void OnMessage(int16 msg,int32 lParam,int32 wParam,int16 zParam=0);

private:

unsigned char commandline[MAX_AT_CMDLEN];	// AT commands, and buffer for line data whilst dialing
const char* PAD_response(RESPONSE_TYPE response,bool traceout=true);
RESPONSE_TYPE init_dial(unsigned char *data,int32 ip_addr=0);
int dial(int32 ip_addr);
void answer();

int ResponseTableLen;
int MethodTableLen;

int response_number;				// Numeric element of command response. eg S-reg valus
int32 dial_ip_addr;					// Dial number store
int16 dial_port_number;				// Remote port number for outgoing call

char linestate_flags;

RESPONSE_TYPE response;				// AT command response
int pos;

unsigned char inbuf[PAD_COMMANDLINE_BUFLEN+1];	// Command parsing buffer for input TCP data
unsigned int inbuf_in;							// Command line buffer in pointer	
int prompt;
bool suppress_echo;
char PAD_recall_char;
bool bLocalClear;
};



