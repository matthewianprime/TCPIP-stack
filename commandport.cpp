// commandport.cpp
// A command line interface to the SEB ("telnet" option). Base class CClient

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "icmp.h"
#include "commandport.h"
#include "utils.h"
#include "tcpmodem.h"
#include "ffs.h"
#include "arp.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"
#ifdef SMTP
#include "smtp.h"
extern Csmtp *smtp;
#endif
extern "C" 
{
extern unsigned int FlsRead( unsigned long Faddr );
extern int FLASH_Write(unsigned long Faddr, unsigned *pData, unsigned Count);
extern bool isvalidsector(char sector);
}

int16 request_id;			// For ping
						
enum PARAM_TYPE
	{
	PARAM_UNSUPPORTED = 0,
	PARAM_HELP,
	PARAM_NETMASK,
	PARAM_ON,
	PARAM_OFF,
	PARAM_PORT,
	PARAM_MODULES,
	PARAM_MODULE,
	PARAM_CALLBAR,
	PARAM_HOSTNAME,
	PARAM_PRINT,
	PARAM_ADD,
	PARAM_DELETE,
	PARAM_LEVEL,
	PARAM_LAN,
	PARAM_PPP,
	PARAM_ARP,
	PARAM_LCP,
	PARAM_UART,
	PARAM_IP,
	PARAM_MAC,
	PARAM_GATEWAY,
	PARAM_TCPMODEM,
	PARAM_TCP,
	PARAM_AUTH,
	PARAM_HTML,
	PARAM_SYSTEM,
	PARAM_ALL,
	PARAM_OUT,
#ifndef SEB
	PARAM_DIALNUMBER,
#endif
	PARAM_USERNAME,
	PARAM_PASSWORD,
	PARAM_PROFILE,
	PARAM_REMOTE,
	PARAM_LOCAL
	}m_nParam;
	
struct _tagParamTable
	{
	enum 		PARAM_TYPE	id;
	const char	*key;
	};

enum METHOD_TYPE
	{
	METHOD_UNSUPPORTED = 0,
	METHOD_HELP,
	METHOD_SHOW,
	METHOD_ARP,
	METHOD_SET,
	METHOD_BYE,
	METHOD_PROMPT,
	METHOD_ECHO,
	METHOD_VERSION,
	METHOD_RESTART,
	METHOD_SAVE,
	METHOD_DISCARD,
	METHOD_TRACE,
	METHOD_DIR,
	METHOD_PRINT,
	METHOD_DEL,
	METHOD_FORMAT,
	METHOD_CLEAN,
	METHOD_COPY,
	METHOD_MEMORY,
#ifndef SEB
	METHOD_AT,
#endif
#ifdef ROUTER
	METHOD_MODEM,
	METHOD_ROUTE,
#endif
	METHOD_PING,
	METHOD_TABLE_END
	}m_nMethod;

struct _tagMethodTable
	{
	enum METHOD_TYPE	id;
	const char			*command;
	const char			*help;
	};

static const struct _tagMethodTable MethodTable[] = {
//		Value					command text	command help
	{METHOD_HELP,				szCmdhelp,		szNull},//szNull},
	{METHOD_SHOW,				szCmdshow,		szHelpShow},
	{METHOD_ARP,				szCmdarp,		szHelpArp},
	{METHOD_SET,				szCmdset,		szHelpSet},
	{METHOD_BYE,				szCmdbye,		szHelpBye},
	{METHOD_PROMPT,				szCmdprompt,	szHelpPrompt},
	{METHOD_ECHO,				szCmdecho,		szHelpEcho},
	{METHOD_VERSION,			szCmdVersion,	szHelpVersion},
	{METHOD_RESTART,			szCmdrestart,	szHelpRestart},		// Soft restart
	{METHOD_SAVE,				szCmdsave,		szHelpSave},		// Save settings to flash
	{METHOD_DISCARD,			szCmddiscard,	szHelpDiscard},		// Discard setting changes
	{METHOD_TRACE,				szCmdtrace,		szHelpTrace},
	{METHOD_DIR,				szCmdDir,		szHelpDir},
	{METHOD_PRINT,				szCmdType,		szHelpType},
	{METHOD_DEL,				szCmdDel,		szHelpDel},
	{METHOD_FORMAT,				szCmdFormat,	szHelpFormat},
	{METHOD_CLEAN,				szCmdClean,		szHelpClean},
	{METHOD_COPY,				szCmdCopy,		szHelpCopy},
	{METHOD_MEMORY,				szCmdFreeMem,	szHelpFreeMem},
#ifndef SEB
	{METHOD_AT,					szCmdat,		szNull},
#endif
#ifdef ROUTER
	{METHOD_MODEM,				szCmdmodem,		szNull},// AT command Terminal server mode
	{METHOD_ROUTE,				szCmdroute,		szHelpRoute},
#endif
	{METHOD_PING,				szCmdping,		szHelpPing},
	{METHOD_TABLE_END,			"X",			szNull}
	};

static const struct _tagParamTable ParamTable[] = {
	{PARAM_HELP,				szParamhelp		},
	{PARAM_NETMASK,				szParammask		},
	{PARAM_ON,					szParamon		},
	{PARAM_OFF,					szParamoff		},
	{PARAM_PORT,				szParamport		},
	{PARAM_MODULES,				szParammodules	},	
	{PARAM_MODULE,				szParammodule	},
	{PARAM_CALLBAR,				szParamcallbar	},
	{PARAM_HOSTNAME,			szParamname		},
	{PARAM_PRINT,				szParamprint	},
	{PARAM_ADD,					szParamadd		},
	{PARAM_DELETE,				szParamdelete	},
	{PARAM_LEVEL,				szParamlevel	},
	{PARAM_LAN,					szParamlan		},
	{PARAM_PPP,					szParamppp		},
	{PARAM_ARP,					szParamarp		},
	{PARAM_LCP,					szParamlcp		},// and ipcp
	{PARAM_UART,				szParamDte		},
	{PARAM_IP,					szParamip		},
	{PARAM_MAC,					szParammac		},
	{PARAM_GATEWAY,				szParamGateway	},
	{PARAM_TCPMODEM,			szParammodem	},
	{PARAM_TCP,					szParamtcp		},
	{PARAM_AUTH,				szParamauth		},
	{PARAM_HTML,				szParamhtml		},	
	{PARAM_SYSTEM,				szParamsystem	},	
	{PARAM_ALL,					szParamall		},// debug level
	{PARAM_OUT,					szParamout		},// debug level
#ifndef SEB
	{PARAM_DIALNUMBER,			szParamdial		},
#endif
	{PARAM_USERNAME,			szParamuser		},
	{PARAM_PASSWORD,			szParampass		},
	{PARAM_PROFILE,				szProfile		},
	{PARAM_REMOTE,				szParamRemote	},
	{PARAM_LOCAL,				szParamLocal	}
	};

//--------------------------------------------------------------------------
void CCommandPort::init(int HW_IFparam,CProtocol_L3* pTCP,bool bAutoDelete)
{
transport = pTCP;
echo = ECHO_ON;

password_entered = PASSWORD_AND_USERNAME_ACCEPTED;
if(lan[ACTIVE].username[0] != NULL)
	{
	password_entered = PASSWORD_AND_USERNAME_REQUIRED;
	strcpy((char*)sPrompt,szUsernamePrompt);
	echo = ECHO_STAR;									// Don't echo username / password !
	}
else
	strcpy((char*)sPrompt,szPromptDefault);	// Default prompt
	
name = szCommandPort;

HW_IF = HW_IFparam;
inbuf_in = 0;
request_id = 100;				// For ping

quit = false;
autodelete =  bAutoDelete;		// Delete instance when quit flag is set
szBanner = szDefaultBanner;		// Welcome message sent when a client connects

prompt = true;
pinging=false;

MethodTableLen = sizeof(MethodTable)/ sizeof(struct _tagMethodTable);
ParamTableLen = sizeof(ParamTable)/	sizeof(struct _tagParamTable);
outbuf = &packet_buf[TCP_DATA_OFFSET];
MRU = DEFAULT_TCP_SEGMENT_SIZE;

if(HW_IF != HW_NONE)
	{
	// Banner when commandport is the shell 
	modem_config.autobaud_flag = 0;
	UpdateUart(HW_IF);
	serialout(szBanner,HW_IF);
	}
}

//--------------------------------------------------------------------------
int CCommandPort::serial_receive(unsigned char **data,unsigned int *plen,unsigned int window)
{
quit=false;

receive(data,plen,PPP_MRU);

return 1;
}

//--------------------------------------------------------------------------
int CCommandPort::receive(unsigned char **rxdata,unsigned int *plen,unsigned int window)
// Returns TERMINATE - terminate TCP session.
// Returns ACKNOWLEDGE - normal operation
{
bool eol = false;			// End of Line

unsigned char *data = *rxdata;
unsigned int len = *plen;	
data[len] = 0;				// Null terminate command string
outbuf[0] = 0;

pinging=false;				// Cancel pinging on receipt of a character

// Append the new data to the command line buffer inbuf
memcpy(&inbuf[inbuf_in],data,min(len+1,COMMANDLINE_BUFLEN-inbuf_in));// Include null char
inbuf_in += len;

eol = DoCommandLine(inbuf,min(inbuf_in,COMMANDLINE_BUFLEN));

if(quit)
	// End telnet session. User may have typed "bye"
	return TERMINATE;

if(!eol & ((inbuf_in+len) >= COMMANDLINE_BUFLEN))
	{
	// Output "command too long" message
	sprintf((unsigned char*)outbuf,szCmdLen,(inbuf_in+len));
	inbuf_in = 0;
	}

if(eol)
	{
	// Command line was entered and processed. 
	// Issue new command prompt and reset command line buffer
	if(prompt)
		{
		if(!dirty)
			strcat((char*)outbuf,(const char*)sPrompt);
		else
			strcat((char*)outbuf,szPromptUnsaved);
		};

	prompt=true;		// false is set to suppress new prompt, eg ping command.
	inbuf_in = 0;		// Command line makeup buffer pointer
	}
else 
	{
	// Command line not yet complete
	// Echo TTY commmands to user
	if(echo == ECHO_ON)								// Echo commands on
		strcpy((char*)outbuf,(const char*)data);
	else if(echo == ECHO_STAR)						// Echo a "*" ( for password entry )
		strcpy((char*)outbuf,(const char*)"*");		// Expect 1 char at a time only.
	else *outbuf = NULL;							// Echo commands off
	}

*plen = strlen((const char*)outbuf);
*rxdata =(unsigned char*)outbuf;
return ACKNOWLEDGE;
}

//--------------------------------------------------------------------------
void CCommandPort::OnMessage(int16 messg,int32 lParam,int32 wParam,int16 zParam)
{
int32 response_time;

switch(messg)
	{
	case MSG_ECHO_REPLY:
	request_id++;	// ID for the next request

	// Got a response to a PING
	response_time = getTickCount() - wParam;
	sprintf(msg,szIcmp_reply,lParam,response_time);
	transport->output(msg,(int)strlen((const char*)msg),ACK,0,HW_LAN);

	if(pinging)
		{
		// Send another request. Pinging stops if a char is typed
		PostMessage(icmp,MSG_ECHO_REQUEST,lParam,(int32)request_id,(int16)this,1000L);
		PostMessage((CObj*)this,MSG_ECHO_REQUEST,lParam,getTickCount(),request_id,2000L);
		}
	else
		// Redisplay command prompt
		transport->output((unsigned char*)sPrompt,(int)strlen((const char*)sPrompt),ACK,0,HW_LAN);	
	break;
	
	case MSG_ECHO_REQUEST:
	request_id++;	// ID for the next request

	// No response timeout
	transport->output((unsigned char*)szIcmp_timeout,(int)strlen(szIcmp_timeout),ACK,0,HW_LAN);

	if(pinging)
		{
		// Send another request. Pinging stops if a char is typed
		PostMessage(icmp,MSG_ECHO_REQUEST,lParam,(int32)request_id,(int16)this,1000L);
		PostMessage((CObj*)this,MSG_ECHO_REQUEST,lParam,getTickCount(),request_id,2000L);
		}
	else
		// Redisplay command prompt
		transport->output((unsigned char*)sPrompt,(int)strlen((const char*)sPrompt),ACK,0,HW_LAN);
	break;
	}
}

//--------------------------------------------------------------------------
int16 CCommandPort::GetTransmitBuffer()
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
int CCommandPort::OnTransport(int message,int)
// Base class override called when this instance is deleted
{
switch(message)
	{
	case TRANSPORT_CLOSE:
	// Release trace if we own it.
	if((hw->uart[HW_TRACE].owner == OWNER_TCPPORT) &&
		(hw->uart[HW_TRACE].pClient == (taguart::CClient*)this))
		{
		hw->uart[HW_TRACE].owner = OWNER_NONE;				
		hw->uart[HW_TRACE].pClient = NULL;
		}
	break;
	}
return 1;
}

//--------------------------------------------------------------------------
bool CCommandPort::DoCommandLine(unsigned char* CurrentLine,unsigned int len)
// Parse a line of input
// Command words up to 2 layers deep, one param per bottom layer word.
//	SET		CONFIG		IP		1.2.3.4
//	L0 Verb	L1 Verb		Param	Value
{
#ifdef ROUTER
int32 ip_mask, gateway;
int if_unit;
#endif

int32 ip_addr;
int32 pos;
int16 i,n,m,index=0;
char sector_save;
int16* pMac;
unsigned char* pEndLine;

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
unsigned char *del = CurrentLine;
while(del != NULL)
	{
	del = (unsigned char*)strchr((const char*)CurrentLine,CH_DEL);
	if (del == CurrentLine)
		// First char of data is a del character.
		strcpy((char*)del,(char*)del+1);
	else if(del > CurrentLine)
		// Mid string char is a del. Delete preceding character
		strcpy((char*)del-1,(char*)del+1);
	}

// Remove leading whitespace characters
CurrentLine = TrimLeft(CurrentLine,0);
len = (unsigned int)strlen((const char*)CurrentLine);
if(len == 0)
	return true;

if( password_entered < PASSWORD_AND_USERNAME_ACCEPTED)
	{
	switch(password_entered)
		{
		case PASSWORD_AND_USERNAME_REQUIRED:
		if(0 == strcmp((char*)CurrentLine,(char*)lan[ACTIVE].username))
			{
			// Username is correct
			strcpy((char*)sPrompt,szPasswordPrompt);
			password_entered = USERNAME_ACCEPTED;
			}
		break;
		
		case USERNAME_ACCEPTED:
		if(0 == strcmp((char*)CurrentLine,(char*)lan[ACTIVE].password))
			{
			// Password is correct
			strcpy((char*)sPrompt,szPromptDefault);
			password_entered = PASSWORD_AND_USERNAME_ACCEPTED;
			echo = ECHO_ON;
			}
		else
			{
			// Password incorrect
			strcpy((char*)sPrompt,szUsernamePrompt);
			password_entered = PASSWORD_AND_USERNAME_REQUIRED;
			}
		break;
		}
	return true;
	}

m_nParam = PARAM_HELP;	// Default
m_nMethod=(METHOD_TYPE) ParseMethod(CurrentLine,len);

//sprintf(msg,"Commandport parsing result=%u",m_nMethod);
//OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);

#ifndef SEB	
int OWNER_IF_NUM;
#endif
#ifdef ROUTER
CClient* cli;
#endif

switch (m_nMethod)
	{
	case METHOD_TRACE:
		// Commands affecting trace output
		m_nParam = (PARAM_TYPE)ParseParam(CurrentLine,len);
		switch (m_nParam)
			{
			case PARAM_OUT:
			// Send text to trace port eg. trace out hello
			OutputDebugString(LOG_DEBUG,LOG_ALL,szEndline);			
			OutputDebugString(LOG_DEBUG,LOG_ALL,(const char*)CurrentLine);
			break;
			case PARAM_HELP:
			strcpy((char*)outbuf,szHelpTrace);
			break;
			case PARAM_LAN:
			traceout.module |= LOG_LAN;
			break;
			case PARAM_PPP:
			traceout.module |= LOG_PPP;
			break;			
			case PARAM_ARP:
			traceout.module |= LOG_ARP;
			break;			
			case PARAM_LCP:// and ipcp and fsm
			traceout.module |= LOG_LCP;
			break;			
			case PARAM_AUTH:
			traceout.module |= LOG_AUTH;
			break;			
			case PARAM_TCP:
			traceout.module |= LOG_TCP;
			break;
			case PARAM_HTML:
			traceout.module |= LOG_HTML;
			break;
			case PARAM_IP:
			traceout.module |= LOG_IP;
			break;
			case PARAM_TCPMODEM:
			traceout.module |= LOG_TCPMODEM;
			break;			
			case PARAM_SYSTEM:
			traceout.module |= LOG_SYSTEM;
			break;			
			case PARAM_ALL:// LOG level
			traceout.module = LOG_ALL;
			break;
						
			case PARAM_OFF:
			// Trace -> DTE1
			traceout.module = LOG_NONE;
			traceout.level = LOG_DEBUG;
			hw->uart[HW_TRACE].owner = OWNER_NONE;				
			hw->uart[HW_TRACE].pClient = NULL;
   			break;
						
			case PARAM_LEVEL:
			traceout.level = atoi((const char*)CurrentLine);
			// Deliberate fall through to default case
						
			default:
			m_nParam = PARAM_OFF;
			sprintf(msg,szTraceLevel,traceout.level);
			strcpy((char*)outbuf,(const char*)msg);
			break;			
			}	
	break;

#ifndef SEB	
	case METHOD_AT:
		// This objects owner wants to talk directly to a TA port instead of the commandport
		// Only for DTE ports, not telnet user
		// 1. Checka TA port is available
		if(!IsHardwareInterfaceAvailable(HW_TA))
			{
			strcpy((char*)outbuf,szCmdPortnoTA);
			break;
			}		
		// 2. Get this objects owner  (a TCP session or a DTE port)
		OWNER_IF_NUM = GetHardwareOwner((CClient*)this);
		if((OWNER_IF_NUM == HW_NONE) || ( OWNER_IF_NUM >= NUM_DTE_INTERFACES))
			{
			// TA access only for DTE interfaces
			strcpy((char*)outbuf,szCmdPortnoTA);
			break;
			}

		// 3. Release this object from its owner
		ReleaseHardwareInterface(OWNER_IF_NUM,OWNER_SHELL);
				
		// 3. Connect the owner (a DTE port) to an ISDN adapter
		GetHardwareInterface(OWNER_IF_NUM,NULL,HW_TA);
		if(OWNER_IF_NUM == HW_NONE)
			{
			OutputDebugString(LOG_DEBUG,LOG_SYSTEM,szCmdPortnoTA);
			break;
			}		
		prompt=false;
		break;
#endif

	case METHOD_RESTART:
		// Reboot now !
		watchDogTimerDisable = 1;
		break;

	case METHOD_DISCARD:
		// Load defaults
		ffs->ffsReset();
		if(ffs->open(MODE_OPENEXISTING,(char*)profile,0))
			{
			ffs->restore_from_flash(SAVE_RESTORE_ALL);
			ffs->close();
			}
		ffs->ffsReset();	
		if(ffs->open(MODE_OPENEXISTING,szLAN,0))
			{
			ffs->restore_from_flash(SAVE_RESTORE_LAN);
			ffs->close();
			}		
		dirty=false;
		break;
		
	case METHOD_BYE:
		strcpy((char*)outbuf,szBye);
		quit = true;
		break;

	case METHOD_VERSION:
		// Display full build information
		version_2ascii(outbuf);
		break;
		
	case METHOD_PING:
		// Ping until user presses a key
		ip_addr = ascii_2ip(CurrentLine);
		sprintf(outbuf,szPinging,ip_addr);
		// Don't want the prompt redisplayed
		prompt = false;
		
		// Request ICMP to send a ping immediately
		PostMessage(icmp,MSG_ECHO_REQUEST,ip_addr,(int32)request_id,(int16)this,1L);
		// Set a no-response timer. 
		// If no reply is received this timer will trigger a "no reply" message.
		// If a reply is received, ICMP will delete this timer (matching request_id)
		// and post this object a MSG_ECHO_REPLY message.
		PostMessage((CObj*)this,MSG_ECHO_REQUEST,ip_addr,getTickCount(),request_id,2000L);
		
		*CurrentLine = NULL;
		pinging = true;
		break;

	case METHOD_HELP:
		m_nMethod=(METHOD_TYPE)	ParseMethod(CurrentLine,len);

		if(m_nMethod == METHOD_UNSUPPORTED)
			{
			strcpy((char*)outbuf,szHelp);
			i=2;
			// No parameter. Display commands
			while(MethodTable[i].id < METHOD_TABLE_END)
				{
				strcat((char*)outbuf,MethodTable[i].command);
				strcat((char*)outbuf,szEndline);
				i++;
				};
			strcat((char*)outbuf,szHelpEnd);
			}
		else
			strcpy((char*)outbuf,MethodTable[(int)m_nMethod-1].help);

		m_nMethod = METHOD_HELP;
		break;

	case METHOD_ARP:
		// ARP table set/display routines
		m_nParam = (PARAM_TYPE)ParseParam(CurrentLine,len);
		switch (m_nParam)
			{
			case PARAM_PRINT:
				if(HW_IF == HW_NONE)
					{
					// LAN connection (not shell)
					transport->output((unsigned char*)szARPCacheDisplay,(int)strlen(szARPCacheDisplay),ACK,0,HW_LAN);
					while(index < ARP_CACHELEN)
						{
						msg[0]=0;
						index = arpcache_2ascii(msg,index);
						transport->output(msg,(int)strlen((const char*)msg),ACK,0,HW_LAN);
						};
					}
				else
					{
					// shell instance
					serialout(szARPCacheDisplay,HW_IF);

					while(index < ARP_CACHELEN)
						{
						index = arpcache_2ascii(msg,index);
						serialout((char*)msg,HW_IF);
						};
					}
				outbuf[0]=NULL;
			break;

			case PARAM_DELETE:
				pos = getparam(CurrentLine,ipstr);
				
				ip_addr=(ascii_2ip(ipstr));
				if(deletearpcacheentry(ip_addr))
					// ARP table entry(s) were deleted
					sprintf(outbuf,szDeleted);
				else
					sprintf(outbuf,szNotDeleted);
			break;
							
			case PARAM_ADD:
				// Add a static ARP cache entry
				// arp add <ip> <mac>
				pos = getparam(CurrentLine,ipstr);
				ip_addr = ascii_2ip(ipstr);
				// Trim out the Ip address param
				CurrentLine = TrimLeft(CurrentLine,pos);
				pos = getparam(CurrentLine,ipstr);
				int16* mac_addr = ascii_2mac(ipstr);
				if((ip_addr !=0) && (mac_addr != NULL))
					{
					arp->addtocache(ip_addr,mac_addr,ARP_STATIC);
					sprintf(outbuf,szAdded);
					break;
					}
				sprintf(outbuf,szNotAdded);
			break;
			}
		break;			

	case METHOD_SET:
		m_nParam = (PARAM_TYPE)ParseParam(CurrentLine,len);
		switch (m_nParam)
			{
			case PARAM_HELP:
				strcpy((char*)outbuf,szHelpSet);
				break;
			case PARAM_HOSTNAME:
				strcpy((char*)lan[UNSAVED].hostname,(const char*)CurrentLine);
				dirty=true;
				strcpy((char*)outbuf,szSaveSetting);
				break;
			case PARAM_IP:
				lan[UNSAVED].ip_addr = ascii_2ip(CurrentLine);
				lan[UNSAVED].subnet = lan[UNSAVED].ip_addr & lan[UNSAVED].netmask;
				dirty=true;
				sprintf(outbuf,szSaveSetting);
				break;
			case PARAM_PORT:
				m_nParam = (PARAM_TYPE)ParseParam(CurrentLine,len);
				i = atoi((char*)CurrentLine);
				if( i==0 )
					{
					m_nParam = PARAM_UNSUPPORTED;
					break;
					}
				switch(m_nParam)
					{
					case PARAM_REMOTE:
					modem_config.remote_port = i;
					break;;
					case PARAM_LOCAL:
					modem_config.local_port = i;
					break;
					}
				break;
			case PARAM_GATEWAY:
				lan[UNSAVED].gateway = ascii_2ip(CurrentLine);
				dirty=true;
				sprintf(outbuf,szSaveSetting);
				break;			
			case PARAM_MAC:
				if(ffs->GetMac(mac_addr))
					{// Already have a mac addr.
					strcat((char*)outbuf,szMACerr);
					break;
					}
				pMac = ascii_2mac(CurrentLine);
				mac_addr[0] = pMac[0];
				mac_addr[1] = pMac[1];
				mac_addr[2] = pMac[2];
				ffs->SetMac(mac_addr);

				sprintf(outbuf,szMac,mac_addr);
				strcat((char*)outbuf,szSaveSetting);

				OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);
				break;
								
			case PARAM_NETMASK:
				lan[UNSAVED].netmask = ascii_2ip(CurrentLine);
				lan[UNSAVED].subnet = lan[UNSAVED].ip_addr & lan[UNSAVED].netmask;
				dirty=true;
				strcpy((char*)outbuf,szSaveSetting);
				break;

			case PARAM_USERNAME:
				strcpy((char*)lan[UNSAVED].username,(char*)CurrentLine);
				dirty = true;
				break;
			case PARAM_PASSWORD:
				strcpy((char*)lan[UNSAVED].password,(char*)CurrentLine);
				dirty = true;
				break;
			case PARAM_PROFILE:
				// Set a new boot profile
				ffs->ffsReset();
				if(!ffs->create_bootfile((char*)CurrentLine))
					strcpy((char*)outbuf,szFileNotFound);
				break;
#ifndef SEB
			case PARAM_DIALNUMBER:
				memcpy((char*)ppp_if[HW_IF].dialnumber,CurrentLine,MAXLEN1);
				break;
#endif
#ifdef ROUTER
	case METHOD_MODEM:
		cli = (CClient*) new CTCPModem;
		if (cli == NULL)
			break;
			
		if(!IsHardwareInterfaceAvailable(HW_TA))
			{
			strcpy((char*)outbuf,szCmdPortnoTA);
			break;
			}
					
		// 2. Get this objects owner  (a TCP session or a DTE port)
		OWNER_IF_NUM = GetHardwareOwner((CClient*)this);
		if((OWNER_IF_NUM == HW_NONE) || ( OWNER_IF_NUM >= NUM_DTE_INTERFACES))
			{
			// TA access only for DTE interfaces
			strcpy((char*)outbuf,szCmdPortnoTA);
			break;
			}

		// 3. Release this object from its owner
		ReleaseHardwareInterface(OWNER_IF_NUM,OWNER_SHELL);
				
		// 3. Connect the owner (a DTE port) to CTCPmodem instance
		GetHardwareInterface(OWNER_TCPPORT,cli,OWNER_IF_NUM);
		if(OWNER_IF_NUM == HW_NONE)
			{
			OutputDebugString(LOG_DEBUG,LOG_SYSTEM,szCmdPortnoTA);
			break;
			}		
		prompt=false;
	
	break;

	case METHOD_ROUTE:
		// route add ip mask gateway if
		m_nParam = (PARAM_TYPE)ParseParam(CurrentLine,len);
		switch (m_nParam)
			{
			case PARAM_ADD:
				m_nParam = PARAM_UNSUPPORTED;
				pos = getparam(CurrentLine,ipstr);
				ip_addr = ascii_2ip(ipstr);
				CurrentLine = TrimLeft(CurrentLine,pos);
				pos = getparam(CurrentLine,ipstr);
				ip_mask = ascii_2ip(ipstr);
				CurrentLine = TrimLeft(CurrentLine,pos);
				pos = getparam(CurrentLine,ipstr);
				gateway = ascii_2ip(ipstr);
				CurrentLine = TrimLeft(CurrentLine,pos);
				pos = getparam(CurrentLine,ipstr);
				if_unit = atoi((const char*)ipstr);
				if(addroute(ip_addr,ip_mask,gateway,if_unit))
					{
					sprintf(outbuf,szAdded);
					m_nParam = PARAM_ADD;
					break;
					}
				sprintf(outbuf,szNotAdded);
				break;

			case PARAM_DELETE:
				pos = getparam(CurrentLine,ipstr);
				ip_addr=ascii_2ip(ipstr);
					{
					if(deleteroute(ip_addr))
						// Route(s) were deleted
						sprintf(outbuf,szDeleted);
					else
						sprintf(outbuf,szNotDeleted);
					}
				sprintf(outbuf,szEndline);
				CurrentLine[0] = 0;
				break;

			case PARAM_PRINT:
				sprintf(outbuf,szRouteDisplay);
				len= strlen((const char*)outbuf);
				rtable_2ascii(&outbuf[len]);
				break;
			}
		break;
#endif
			default:
				m_nParam = PARAM_UNSUPPORTED;
			}
		break;
	
	case METHOD_MEMORY:
		// Display free memory (heap)
		sprintf(outbuf,szFreeMem,freemem());
		break;
		
	case METHOD_FORMAT:
	// Format a flash sector
	if(	isvalidsector(CurrentLine[0]) )
		ffs->format(CurrentLine[0]);
	else
		sprintf(outbuf,szFileSectorUnknown,CurrentLine[0]);
	break;
	
	case METHOD_CLEAN:
	// Clear deleted files from flash
	ffs->driveclean();
	break;
	
	case METHOD_COPY:
//		ffs->copy("MATT","XMATT",'G');
		break;

	case METHOD_DEL:
		ffs->ffsReset();
		if(ffs->delfile(CurrentLine))
			strcpy((char*)outbuf,szFileDeleted);
		else
			strcpy((char*)outbuf,szFileNotFound);
		break;
		
	case METHOD_PRINT:
		// Print file contents to screen.
		char* extn;
		ffs->ffsReset();
		if(!ffs->isvalid_file_extn(CurrentLine, &extn))
			{
			strcpy((char*)outbuf,szEndline);
			sprintf(&outbuf[2],szFileBadFormat,CurrentLine);
			break;
			}
					
		if(!ffs->open(MODE_OPENEXISTING,(char*)CurrentLine,extn))
			{
			strcpy((char*)outbuf,szEndline);
			sprintf(&outbuf[2],szFileOpenNotFound,&ffs->fs.sector,CurrentLine,extn);
			break;
			}
			
		if(0==(strcmp(ffs->current_file.ext,szFILETYPE_TEXT)))
			{
			// A text file. Print up to 100 bytes
			strcpy((char*)outbuf,szEndline);
			ffs->read((int16*)&outbuf[2],min(ffs->current_file.len,100));
			outbuf[min(2+ffs->current_file.len,100)]=NULL;
			}
		else
			{
			// A binary file. Print up to 50 int16s as hex
			ffs->read((int16*)msg,min(ffs->current_file.len,50));
			hexformatstring(msg,min(ffs->current_file.len,50),(char*)outbuf,500);
			}

		ffs->close();
		break;
		
	case METHOD_DIR:
		// As in dos
		ffs->ffsReset();
		sector_save = NULL;
		if(	isvalidsector(CurrentLine[0]) )
			{
			// Sector has been specified eg "dir F"
			sector_save = ffs->setsector(CurrentLine[0],false);
			if(	sector_save == NULL)
				{
				sprintf(outbuf,szFileSectorBad,&CurrentLine[0]);
				break;
				}
			}

		msg[0] = NULL;
		i=0;n=0;m=0;
		sprintf(msg,szDirectory,&ffs->fs.sector);
		strcpy((char*)outbuf,(char*)msg);

		while(ffs->enumfiles())
			{
			if(ffs->enumerate_file.filename[0] == NULL)
				// Deleted file. Show it in the directory listing
				ffs->enumerate_file.filename[0] = '_';
				
			// Space pad filename to 8 chars for clean column display
			len = strlen(ffs->enumerate_file.filename);
			for(;len < FILENAME_LEN;len++)
				ffs->enumerate_file.filename[len] = ' ';
			ffs->enumerate_file.filename[FILENAME_LEN] = NULL;
			
			sprintf(msg,szDirList,ffs->enumerate_file.filename,ffs->enumerate_file.ext,ffs->enumerate_file.len);
			strcat((char*)outbuf,(char*)msg);
			i++;
			n += ffs->enumerate_file.len;
			};

		m = FLASH_SECTOR_SIZE - (ffs->enumerate_file.dataptr + ffs->enumerate_file.len);	// Disk free space
		sprintf(msg,szDirListEnd,i,n,m);
		strcat((char*)outbuf,(char*)msg);
		
		if(sector_save)
			ffs->setsector(sector_save);
		break;
		
	case METHOD_SAVE:
	// Save system
	ffs->ffsReset();
	ffs->delfile(profile,true);

	if(ffs->open(MODE_CREATE,(char*)profile,extn))
		{
		ffs->save_to_flash(SAVE_RESTORE_ALL);
		ffs->close();
		}
		
	if(dirty)
	// Save LAN settings
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

	case METHOD_SHOW:
		m_nParam = (PARAM_TYPE)ParseParam(CurrentLine,len);
		switch (m_nParam)
			{
/*
// For debugging only
			case PARAM_USERNAME:
				sprintf(outbuf,szcrlf_percent_s,(char*)lan[UNSAVED].username);
				break;
			case PARAM_PASSWORD:
				sprintf(outbuf,szcrlf_percent_s,(char*)lan[UNSAVED].password);
				break;
*/
			case PARAM_PROFILE:
				sprintf(outbuf,szcrlf_percent_s,(char*)profile);
				break;
				
			case PARAM_LAN:
				sprintf(msg,szSTAR_C1,lan[UNSAVED].ip_addr,lan[UNSAVED].netmask,lan[UNSAVED].gateway,lan[UNSAVED].subnet,mac_addr,modem_config.local_port,modem_config.remote_port);
				strcat((char*)outbuf,(const char*)msg);
				break;

#ifndef SEB
			case PARAM_DIALNUMBER:
				strcat((char*)outbuf,(const char*)ppp_if[HW_IF].dialnumber);
				break;
#endif
			
			case PARAM_UART:
				// show dte n
				m_nParam = (PARAM_TYPE)ParseParam(CurrentLine,len);				
				if(m_nParam != PARAM_NUMERIC)
					break;

				i = (int)m_nNumber - 0x30;		// The uart number

				uart_2ascii(msg,i);
				strcat((char*)outbuf,(const char*)msg);
				break;
				
			case PARAM_MODULES:
				// Display client objects
				strcpy((char*)outbuf,(char*)szEndline);
				objects_2ascii(&outbuf[2]);
				break;
				
			case PARAM_CALLBAR:
				sprintf(msg,szCallbarDisplay,lan[UNSAVED].from_ip_addr1,lan[UNSAVED].to_ip_addr1,lan[UNSAVED].from_ip_addr2,lan[UNSAVED].to_ip_addr2);
				strcat((char*)outbuf,(const char*)msg);
				break;

			case PARAM_MODULE:
				// Display a client object based on class type
				m_nParam = (PARAM_TYPE)ParseParam(CurrentLine,len);				
				if(m_nParam != PARAM_NUMERIC)
					break;

				i = (int)m_nNumber - 0x30 -1;		// 1 based object number
				if(ip_protocols[i] == NULL)
					{
					sprintf(&outbuf[n],szNoObject,i+1);
					break;
					}

				switch(ip_protocols[i]->protocol)
					{
					case IP_TCP:

					sprintf(outbuf,szTCPObjectDisplay,
					ip_protocols[i]->name,
					ip_protocols[i]->pClient? ip_protocols[i]->pClient->name:szNone,ip_protocols[i],
					((CTcp*)ip_protocols[i])->RTT_MS,
					((CTcp*)ip_protocols[i])->SRTT_MS,
					((CTcp*)ip_protocols[i])->RTO_MS,
					((CTcp*)ip_protocols[i])->BACKOFF_TIMEOUT_MS,
					((CTcp*)ip_protocols[i])->rtx_buf_size,
					((CTcp*)ip_protocols[i])->total_resends);
/*
					sprintf(outbuf,"\r\n%s\r\ndefault_mss=%u\r\nMSS=%u\r\nGettransmitBuffer()=%u\r\nrtx_buf_in=%u\r\nrtx_buf_out=%u\r\nrtx_buf_size=%u\r\nresend_count=%u\r\n",
					ip_protocols[i]->name,
					((CTcp*)ip_protocols[i])->default_mss,
					((CTcp*)ip_protocols[i])->MSS,
					((CTcp*)ip_protocols[i])->GettransmitBuffer(),
					((CTcp*)ip_protocols[i])->rtx_buf_in,
					((CTcp*)ip_protocols[i])->rtx_buf_out,
					((CTcp*)ip_protocols[i])->rtx_buf_size,
					((CTcp*)ip_protocols[i])->total_resends);
*/
					break;

					case IP_ICMP:
					sprintf(outbuf,szObjectDisplay,
					ip_protocols[i]->name,
					ip_protocols[i]->pClient? ip_protocols[i]->pClient->name:szNone,ip_protocols[i]);
					break;
					
					case IP_UDP:
					sprintf(outbuf,szUDPObjectDisplay,
					ip_protocols[i]->name,
					ip_protocols[i]->pClient? ip_protocols[i]->pClient->name:szNone,ip_protocols[i]);
					break;					

					default:
					m_nParam = PARAM_UNSUPPORTED;
					break;					
					}

				break;

#ifndef SEB		

			case PARAM_ARP:
				sprintf(outbuf,szARPCacheDisplay);
				len= strlen((const char*)outbuf);
				arpcache_2ascii(&outbuf[len],0);
			break;
									
			case PARAM_PORT:
				// Display ethernet and ISDN port settings
				sprintf(outbuf,szPortDisplay);
				i = strlen((const char*)outbuf);
				// Ethernet port
				sprintf(&outbuf[i],szLANPortsDisplay,lan[ACTIVE].if_name,lan[ACTIVE].ip_addr);

				// ISDN ports
				i=strlen((char*)outbuf);
				for(n = 0; n < NUM_TA_INTERFACES; n++)
					{
					sprintf(&outbuf[i],szISDNPortsDisplay,ppp_if[n].auth,phase_2ascii(ipstr,ppp_if[n].phase),ppp_if[n].auth,ppp_if[n].ip_addr,ppp_if[n].dialnumber);
					i=strlen((char*)outbuf);
					}
			break;
#endif
			default:
				m_nParam = PARAM_UNSUPPORTED;
			break;
			}
		break;

	case METHOD_ECHO:
		m_nParam = (PARAM_TYPE)ParseParam(CurrentLine,len);
		switch (m_nParam)
			{
			case PARAM_ON:
				echo = ECHO_ON;
				break;
			case PARAM_OFF:
				echo = ECHO_OFF;
				break;
			default:
				m_nParam = PARAM_UNSUPPORTED;
			}
		break;
	case METHOD_PROMPT:
		strcpy((char*)sPrompt,szEndline);
		strcat((char*)sPrompt,(const char*)CurrentLine);
		break;
	case METHOD_UNSUPPORTED:
		break;
	}

if(m_nMethod == METHOD_UNSUPPORTED)
	sprintf(outbuf,szBadCommand,CurrentLine);

else if(m_nParam == PARAM_UNSUPPORTED)
	sprintf(outbuf,szUnsupportedParam);

return true;
}

//--------------------------------------------------------------------------
int CCommandPort::ParseMethod(unsigned char* CurrentLine,unsigned int len)
{
int commandlen;

m_nMethod =METHOD_UNSUPPORTED;
// Parse current line for a method (command)from the method table
for(int i =0; i < MethodTableLen; i++)
	{
	commandlen = (unsigned int)strlen((const char*)MethodTable[i].command);
	if(len >= commandlen)
		if(0 == strncmp((const char*)CurrentLine, MethodTable[i].command,commandlen ))
			{
			// Trim the method out of the command line
			m_nMethod = MethodTable[i].id;
			TrimLeft(CurrentLine,commandlen);
			len = (unsigned int)strlen((const char*)CurrentLine);
			break;
			}
	}

return m_nMethod;
}

//--------------------------------------------------------------------------
int CCommandPort::ParseParam(unsigned char* CurrentLine,unsigned int len)
// Parse the current line for a method (command)from the method table
{
int commandlen;

m_nParam =PARAM_UNSUPPORTED;

for(int i =0; i < ParamTableLen; i++)
	{
	commandlen = (int)strlen((const char*)ParamTable[i].key);
	if(len >= commandlen)
		if(0== strncmp((const char*)CurrentLine, ParamTable[i].key,commandlen ))
			{
			// Trim the method out of the command line
			m_nParam = ParamTable[i].id;
			TrimLeft(CurrentLine,commandlen);
			len = (unsigned int)strlen((const char*)CurrentLine);
			break;
			}
		if((CurrentLine[0] >= '0') && (CurrentLine[0] <= '9'))
			{
			// A number..only 1 digit allowed currently
			m_nNumber = CurrentLine[0];
			m_nParam = (PARAM_TYPE)PARAM_NUMERIC;
			TrimLeft(CurrentLine,1);
			len = (unsigned int)strlen((const char*)CurrentLine);
			break;			
			}
		}

return m_nParam;
}

//--------------------------------------------------------------------------
int CCommandPort::getparam(unsigned char* CurrentLine,unsigned char* dest)
// Get the next word from CurrentLine.
// returns : length of word and text in string "dest"
{
int pos;
int len = (int)strlen((const char*)CurrentLine);

for(pos=0; pos < len; pos++)
	{
	if((CurrentLine[pos] == ' ') ||
		(CurrentLine[pos] == '\r') ||
		(CurrentLine[pos] == '\n') ||
		(CurrentLine[pos] == '\t'))

		break;
	dest[pos] = CurrentLine[pos];
	}

dest[pos] = NULL;
return pos;
}
