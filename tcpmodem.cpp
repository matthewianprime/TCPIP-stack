// CTCPModem.cpp
// TCP Terminal server using AT command interface.
// Also implements rfc2217 for inband control signal data
// ATD<ip-addr>

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "tcpmodem.h"
#include "utils.h"
#include "arp.h"
#include "ip.h"
#include "ffs.h"
#include "lan.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

extern int16 tcp_transient_port_number;			// Our Port number when making an outgoing TCP call
#ifdef SMTP
#include "smtp.h"
extern Csmtp *smtp;
#endif

static const struct CTCPModem::_tagResponseTable ResponseTable[] = 
	{
	{CTCPModem::RESPONSE_NONE,						szNull,				szNull				},
	{CTCPModem::RESPONSE_OK,						sz0,				szOK				},
	{CTCPModem::RESPONSE_RING,						sz2,				szRING				},
	{CTCPModem::RESPONSE_ERROR,						sz4,				szERROR				},
	{CTCPModem::RESPONSE_CONNECT,					sz1,				szCONNECT			},
	{CTCPModem::RESPONSE_NOCARRIER,					sz3,				szNO_CARRIER		},
	{CTCPModem::RESPONSE_IDENTITY,					szATI,				szATI				},
	{CTCPModem::RESPONSE_ECHO,						szNull,				szNull				},
	{CTCPModem::RESPONSE_DISPLAY_LAN_SETTINGS,		szSTAR_C1,			szSTAR_C1			},
	{CTCPModem::RESPONSE_DISPLAY_AT_SETTINGS,		szSTAR_C,			szSTAR_C			},
	{CTCPModem::RESPONSE_DISPLAY_ARP_TABLE,			szNull,				szNull				},
	{CTCPModem::RESPONSE_DISPLAY_OBJECTS,			szNull,				szNull				},
	{CTCPModem::RESPONSE_DISPLAY_BUILDINFO,			szBuildInfo,		szBuildInfo			},
	{CTCPModem::RESPONSE_DISPLAY_FREEMEM,			szFreememDisplay,	szFreememDisplay	},
	{CTCPModem::RESPONSE_DISPLAY_LAN,				szSTAR_C3,			szSTAR_C3			},
	{CTCPModem::RESPONSE_NO_DIALTONE,				sz6,				szNO_DIALTONE		},
	{CTCPModem::RESPONSE_ABORTED,					szABORTED,			szABORTED			},
	{CTCPModem::RESPONSE_NUMBER,					szpercent_u_crlf,	szpercent_u_OKcrlf	},
	{CTCPModem::RESPONSE_MSG,						szNull,				szNull				}
	};

//--------------------------------------------------------------------------
void CTCPModem::init(int HW_IFparam,CProtocol_L3* pTCP,bool bAutoDelete)
// Called:
// 1: At startup with pTCP==NULL if this is the shell instance
// 2: On an incoming TCP connection
{
outbuf = &packet_buf[TCP_DATA_OFFSET];
//com_local_rfc2217 = false;				// Per - connection variable
//com_remote_rfc2217 = false;				// Per - connection variable
modemstate_flags = 0;
linestate_flags = 0;

name = szTCPModem;
transport = pTCP;
szBanner = NULL;
quit = false;
autodelete =  bAutoDelete;		// Delete instance when quit flag is set
HW_IF = HW_IFparam;

plus_count = 0;					// Counts '+' chars to check for escape sequence
ring_count = 0;

state 			= STATE_LOOK_FOR_A;
linestate		= LINESTATE_OFFLINE;
response		= RESPONSE_OK;
transport_state = TRANSPORT_CLOSE;
pos= 0;

rfc2217_state = NOT_NEGOTIATED_2217;

ResponseTableLen = sizeof(ResponseTable)/ sizeof(struct _tagResponseTable);

response_buf = &packet_buf[1000];		// AT cmd responses.
*response_buf = NULL;
MRU = TCP_BUFLEN;						// Default MRU for TCP

// Restore AT defaults only if not the shell AT decoder
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

// Default AT commands from web page
if(strlen(modem_config.at_cmd))
	{
	// If user has entered "AT" or "at", remove it
	if( (( modem_config.at_cmd[0] == 'a') || ( modem_config.at_cmd[0] == 'A')) &&
		(( modem_config.at_cmd[1] == 't') || ( modem_config.at_cmd[1] == 'T')) )
 		pos=2;

	// parse() destroys the input string...
	strcpy((char*)ipstr,&modem_config.at_cmd[pos]);
	response = (RESPONSE_TYPE)parse(ipstr);

	// Will Echo the response to DTE0 - but not the command
	}

if(transport)
	pTCP->idletimer_ms = modem_config.tcp_idletimer;			// Max idle time before deletion
else
	// Output OK when first initialised, but not on an incoming call
	serialout(AT_response(response),HW_IF);

return;
}

//--------------------------------------------------------------------------
int CTCPModem::receive(unsigned char **rxdata,unsigned int *plen,unsigned int window)
// Received data from LAN, send it out on the serial interface
// Returns to TCP : ACKNOWLEDGE, TERMINATE, NORESPONSE (generally means response already sent) 
{
unsigned char *data = *rxdata;
unsigned int len = *plen;
data[len] = NULL;
// No data to send back (length=0)
*plen = 0;
int32 ms;
int TelnetOptionLen=0;
int32 uart_buffered_bytes;
int16 offset;
char* pIAC=NULL;

if(modem_config.com_enable_rfc2217)
	{
	// Telnet mode
	// See if there are telnet negotiation options in the data..
	// First, loop removing any sequence IAC IAC and replace with IAC (transparency escape sequence)

	do
		{
//		pIAC = (char*)memchr(data,IAC,len);
		if(pIAC != NULL)
			{
			if(*(pIAC+1) == IAC)
				{
				// IAC IAC Escape sequence detected
				offset = (unsigned char*)pIAC-data;	// Offset where sequence starts
				offset = len - offset;
				// Copy overwriting first IAC character
				memmove(pIAC,pIAC+1,len-offset);
				len-=1;
				}
			}
		}while((pIAC != NULL) && (len > 0));

	*plen=len;
	TelnetOptionLen = TelnetOptions(rxdata,plen);

	if(*plen)
		{
		// TelnetOptions() sets *plen to the length of its reply
		// This is the position of any responses to the telnet negotiation options
		// Any data sharing the frame with telnet negotiation options is discarded when we return
		*rxdata = (unsigned char*)packet_buf + TCP_DATA_OFFSET;
		return ACKNOWLEDGE;
		}
	
//	sprintf(msg,"TelnetOptions parsed %u of %u bytes",TelnetOptionLen,len);
//	OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);

	// Data length without telnet negotiation options
	len -= TelnetOptionLen;
	// Offset data pointer to the start of telnet data
	data += TelnetOptionLen;
	// There were both telnet data and telnet negotiation options in this frame.
	// Nb.
	// Fall thru to process received chars, ( pointed at by data )
	// If *plen>0 there is a response to the telnet options at ( pointed at by *rxdata )
	}

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
int CTCPModem::serial_receive(unsigned char **ppdata,unsigned int *plen,unsigned int window)
// Received data from serial port
// We are emulating a modem, so data is either:
// - an AT command string
// - data to be sent over the lan connection
{
unsigned char *data = *ppdata;
unsigned char *data_saved = *ppdata;
data[*plen] = 0;						// Null terminate command string
unsigned char *pdata = data;
int charstoparse = *plen;
int cmdlen = charstoparse;
*plen = 0;
if(linestate != LINESTATE_ONLINE)
	response_buf[0] = NULL;				// Response message buffer.nb This would corrupt Tx data when online

sprintf(msg,"<DTE Rx %u> ",charstoparse);
OutputDebugString(LOG_INFO,LOG_SERIAL,(const char*)msg,false);
printformatstring(msg,0,(char*)data,min(charstoparse,20),traceout.hexmode,false);
OutputDebugString(LOG_INFO,LOG_SERIAL,(const char*)msg);

switch(linestate)
	{
	case LINESTATE_DIAL_WAIT:
	// Dial / answer abort
	// Abort dial
	if(transport)
		transport->close();
	response = RESPONSE_ABORTED;
	linestate = LINESTATE_OFFLINE;
	state = STATE_ENDLINE;
	charstoparse = 0;
	// Restore autobaud setting
	UpdateUart(HW_IF,OFFLINE);
	break;
	
	case LINESTATE_ONLINE:
	// Send data to remote TCP
	if(transport)
		transport->output(data,cmdlen, ACK,0,HW_LAN);
	
	// Check for "+++". 
	while((charstoparse--) && (plus_count < 3))
		{
		if(((*pdata & 0x7f) == modem_config.S2_escape_char) && (modem_config.S2_escape_char != 0))		// Generally a " + " char
			plus_count++;
		else
			plus_count = 0;
		}

	if(plus_count < 3)
		// Online data, no data to return
		return 0;

	// +++ received
	// Fall through switch to respond OK
	OutputDebugString(LOG_DEBUG,LOG_TCP,szModemLocalAccess);
	linestate = LINESTATE_ONLINE_CMDS;
	response = RESPONSE_OK;
	state = STATE_ENDLINE;
	plus_count = 0;
	charstoparse = 0;
	cmdlen = 0;
	response_buf[0] = NULL;						// Contains data from last frame
	break;

	case LINESTATE_ONLINE_CMDS:
	case LINESTATE_OFFLINE:

	do
		{
		// Loop parsing chars
		switch(state)
			{
			case STATE_LOOK_FOR_A:
			// Nb. Autobaud only detects Even parity if capital 'A'
			// nb. Autobaud algorithm does not detect:
			// 'a' even parity
			// 'A' odd parity
			response = RESPONSE_NONE;		// No echo until 'A' received
		
			if((((*pdata)&0x7f) == 'A') || (((*pdata)&0x7f) == 'a'))
				{
				if(modem_config.autobaud_flag)
					{
					// Respond using 8n1 until parity detected
					modem_config.parity_flag = FORMAT_8N;
					hw->uart[HW_IF].uart_parity_rx=modem_config.parity_flag;
					hw->uart[HW_IF].uart_parity_tx=modem_config.parity_flag;
					}

				state = STATE_LOOK_FOR_T;
				response = RESPONSE_ECHO;
				}		
			charstoparse -= 1;
			break;

			case STATE_LOOK_FOR_T:
			if((((*pdata)&0x7f) == 'T') || (((*pdata)&0x7f) == 't'))
				{
				// Detect even parity if autobaud and capital T
				if( (modem_config.autobaud_flag) && (((*pdata)&0x7f) == 'T'))
					{
					modem_config.parity_flag = (*pdata) >> 7;				// If set, data is even parity
					hw->uart[HW_IF].uart_parity_rx=modem_config.parity_flag;
					hw->uart[HW_IF].uart_parity_tx=modem_config.parity_flag;
					}

				state = STATE_LOOK_FOR_ENDLINE;
				pos=0;								// Start making line for parser
				}

			else if((((*pdata)&0x7f) == 'A') || (((*pdata)&0x7f) == 'a'))
				{
				// Got "AA"
				response = RESPONSE_ECHO;	
				state = STATE_LOOK_FOR_T;
				}
			else
				{
				// Got "An"
				response = RESPONSE_NONE;	
				state = STATE_LOOK_FOR_A;
				}
				
			charstoparse-=1;
			break;
	
			case STATE_LOOK_FOR_ENDLINE:
			// Hayes Accura V92 endline behaviour:
			// <cr> OR <cr> <lf>	both respond <cr><cr><lf>OK<cr><lf>
			// <lf> echoed but not taken as end line char
			// <lf><cr> responds <cr><cr><lf>ERROR<cr><lf>
			if((pos + charstoparse) >= MAX_AT_CMDLEN)
				{
				// Command too long
				state = STATE_ENDLINE;
				response = RESPONSE_ERROR;
				charstoparse = 0;
				break;
				}

			strip_parity(pdata,charstoparse);
		
			// Add the data to the command line buffer
			while(charstoparse--)
				{
				commandline[pos++] = *pdata;

				if(*pdata == CH_CR)
					{
					// Command termination char detected. 
					// Echo received character(s)
					*(pdata+1) = NULL;								// Don't echo a <lf> following the <cr> - or anything else
					if(modem_config.echoflag)
						serialout((char*)data_saved,HW_IF);
						
					// Parse the commands
//					OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(const char*)" ");// Endline on trace o/p
					state = STATE_ENDLINE;
					commandline[pos-1] = NULL;							// Remove endline char
					response = (RESPONSE_TYPE)parse(commandline);		// Changes "state"
					charstoparse = 0;
					pos=0;
					break;
					}
				*pdata++;
				}// End while
			break;					// end case STATE_LOOK_FOR_ENDLINE
			}						// end switch(state)
		pdata++;
		}while(charstoparse > 0);	// Parse all chars in line
	}								// end switch

if(state == STATE_ENDLINE)
	// Command line parsed
	state = STATE_LOOK_FOR_A;
	
if(response == RESPONSE_ECHO)
	{
	// AT command mode. Command line not terminated yet.
	if(modem_config.echoflag)
		// Echo rcvd characters
		*plen = cmdlen;
	return 0;
	}

// AT commmand line terminated
// Copy response text into response buffer
AT_response(response);
*plen = strlen((const char*)response_buf);
memcpy(data,response_buf,*plen);

// Trace the response
//OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(const char*)response_buf);

// Restore autobaud setting. Autobaud stays off while online, DIALING and ANSWERING
if(linestate == LINESTATE_OFFLINE)
	{
	if(modem_config.autobaud_flag)
		{
		// Parity may have been detected and set other than "none"
		// Nb Don't change uart_parity_tx as it affects the parity of our response
		modem_config.parity_flag = FORMAT_8N;
		hw->uart[HW_IF].uart_parity_rx=modem_config.parity_flag;
		
		hw->uart[HW_IF].uart_autobaud_active = 1;

		// Update baudrate var for screen dumps
		modem_config.baudrate_flag =  hw->uart[HW_IF].baudOversampleRate;
		modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
		}
	}
	
return 0;
}

//--------------------------------------------------------------------------
int16 CTCPModem::GetTransmitBuffer()
// Return how many bytes we can accept from the serial interface
// This depends on how much TCP can accept if we are online.
{
if((transport) && (linestate == LINESTATE_ONLINE))
	return transport->GetTransmitBuffer();

if((transport) && (linestate == LINESTATE_DIAL_WAIT) && (modem_config.S42_dialabort==0))
	// Waiting for a call to complete.
	// Buffer serial data
	return 0;

// Command mode.
// AT decoder accepts both 1-char-at-a-time, or command fragmented into strings
return hw->uart[HW_IF].tx_buffer_len;
}

//--------------------------------------------------------------------------
int16 CTCPModem::GetReceiveBuffer()
// Return how many bytes we can accept from the LAN
{
if((transport) && (linestate == LINESTATE_ONLINE))
	return hw->uart[HW_IF].tx_buffer_len - hw->uartOutputQueue(HW_IF);

return 0;
}

//--------------------------------------------------------------------------
int CTCPModem::OnTransport(int message,int param)
// Message from transport layer ( TCP or UDP )
{
char eia_state = 0;
response_buf[0] = NULL;
unsigned char *p;

switch(message)
	{
	case TRANSPORT_CLOSE:
	// Transport deleted - TCP connection closed

	// DCD  off
	if(modem_config.dcdflag != 0)
		hw->uart[HW_IF].flag_dcd = LO;

	if(linestate > LINESTATE_OFFLINE)
		serialout(AT_response(RESPONSE_NOCARRIER),HW_IF);

	// Reset ring counter
	ring_count = 0;
	// Stop RING message being output
	untimeout(MSG_ALL);
	// Flush UART buffer. If session closed with CTS off this turns it back on
	hw->uartInputFlush(HW_IF);	

	// Restore autobaud setting
	UpdateUart(HW_IF,OFFLINE);
	
	transport_state = TRANSPORT_CLOSE;
	state = STATE_LOOK_FOR_A;
	linestate = LINESTATE_OFFLINE;
	break;

	case TRANSPORT_OPEN:
	// Transport layer is available to pass data

	transport_state = TRANSPORT_OPEN;

	rfc2217_state = NOT_NEGOTIATED_2217;

	if(linestate == LINESTATE_OFFLINE)
		{
		// Incoming call
		if((ring_count >= modem_config.S0_answer_rings) && (modem_config.S0_answer_rings > 0 ))
			answer();
		break;
		}

	if(linestate < LINESTATE_ONLINE)
		// Waiting for a dial or answer to complete
		answer();

	break;


	case TRANSPORT_CALL:
	// Incoming call (TCP SYN received)
	// Start "RING" messages

	if(transport_state == TRANSPORT_CALL)
		// Repeat of initial SYN. OK, but don't report RING again
		break;

	// Check we have an empty serial buffer,
	// else return 0 for call to be rejected
	// - unless trace o/p is on our serial port
	if(hw->uartOutputQueue(HW_IF) >0)
		{
		if(hw->uart[HW_TRACE].owner != HW_IF)
			return 0;
		}
	
	transport_state = TRANSPORT_CALL;
	serialout(AT_response(RESPONSE_RING),HW_IF);
	ring_count = 1;

	// Additional RINGs output on timer expiry
	// (Already sent first RING for a fast connect)
	timeout(0,1000);
	break;
	
	case TRANSPORT_DTEPORT:
	p = &packet_buf[TCP_DATA_OFFSET]; 
	
	// nb Tactical software ignores CTS and DSR in RFC2217 mode.
	// RTS is used for local flow control only
	if(param & DELTA_RTS)
		return 0;
		
	if(param & DELTA_DTR)
		{
		// Use for DCD line when connected to a DCE
		// DTR state change
		sprintf(msg,szModemDTR,(param & DTR_STATE)? szParamon : szParamoff);
		OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(const  char*)msg);
		
		if((transport) && 
			(rfc2217_state & NEGOTIATED_SEND_2217))
			{
			// Inform telnet peer
			eia_state = 0x10 | 0x20 | 0x08;		// CTS | DSR | Delta DCD
			if(hw->uart[HW_IF].flag_dtr) 
				eia_state = eia_state | 0x80;	// DCD

			OPTION_CHAR(NOTIFY_MODEMSTATE,eia_state,p);
			transport->output(&packet_buf[TCP_DATA_OFFSET],7, ACK,0,HW_LAN);
			}
			
		if(!(param & DTR_STATE))
			{
			// DTR dropped, Flush UART buffer.
			hw->uartInputFlush(HW_IF);	

			if(modem_config.dtrflag != 0)
				{
				// Hang up
//				if(linestate != LINESTATE_OFFLINE)
//					transport->close();
				}
			}
		else
			{
			// DTR rise
			if( (modem_config.dtrflag == 3) && (linestate == LINESTATE_OFFLINE) )
				{
				// DTR dial mode is on
				sprintf(msg,szModemDialing,modem_config.dtrdialnumber,modem_config.remote_port);
				OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(const char*)msg);

				response = init_dial(NULL,modem_config.dtrdialnumber);
				AT_response(response);
				if(response_buf[0] != NULL)
					serialout((char*)response_buf,HW_IF);
				}
			}
		}
	break;
	};
	
return 1;
}

//--------------------------------------------------------------------------
const char* CTCPModem::AT_response(RESPONSE_TYPE response,bool tracelog)
// Response text copied to response_buf.
// Returns pointer to response_buf
{
const char *connectspeed = szEndline;
int len=0;
int index=0;

switch(response)
	{
	case RESPONSE_DISPLAY_AT_SETTINGS:
	sprintf(msg,ResponseTable[response].verbose,HW_2ascii(HW_IF),linestate_2ascii(linestate),modem_config.verboseflag,modem_config.dcdflag,modem_config.dtrflag,modem_config.quietflag,modem_config.S0_answer_rings,modem_config.autobaud_flag? szON:szOFF,dataformat_2ascii(modem_config.parity_flag),flowcontrol_2ascii(modem_config.flowcontrol_flag),modem_config.echoflag);
	strcat((char*)response_buf,(const char*)msg);
	goto trace;

	case RESPONSE_DISPLAY_LAN_SETTINGS:
	sprintf((unsigned char*)msg,ResponseTable[response].verbose,lan[UNSAVED].ip_addr,lan[UNSAVED].netmask,lan[UNSAVED].gateway,lan[UNSAVED].subnet,mac_addr,modem_config.local_port,modem_config.remote_port,modem_config.tcp_idletimer/1000);
	strcat((char*)response_buf,(const char*)msg);
	goto trace;

	case RESPONSE_IDENTITY:
	sprintf((unsigned char*)msg,ResponseTable[response].verbose,szSoftwareVersion);
	strcat((char*)response_buf,(const char*)msg);
	goto trace;

	case RESPONSE_NUMBER:
	sprintf((unsigned char*)msg,ResponseTable[response].verbose,response_number);
	strcat((char*)response_buf,(const char*)msg);
	goto trace;

	case RESPONSE_DISPLAY_ARP_TABLE:
	serialout(szARPCacheDisplay,HW_IF);
	while(index < ARP_CACHELEN)
		{
		msg[0]=0;
		index = arpcache_2ascii(msg,index);
		serialout((char*)msg,HW_IF);
		};
	response_buf[0]=NULL;
	goto trace;

	case RESPONSE_DISPLAY_OBJECTS:
	objects_2ascii(response_buf);
	goto trace;

	case RESPONSE_DISPLAY_BUILDINFO:
	version_2ascii(msg);
	strcat((char*)response_buf,(const char*)msg);
	goto trace;
	
	case RESPONSE_DISPLAY_FREEMEM:
	len = freemem();
	sprintf((unsigned char*)msg,ResponseTable[response].verbose,len);
	strcat((char*)response_buf,(const char*)msg);
	goto trace;

	case RESPONSE_DISPLAY_LAN:
	plan->link_test(ipstr);
	sprintf((unsigned char*)msg,szSTAR_C3,ipstr,plan->framessent,plan->framesreceived,plan->framesdiscarded,plan->frameoverruns);
	strcat((char*)response_buf,(const char*)msg);	
	goto trace;
		
	case RESPONSE_MSG:
	// Response is in "msg"
	strcpy((char*)response_buf,(char*)msg);
	goto trace;
	}
	
if(modem_config.quietflag)
	goto trace;

// CONNECT <speed>
msg[0] = ' ';
switch(modem_config.ATX)
	{
	case 1:
	strcpy((char*)&msg[1],szConnectSpeedIP);
	strcat((char*)msg,szEndline);
	break;
	case 2:
	strcpy((char*)&msg[1],(char*)baudrate_2ascii(HW_IF));
	strcat((char*)msg,szEndline);
	break;
	default:
	strcpy((char*)msg,szEndline);
	break;
	}
connectspeed = (char*)msg;	

len = strlen((char*)response_buf);

if(modem_config.verboseflag)
	sprintf(&response_buf[len],ResponseTable[response].verbose,connectspeed);
else
	strcat((char*)response_buf,ResponseTable[response].numeric);

trace:
if((tracelog)&&(traceout.module & LOG_SERIAL))
	{
	// Response to trace port
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
void CTCPModem::answer()
// Go to online state
{
// Autobaud off when online .. but leave config.autobaud unchanged so
// the current setting can be restored when the call terminates.
UpdateUart(HW_IF,ONLINE);

// Waiting for a dial or answer to complete
serialout(AT_response(RESPONSE_CONNECT),HW_IF);
	
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
CTCPModem::RESPONSE_TYPE CTCPModem::init_dial(unsigned char *data,int32 ip_addr)
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

if( data[0] == 'L' )
	{
	// ATDL dial last number
	ip_addr = dial_ip_addr;
	goto set_dial_timer;
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
int CTCPModem::dial(int32 ip_addr)
// Dial the specified ip address.
// The ip should be present in the ARP cache
{
if(linestate != LINESTATE_DIAL_WAIT)
	// Dial operation was aborted
	return RESPONSE_NONE;

// Is the gateway or destination IP address in our arp cache ?
if(!arp->isincache(ip_addr,NULL))
	{
	// Restore autobaud setting
	UpdateUart(HW_IF,ONLINE);
	linestate = LINESTATE_OFFLINE;
	return RESPONSE_NO_DIALTONE;
	}

// Initiate a tcp connection - create a TCP transport instance
int n = ipV4->CreateTransport(IP_TCP,HW_LAN,tcp_transient_port_number,this,ip_addr);
transport = ip_protocols[n];
if((!transport) || (n == 0))
	{
	// Out of memory ?
	linestate = LINESTATE_OFFLINE;
	// Restore autobaud setting
	UpdateUart(HW_IF,OFFLINE);
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
int CTCPModem::atoi2(char* str,int* numparsed)
// Helper for command parsing
// As atoi except returns the number of chars parsed
{
int i;
#define LIMIT 20	// Max chars to look at

if ((str[0]<'0') || (str[0]>'9') )
	// Not numeric
	return -1;
	
for(i=0;i<LIMIT;i++)
	{
	if ( (str[i]<'0') || (str[i]>'9') )
		break;
	}

// i is the numeric string length
*numparsed = *numparsed + i;
return atoi((char*)str);	
}


//--------------------------------------------------------------------------
int CTCPModem::parse(unsigned char *pData)
// Returns length of response
{
unsigned char *data = pData;
char ch;
int16* pMac;
int m,n;				// Scratch vars
int32 ip_addr;
volatile int parsed = 0;
			
response = RESPONSE_OK;
int length = strlen((const char*)data);
if(length == 0)
	// AT entered on its own
	return response;

data[length] = NULL;

// Parse command for delete chars.
unsigned char *del = data;
while(del != NULL)
	{
	del = (unsigned char*)strchr((const char*)data,CH_DEL);
	if (del == data)
		// First char of data is a del character.
		memcpy(del,del+1,length+1);

	if(del > data)
		memcpy(del-1,del+1,length+1);
	}

// AT decoder is uppercase only
makeupper(data);

length = strlen((const char*)data);
if(length == 0)
	// AT entered on its own
	return response;

parse_next_command:

switch(data[parsed])
	{
#ifdef SMTP
	case 'W':
//	smtp = new(Csmtp);
//	smtp->SendMail(char* dest_email_addr,char* mail_subject,char *mail_body);
	smtp->Init_SendMail("user@dest.com","SSubject","BBody");
	break;
#endif
	
	// Offline or online commands. 
	// Default case contains switch of online-only and offline-only commands	
	case 'F':
	ch = data[++parsed];
	//Note: uart_parity  0: 8bit-data 1: 7bit-data,even 2: 7bit-data,odd	
	switch(ch)
		{
		case '0':
		case '1':
		case '2':		
		modem_config.parity_flag = ch - '0';
		if(ch != '0')
			// Fixed parity, set autobaud off
			UartAutobaudEnable(HW_IF,false);

		UpdateUart(HW_IF,OFFLINE);
		break;

		default:
		response = RESPONSE_ERROR;
		break;
		}
	break;
	
	case 'O':
	case 'A':
	if((transport_state == TRANSPORT_CALL) || (transport_state == TRANSPORT_OPEN ))
		{
		pos=0;	// No buffered data to send upon connection established
		answer();
		response_buf[0]=NULL;
		response = RESPONSE_NONE;
		}
	else
		response = RESPONSE_ERROR;
		
	parsed = length;		// No command follows ATA/O
	break;

	case 'B':
	// Baudrate
	ch = data[++parsed];
	if ( (ch>='0') && (ch<='7') )
		{
		// Depends ON BAUDOVERSAMPLERATE_MODE in hardware.cpp
		// 0 = BAUDRATE_115200, 5,6,7=4800;
		// SEB 0,1,2 = 38400 7 = BAUDRATE_1200
		ch = ch - '0';
		modem_config.baudrate_flag =  hw->setBaudOversampleRate[ch];
		UartAutobaudEnable(HW_IF,false);
		UpdateUart(HW_IF,OFFLINE);
		}
	else
		response = RESPONSE_ERROR;

	break;

	case 'E':
	ch = data[++parsed];
	if ( (ch>='0') && (ch<='1') )
		modem_config.echoflag = ch - '0';
	else
		response = RESPONSE_ERROR;
	break;

	case 'H':
	// Accept no more commands on the line
	parsed = length;
	if(linestate >= LINESTATE_OFFLINE)
		{
		// Wait for TCP to close before "NO CARRIER"
		if(transport)
			{
			transport->close();
			response = RESPONSE_NONE;
			break;
			}
		}

	// Stop RING message being output
	untimeout(MSG_ALL);
	// Offline
	response = RESPONSE_OK;
	break;
				
	case 'M':
	// Speaker on/off command
	case 'C':
	// Dataflex command ATC4 Ignore CLI
//	case 'F':
	// Dataflex command ATF0 = Highest line speed select
	case 'G':
	// Dataflex command ATG0 - No guard tone
	ch = data[++parsed];
	if (! ((ch>='0') && (ch<='9')) )
		response = RESPONSE_ERROR;
	break;	

	case 'I':
	ch = data[parsed+1];
	if ( (ch>='0') && (ch<='9') )
		parsed++;									// ATIn

	if (ch == '8')
		// Display full build information
		AT_response(RESPONSE_DISPLAY_BUILDINFO,false);	// ATI8
	else
		AT_response(RESPONSE_IDENTITY,false);				// ATI or ATIn
	break;

	case 'S':
	// S-registers
	parsed++;
	response = RESPONSE_OK;

	// Get the S-reg number
	m = atoi2((char*)&data[parsed],(int*)&parsed);		// m is S-reg number

	if( m == -1 )
		{
		// Syntax error - not numeric after ATS
		response = RESPONSE_ERROR;
		break;
		}

	// Is a value being enquired ?
	if(data[parsed] == '?')
		goto enquire;

	// Is a value being set ?
	if(data[parsed] != '=')
		{
		// Syntax error - '?' or '=' is not after ATS
		response = RESPONSE_ERROR;
		break;
		}

	parsed++;
	n = atoi2((char*)&data[parsed],(int*)&parsed);		// n is S-reg value
	if( n == -1 )
		{
		// Syntax error - not numeric after ATSn=
		response = RESPONSE_ERROR;
		break;
		}
	parsed += n;
	parsed-=1;// incremented at end of case

	// Set an S-register value
	switch (m)
		{
		case 0:
		//ATS0= rings to answer
		modem_config.S0_answer_rings = n;
		break;
			
		case 2:
		//ATS2= command mode escape char. Enter the decimal value not the char
		modem_config.S2_escape_char = n;
		break;

		case 42:
		modem_config.S42_dialabort = n;
		break;

		case 1007:
		modem_config.tcp_idletimer = n;
		break;
		
		case 1008:
		modem_config.local_port = n;
		break;

		case 1009:
		modem_config.remote_port = n;
		break;

		default:
		response = RESPONSE_ERROR;
		break;
		}
	break;

enquire:
	switch(m)
		{
		case 0:
		response_number = modem_config.S0_answer_rings;
		response = RESPONSE_NUMBER;
		break;
		case 2:
		response_number = modem_config.S2_escape_char;
		response = RESPONSE_NUMBER;
		break;
		case 42:
		response_number = modem_config.S42_dialabort;
		response = RESPONSE_NUMBER;
		break;
		default:
		response = RESPONSE_ERROR;
		break;
		}

	break;
	
	case 'Q':
	ch = data[++parsed];
	if ( (ch>='0') && (ch<='1') )
		modem_config.quietflag = ch - '0';
	else
		response = RESPONSE_ERROR;
	break;
		
	case 'V':
	ch = data[++parsed];
	if ( (ch>='0') && (ch<='1') )
		modem_config.verboseflag = ch - '0';
	else
		response = RESPONSE_ERROR;
	break;
	
	case 'X':
	// Dataflex command ATX4 - Report dial and busy
	ch = data[++parsed];
	if ( (ch>='0') && (ch<='2') )
		modem_config.ATX = ch - '0';
	else
		response = RESPONSE_ERROR;
	break;

	case 'Y':
	// Reboot
	watchDogTimerDisable=1;
	break;
	
	case '*':
	// A screen display must be the last command on the command line
	// because multiple screen displays will overrun the o/p buffer
	ch = data[++parsed];
	switch (ch)
		{
		case 'C':
		ch = data[parsed+1];
		if (ch=='1')
			AT_response(RESPONSE_DISPLAY_LAN_SETTINGS,false);	//AT*C1
		else if (ch=='2')
			AT_response(RESPONSE_DISPLAY_ARP_TABLE,false);		//AT*C2
		else if (ch=='3')
			AT_response(RESPONSE_DISPLAY_LAN,false);			//AT*C3
		else if (ch=='4')
			AT_response(RESPONSE_DISPLAY_FREEMEM,false);		//AT*C4
		else if (ch=='5')
			AT_response(RESPONSE_DISPLAY_OBJECTS,false);		//AT*C5
		else
			AT_response(RESPONSE_DISPLAY_AT_SETTINGS,false);	//AT*C
		parsed = length;
		break;
		
		case 'S':
		// Dataflex command AT*Sn remote access security level n
		ch = data[++parsed];
		if (! ((ch>='0') && (ch<='1')) )
			response = RESPONSE_ERROR;
		break;

		default:
		response = RESPONSE_ERROR;
		break;		
		}
	break;

	case 'P':
	// Dataflex command ATT - Pulse dial
	ffs->update_sofware();
	break;
	case 'T':
	// Dataflex command ATT - Tone dial
	break;

	default:

	if(linestate != LINESTATE_OFFLINE)
		{
		response = RESPONSE_ERROR;
		goto end_switch;		
		}
		
	// Offline-only commands
	switch(data[parsed])
		{
		case 'D':
		// Syntax 1 atdaaabbbcccddd Pnn(a.b.c.d = ip addr, nn= port no. Use port 2000 if not spec'd)
		// Syntax 1 atda.b.c.d Pnn
		response = init_dial(++data);
		parsed = length;
		return response;

		case 'Z':
		// Restore active settings from flash
		ffs->ffsReset();
		if(ffs->open(MODE_OPENEXISTING,(char*)profile,0))
			{
			ffs->restore_from_flash(SAVE_RESTORE_AT);
			ffs->close();
			}
		// Set UART
		UpdateUart(HW_IF,OFFLINE);
		parsed = length;
		break;

		case '!':
		switch (data[++parsed])
			{
			case 'T':
			// AT!T disable TCP idle disconnect. Not saved
			if(transport)
				transport->time = 0xffffffffL;
			break;
			
			case 'D':
			// User is configuring DTR dial number eg AT!D10.0.0.99
			modem_config.dtrdialnumber = ascii_2ip(&data[++parsed]);
			parsed = length;
			break;
			
			case 'M':
			if(ffs->GetMac(mac_addr))
				{// Already got a mac addr
				response = RESPONSE_ERROR;
				parsed = length;
				break;
				}				
			pMac = ascii_2mac(&data[++parsed]);
			if(pMac == NULL)
				{
				response = RESPONSE_ERROR;		
				parsed = length;
				break;
				}
			mac_addr[0] = pMac[0];
			mac_addr[1] = pMac[1];
			mac_addr[2] = pMac[2];

			ffs->SetMac(mac_addr);
			parsed += 17;
			break;

			case 'N':
			// User is configuring our IP Netmask
			ip_addr = ascii_2ip(&data[++parsed]);
			if(ip_addr == 0xFFFFFFFF)
				{// No numeric port number entered
				response = RESPONSE_ERROR;
				break;
				}
			lan[UNSAVED].netmask = ip_addr;
			lan[UNSAVED].subnet = lan[UNSAVED].ip_addr & lan[UNSAVED].netmask;
			parsed = length;
			dirty = true;
			break;

			case 'I':
			// User is configuring our IP address
			ip_addr = ascii_2ip(&data[++parsed]);
			if(ip_addr == 0xFFFFFFFF)
				{// No numeric port number entered
				response = RESPONSE_ERROR;
				break;
				}				
			lan[UNSAVED].ip_addr = ip_addr;
			lan[UNSAVED].subnet = lan[UNSAVED].ip_addr & lan[UNSAVED].netmask;
			lan[UNSAVED].dhcp_server = 0;			// Clear DHCP server ip addr.
			parsed = length;
			dirty = true;
			break;
		
			case 'G':
			// User is configuring our IP gateway
			ip_addr = ascii_2ip(&data[++parsed]);
			if(ip_addr == 0xFFFFFFFF)
				{// No numeric port number entered
				response = RESPONSE_ERROR;
				break;
				}
			lan[UNSAVED].gateway = ip_addr;
			parsed = length;
			dirty = true;
			break;

			case 'P':
			// User is configuring the remote TCP port number for dial conections
			parsed++;
			if(!((data[parsed]>='0') && (data[parsed]<='9')))
				{// No numeric port number entered
				response = RESPONSE_ERROR;
				break;
				}
	
			modem_config.remote_port=atoi((char*)&data[parsed]);
			if(modem_config.remote_port == 0)
				{
				modem_config.remote_port = MODEM_PORT;
				response = RESPONSE_ERROR;
				}

			parsed = length;
			break;

			case 'L':
			// User is configuring the local TCP port number for dial conections
			parsed++;
			if(!((data[parsed]>='0') && (data[parsed]<='9')))
				{// No numeric port number entered
				response = RESPONSE_ERROR;
				break;
				}
	
			modem_config.local_port=atoi((char*)&data[parsed]);
			if(modem_config.local_port == 0)
				{
				modem_config.local_port = MODEM_PORT;
				response = RESPONSE_ERROR;
				}

			parsed = length;
			break;

			case 'C':
			// Configure LEDS
			// 0=off,1=on,2=on for 5 seconds at startup
			++parsed;
			if((data[parsed]<'0') || (data[parsed]>'2'))
				{// No numeric port number entered
				response = RESPONSE_ERROR;
				break;
				}
			data[parsed] = data[parsed] - '0';
			if(data[parsed] == 0)
				plan->configure_leds(LEDS_OFF);
			else if(data[parsed] == 1)
				plan->configure_leds(LEDS_ON);
			else if(data[parsed] == 2)
				// LEDs blink on very very briefly now if we are past startup
				plan->configure_leds(LEDS_ON_AT_STARTUP);
			
			lan[UNSAVED].led_mode = data[parsed];
			dirty = true;
			break;
			
			default:
				response = RESPONSE_ERROR;
			break;
			}
		break;
			
		case '&':
		switch (data[++parsed])
			{
			case 'V':							// AT&V same as AT*Cn
			ch = data[parsed+1];
			if (ch=='1')
				AT_response(RESPONSE_DISPLAY_LAN_SETTINGS,false);	//AT*C1 / V1
			else
				AT_response(RESPONSE_DISPLAY_AT_SETTINGS,false);	//AT*C / v
			parsed = length;
			break;
		
			case 'B':
			// Dataflex autobaud enable command AT&Bn
			ch = data[++parsed];
			if ( (ch>='0') && (ch<='1') )			
				{
				ch -= '0';
				ch &= 0xff;
				UartAutobaudEnable(HW_IF,(bool)ch);
				UpdateUart(HW_IF,OFFLINE);
				}
			else
				response = RESPONSE_ERROR;			
			break;

			case 'W':
			ch = data[parsed+1];
			if ( (ch>='0') && (ch<='1') )			// Dataflex support ATW0, ATW1
				parsed++;
			
			ffs->ffsReset();
			ffs->del(profile,szFILETYPE_PROFILE);

			// Save system settings to flash
			ffs->ffsReset();
			if(ffs->open(MODE_CREATE,(char*)profile,szFILETYPE_PROFILE))
				{
				ffs->save_to_flash(SAVE_RESTORE_ALL);
				ffs->close();
				}
				
			// Save LAN settings to flash if they have changed
			if(dirty)
				{
				ffs->ffsReset();
				ffs->del((unsigned char*)szLAN,szFILETYPE_SYSTEM,true);
				if(ffs->open(MODE_CREATE,szLAN,szFILETYPE_SYSTEM))
					{
					ffs->save_to_flash(SAVE_RESTORE_LAN);
					ffs->close();
					dirty = false;
					}
				}	
			break;

			case 'F':
			ch = data[parsed+1];
			if ( (ch>='0') && (ch<='4') )			// Dataflex support AT&F0, AT&F1
				{
				parsed++;
				ch -= '0';
				ch &= 0xff;
				}
			else ch=0;

			// Restore AT decoder defaults
			ATFactoryReset(HW_IF,(int)ch);
			UpdateUart(HW_IF,OFFLINE);
			break;

			case 'D':
			// DTR options
			// &D3 = DTR rise dial
			ch = data[++parsed];
			if ( (ch>='0') && (ch<='3') )
				modem_config.dtrflag = ch - '0';
			else
				response = RESPONSE_ERROR;
			break;
				
			case 'C':
			// DCD options
			ch = data[++parsed];
			if ( (ch>='0') && (ch<='1') )
				{
				modem_config.dcdflag = ch - '0';
				if(ch == '0')
					hw->uart[HW_IF].flag_dcd = LO;
				else
					hw->uart[HW_IF].flag_dcd = HI;
				}
			else
				response = RESPONSE_ERROR;
			break;
			
			case 'T':			
			// AT&T<n> Trace <level> to DTE1.
			ch = data[++parsed];
			if (ch == '0')
				{// Debug trace to DTE0 off, trace level back to default
				hw->uart[HW_TRACE].owner = OWNER_NONE;
				traceout.level = LOG_DEFAULT;
				}
			else if ((ch>='1') && (ch<='5'))
				{// Debug trace to this serial port. Numeric parameter sets the level
				traceout.level = (int)ch - '0';
				if(traceout.level < 6)
					traceout.module = LOG_ALL - LOG_SERIAL - LOG_IP - LOG_TCP;
				else
					traceout.module = LOG_ALL;

				hw->uart[HW_TRACE].owner = HW_IF;
				}
			else
				response = RESPONSE_ERROR;
			break;
			
			case 'Y':
			// Dataflex command AT&Y0 - use profile 0 on power up
			case 'S':
			// Dataflex command AT&S0 force DSR on
			ch = data[++parsed];
			if (! ((ch>='0') && (ch<='1')) )						// Dataflex support ATW0, ATW1
				response = RESPONSE_ERROR;
			break;

			case 'R':
			//RFC2217 enable
			ch = data[++parsed];
			if (! ((ch>='0') && (ch<='1')) )						// Dataflex support ATW0, ATW1
				response = RESPONSE_ERROR;
			modem_config.com_enable_rfc2217 = ch - '0';
			break;
						
			case 'K':
			// Flow control
			ch = data[++parsed];
			if (! ((ch>='0') && (ch<='4')) )						// AT&K 1-4. 4 special case - change to CTS online, none offline
				response = RESPONSE_ERROR;

			modem_config.flowcontrol_flag = (int) ch - '0';
			if( modem_config.flowcontrol_flag == uartFlowControl_XONXOFF )
				// Clear xoff condition
				hw->uart[HW_IF].rxXon=1;
				
			UpdateUart(HW_IF,OFFLINE);
			break;
			
			default:
				response = RESPONSE_ERROR;
			break;
			}
		break;

		default:
		response = RESPONSE_ERROR;
		break;
		} // end switch(*data) - offline
	}// End case default

if((response != RESPONSE_ERROR) && (++parsed < length))
	goto parse_next_command;
	
end_switch:
pos = 0;
state = STATE_LOOK_FOR_A;
return response;
}

//--------------------------------------------------------------------------
void CTCPModem::OnMessage(int16 messg,int32 lParam,int32 wParam,int16 zParam)
// A timer has expired.
// Action is based on parameter "arg"
{
int response;

if( linestate == LINESTATE_DIAL_WAIT )
	{
	// Delayed dial operation whilst ARPing for destination ip addr
	response = dial(dial_ip_addr);
	serialout(AT_response((CTCPModem::RESPONSE_TYPE)response),HW_IF);
	return;
	}

if(( linestate < LINESTATE_ONLINE ) && (transport_state != TRANSPORT_CLOSE ))
	{
	response_buf[0] = NULL;
	ring_count++;
	
	if((ring_count >= modem_config.S0_answer_rings) && ( modem_config.S0_answer_rings > 0 ))
		// Ring count reached, answer call
		answer();	
	else
		{
		// Ring count not reached, output RING
		serialout((const char*)AT_response(RESPONSE_RING),HW_IF);
		// Output additional RINGs on timer
		timeout(0,2500);
		}
	}
}

//--------------------------------------------------------------------------
int CTCPModem::TelnetOptions(unsigned char **rxdata,unsigned int *plen)
// Parse telnet negotiation options for rfc2217 COM port control options.
// We are the "access server" in rfc2217 ie. We accept configuration commands and generate notification commands.
// The negotiation of the com port control option protocol uses the standard Telnet negotiation protocol mechanism
// Once DO COM_PORT_OPTION and WILL COM_PORT_OPTION have been negotiated, the client may send any of the following commands
// All option commands and responses commence with 0x255 / IAC ( "Interpret As Command" ).
// This routine extracts the options from the data, and responds to the sender as required
// Returns:
// *plen length of response
// response data at (unsigned char*)packet_buf + TCP_DATA_OFFSET
// return value is the number of bytes parsed
{
unsigned char *data = *rxdata;
unsigned char *pTelnet = *rxdata;
unsigned int len = *plen;
unsigned char* response = (unsigned char*)packet_buf + TCP_DATA_OFFSET;	// Response buffer
int16 parsed,OptionsLen;
int16 temp;

// No data to send back
*plen = 0;

	while(( *pTelnet == IAC) && (len - (pTelnet - data) >= 3))	
		{
		// We are pointing at an IAC character, and...
		// We have >= 3 bytes to parse. ( A telnet negotiation option is 3 bytes long )
		pTelnet++;
		// The next octet is a "telnet option negotiation verb"
		switch(*(pTelnet++))
			{
			case WILL:
			if(*pTelnet != COM_PORT_OPTION)
				{
//				sprintf(msg,"WILL Unknown opt 0x%02X",*pTelnet);
//				OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(char*)msg);
				// Respond with DONT (accept rfc2217 commands) to this option
				PUTCHAR(IAC,response);
				PUTCHAR(DONT,response);
				PUTCHAR(*pTelnet,response);
				pTelnet++;
				break;
				}
			if((*pTelnet == COM_PORT_OPTION) && modem_config.com_enable_rfc2217 )
				{
				// The sender of this command is willing to send com port control option commands.
				OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,szRemoteSends2217);
				// Respond with DO (accept rfc2217 commands) to this option
				PUTCHAR(IAC,response);
				PUTCHAR(DO,response);
				PUTCHAR(*pTelnet,response);
				rfc2217_state |= NEGOTIATED_RECEIVE_2217;
				}
			else
				{
				// Respond with DONT (accept rfc2217 commands) to this option
				PUTCHAR(IAC,response);
				PUTCHAR(DONT,response);
				PUTCHAR(*pTelnet,response);
				}
			pTelnet++;
			break;

			case DO:
			if(*pTelnet != COM_PORT_OPTION)
				{
//				sprintf(msg,"DO Unknown opt %02X",*pTelnet);
//				OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(char*)msg);
				// Respond with DONT (accept rfc2217 commands) to this option
				PUTCHAR(IAC,response);
				PUTCHAR(WONT,response);
				PUTCHAR(*pTelnet,response);
				pTelnet++;
				break;
				}
			if((*pTelnet == COM_PORT_OPTION) && modem_config.com_enable_rfc2217 )
				{
				// The sender of this command is willing to accept com port control option commands
				// Respond with WILL (send rfc2217 commands) as option is enabled
				OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,szRemoteAccpts2217);
				PUTCHAR(IAC,response);
				PUTCHAR(WILL,response);
				PUTCHAR(*pTelnet,response);
				rfc2217_state |= NEGOTIATED_SEND_2217;
				}
			else
				{
				// Respond with WONT (send rfc2217 commands) as option is disabled
				PUTCHAR(IAC,response);
				PUTCHAR(WONT,response);
				PUTCHAR(*pTelnet,response);
				}
			pTelnet++;
			break;
		
			case DONT:
			// The sender of this command refuses to accept com port control options commands.
			// That is normal as we are the server.
			// Shouldn't go here as we never send DO
			case WONT:// The sender of this command refuses to send com port control option commands.
			pTelnet++;
			break;

			case SB:
			// Start of some configuration data..
			if(*(pTelnet++) != COM_PORT_OPTION)
				{
				// Not rfc2217 COM PORT config data
				sprintf(msg,"<SB> followed by 0x%02X",*(pTelnet-1));
				OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(char*)msg);
				return 0;
				}
			if ((rfc2217_state & NEGOTIATED_2217) == 0)
				OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(char*)"2217 Options are not negotiated !");
			else
				{
//				sprintf(msg,"ParseCOM_PORTOptions 0x%02X len %u",*(pTelnet-3),len-((pTelnet-3) - data));
//				OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(char*)msg);

				pTelnet -= 3;			// Rewind buffer to point at <IAC> <SB> <COM_PORT_OPTION>
				OptionsLen = len-(pTelnet - data);
				// Returns the number of chars parsed
				// Corrupts packet_buf[TCP_DATA_OFFSET + 200] if it responds
				temp =  ParseCOM_PORTOptions(pTelnet,&OptionsLen,NULL);

//				sprintf(msg,"ParseCOM_PORTOptions returned %u of %u",temp,len-(pTelnet - data));
//				OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(char*)msg);

				pTelnet += temp;
				}
			break;
			
			default:
			// Unrecognised telnet option negotiation verb
//			OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,szModem2217unknown);
			sprintf(msg,"Unknown option 0x%02X",*(pTelnet-1));
			OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(char*)msg);
			// Ignore.
			return 0;
			}
		}

// Send our serial port status if they negotiated 2217 with us
parsed = pTelnet - data;

// Length of response
*plen = (int) (response - (packet_buf + TCP_DATA_OFFSET));

//sprintf(msg,"TelnetOptions=%u",parsed);
//OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(const  char*)msg);
return parsed;	// Chars parsed
}

int CTCPModem::ParseCOM_PORTOptions(unsigned char *data,unsigned int *plen,unsigned char **unused)
{
const char* pMsg;
unsigned int len = *plen;
unsigned char *pTelnet = data;
int32 lParam;
char cParam;
// Response buffer
unsigned char* p = (unsigned char*)packet_buf + TCP_DATA_OFFSET+ 200;

	// COM_PORT_OPTION has been negotiated with the remote
	// Here, we accept option settings eg baudrate, parity..
	// Each option takes the following format:
	// | IAC | SB | OPTION | DATA(0-4 bytes) | IAC | SE |
parse_next:
	while(( *pTelnet == IAC) &&
	(len - (pTelnet - data) >= 5))
		{
		// The next octet is a SE (end) or SB (begin)command.
		pTelnet++;
		switch(*pTelnet++)
			{
			case SB:
			// Start of a new option
			break;
						
			case SE:
			// End of current option
			goto parse_next;

			default:
			// Error. Expected end or start of option
			goto done;
			}

		if(*(pTelnet++) != COM_PORT_OPTION)
			{
			// Not COM_PORT_OPTION command
			goto done;
			}

		msg[0] = NULL;				

		switch(*(pTelnet++))
			{
			case SIGNATURE:
			do
				{
				GETCHAR(cParam,pTelnet);
				// Copy to destination string, if wanted.
				// Note IAC IAC == IAC (escaped)
				}while(cParam != IAC);
			pTelnet++;
			strcpy((char*)msg,szSIGNATURE);
			break;

			case SET_BAUDRATE:
			GETLONG(lParam,pTelnet);

			// Convert baudrate to a BAUDOVERSAMPLERATE_ constant..
			// Get our current baudrate as a string in baudstr
			baudrate_2ascii(HW_IF);

			switch(lParam)
				{
				case 0:
				// They are requesting our current setting
				break;
#ifdef BAUD_1200
				case 38400:
				lParam=BAUDOVERSAMPLERATE_SLOW38400;
				break;						
				case 19200:
				lParam=BAUDOVERSAMPLERATE_SLOW19200;
				break;
				case 9600:
				lParam=BAUDOVERSAMPLERATE_SLOW9600;
				break;
				case 4800:
				lParam=BAUDOVERSAMPLERATE_SLOW4800;
				break;
				case 2400:
				lParam=BAUDOVERSAMPLERATE_SLOW2400;
				break;
				case 1200:
				lParam=BAUDOVERSAMPLERATE_SLOW1200;
				break;
				case 460800:
				// Selects autobaud
				break;
				default:
				// Unsupported speed. Select max.
				lParam=BAUDOVERSAMPLERATE_SLOW38400;
				break;						
#else
				case 115200:
				lParam=BAUDOVERSAMPLERATE_115200;
				break;
				case 57600:
				lParam=BAUDOVERSAMPLERATE_57600;
				break;
				case 38400:
				lParam=BAUDOVERSAMPLERATE_38400;
				break;
				case 19200:
				lParam=BAUDOVERSAMPLERATE_19200;
				break;
				case 9600:
				lParam=BAUDOVERSAMPLERATE_9600;
				break;
				case 4800:
				lParam=BAUDOVERSAMPLERATE_4800;
				break;
				case 460800:
				// Selects autobaud
				break;
				default:
				// Unsupported speed. Select max.
				lParam=BAUDOVERSAMPLERATE_115200;
				break;						
#endif
				}

			if(lParam == 460800)
				{
				// There is no setting for autobaud, so I use this UART speed instead 
				UartAutobaudEnable(HW_IF,true);
				strcpy((char*)msg,szModemAutobaud);
				break;
				}

			if(lParam == 0 )
				// Baudrate request
				sprintf(msg,szModemBaudRequest,baudstr);			
			else
				{
				// Set baudrate
				modem_config.baudrate_flag = lParam;
				UartAutobaudEnable(HW_IF,false);
				UpdateUart(HW_IF);
				sprintf(msg,szSET_BAUDRATE,baudstr);
				}

			// Send them our current baudrate setting
			lParam = atol((const char*)baudstr);
			OPTION_LONG(SET_BAUDRATE,lParam,p);
			break;

			case SET_DATASIZE:
			// Ignored. Datasize=8 if no parity, 7 if parity is set.
			GETCHAR(cParam,pTelnet);
			sprintf(msg,szSET_DATASIZE,cParam);
			OPTION_CHAR(SET_DATASIZE,cParam,p);
			break;

			case SET_PARITY:
			// 0=REQUEST 1=NONE , ODD, EVEN, MARK
			// If parity is selected, I set 7 data bits
			GETCHAR(cParam,pTelnet);
			sprintf(msg,szSET_PARITY,cParam);
			OPTION_CHAR(SET_PARITY,cParam,p);

			switch(cParam)
				{
				case 0:							// Request. Don't change current configuration
				cParam = modem_config.parity_flag;
				break;
				case 1:							// No parity
				cParam = FORMAT_8N;				// 8N
				break;
				case 2:							// Odd parity
				cParam = FORMAT_7O;				// 7O
				break;
				case 3:							// Even parity
				cParam = FORMAT_7E;				// 7E
				break;
				case 4:							// Mark parity
				cParam = FORMAT_8N;				// 8N. Our UART doesn't support mark parity
				break;
				}
		
			modem_config.parity_flag = (int16)cParam;
			UpdateUart(HW_IF);
			break;
	
			case SET_STOPSIZE:
			// 0=REQUEST 1=1 STOP, 2=2, 3=1.5
			// Our UART only supports 1 stop bit. Always respond '1'
			GETCHAR(cParam,pTelnet);
			sprintf(msg,szSET_STOPSIZE,cParam);
			cParam=1;
			OPTION_CHAR(SET_STOPSIZE,cParam,p);
			break;
	
			case SET_CONTROL:
			GETCHAR(cParam,pTelnet);
			OPTION_CHAR(SET_CONTROL,cParam,p);
			pMsg = NULL;
	
			switch(cParam)
				{
				case FLOW_REQUEST_IN:
				case FLOW_REQUEST:
				pMsg = szFLOW_REQUEST;
				break;
				case FLOW_NONE_IN:
				case FLOW_NONE:
				pMsg = szFLOW_NONE;
				break;
				case FLOW_XON_IN:
				case FLOW_XON:
				pMsg = szFLOW_XON;
				break;
				case FLOW_RTS_IN:
				case FLOW_RTS:
				pMsg = szFLOW_RTS;
				break;
				case DTR_REQUEST:
				pMsg = "DTR_REQUEST";
				OPTION_CHAR(DTR_REQUEST,DTR_ON,p);
				break;
				case DTR_ON:
				pMsg = szDTR_ON;
				// We are DCE. Set DCD high
				hw->uart[HW_IF].flag_dcd = HI;
				break;
				case DTR_OFF:
				pMsg = szDTR_OFF;
				// We are DCE. Set DCD low
				hw->uart[HW_IF].flag_dcd = LO;
				break;
				case RTS_REQUEST:
				pMsg = "RTS_REQUEST";
				OPTION_CHAR(RTS_REQUEST,RTS_ON,p);
				break;
				case RTS_ON:
				pMsg = szRTS_ON;
				// We are DCE. Set CTS high
//				hw->uart[HW_IF].flag_cts = HI;
				break;
				case RTS_OFF:
				pMsg = szRTS_OFF;
				// We are DCE. Set CTS low
//				hw->uart[HW_IF].flag_cts = LO;
				break;
				}
			OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(const char*)pMsg);
			break;

			case NOTIFY_LINESTATE:
			// They want notification of framing/parity errs etc, and txe,timeout etc. Bitmapped option
			GETCHAR(cParam,pTelnet);
			OPTION_CHAR(NOTIFY_LINESTATE,cParam,p);
			sprintf(msg,szNOTIFY_LINESTATE,cParam);
			break;

			case NOTIFY_MODEMSTATE:
			// They want notification of DCD, CTS etc. Bitmapped option					
			GETCHAR(modemstate_flags,pTelnet);
			OPTION_CHAR(NOTIFY_MODEMSTATE,modemstate_flags,p);
			sprintf(msg,szNOTIFY_MODEMSTATE,modemstate_flags);
			break;

			case SET_LINESTATE_MASK:
			// Set the mask for NOTIFY_LINESTATE option
			GETCHAR(linestate_flags,pTelnet);
			OPTION_CHAR(SET_LINESTATE_MASK,linestate_flags,p);
			sprintf(msg,szSET_LINESTATE_MASK,linestate_flags);
			break;

			case SET_MODEMSTATE_MASK:
			// Set the mask for NOTIFY_MODEMSTATE option
			GETCHAR(cParam,pTelnet);
			sprintf(msg,szSET_MODEMSTATE_MASK,cParam);

cParam = 0x01 | 0x02 | 0x08 | 0x10 | 0x20;
if(hw->uart[HW_IF].flag_dtr == 1)
	cParam = cParam | 0x80;
OPTION_CHAR(SET_MODEMSTATE_MASK,cParam,p);

			break;

			case PURGE_DATA:
			GETCHAR(cParam,pTelnet);
			OPTION_CHAR(PURGE_DATA,cParam,p);
			sprintf(msg,szPURGE_DATA,cParam);

//			switch(cParam)
//				{
//				case 1:
//				hw->uartInputFlush(HW_IF);
//				break;
//				case 2:
//				hw->uartOutputFlush(HW_IF);
//				break;
//				case 3:
//				hw->uartOutputFlush(HW_IF);
//				hw->uartInputFlush(HW_IF);
//				break;
//				}
			break;
					
			case FLOWCONTROL_SUSPEND:
			OPTION_CHAR(FLOWCONTROL_SUSPEND,cParam,p);
			strcpy((char*)msg,szFLOWCONTROL_SUSPEND);
			break;					

			case FLOWCONTROL_RESUME:
			OPTION_CHAR(FLOWCONTROL_RESUME,cParam,p);
			strcpy((char*)msg,szFLOWCONTROL_RESUME);
			break;

			default:
			sprintf(msg,"Skipping unknown option 0x%02X",*(pTelnet-1));
			// Skip past this option..
			while(!( ( *pTelnet == IAC) && (*(pTelnet+1) == SE)) )
				pTelnet++;
			break;			
			}// End switch
		// Trace out COM_PORT_OPTION parameter info
		if(msg[0] != NULL)
			OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(const char*)msg);
		} // end while

done:
// Skip past the tail of the last option..
while(( *pTelnet == IAC) ||
	( *pTelnet == SE))
	pTelnet++;	
		
// Response length
//unsigned char* p = (unsigned char*)packet_buf + TCP_DATA_OFFSET+ 200;
len = p - (packet_buf + TCP_DATA_OFFSET+ 200);
//*response = packet_buf + TCP_DATA_OFFSET+ 200;
transport->output(packet_buf + TCP_DATA_OFFSET+ 200,len, ACK,0,HW_LAN);

// Return chars parsed
return pTelnet - data;
}

