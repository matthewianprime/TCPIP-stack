// CPad.cpp
// X28 PAD interface

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "pad.h"
#include "utils.h"
#include "arp.h"
#include "ip.h"
#include "ffs.h"
#include "lan.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

extern int16 tcp_transient_port_number;			// Our Port number when making an outgoing TCP call

// Paknet PAD values for X3 parameter 11 - baudrate
// 2=300baud,3=1200,4=600,6=150,12=2400,13=4800,14=9600
// DSP additional
// 15=19200,16=38400,17=57600,18=115200
#ifdef BAUD_1200
	static const char BaudRateTable[]= {0,0,0,7,\
						0,0,0,0,0,0,0,0,6,5,\
						4,3,2,0,0,0};
#else
// TODO support 115200, but value is 0 !!
	static const char BaudRateTable[]= {0,0,0,0,\
						0,0,0,0,0,0,0,0,0,7,\
						4,3,2,1,0,0};
#endif


static const struct CPad::_tagResponseTable ResponseTable[] = 
	{
	{CPad::RESPONSE_NONE,						szNull					},
	{CPad::RESPONSE_PROMPT,						szPADprompt				},
	{CPad::RESPONSE_ERROR,						szPADError				},
	{CPad::RESPONSE_PAR,						szPAR18is				},
	{CPad::RESPONSE_RING,						szNull					},				
	{CPad::RESPONSE_LINEDROP_LOCAL,				szPADClrConf			},
	{CPad::RESPONSE_LINEDROP_REMOTE,			szPADClrDte				},
	{CPad::RESPONSE_COM,						szPADCom				},
	{CPad::RESPONSE_NO_DIALTONE,				szPADClrDte				},
	{CPad::RESPONSE_NO_CARRIER,					szPADClrDte				},
	{CPad::RESPONSE_HELP,						szPADHelp				},
	{CPad::RESPONSE_STAT,						szPADStat				},
	{CPad::RESPONSE_DISPLAY_BUILDINFO,			szBuildInfo				},
	{CPad::RESPONSE_DTEPORT,					szNull					},
	{CPad::RESPONSE_DISPLAY_LAN_SETTINGS,		szSTAR_C1				},
	{CPad::RESPONSE_DISPLAY_ARP_CACHE,			szNull					},
	{CPad::RESPONSE_DISPLAY_FREEMEM,			szFreeMem				},
	{CPad::RESPONSE_NULL,						szNull					}
	};


enum METHOD_TYPE
	{
	METHOD_UNSUPPORTED = 0,
	METHOD_HELP1,
	METHOD_HELP2,
	METHOD_SET,
	METHOD_CALL,
	METHOD_CLR,
	METHOD_CON,
	METHOD_STAT,
	METHOD_PAR,
	METHOD_RSET,
	METHOD_DTEPORT,
	METHOD_PORT,
	METHOD_RPAR,
	METHOD_VER,				// Proprietary. Report version info
	METHOD_LAN_SETTINGS,	// Proprietary. Show LAN configuration
	METHOD_ARP_CACHE,
	METHOD_FREEMEM,
	METHOD_RESTART			// Proprietary. ATY for AT decoder compatibility
	}m_nPadCommand;
	
struct _tagMethodTable
	{
	enum METHOD_TYPE	id;
	const char			*key;
	};

static const struct _tagMethodTable MethodTable[] = {
	{METHOD_HELP1,				szPADcmdHelp1		},
	{METHOD_HELP2,				szPADcmdHelp2		},
	{METHOD_SET,				szPADcmdSET			},		// Set X3 parameter(s). 2 Syntaxes
	{METHOD_CALL,				szPADcmdCALL		},		// Make a call
	{METHOD_CLR,				szPADcmdCLR			},		// Clear a call (online commands mode)
	{METHOD_CON,				szPADcmdCON			},		// Reconnect a call (online commands mode)
	{METHOD_STAT,				szPADcmdSTAT		},		// Status enquiry
	{METHOD_PAR,				szPADcmdPAR			},		// View X3 parameter listing
	{METHOD_RPAR,				szPADcmdRPAR		},		// X29 not supported
	{METHOD_RSET,				szPADcmdRSET		},		// X29 not supported
	{METHOD_DTEPORT,			szPADcmdDTE			},		// Display TCP port number
	{METHOD_PORT,				szPADcmdPORT		},		// Set TCP port number
	{METHOD_VER,				szPADcmdVER			},		// Display version info
	{METHOD_LAN_SETTINGS,		szPADcmdLANSETTINGS	},
	{METHOD_ARP_CACHE,			szPADcmdARPCACHE	},
	{METHOD_FREEMEM,			szPADCmdFreemem		},
	{METHOD_RESTART,			szPADcmdrestart		}		// ATY reset command
	};

//--------------------------------------------------------------------------
int16 CPad::GetTransmitBuffer()
// Return how many bytes we can accept from the serial interface
// This depends on how much TCP can accept if we are online.
{
if((transport) && (linestate == LINESTATE_ONLINE))
	return transport->GetTransmitBuffer();

if((transport) && (linestate == LINESTATE_DIAL_WAIT))
	// Waiting for a call to complete.
	// Buffer serial data
	return 0;

// Command mode.
// PAD decoder accepts both 1-char-at-a-time, or command fragmented into strings
// TODO CONSIDER MAX COMMAND LENGTH
return hw->uart[HW_IF].tx_buffer_len;
}

//--------------------------------------------------------------------------
int16 CPad::GetReceiveBuffer()
// Return how many bytes we can accept from the LAN
{
if((transport) && (linestate == LINESTATE_ONLINE))
	return hw->uart[HW_IF].tx_buffer_len - hw->uartOutputQueue(HW_IF);
else
	return 0;
}

//--------------------------------------------------------------------------
int CPad::OnTransport(int message,int param)
// Message from transport layer ( TCP or UDP )
{
response_buf[0] = NULL;

switch(message)
	{
	case TRANSPORT_CLOSE:
	// Transport deleted - TCP connection closed

	// DCD off
	if(modem_config.dcdflag != 0)
		hw->uart[HW_IF].flag_dcd = LO;

	if(linestate == LINESTATE_DIAL_WAIT)
		{
		serialout(PAD_response(RESPONSE_NO_CARRIER),HW_IF);
		serialout(PAD_response(RESPONSE_PROMPT),HW_IF);
		}
	else if(linestate > LINESTATE_OFFLINE)
		{
		if(bLocalClear)
			serialout(PAD_response(RESPONSE_LINEDROP_LOCAL),HW_IF);
		else
			serialout(PAD_response(RESPONSE_LINEDROP_REMOTE),HW_IF);

		serialout(PAD_response(RESPONSE_PROMPT),HW_IF);
		}

	// Reset the forward delay
	hw->uart[HW_IF].flag_interCharTimeoutRx_time = UART_INTERCHAR_TIMEOUT;

	bLocalClear = false;
	
	// Stop RING message being output
	untimeout(MSG_ALL);
	// Flush UART buffer. If session closed with CTS off this turns it back on
	hw->uartInputFlush(HW_IF);	

	// Restore autobaud setting
	// TOFO should this be OFFLINE ?
	UpdateUart(HW_IF,ONLINE);
	
	transport_state = TRANSPORT_CLOSE;
	linestate = LINESTATE_OFFLINE;
	inbuf_in = 0;
	break;

	case TRANSPORT_OPEN:
	// Transport layer is available to pass data

	transport_state = TRANSPORT_OPEN;

	if((linestate == LINESTATE_OFFLINE) || (linestate < LINESTATE_ONLINE))
		// Incoming call
		// Or waiting for a dial or answer to complete
		answer();

	break;


	case TRANSPORT_CALL:
	// Incoming call (TCP SYN received)

	if(transport_state == TRANSPORT_CALL)
		// Repeat of initial SYN. OK, but don't report RING again
		break;

	transport_state = TRANSPORT_CALL;

	// Timeout routine answers call
	timeout(0,1000);
	break;
	
	case TRANSPORT_DTEPORT:
	if(param & DELTA_DTR)
		{
		// DTR state change
		sprintf(msg,szModemDTR,(param & DTR_STATE)? szParamon : szParamoff);
		OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(const  char*)msg);
		
		if(!(param & DTR_STATE))
			{
			// DTR dropped, Flush UART buffer.
			hw->uartInputFlush(HW_IF);	

			if(modem_config.dtrflag != 0)
				{
				// Hang up
				if(linestate != LINESTATE_OFFLINE)
					transport->close();
				}
			}
		}
	break;
	};
	
return 1;
}

//--------------------------------------------------------------------------
void CPad::init(int HW_IFparam,CProtocol_L3* pTCP,bool bAutoDelete)
// Called:
// 1: At startup with pTCP==NULL if this is the shell instance
// 2: On an incoming TCP connection
{
outbuf = &packet_buf[TCP_DATA_OFFSET];
linestate_flags = 0;
dial_port_number = modem_config.remote_port;		// Dial to TCP modem port by default. (Set with AT!P)

name = szPAD;
transport = pTCP;
szBanner = NULL;
quit = false;
autodelete =  bAutoDelete;				// Delete instance when quit flag is set
HW_IF = HW_IFparam;
prompt=true;
inbuf_in = 0;
suppress_echo = false;
bLocalClear = false;

X3params.P1_DLE_escape= 1;					// Value for Control-P (DLE)
PAD_recall_char = CH_DLE;					// !!! Must include parity bit !!!! Must correspond to X3params.P1_DLE_escape setting
X3params.P2_echo = PAD_ECHO_ON;
X3params.P16_ch_delete = CH_DEL;
X3params.P17_ch_bufferdelete = CH_CTRLX;
X3params.P18_ch_bufferdisplay = CH_CTRLR;
X3params.P4_datafwd = 1;					// Default forward on idle

linestate		= LINESTATE_OFFLINE;
response		= RESPONSE_PROMPT;
transport_state = TRANSPORT_CLOSE;
pos= 0;

ResponseTableLen = sizeof(ResponseTable)/ sizeof(struct _tagResponseTable);
MethodTableLen = sizeof(MethodTable)/ sizeof(struct _tagMethodTable);

response_buf = &packet_buf[1000];		// PAD cmd responses.
*response_buf = NULL;
MRU = TCP_BUFLEN;						// Default MRU for TCP

// Restore PAD defaults only if not the shell PAD
if(autodelete)
	{
	ATFactoryReset(HW_IF);
	// Restore saved modem_config structure from flash
	ffs->ffsReset();
	if(ffs->open(MODE_OPENEXISTING,(char*)profile,szFILETYPE_PROFILE))
		{
		ffs->restore_from_flash(SAVE_RESTORE_AT);
		ffs->close();
		}
	}

UpdateUart(HW_IF);

if(transport)
	transport->idletimer_ms = modem_config.tcp_idletimer;			// Max idle time before deletion
else
	{
	// Output banner and prompt when first initialised, but not on an incoming call
	serialout((char*)szEndline,HW_IF);
	serialout((char*)szPAD,HW_IF);
	serialout((char*)szPADprompt,HW_IF);
	}

hw->uart[HW_IF].flag_interCharTimeoutRx_time = UART_INTERCHAR_TIMEOUT;
return;
}

//--------------------------------------------------------------------------
int CPad::receive(unsigned char **rxdata,unsigned int *plen,unsigned int window)
// Received data from LAN, send it out on the serial interface
// Returns to TCP : ACKNOWLEDGE, TERMINATE, NORESPONSE (generally means response already sent) 
{
unsigned char *data = *rxdata;
unsigned int len = *plen;
data[len] = NULL;
// No data to send back (length=0)
*plen = 0;

int32 ms;
int32 uart_buffered_bytes;

if(linestate == LINESTATE_ONLINE)
	// Send LAN data to serial port
	{
	// TCP needs to send "window" ie max data we can accept in the next frame 
	// uartOutputQueue returns char count in the uart buffer
	int16 temp = hw->uartOutputQueue(HW_IF);
	if( (len + temp) > hw->uart[HW_IF].tx_buffer_len)
		{
		sprintf(msg,szTCPNoBuffer,(int)(hw->uart[HW_IF].tx_buffer_len-temp),len);
		OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);
		}
		
	uart_buffered_bytes = (int32)serialout((const char*)data,HW_IF,len);

	// Avoid silly window size syndrome
	if(uart_buffered_bytes > TCP_DELAYED_ACK_BUFFER_THRESHOLD )
		{
		// Delay the ACK by the time required to empty our serial buffer
		ms = ( uart_buffered_bytes * modem_config.character_time_uS ) >>10;		//  >>10 == /1024 uS->mS
		ms=min(ms,500);
		transport->state=DELAYED_ACK;

		// Get TCP to send ACK after "ms" milliseconds
		PostMessage(transport,MSG_TIMER,0,0,0,ms);
//		sprintf(msg,szTCPDelayedACK,ms,config.character_time_uS);
//		OutputDebugString(LOG_DEBUG,LOG_TCP,(const char*)msg);
		
		return NORESPONSE;
		}
 	}

if(quit)
	return TERMINATE;
	
return ACKNOWLEDGE;
}

//--------------------------------------------------------------------------
int CPad::serial_receive(unsigned char **ppdata,unsigned int *plen,unsigned int window)
// Received data from serial port
// We are emulating a modem, so data is either:
// - an AT command string
// - data to be sent over the lan connection
{
bool eol = false;					// End of Line flag
unsigned char *data = *ppdata;
unsigned int len = *plen;
data[len] = 0;						// Null terminate command string
outbuf[0] = 0;
if(linestate != LINESTATE_ONLINE)
	response_buf[0] = NULL;				// Response message buffer.nb This would corrupt Tx data when online

switch(linestate)
	{
	case LINESTATE_DIAL_WAIT:
	// Dial / answer abort
	if(transport)
		transport->close();
	linestate = LINESTATE_OFFLINE;
	serialout(PAD_response(RESPONSE_LINEDROP_LOCAL),HW_IF);

	// Restore autobaud setting
	UpdateUart(HW_IF);
	// Issue prompt
	eol=true;	
	break;
	
	case LINESTATE_ONLINE:
	// Check for PAD recall char. Default = ^P<cr>
	inbuf_in = 0;
	suppress_echo = true;								// Don't echo the data

	if(X3params.P1_DLE_escape != 0)						// 0 = no PAD recall char.
		{
		if(strchr((char*)data,PAD_recall_char))
			{
			// PAD recall char received
			OutputDebugString(LOG_DEBUG,LOG_TCP,szModemLocalAccess);
			linestate = LINESTATE_ONLINE_CMDS;
			serialout(PAD_response(RESPONSE_PROMPT),HW_IF);
			// Reset the forward delay
			hw->uart[HW_IF].flag_interCharTimeoutRx_time = UART_INTERCHAR_TIMEOUT;
			len = 0;									// Don't send escape char to remote
			}
		}

	// Send data to remote TCP
	if(transport && len)
		transport->output(data,len, ACK,0,HW_LAN);		// Online data, no data to return
	break;

	case LINESTATE_ONLINE_CMDS:
	case LINESTATE_OFFLINE:
	// Append the new data to the command line buffer inbuf

	if((inbuf_in+len) >= PAD_COMMANDLINE_BUFLEN)
		{
		// Output "command too long" message
		sprintf((unsigned char*)outbuf,szCmdLen,(inbuf_in+len));
		eol=true;
		prompt=true;								// Issue a new prompt
		}
	else
		{// Process the command line
		memcpy(&inbuf[inbuf_in],data,len);
		inbuf_in += len;
		inbuf[inbuf_in]=NULL;
		eol = DoCommandLine(inbuf,inbuf_in);
		}
	break;
	}
	
if(eol)
	{
	// Command line was entered and processed. 
	// Issue new command prompt and reset command line buffer
	if(prompt)
		strcat((char*)outbuf,szPADprompt);

	prompt=true;		// prompt=false is set to suppress new prompt, eg ping command.
	inbuf_in = 0;		// Command line makeup buffer pointer
	}
else 
	{
	// Command line not yet complete
	// Echo TTY commmands to user
	if((X3params.P2_echo == PAD_ECHO_ON) && (!suppress_echo))								// Echo commands on
		strcpy((char*)outbuf,(const char*)data);
	else *outbuf = NULL;							// Echo commands off
	}

suppress_echo = false;
*plen = strlen((const char*)outbuf);
*ppdata =(unsigned char*)outbuf;
return 0;
}


//--------------------------------------------------------------------------
const char* CPad::PAD_response(RESPONSE_TYPE response,bool tracelog)
// Response text copied to response_buf.
// Returns pointer to response
{
int index=0;
int16 free_mem;
int len;
response_buf[0]=NULL;

// Responses requiring sprintf are a special case..
switch(response)
	{
	case RESPONSE_STAT:
	// Paknet responds ENGAGED or FREE
	sprintf(response_buf,ResponseTable[response].verbose,linestate_2ascii(linestate));
	goto trace;
	
	case RESPONSE_DISPLAY_BUILDINFO:
	version_2ascii(response_buf);
	goto trace;
	
	case RESPONSE_DTEPORT:
	//sprintf(response_buf,ResponseTable[response].verbose,dial_port_number);
	uart_2ascii(response_buf,HW_IF);
	goto trace;
		
	case RESPONSE_DISPLAY_LAN_SETTINGS:
	sprintf(response_buf,ResponseTable[response].verbose,lan[UNSAVED].ip_addr,lan[UNSAVED].netmask,lan[UNSAVED].gateway,lan[UNSAVED].subnet,mac_addr,modem_config.local_port,modem_config.remote_port,modem_config.tcp_idletimer/1000);
	goto trace;
	
	case RESPONSE_DISPLAY_ARP_CACHE:
	serialout(szARPCacheDisplay,HW_IF);
	while(index < ARP_CACHELEN)
		{
		msg[0]=0;
		index = arpcache_2ascii(msg,index);
		serialout((char*)msg,HW_IF);
		};
	goto trace;
	
	case RESPONSE_DISPLAY_FREEMEM:
	free_mem = freemem();
	sprintf(response_buf,ResponseTable[response].verbose,free_mem);
	goto trace;
	
	case RESPONSE_PAR:
	// Display X3 parameters
	sprintf(response_buf,ResponseTable[response].verbose,
//			X3params.P0_Paknet_protocol,
			X3params.P1_DLE_escape,
			X3params.P2_echo,
			X3params.P3_fwd_chars,
			X3params.P4_datafwd,
			X3params.P5_flow,
			X3params.P6_suppress_service,
			X3params.P7_break,
			X3params.P8_delivery,
			X3params.P9_cr_pad,
			X3params.P10_linefold,
			X3params.P11_baudrate,
			X3params.P12_flowcontrol,
			X3params.P13_insertLF,
			X3params.P14_lfpadding,
			X3params.P15_edit,
			X3params.P16_ch_delete,
			X3params.P17_ch_bufferdelete,
			X3params.P18_ch_bufferdisplay
			);
	goto trace;
	}
				
strcpy((char*)response_buf,ResponseTable[response].verbose);

trace:
if((tracelog)&&(traceout.module & LOG_SERIAL))
	{
	// Response to trace port
	len=strlen((const char*)response_buf);
	if(len>0)
		{
		sprintf(msg,"<DTE Tx %u> ",len);
		OutputDebugString(LOG_INFO,LOG_SERIAL,(const char*)msg,false);
		printformatstring(msg,0,(char*)response_buf,min(len,20),traceout.hexmode,false);
		OutputDebugString(LOG_INFO,LOG_SERIAL,(const char*)msg);
		}
	}
	
return (const char*)response_buf;
}

//--------------------------------------------------------------------------
void CPad::answer()
// Go to online state
{
// Autobaud off when online .. but leave config.autobaud unchanged so
// the current setting can be restored when the call terminates.
UpdateUart(HW_IF,ONLINE);

// Waiting for a dial or answer to complete
serialout(PAD_response(RESPONSE_COM),HW_IF);
//Set the forward delay
hw->uart[HW_IF].flag_interCharTimeoutRx_time = X3params.P4_datafwd * INTERCHAR_TIMEOUT_CALIBRATION;

linestate = LINESTATE_ONLINE;

// DCD on
hw->uart[HW_IF].flag_dcd = HI;

// Send any buffered data
if(pos)
	// Send any data buffered during connect sequence
	transport->output(commandline,pos, ACK,0,HW_LAN);
else
	// They need our ACK to update our receive  Window
	transport->output(NULL,NULL, ACK,0,HW_LAN);
}

//--------------------------------------------------------------------------
CPad::RESPONSE_TYPE CPad::init_dial(unsigned char *data,int32 ip_addr)
// To dial, we need:
// 1. TCP transport layer.
// 2. Initiate a TCP connection
// 3. Mac addr of destination

// Params:
// ip_addr -> ip address to dial. If ip_addr is 0 param "data" is used instead
// data -> Valid dial$ in the forms:
// OR "P aaabbbcccddd" or "Paaabbbcccddd" or "P a.b.c.d" or "Pa.b.c.d"
// OR "T aaabbbcccddd" or "Taaabbbcccddd" or "T a.b.c.d" or "Ta.b.c.d"
// SUFFIXED WITH "Pnn"		(port number)
// or SUFFIXED WITH "P nn"	(port number)

// Returns:
// RESPONSE_NO_DIALTONE		remote ip addr not in cache OR ip addr is bad or zero OR CreateTransport() fails
// RESPONSE_NONE 			dial initiated
{
dial_port_number = modem_config.remote_port;		// Dial to TCP modem port by default. (Set with AT!P)
char* pP;									// Ptr to char 'P' which precedes the port number in the dial string
pos=0;										// No. of chrs in cmd buffer.Reset as we will buffer online data till connected

if(linestate != LINESTATE_OFFLINE)
	return RESPONSE_ERROR;

if(ip_addr != 0)
	// ip_addr param supplied.
	goto set_dial_timer;

// A string param was supplied
// Remove Pulse or Tone dial specifier if present in the string.
if(( data[0] == 'T' ) || ( data[0] == 'P' ))
	// User specified tone or pulse dial. Remove from dial string
	data++;
	
// Remove leading space(s) from dial string ( spaces following ATD, ATDT or ATDP )
TrimLeft(data,0);

// Extract the port number from the dial string if present
pP = strchr((const char*)data,'P');
if(pP)
	{
	if( (*(pP+1)>='0') && (*(pP+1)<='9') )
		// User supplied destination port
		dial_port_number = atoi((const char*)pP+1);

	*pP = NULL;	// Null terminate ip string for ascii_2ip_nodots or ascii_2ip.
	}

// Dial string can be in dotted decimal notation OR a 12-character string
ip_addr = ascii_2ip(data);						// Dotted ip string
if(ip_addr == 0xFFFFFFFF)
	// Bad dotted string format. Try the 12 char string format
	ip_addr = ascii_2ip_nodots(data);			// 12 digit ip string, no dots

if(ip_addr == 0xFFFFFFFF)
	{
	// Bad ip addr specified
	sprintf(msg,szModemBadNumber,data);
	OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(const char*)msg);
	goto fail;
	}

// We now have the ip address and remote port number (if specified)

set_dial_timer:
// Set a timer which initiates the dial on expiry

// Refresh remote ip address in ARP cache by sending an ARP request
arp->output_arp(ip_addr,NULL);

// Set the timer to initiate dial. 
// By the time it expires we should have the remotes Ip in the ARP cache if it is present on the LAN
timeout(0,1000);

// Save the dial number for dial()
dial_ip_addr = ip_addr;

// Autobaud off while dialing
UpdateUart(HW_IF,ONLINE);

linestate = LINESTATE_DIAL_WAIT;
return RESPONSE_NONE;

fail:
linestate = LINESTATE_OFFLINE;
return RESPONSE_NO_DIALTONE;
}


//--------------------------------------------------------------------------
int CPad::dial(int32 ip_addr)
// Dial the specified ip address.
// The ip should be present in the ARP cache
{
if(linestate != LINESTATE_DIAL_WAIT)
	// Dial operation has already been aborted
	return RESPONSE_NONE;
	
// Is the gateway or destination IP address in our arp cache ?
if(!arp->isincache(ip_addr,NULL))
	{
	// Restore autobaud setting
	UpdateUart(HW_IF);
	linestate = LINESTATE_OFFLINE;
	return RESPONSE_NO_CARRIER;
	}

// Initiate a tcp connection - create a TCP transport instance
int n = ipV4->CreateTransport(IP_TCP,HW_LAN,tcp_transient_port_number,this,ip_addr);
transport = ip_protocols[n];
if((!transport) || (n == 0))
	{
	// Out of memory ?
	linestate = LINESTATE_OFFLINE;
	// Restore autobaud setting
	UpdateUart(HW_IF);
	return RESPONSE_ERROR;
	}
		
// Get Transport to allocate a retransmission buffer
transport->CreateClient(tcp_transient_port_number);
transport->idletimer_ms = modem_config.tcp_idletimer;			// Max idle time before deletion

// Our (source) port number for this connection
tcp_transient_port_number++;

sprintf(msg,szModemDialing,ip_addr,dial_port_number);
OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(const char*)msg);

// Send a TCP active open frame.....
transport->state = SYN_SENT;
transport->port_remote = dial_port_number;

transport->output(NULL,0,SYN,0,HW_LAN);
((CTcp*)transport)->tx_seq_num++;

return RESPONSE_NONE;
}

//--------------------------------------------------------------------------
bool CPad::DoCommandLine(unsigned char* CurrentLine,unsigned int len)
// Parse a line of input
{
unsigned char* pEndLine;
unsigned char* pComma;
unsigned char *pDel;
int iRes;
int pos=0;
int16 parameter=0;
int16 value=0;
response = RESPONSE_NONE;	// Command response

// Ignore parity
strip_parity(CurrentLine,len);

if(len == 0)
	return false;

CurrentLine[len] = 0;		// Treat as string

if(len == 1)
	{
	pEndLine = (unsigned char*) strchr((const char*)CurrentLine,0x0A);
	if(pEndLine)
		{// Line consisted of <lf> and nothing else
		// Hyperterm does this if "append LFs to CRs" option is set
		// Don't issue another prompt
		prompt = false;
		return true;
		}
	}
	
if(len == 1)
	{
	if(CurrentLine[0] == 0x0A)
		{// Line consisted of <lf> and nothing else
		// Hyperterm does this if "append LFs to CRs" option is set
		// Don't issue another prompt
		prompt = false;
		return true;
		}
	}

int chars_to_strip =0;
int strip_pos=0;
bool strip_done;
// Strip any leading cr and lfs
do	{
	strip_done=false;
	if((CurrentLine[strip_pos] == 0x0D) || (CurrentLine[strip_pos] == 0x0A))
		{
		// Strip a leading cr OR lf
		chars_to_strip++;
		strip_done=true;
		}
	strip_pos++;
	}while((strip_done) && (strip_pos<len));

if(strip_done)
	{
	// Strip out the characters
	len-=chars_to_strip;		
	memmove(CurrentLine,&CurrentLine[1],len);

	if(len == 0)
		{
		// Stripped out the whole command line
		prompt=true;
		return true;
		}

	// Issue a prompt and continue parsing
	serialout(PAD_response(RESPONSE_PROMPT),HW_IF);	
	}

// Look for a buffer delete character
pEndLine = (unsigned char*) strchr((const char*)CurrentLine,X3params.P17_ch_bufferdelete);
if(pEndLine != NULL)
	{
	// Delete line of input,display new prompt
	inbuf_in = 0;
	return true;
	}


// Look for a buffer redisplay character
pEndLine = (unsigned char*) strchr((const char*)CurrentLine,X3params.P18_ch_bufferdisplay);
if(pEndLine != NULL)
	{
	// Redisplay line of input on a new line
	*pEndLine = NULL;		// Remove the buffer display char.(Trailing chars are lost)
	len--;
	suppress_echo = true;	// Don't echo this redisplay character

	// Handle backspace chars and regenerate the line
	pDel = CurrentLine;
	while(pDel != NULL)
		{
		pDel = (unsigned char*)strchr((const char*)CurrentLine,X3params.P16_ch_delete);

		if (pDel == CurrentLine)
			// First char of data is a del character.
			strcpy((char*)pDel,(char*)pDel+1);
		else if(pDel > CurrentLine)
			// Mid string char is a del. Delete preceding character
			strcpy((char*)pDel-1,(char*)pDel+1);
		}
	
	// Current line regenerated without DELs. Redisplay new prompt and the line
	inbuf_in = strlen((char*)CurrentLine);
	serialout(PAD_response(RESPONSE_PROMPT),HW_IF);
	serialout((char*)CurrentLine,HW_IF);
	return false;
	}

pEndLine = (unsigned char*) strchr((const char*)CurrentLine,CH_CR);
if(pEndLine == NULL)
	// Input line not terminated
	return false;

*pEndLine = 0;								// String ends at first <cr> in line
sprintf(outbuf,szEndline);					// Newline to user

len = (unsigned int)strlen((const char*)CurrentLine);
if(len == 0)
	// Line consisted of <cr> and nothing else
	return true;

// Handle backspace characters
pDel = CurrentLine;
while(pDel != NULL)
	{
	pDel = (unsigned char*)strchr((const char*)CurrentLine,X3params.P16_ch_delete);
	if (pDel == CurrentLine)
		// First char of data is a del character.
		strcpy((char*)pDel,(char*)pDel+1);
	if(pDel > CurrentLine)
		// Mid string char is a del. Delete preceding character
		strcpy((char*)pDel-1,(char*)pDel+1);
	}

// Remove leading whitespace characters
CurrentLine = TrimLeft(CurrentLine,0);
len = (unsigned int)strlen((const char*)CurrentLine);
if(len == 0)
	return true;

makeupper(CurrentLine);

//sprintf(msg,"PAD parsing %s ",CurrentLine);
//OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);

//m_nParam = PARAM_HELP;	// Default
m_nPadCommand=(METHOD_TYPE) ParseCommand(CurrentLine,len);

//sprintf(msg,"result=%u",m_nPadCommand);
//OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);

switch (m_nPadCommand)
	{
	case METHOD_HELP1:
	case METHOD_HELP2:
	response = RESPONSE_HELP;
	break;

	case METHOD_PAR:
	// Display X3 parameters
	response = RESPONSE_PAR;
	break;

	case METHOD_PORT:
	iRes = sscanf((char*)&CurrentLine[pos],"%i",&value);
	if(iRes!= 1)
			// No param supplied
			response = RESPONSE_ERROR;
	else modem_config.remote_port = value;
	break;
	
	case METHOD_DTEPORT:
	response = RESPONSE_DTEPORT;
	break;
		
	case METHOD_CLR:
	if(linestate == LINESTATE_ONLINE_CMDS)
		{
		// Hang up
		transport->close();
		bLocalClear = true;
		prompt = false;
		}
	else
		response = RESPONSE_ERROR;
	break;

	case METHOD_STAT:
	response = RESPONSE_STAT;
	break;
	
	case METHOD_SET:
	// 2 syntaxes:
	// SET parameter-number,new-value
	// SET p1,p2, .. p18
	// Syntax option SET p0:val,p1:val, .... pn:val
	pComma=CurrentLine;						// Don't want this var to initialise to NULL

	do	{
		iRes = sscanf((char*)&CurrentLine[pos],"%i:%i",&parameter,&value);
		if((iRes!= 2) || (parameter > 18))
			{// Param parsing error.
			pComma = NULL;					// Force exit from parsing loop
			response = RESPONSE_ERROR;
			}
		else
			{
			// sscanf assigned values to 2 parameters
			((int16*)(&X3params.P1_DLE_escape))[parameter-1] = value;
			
			// If there is a trailing comma there are more values to set
			pComma = (unsigned char*)strchr((char*)&CurrentLine[pos],',');
			if(pComma)
				{// There are more parameters to parse on this line
				pos = pComma - CurrentLine;
				pos+=1;
				}
			}
		}while(pComma);
		
	// Update the PAD recall char variable
	if(X3params.P1_DLE_escape == 1)
		PAD_recall_char = CH_DLE;
	else if((X3params.P1_DLE_escape >=32) && (X3params.P1_DLE_escape <=126))
		PAD_recall_char = X3params.P1_DLE_escape;
	else 
		// Error ! Bad value in X3params.P1_DLE_escape. Reset to 0
		X3params.P1_DLE_escape = 0;

	// Update the baud rate
	if(X3params.P11_baudrate <= 18)
		{
		if(BaudRateTable[X3params.P11_baudrate] != 0)
			{
			modem_config.baudrate_flag =  hw->setBaudOversampleRate[BaudRateTable[X3params.P11_baudrate]];
			UartAutobaudEnable(HW_IF,false);
			UpdateUart(HW_IF);
			}
		}
	
	// Update the delay timer. Set in 1/20th (50ms) second intervals
	// Convert from 50ms to 1ms units
	X3params.P4_datafwd = max(X3params.P4_datafwd,1);
	break;

	case METHOD_RESTART:
	// Reboot now !
	watchDogTimerDisable=1;
	break;
	
	case METHOD_VER:
	response = RESPONSE_DISPLAY_BUILDINFO;
	break;
	
	case METHOD_LAN_SETTINGS:
	response = RESPONSE_DISPLAY_LAN_SETTINGS;
	break;
	
	case METHOD_ARP_CACHE:
	response = RESPONSE_DISPLAY_ARP_CACHE;
	break;

	case METHOD_FREEMEM:
	response = RESPONSE_DISPLAY_FREEMEM;
	break;

	case METHOD_RPAR:
	case METHOD_RSET:
	// Paknet : reset the SVC
	response = RESPONSE_ERROR;
	break;

	case METHOD_CALL:
	// Make a call
	if(linestate == LINESTATE_OFFLINE)
		{
		// Dial call
		response = init_dial(CurrentLine);
		if(response == RESPONSE_NONE)
			prompt=false;

		break;
		}

	// Already online.
	// Reconnect online command session to line
	// Fall through to call;

	case METHOD_CON:
	// Reconnect online command session to line
	// TODO Verify CON is the correct command
	// Note that CALL does the same in online cmds mode
	if(linestate == LINESTATE_ONLINE_CMDS)
		{
		linestate = LINESTATE_ONLINE;	
		// Reset the forward delay
		hw->uart[HW_IF].flag_interCharTimeoutRx_time = X3params.P4_datafwd * INTERCHAR_TIMEOUT_CALIBRATION;	
		response = RESPONSE_COM;
		prompt = false;
		inbuf_in = 0;
		}
	else
		// Can't reconnect
		response = RESPONSE_ERROR;
	break;

	default:
	// Check for a numeric string which is a dial command
	// Nb. Ease2 software sends nnnn<ip-addr 12 digits>nn  (n=numeric)
	if(( CurrentLine[0] >='0') && ( CurrentLine[0] <='9'))
		{
		CurrentLine[12+4] = NULL;
		response = init_dial(&CurrentLine[4]);
		if(response == RESPONSE_NONE)
			prompt=false;

		// Don't leave set to METHOD_UNSUPPORTED
		m_nPadCommand = METHOD_CALL;
		}
	else
		response = RESPONSE_ERROR;
		
	break;
	}

if(m_nPadCommand == METHOD_UNSUPPORTED)
	response = RESPONSE_ERROR;

// Echo the response to DTE - but not the command
serialout(PAD_response(response),HW_IF);

return true;
}

//--------------------------------------------------------------------------
int CPad::ParseCommand(unsigned char* CurrentLine,unsigned int len)
{
int keylen;

m_nPadCommand =METHOD_UNSUPPORTED;
// Parse current line for a method (command)from the method table
for(int i =0; i < MethodTableLen; i++)
	{
	keylen = (unsigned int)strlen((const char*)MethodTable[i].key);
	if(len >= keylen)
		if(0 == strncmp((const char*)CurrentLine, MethodTable[i].key,keylen ))
			{
			// Trim the method out of the command line
			m_nPadCommand = MethodTable[i].id;
			TrimLeft(CurrentLine,keylen);
			len = (unsigned int)strlen((const char*)CurrentLine);
			break;
			}
	}

return m_nPadCommand;
}

//--------------------------------------------------------------------------
void CPad::OnMessage(int16 messg,int32 lParam,int32 wParam,int16 zParam)
// A timer has expired.
// Action is based on parameter "arg"
{
int response;

if( linestate == LINESTATE_DIAL_WAIT )
	{
	// Delayed dial operation whilst ARPing for destination ip addr
	response = dial(dial_ip_addr);
	serialout(PAD_response((CPad::RESPONSE_TYPE)response),HW_IF);
	// If dial failed, re-issue prompt following cause message
	if((CPad::RESPONSE_TYPE)response != RESPONSE_NONE)
		serialout(PAD_response(RESPONSE_PROMPT),HW_IF);
	return;
	}

if(( linestate < LINESTATE_ONLINE ) && (transport_state != TRANSPORT_CLOSE ))
	{
	response_buf[0] = NULL;
	answer();	
	}
}
