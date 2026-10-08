// smtp.cpp 
// base class CTcpClient
// Implements SMTP email client and server rfc821

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "smtp.h"
#include "utils.h"
#include "arp.h"
#include "ip.h"
#include "ffs.h"

extern int16 tcp_transient_port_number;			// Our Port number when making an outgoing TCP call

// SMTP 1.0 methods (commands to/from a SMTP server/client)
static const struct Csmtp::_tagMethodTable MethodTable[] = {
	{	Csmtp::METHOD_HELO,			szSMTPhelo			},
	{	Csmtp::METHOD_MAILFROM,		szSMTPMailFrom		},
	{	Csmtp::METHOD_RCPTTO,		szSMTPrcptto		},
	{	Csmtp::METHOD_MSGID,		szSMTPmsgid			},
	{	Csmtp::METHOD_DATA,			szSMTPdata			},
	{	Csmtp::METHOD_QUIT,			szSMTPquit			},
	{	Csmtp::METHOD_RESET,		szSMTPreset			},
	{	Csmtp::METHOD_NOOP,			szSMTPnoop			},
	{	Csmtp::METHOD_MSGSTARTEND,	szSMTPmsgstartend	},
	{	Csmtp::METHOD_220,			szSMTP220			},
	{	Csmtp::METHOD_250,			szSMTP250			},
	{	Csmtp::METHOD_550,			szSMTP550			},
	{	Csmtp::METHOD_251,			szSMTP251			},
	{	Csmtp::METHOD_450,			szSMTP450			},
	{	Csmtp::METHOD_451,			szSMTP451			},
	{	Csmtp::METHOD_551,			szSMTP551			},
	{	Csmtp::METHOD_552,			szSMTP552			},
	{	Csmtp::METHOD_553,			szSMTP553			},
	{	Csmtp::METHOD_354,			szSMTP354			},
	{	Csmtp::METHOD_554,			szSMTP554			},
};

//--------------------------------------------------------------------------
int Csmtp::OnTransport(int message,int unused)
// Message from transport layer (TCP or UDP)
{
switch(message)
	{
	case TRANSPORT_CLOSE:
	// TCP connection closed.
	// Called by CProtocol_L3::~CProtocol_L3, base class of transport protocol 
	// Release ownership of any uart resources (DTE,TA or the trace port)
	break;
	
	case TRANSPORT_OPEN:
	// Transport layer is available to pass data (send mail)
	smtp_state = SMTP_SEND_HELO;
	break;
	}

return 0;
}

//--------------------------------------------------------------------------
int16 Csmtp::GetTransmitBuffer()
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
void Csmtp::init(int HW_CLASS,CProtocol_L3* pTCP,bool bAutoDelete)
{
name = szSMTP;
transport = pTCP;

szBanner = NULL;
quit = false;
autodelete = bAutoDelete;			// Delete instance when quit flag is set
MRU = DEFAULT_TCP_SEGMENT_SIZE;
outbuf = &packet_buf[TCP_DATA_OFFSET];
szBanner = szSMTPBanner;	
smtp_state = SMTP_LISTEN;
MethodTableLen = sizeof(MethodTable)/ sizeof(struct _tagMethodTable);

return;
}

//--------------------------------------------------------------------------
int Csmtp::serial_receive(unsigned char **ppdata,unsigned int *len,unsigned int window)
{
// Don't expect any serial data. There is no response
*len = 0;
return 0;
}

//--------------------------------------------------------------------------
Csmtp::METHOD_TYPE Csmtp::ParseInput(unsigned char* CurrentLine,unsigned int len)
{
int keylen;
METHOD_TYPE Method = METHOD_UNSUPPORTED;

// Parse current line for a method (command)from the method table
for(int i =0; i < MethodTableLen; i++)
	{
	keylen = (unsigned int)strlen((const char*)MethodTable[i].key);
	if(len >= keylen)
		if(0== strncmp((char*)CurrentLine, MethodTable[i].key,keylen ))
			{
			Method = MethodTable[i].id;
			break;
			}
	}

return Method;
}

//---------------------------------------------------------------
int Csmtp::Init_SendMail(char* dest_email_addr,char* mail_subject,char *mail_body)
{
// TODO add a config for this
SMTP_ip_addr=0xC0A8005AL;	// 192.168.0.90

// Is the SMTP servers IP address in our arp cache ?
if(!arp->isincache(SMTP_ip_addr,NULL))
	// Refresh remote ip address in ARP cache by sending an ARP request
	arp->output_arp(SMTP_ip_addr,NULL);

// TODO check the email addr is valid.

// Set the timer to initiate sendmail. 
// By the time it expires we should have the remotes IP in the ARP cache if it is present on the LAN
timeout(0,500);
	
smtp_state = SMTP_INIT_SENDMAIL;
return 1;
}

//---------------------------------------------------------------
int Csmtp::SendMail(char* dest_email_addr,char* mail_subject,char *mail_body)
{
// Is the SMTP servers IP address in our arp cache ?
if(!arp->isincache(SMTP_ip_addr,NULL))
	{
	// Can't find SMTP server
//	sprintf(msg,"Cannot locate SMTP server %a",SMTP_ip_addr);
//	OutputDebugString(LOG_DEBUG,LOG_UDP,(const char*)msg);
	return 0;
	}

// Initiate a tcp connection to outgoing mail server.
// Get IP to create a TCP transport instance with "this" as its client
int n = ipV4->CreateTransport(IP_TCP,HW_LAN,tcp_transient_port_number,this,SMTP_ip_addr);
transport = ip_protocols[n];
if((!transport) || (n == 0))
	return 0;

mail_config.smtp_local_port = tcp_transient_port_number++;
//mail_config.pSMTP = this;

transport = ip_protocols[n];
transport->CreateClient(mail_config.smtp_local_port);

//sprintf(msg,"Connecting to SMTP server %a",SMTP_ip_addr);
//OutputDebugString(LOG_DEBUG,LOG_UDP,(const char*)msg);

// Send a TCP active open frame.....
transport->state = SYN_SENT;
transport->port_remote = SMTP_PORT;
((CTcp*)transport)->tx_seq_num = 100;

transport->output(NULL,0,SYN,0,HW_LAN);
((CTcp*)transport)->tx_seq_num++;
// Transport will send a TRANSPORT_OPEN message back to this instance when ready

smtp_state = SMTP_SEND_HELO;
return 0;
}

//--------------------------------------------------------------------------
void Csmtp::OnMessage(int16 messg,int32 lParam,int32 wParam,int16 zParam)
// A timer has expired.
// Action is based on parameter "arg"
{
if(smtp_state == SMTP_INIT_SENDMAIL)
	SendMail("user@dest.com","SSubject","BBody");
}

//--------------------------------------------------------------------------
int Csmtp::receive(unsigned char **rxdata,unsigned int *plen,unsigned int window)
// Received data from LAN
// Returns to TCP : ACKNOWLEDGE, TERMINATE, NORESPONSE (generally means response already sent) 
{
unsigned int len = *plen;
unsigned char *data = *rxdata;
char *pMsgEnd;
int iRet = ACKNOWLEDGE;

*plen = 0;

if(smtp_state == SMTP_RX_DATA)
	{
	// Receiving data
	data[len] = NULL;

	// Check for message end
	pMsgEnd = strstr((char*)data,szSMTPmsgstartend);
	if(pMsgEnd != NULL)
		{
		// Strip message end text and recalculate message length
		*pMsgEnd = NULL;
		len = strlen((char*)data);
		m_nMethod = METHOD_MSGSTARTEND;
		}
//	sprintf(msg,"\r\nReceiving mail m_nMethod %u smtp_state %u len %u psh %u\r\n",m_nMethod,smtp_state,len,psh);
//	serialout((char*)msg,HW_DTE0);
	} 
else
	{
	// Receiving / sending commands
	m_nMethod = ParseInput(data,min(len,20));
	sprintf(msg,"\r\nParsing result m_nMethod %u smtp_state %u len %u\r\n",m_nMethod,smtp_state,len);
	serialout((char*)msg,HW_DTE0);
	outbuf[0]=NULL;
	}

switch(smtp_state)
	{
	case SMTP_INIT_SENDMAIL:
	// Should not receive anything in this state
	outbuf[0] = NULL;
	break;
	
	case SMTP_SEND_HELO:				// Client
	// Client send HELO to SMTP server
	strcpy((char*)outbuf,szSMTPhelo);
	strcat((char*)outbuf," seb");
	strcat((char*)outbuf,szEndline);
	// TODO set a no response timeout
	smtp_state = SMTP_SENT_HELO;
	m_nMethod = METHOD_220;			// Don't leave as METHOD_UNSUPPORTED
	break;
	
	case SMTP_LISTEN:					// Server only
	if(m_nMethod == METHOD_HELO)
		{
		// Received HELO from client
		// Send 250 response
		mailfrom[0]=NULL;
		strcpy((char*)outbuf,szSMTP250);
		strcat((char*)outbuf,szSMTPwelcome);
		smtp_state = SMTP_WAIT_USERNAME;
		}
	break;

	case SMTP_WAIT_USERNAME:			// Server only
	if(m_nMethod == METHOD_MAILFROM)
		{
		// Received MAIL FROM from client. respond "250 OK"
		// Send MAIL FROM: command
		// Save "FROM" name
		len = min(len-strlen(szSMTPMailFrom),MAXMAILLEN);
		memcpy(mailfrom,(char*)data+strlen(szSMTPMailFrom)+1,len);
		strcpy((char*)outbuf,szSMTP250);
		strcat((char*)outbuf,szSMTPok);
		smtp_state = SMTP_WAIT_RCPT;
		}
	break;

	case SMTP_WAIT_RCPT:				// Server. Waiting for DATA, or RCPT TO
	if(m_nMethod == METHOD_RCPTTO)
		{
		// Received RCPT TO from client. respond "250"
		// TODO remember recipient (s)
		strcpy((char*)outbuf,szSMTP250);
		strcat((char*)outbuf,szSMTPok);
		}
	else if (m_nMethod == METHOD_DATA)
		{
		// Received DATA from client. respond "250"
		strcpy((char*)outbuf,szSMTP250);
		strcat((char*)outbuf,szSMTPsendmail);
		smtp_state = SMTP_RX_DATA;
		}

	break;
	
	case SMTP_RX_DATA:						// Server. Expect email data
	if(m_nMethod == METHOD_MSGSTARTEND)
		{
		// Received message end from client. Respond "250"
		// Client should send QUIT and close TCP connection
		sprintf(msg,"You have mail from %s\r\n",mailfrom);
		serialout((char*)msg,HW_DTE0);
		strcpy((char*)outbuf,szSMTP250);
		strcat((char*)outbuf,szEndline);
		smtp_state = SMTP_WAIT_QUIT;
		break;
		}
	else
		outbuf[0]=NULL;

	break;
	
	case SMTP_WAIT_ESTABLISH:			// Client. Waiting for transport to open
	break;
	case SMTP_SENT_HELO:				// Client. Sent HELO, awaiting HELO
	if(m_nMethod == METHOD_250)
		{
		// Received 250 response to our HELO from server
		// Send MAILFROM command
		strcpy((char*)outbuf,szSMTPMailFrom);
		strcat((char*)outbuf," <seb@digitalsp.co.uk>");
		strcat((char*)outbuf,szEndline);
		smtp_state = SMTP_SENT_MAILFROM;
		}
	else
		// Abort
		m_nMethod = METHOD_UNSUPPORTED;
	break;

	case SMTP_SENT_MAILFROM:			// Client. Sent MAILFROM, awaiting 250 OK
	if(m_nMethod == METHOD_250)
		{
		strcpy((char*)outbuf,szSMTPRcptTo);
		strcat((char*)outbuf," <admin@nt4.com>");
		strcat((char*)outbuf,szEndline);
		smtp_state = SMTP_SENT_MAILTO;
		}
	break;

	case SMTP_SENT_MAILTO:				// Client. Sent MAILTO, awaiting 250 OK
	if(m_nMethod == METHOD_250)
		{
		strcpy((char*)outbuf,szSMTPdata);
		strcat((char*)outbuf,szEndline);
		smtp_state = SMTP_SENT_DATA;
		}
	break;

	case SMTP_SENT_RCPT_TO:				// Client. Sent RCPT To, awaiting 250 OK
	break;
	
	case SMTP_SENT_DATA:				// Client. Sent DATA, awaiting 250
	if(m_nMethod == METHOD_354)
		{
		// Send the email, end with <cr><lf>.<cr><lf>
		strcat((char*)outbuf,"This is an email\r\n");
		strcat((char*)outbuf,szSMTPmsgstartend);
		smtp_state = SMTP_SENT_MAIL;
		}
	break;
	case SMTP_SENDING_MAIL:				// Client. Sending mail
	break;
	case SMTP_SENT_MAIL:				// Client. Sent mail, awaiting 250 OK
	if(m_nMethod == METHOD_250)
		{
		strcpy((char*)outbuf,szSMTPquit);
		smtp_state = SMTP_SENT_QUIT;
		}
	else
		{
		strcpy((char*)outbuf,szSMTPreset);
		smtp_state = SMTP_RESET;
		}
	break;
	case SMTP_SENT_QUIT:
	iRet = TERMINATE;
	break;
	
	case SMTP_WAIT_QUIT:
	if(m_nMethod == METHOD_QUIT)
		{
		// Received QUIT from client. Respond 221 SMTP CLOSED. They could close TCP, or we can
		// Send MAILFROM command
		strcat((char*)outbuf,szSMTPclose);
		smtp_state = SMTP_RESET;
		}

	case SMTP_RESET:					// Mail received. Quit, or receive another
	if(m_nMethod == METHOD_RESET)
		{
		// Means reinit, they may have more messages to send (?)
		strcpy((char*)outbuf,szSMTP250);
		strcat((char*)outbuf,szSMTPok);
		smtp_state = SMTP_WAIT_USERNAME;
		}
	else
		iRet = TERMINATE;
	break;
	};
		
if(m_nMethod == METHOD_UNSUPPORTED)
	{
	// Received unknown response from client/server - abort
	// Send MAILFROM command
	strcpy((char*)outbuf,szSMTPreset);
	smtp_state = SMTP_RESET;
	}

*plen = strlen((char*)outbuf);
*rxdata = outbuf;
return iRet;
}
