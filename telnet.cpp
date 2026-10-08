// telnet.cpp 
// base class CTcpClient

// Implements a terminal server so LAN connected user
// can access the routers' onboard ISDN adapter, typically
// (although not necessarily) for an X.25 over Telnet/TCP session

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "telnet.h"
#include "utils.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

//--------------------------------------------------------------------------
int CTelnet::OnTransport(int message,int unused)
// Message from transport layer (TCP or UDP)
{
switch(message)
	{
	case TRANSPORT_CLOSE:
	// TCP connection closed.
	// Called by CProtocol_L3::~CProtocol_L3, base class of transport protocol 
	// Release ownership of any uart resources (DTE,TA or the trace port)
	if(hw->uart[IF_NUM].pClient == (taguart::CClient*)this)
		{
		hw->uart[IF_NUM].owner = OWNER_NONE;				
		hw->uart[IF_NUM].pClient = NULL;
		}
	}
return 1;
}

//--------------------------------------------------------------------------
int16 CTelnet::GetTransmitBuffer()
// Returns how many bytes we can accept from serial interface
// This depends on how much TCP can accept
{
if(transport)
	return transport->GetTransmitBuffer();

else
	// Command mode.
	return UART_BUFLEN;
}

//--------------------------------------------------------------------------
void CTelnet::init(int HW_CLASS,CProtocol_L3* pTCP,bool bAutoDelete)
// Open a serial port. If a valid number is supplied, use it otherwise 
// use first available port.
{
name = szTrace;//szTelnet;
transport = pTCP;
traceout.module = LOG_ALL - LOG_BUG - LOG_IP;		

szBanner = NULL;
quit = false;
autodelete = bAutoDelete;			// Delete instance when quit flag is set
echo = false;
trace = false;
MRU = DEFAULT_TCP_SEGMENT_SIZE;
outbuf = &packet_buf[TCP_DATA_OFFSET];
	
if(HW_CLASS == HW_TRACE)
	{
	// Initiated as a trace port
	trace = true;
	IF_NUM = HW_TRACE;
	
	// Don't trace our own messages !
	traceout.suppress = false;
	traceout.pause = false;
	traceout.hexmode = false;

	hw->uart[HW_TRACE].owner = OWNER_TCPPORT;
	hw->uart[HW_TRACE].pClient = (taguart::CClient*)this;

	hw->uartOutputFlush(HW_TRACE);

	szBanner = szTraceBanner;
	return;
	}

#ifndef SEB
// Get a DTE or TA port, dictated by HW_CLASS parameter
// Could be a specified port, or a port class ie. TA or DTE port.
IF_NUM = GetHardwareInterface(OWNER_TCPPORT,(CClient*)this,HW_CLASS);
if(IF_NUM == HW_NONE)
	{
	quit = true;
	return;
	}
#endif

return;
}

//--------------------------------------------------------------------------
int CTelnet::serial_receive(unsigned char **ppdata,unsigned int *len,unsigned int window)
// Received data from serial device/stdout, send it out on the LAN
{
unsigned char *data = *ppdata;
data[*len] = NULL;				// Null terminate command string
traceout.suppress = true;
transport->output(data,min(*len,1500), ACK,0,HW_LAN);
traceout.suppress = false;

// There is no response
*len = 0;
return 0;
}

//--------------------------------------------------------------------------
int CTelnet::receive(unsigned char **rxdata,unsigned int *plen,unsigned int window)
// Received data from LAN
// Send it out on the serial interface (DTE/TA)
// Returns to TCP : ACKNOWLEDGE, TERMINATE, NORESPONSE (generally means response already sent) 
{
unsigned char *data = *rxdata;		
data[1]=0;				// All commands are single character

*rxdata = msg;
msg[0]=NULL;
*plen=0;

if(IF_NUM == HW_TRACE)
	{
	traceout.suppress=false;

	// This is the trace port
	switch((char)(data[0]& 0xff))
		{
		case '?':
		case 'H':
		case 'h':
		// Show help
		*rxdata = (unsigned char*)szTraceHelp;
		*plen = strlen(szTraceHelp);
		traceout.pause = false;
		break;
		case 'p':
		case 'P':
		// Pause / resume trace
		traceout.pause = ! traceout.pause;
		if(traceout.pause)
			{
			*rxdata = (unsigned char*)szPaused;
			*plen = strlen(szPaused);
			}
		break;
/*
		case 'a':
		case 'A':
		// Trace all - except IP. It clutters the trace
		traceout.module = LOG_ALL - LOG_BUG - LOG_IP;		
		sprintf(msg,szTraceAll,data);
		*plen = strlen((const char*)msg);
		break;
		case 'c':
*/
		case 'c':
		case 'C':
		// Clear trace
		*rxdata = (unsigned char*)szClrScr;
		*plen = strlen(szClrScr);
		traceout.pause = false;
		break;
		case 'i':
		case 'I':
		// Toggle IP trace on/off
		traceout.module ^= LOG_IP;
		sprintf(msg,szTraceIP,((traceout.module & LOG_IP) == LOG_IP)? szON:szOFF);
		*plen = strlen((const char*)msg);
		break;
		case 't':
		case 'T':
		// Toggle TCP trace on/off
		traceout.module ^= LOG_TCP;
		sprintf(msg,szTraceTCP,((traceout.module & LOG_TCP) == LOG_TCP)? szON:szOFF);
		*plen = strlen((const char*)msg);
		break;
		case 's':
		case 'S':
		// Toggle Serial trace off->on/ascii->on/hex->off
		strcpy((char*)msg,szTraceSerial);
		if((traceout.module & LOG_SERIAL)==LOG_SERIAL)
			{
			if(traceout.hexmode)
				{
				// Toggle serial trace off
				traceout.module ^= LOG_SERIAL;
				traceout.hexmode = false;
				strcat((char*)msg,szOFF);
				}
			else
				{
				// Serial trace on with hex display
				traceout.hexmode = true;
				strcat((char*)msg,szONhex);
				}
			}
		else
			{
			// Toggle serial trace on, Ascii display
			traceout.module |= LOG_SERIAL;
			traceout.hexmode = false;
			strcat((char*)msg,szON);
			}

		strcat((char*)msg,szEndline);
		*plen = strlen((const char*)msg);
		break;
		case 'a':
		case 'A':
		// Toggle Arp trace on/off
		traceout.module ^= LOG_ARP;
		sprintf(msg,szTraceArp,((traceout.module & LOG_ARP) == LOG_ARP)? szON:szOFF);
		*plen = strlen((const char*)msg);
		break;

		case 'u':
		case 'U':
		// Toggle UDP trace on/off
		traceout.module ^= LOG_UDP;
		sprintf(msg,szTraceUDP,((traceout.module & LOG_UDP) == LOG_UDP)? szON:szOFF);
		*plen = strlen((const char*)msg);
		break;

		default:
		if(((data[0]& 0xff) >= '0') && ((data[0]& 0xff) <= '5'))
			{
			traceout.pause = false;
			traceout.level=(data[0]& 0xff)-0x30;
			sprintf(msg,szTraceShowLevel,data);
			*plen = strlen((const char*)msg);
			}
		break;

		}

	return ACKNOWLEDGE;
	}

#ifndef SEB	

// Send the chars to the ISDN TA / DTE port
ppp->serialout(IF_NUM,(unsigned char*)data,&bytesWritten);

if(echo)
	// Echo TTY commmands back to user
	strcpy(outbuf,data);

*plen = strlen(outbuf);
*rxdata = outbuf;
#endif

return ACKNOWLEDGE;
}
