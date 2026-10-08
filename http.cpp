// http.cpp.  Implementation of HTTP1.1 protocol
// SEBs web browser interface

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "http.h"
#include "utils.h"
#include "lan.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"
#include "ffs.h"
#include "tcpmodem.h"

// Calling params for ParseReqText
#define SERIALISE_FLASH		1
#define SERIALISE_WEBPAGE	2
#define SERIALISE_FILE		3

int serialise_destination;		// Set to a SERIALISE_xxx value
#define METHOD_LOGGEDIN	10		// Login persist case

#ifdef SMTP
static const char szMailButton[]=			"Send mail";
static const char szMailtitle[]=			"Email";
#endif

// HTML pages accessible from the main (index) page, their titles and the text for their buttons:
//				Page ID				Button text			Page title
static struct Chttp::_tagHttpTable HttpTable[] = {
	{	Chttp::PAGE_UNSUPPORTED,	NULL,				NULL				},
	{	Chttp::PAGE_LAN,			szLANinterface,		szLANConfig			},
	{	Chttp::PAGE_SERIAL,			szSerialButton,		szSerialConfig		},
	{	Chttp::PAGE_PPP_WAN,		szDemandInterfaces,	szDemandConfig		},
	{	Chttp::PAGE_ROUTING,		szRoutingTable,		szRoutingConfig		},
#ifndef SEB
	{	Chttp::PAGE_RAS,			szDUN,				szDUNConfig			},
#endif
	{	Chttp::PAGE_LOADSAVECONFIG,	szLoadSaveSettings,	szLoadSave			},
	{	Chttp::PAGE_SET_PASSWD,		szSecurityButton,	szSecurity			},
	{	Chttp::PAGE_INDEX,			NULL,				szIndex				},
	{	Chttp::PAGE_CALLBAR,		szCallBarButton,	szCallBar			},
	{	Chttp::PAGE_ENTER_PASSWD,	NULL,				szSecurity			},
#ifdef SMTP
	{	Chttp::PAGE_SENDMAIL,		szMailButton,		szMailtitle			}
#endif
#ifdef DHCP_SERVER
	{	Chttp::PAGE_DHCP,			szDHCPButton,		szDHCPtitle			}
#endif
};

// HTTP 1.0 methods (commands from a web browser)
static const struct Chttp::_tagMethodTable MethodTable[] = {
	{	Chttp::METHOD_GET,		szHTTPMethodGet		},
	{	Chttp::METHOD_POST,		szHTTPMethodPost	},
	{	Chttp::METHOD_HEAD,		szHTTPMethodHead	},
	{	Chttp::METHOD_PUT,		szHTTPMethodPut		},
	{	Chttp::METHOD_DELETE,	szHTTPMethodDelete	}
};

//--------------------------------------------------------------------------
int Chttp::serial_receive(unsigned char **data,unsigned int *len,unsigned int window)
{
return receive(data,len,window);
}

//--------------------------------------------------------------------------
void Chttp::init(int HW_IFparam,CProtocol_L3* pTransport,bool bAutoDelete)
{
name = szHTTP;
transport = pTransport;
HW_IF = HW_IFparam;

szBanner = NULL;
quit = false;
autodelete =  bAutoDelete;			// Delete instance when quit flag is set
echo = true;
auth_ok = false;
outbuf = NULL;						// Unused

HTML_len = 0;						// Length of the webpage we're assembling
MethodTableLen = sizeof(MethodTable)/ sizeof(struct _tagMethodTable);

sForm_Num[0]='T';					// This text identifies the HTML form
MRU = DEFAULT_TCP_SEGMENT_SIZE;
pTransport->idletimer_ms = 5000L;	// Max idle time before deletion
m_nLastMethod = METHOD_UNSUPPORTED;
}

//--------------------------------------------------------------------------
int Chttp::receive(unsigned char **rxdata,unsigned int *plen,unsigned int window)
// Returns to caller (normally TCP)
//  TERMINATE	- Send data (HTML page) then close TCP session - normal for HTML
//  ACKNOWLEDGE	- TCP ACK's the frame 
//	NORESPONSE	- don't ever send 2 ACK responses !
{
unsigned int len = *plen;
unsigned char *data = *rxdata;		
data[len] = NULL;				// Null terminate command string
int m_nButton;
*plen = NULL;
int m_nMethod = ParseReq(data,len);
int NEXT_PAGE = PAGE_INDEX;
int iRet = TERMINATE;

if(m_nLastMethod != METHOD_UNSUPPORTED)
	m_nMethod = m_nLastMethod;


switch(m_nMethod)
	{
	case METHOD_GET:
		// We just got contacted by an HTTP browser
		if(lan[ACTIVE].username[0] != NULL)
			{
			// Request username and password
			// for access to configuration
			CreatePage(PAGE_ENTER_PASSWD);
			break;
			}
		// Now fall through to main page if no password is required..

	case METHOD_LOGGEDIN:
		CreatePage(PAGE_INDEX);
		break;

	case METHOD_POST:
		// A button on one of our HTML pages was pressed
		m_nMethod = METHOD_LOGGEDIN;		// Incase we re-enter switch to display opening page
		form_num = 1;
		int32 button;
		if(-1 == ParseReqText(data,(unsigned char*)szBUTTON,TYPE_INT,&button))
			{
			iRet = ACKNOWLEDGE;
			m_nLastMethod = METHOD_POST;
			*plen = HTML_len;
			*rxdata = (unsigned char*)pHTML;
			return ACKNOWLEDGE;
			}
			
		m_nButton = (int)button;

		sprintf(msg,szBUTTON_PRESSED,m_nButton);
		OutputDebugString(LOG_INFO,LOG_HTML,(const char*)msg);

		switch(m_nButton)	// Action depends on the button pressed...
			{
			case PAGE_SET_PASSWD:
#ifndef SEB
			case PAGE_RAS:
#endif			
			case PAGE_LAN:
			case PAGE_SERIAL:
			case PAGE_PPP_WAN:
			case PAGE_ROUTING:
			case PAGE_LOADSAVECONFIG:
			case PAGE_CALLBAR:
			case PAGE_DHCP:
				// m_n button reflects the button number 
				// pressed on the main page
				CreatePage(m_nButton);
				NEXT_PAGE = NULL;
				break;

#ifdef SMTP
			case 10*PAGE_SENDMAIL + BUTTON_OK:
				break;
#endif

#ifndef SEB
			case 10*PAGE_PPP_WAN + BUTTON_OK:
				// Add demand dial interface
				restore_settings(data,PAGE_PPP_WAN);
				NEXT_PAGE = PAGE_PPP_WAN;
				break;

			case 10*PAGE_PPP_WAN + BUTTON_DELETE:
				// Delete demand-dial interface
//				restore_settings(data,PAGE_PPP_WAN);
				restore_param(data,TYPE_STRING,(int16*)msg);

				// Find interface to delete (skip LAN entry)
				for(int i=1; i < NUM_PPP_LINKS; i++)
					{
					if(0 == strcmp((char*)msg,(char*)ppp_if[i].auth))
						// Found it,delete entry
						ppp_if[i].auth[0] = NULL;
					}
				NEXT_PAGE = PAGE_PPP_WAN;
				break;

			case 10*PAGE_RAS + BUTTON_OK:
				// DUN OK button..
				restore_settings(data,PAGE_RAS);
				break;
#endif

#ifdef DHCP_SERVER
			case 10*PAGE_DHCP + BUTTON_OK:
				restore_settings(data,PAGE_DHCP);
				num_pool_addresses = 1 + address_pool.last_ip_addr - address_pool.first_ip_addr;
				num_pool_addresses = min(MAX_POOL_LEN,num_pool_addresses);
				break;
#endif				
			case 10*PAGE_CALLBAR + BUTTON_OK:
				restore_settings(data,PAGE_CALLBAR);
				dirty = true;
				break;
			
			case 10*PAGE_ENTER_PASSWD + BUTTON_CANCEL:
				// Redisplay login page
				NEXT_PAGE = PAGE_ENTER_PASSWD;
				break;
				
			case 10*PAGE_ENTER_PASSWD + BUTTON_OK:
				// User entered passwd and ID on entry
				if(restore_settings(data,PAGE_ENTER_PASSWD))
					{
					auth_ok=true;
					NEXT_PAGE = PAGE_INDEX;
					break;
					}

				// Bad login. Close TCP session, redisplay login page
				NEXT_PAGE = PAGE_ENTER_PASSWD;
				break;

			case 10*PAGE_SET_PASSWD + BUTTON_OK:
				// New username and password entered, OK pressed.
				if(! restore_settings(data,PAGE_SET_PASSWD) )
					{
					// Passwords didn't match.
					// Tell user and redisplay page
					HttpTable[PAGE_SET_PASSWD].title=szBadPassword;
					NEXT_PAGE = PAGE_SET_PASSWD;
					break;
					}

				// Incase this pages title was changed to "passwords don't match...
				HttpTable[PAGE_SET_PASSWD].title=szSecurity;			
				// New username and password accepted
				break;

			case 10*PAGE_SET_PASSWD + BUTTON_CANCEL:
				// Incase this pages title was changed to "passwords don't match...
				HttpTable[PAGE_SET_PASSWD].title=szSecurity;
				break;

			case 10*PAGE_LOADSAVECONFIG + BUTTON_DISCARD:
				// discard changed LAN settings
				memcpy(&lan[UNSAVED],&lan[ACTIVE],sizeof(lanport));
				dirty = false;
				NEXT_PAGE = PAGE_LOADSAVECONFIG;
				break;
			
			case 10*PAGE_LOADSAVECONFIG + BUTTON_STARTUP:
				// Make this the startup profile
				// Also load the profile now so it is in use immediately
				restore_settings(data,PAGE_LOADSAVECONFIG);
				m_nButton = PAGE_LOADSAVECONFIG;
				
				// Load the specified profile
				if(ffs->open(MODE_OPENEXISTING,(char*)ipstr,szFILETYPE_PROFILE))
					{
					ffs->restore_from_flash(SAVE_RESTORE_ALL);
					ffs->close();
					strcpy((char*)profile,(char*)ipstr);		// Save the profile name for reference
					}
				
				ffs->ffsReset();
				ffs->del((unsigned char*)szProfileBoot,szFILETYPE_TEXT);
				if(strcomp(ipstr,(unsigned char*)szProfileDefault))
					{// Default profile selected
					NEXT_PAGE = PAGE_LOADSAVECONFIG;
					break;
					}
									
				// Set it as the boot filename
				if(ffs->create_bootfile((char*)ipstr))
					strcpy((char*)profile,(char*)ipstr);
				else profile[0] = NULL;
				NEXT_PAGE = PAGE_LOADSAVECONFIG;
				break;

			case 10*PAGE_LOADSAVECONFIG + BUTTON_SAVE:
				// Save settings to profile highlighted in listbox
				restore_settings(data,PAGE_LOADSAVECONFIG);
				// Delete existing profile file
				ffs->ffsReset();

				ffs->del(ipstr,szFILETYPE_PROFILE);
				// Save settings to a new profile
				if(ffs->open(MODE_CREATE,(char*)ipstr,szFILETYPE_PROFILE))
					{
					ffs->save_to_flash(SAVE_RESTORE_ALL);
					ffs->close();
					}			
				NEXT_PAGE = PAGE_LOADSAVECONFIG;
				break;

			case 10*PAGE_LOADSAVECONFIG + BUTTON_SAVE3:
				// Save LAN settings. 
				// ** IP address change not used until rebooted !**
				ffs->ffsReset();
				ffs->del((unsigned char*)szLAN,szFILETYPE_SYSTEM,true);
				if(ffs->open(MODE_CREATE,szLAN,szFILETYPE_SYSTEM))
					{
					ffs->save_to_flash(SAVE_RESTORE_LAN);
					ffs->close();
					dirty = false;
					}
				NEXT_PAGE = PAGE_LOADSAVECONFIG;
				break;
								
			case 10*PAGE_LOADSAVECONFIG + BUTTON_SAVE2:
				// Save current settings to a new profile.
				restore_settings(data,PAGE_LOADSAVECONFIG);
				// ipstr is Profile from listbox
				// macstr is the New profile name
				ffs->ffsReset();
				if(ffs->open(MODE_CREATE,(char*)macstr,szFILETYPE_PROFILE))
					{
					ffs->save_to_flash(SAVE_RESTORE_ALL);
					ffs->close();
					}			
				NEXT_PAGE = PAGE_LOADSAVECONFIG;
				break;
			
			case 10*PAGE_LOADSAVECONFIG + BUTTON_DELETE:
				// Delete highlighted profile...
				restore_settings(data,PAGE_LOADSAVECONFIG);
				// ipstr is Profile from listbox
				// macstr is the New profile name
				ffs->ffsReset();
				ffs->del(ipstr,szFILETYPE_PROFILE);
				NEXT_PAGE = PAGE_LOADSAVECONFIG;
				break;
						
			case 10*PAGE_ROUTING + BUTTON_INSERT:
				// Insert route and refresh page
				restore_settings(data,PAGE_ROUTING);
				if(addroute(ip_addr,ip_mask,gateway,interface_unit))
					HttpTable[PAGE_ROUTING].title=szhtmlRouteAdded;
				else
					HttpTable[PAGE_ROUTING].title=szhtmlRouteNotAdded;

				NEXT_PAGE = PAGE_ROUTING;
				break;
				
			case 10*PAGE_ROUTING + BUTTON_DELETE:
				// Delete route and refresh page
				restore_settings(data,PAGE_ROUTING);
				if(deleteroute(ip_addr))
					HttpTable[PAGE_ROUTING].title=szhtmlRouteDeleted;

				NEXT_PAGE = PAGE_ROUTING;
				break;

			case 10*PAGE_LAN + BUTTON_OK:
				// LAN OK button..
				restore_settings(data,PAGE_LAN);
				// Copy LAN interface settings from configuration scratch area
				if(! strcomp(lan[UNSAVED].hostname,lan[ACTIVE].hostname ))
					dirty=true;

				if((lan[UNSAVED].ip_addr != lan[ACTIVE].ip_addr) ||
					(lan[UNSAVED].netmask != lan[ACTIVE].netmask) ||
						(lan[UNSAVED].gateway != lan[ACTIVE].gateway) ||
						(lan[UNSAVED].permitTFTP != lan[ACTIVE].permitTFTP) ||
						(lan[UNSAVED].tcp_idletimer != lan[ACTIVE].tcp_idletimer) ||
						(lan[UNSAVED].led_mode != lan[ACTIVE].led_mode) ||
						(lan[UNSAVED].idle_reset != lan[ACTIVE].idle_reset))
					// Flag LAN settings change.
					dirty=true;
				break;

			case 10*PAGE_SERIAL + BUTTON_OK:
				restore_settings(data,PAGE_SERIAL);
				break;

			default:
				// Most cancel buttons go here.
				// Top-level menu is displayed
				break;
			}

		if(NEXT_PAGE)
			CreatePage(NEXT_PAGE);
		NEXT_PAGE = PAGE_INDEX;
		break;

	default:
		// Assume this is an ACK. Quit the TCP session
		m_nLastMethod = METHOD_UNSUPPORTED;
		return TERMINATE;
	};

// Send the data and close the TCP session
*plen = HTML_len;
*rxdata = (unsigned char*)pHTML;
quit = true;
return iRet;
}

//--------------------------------------------------------------------------
void Chttp::serialise(int32 data,const char* szDescription,int type)
// Add a text entry box to the webpage.
// Wrapper for CreateHTML_form(
{
switch(serialise_destination)
	{	
	case SERIALISE_FLASH:
		// Save in flash ram
//		flashwrite(data);
		break;

	case SERIALISE_WEBPAGE:
		// Write value to a webpage
		switch(type)
			{
			case TYPE_BOOL:
			strcpy((char*)msg,szChecked);		// String for bool true
			if(data == 0L)
				msg[0] = NULL;					// No string = false

			CreateHTML_form(msg,szDescription,TYPE_BOOL);
			break;

			case TYPE_INT:						// Default
			sprintf(msg,szpercent_lu,(int32)data);
			CreateHTML_form(msg,szDescription,TYPE_STRING);
			break;

			case TYPE_IP:
			CreateHTML_form(ip_2ascii(data,false),szDescription,TYPE_STRING);
			break;

			case TYPE_STRING:
			CreateHTML_form((unsigned char*)data,szDescription,TYPE_STRING);
			break;

			case TYPE_PASSWORD:
			CreateHTML_form((unsigned char*)data,szDescription,TYPE_PASSWORD);
			break;
			}

		break;
	case SERIALISE_FILE:
		// Save as text
	break;
	}
}

//--------------------------------------------------------------------------
int Chttp::CreateHTML_header(unsigned char* fn,unsigned int window)
// First of 3 functions used to construct a web page with forms
{
pHTML = (char*)packet_buf + TCP_DATA_OFFSET;
strcpy(pHTML,szTitle);
strcat(pHTML,szBody);
strcat(pHTML,szForm);
HTML_len = strlen(pHTML);
return 0;
}

//--------------------------------------------------------------------------
int Chttp::CreateHTML_ender(unsigned char* fn,unsigned int window)
// End the HTML page
{
strcat(pHTML,szUnForm);
strcat(pHTML,szUnBody);
strcat(pHTML,szUnHTML);
HTML_len = strlen(pHTML);
return true;
}

//--------------------------------------------------------------------------
void Chttp::AddHTML_list_option(const char* title,int value,int selectedvalue)
// Adds an item to a droplist for CreateHTML_list() function
// Parameters:
// Title is the list box item text
// value is the HTML value field
// selectedvalue
{
if(value == 0xffff)
	{
	// No numeric value for option.
	// Caller wants a text return from this droplist
	if(selectedvalue == value)
		sprintf(msg,szOptionSelected,value);
	else
		sprintf(msg,szOption,value);
	}
else	
	{
	// Value specified for option
	// The droplist will return a numeric value
	if(selectedvalue == value)
		sprintf(msg,szOptionValueSelected,value);
	else
		sprintf(msg,szOptionValue,value);
	}
	
strcat(pHTML,(const char*)msg);
strcat(pHTML,title);
strcat(pHTML,szUnOption);
}

//--------------------------------------------------------------------------
int Chttp::CreateHTML_list(const char* title,int LIST_STYLE,int16 selectedvalue)
// Adds a droplist to the page
// Note on return valuse when a POST button is pressed:
// Droplists can return either their text value or an integer value
// Function AddHTML_list_option(...) generates either of the following forms of string:
// <option value="96">1200</option> 	returns int 96
// <option>1200</option> 				returns string "1200"
//The second string type is generated if parameter 2 of the function is -1.
// .. both types are used
{
int value=0;
sprintf((unsigned char*)pHTML+HTML_len,szDropListStart,title,form_num++);
msg[0] = NULL;

switch(LIST_STYLE)
	{
	case LIST_DROPLIST_SHELL:
	AddHTML_list_option(szShellAT,PROFILE_AT,selectedvalue);
	AddHTML_list_option(szShellTelnet,PROFILE_TELNET,selectedvalue);
#ifdef PAD
	AddHTML_list_option(szShellPAD,PROFILE_PAD,selectedvalue);
#endif
	break;
	
	case LIST_DROPLIST_PROFILES:
	ffs->ffsReset();
	while(ffs->enumfiles())
			{
			if((ffs->enumerate_file.filename[0] != NULL) && (0==strcmp(ffs->enumerate_file.ext,szFILETYPE_PROFILE)))
				{
				if(strcomp((unsigned char*)ffs->enumerate_file.filename,profile))
					selectedvalue = 0xffff;					// Default profile selected in the list
				AddHTML_list_option((const char*)ffs->enumerate_file.filename,0xffff,selectedvalue);
				selectedvalue = 0;
				}
			}
	break;
	
	case LIST_DROPLIST_BAUD:
#ifdef BAUD_1200
	AddHTML_list_option(sz38400,BAUDOVERSAMPLERATE_SLOW38400,selectedvalue);
	AddHTML_list_option(sz19200,BAUDOVERSAMPLERATE_SLOW19200,selectedvalue);
	AddHTML_list_option(sz9600,BAUDOVERSAMPLERATE_SLOW9600,selectedvalue);
	AddHTML_list_option(sz4800,BAUDOVERSAMPLERATE_SLOW4800,selectedvalue);
	AddHTML_list_option(sz2400,BAUDOVERSAMPLERATE_SLOW2400,selectedvalue);
	AddHTML_list_option(sz1200,BAUDOVERSAMPLERATE_SLOW1200,selectedvalue);
#else
	AddHTML_list_option(sz115200,BAUDOVERSAMPLERATE_115200,selectedvalue);
	AddHTML_list_option(sz57600,BAUDOVERSAMPLERATE_57600,selectedvalue);
	AddHTML_list_option(sz38400,BAUDOVERSAMPLERATE_38400,selectedvalue);
	AddHTML_list_option(sz19200,BAUDOVERSAMPLERATE_19200,selectedvalue);
	AddHTML_list_option(sz9600,BAUDOVERSAMPLERATE_9600,selectedvalue);
	AddHTML_list_option(sz4800,BAUDOVERSAMPLERATE_4800,selectedvalue);
#endif
	AddHTML_list_option(szAutobaud,99,selectedvalue);
	break;

	case LIST_DROPLIST_FLOW:
	AddHTML_list_option(szNOFLOW,value++,selectedvalue);
	AddHTML_list_option(szXONXOFF,value++,selectedvalue);
	AddHTML_list_option(szRTSCTS,value++,selectedvalue);
	AddHTML_list_option(szCTS,value++,selectedvalue);
	AddHTML_list_option(szRTSCTS_ONLINE,value++,selectedvalue);	// Doesn't work
	break;

 	case LIST_DROPLIST_LEDS:
	AddHTML_list_option(szOFF,value++,selectedvalue);
	AddHTML_list_option(szON,value++,selectedvalue);
	AddHTML_list_option("On during startup",value++,selectedvalue);
	break;

 	case LIST_DROPLIST_FORMAT:
	AddHTML_list_option(sz8N,value++,selectedvalue);
	AddHTML_list_option(sz7E,value++,selectedvalue);
	AddHTML_list_option(sz7O,value++,selectedvalue);
	break;
 	
	case LIST_DROPLIST_LAN:
	// Interface drop list ( LAN and WAN )
	// LAN interface
	strcat((char*)msg,szOption);
	strcat((char*)msg,szLAN);
	strcat((char*)msg,szUnOption);

#ifndef SEB
	// WAN interfaces
	for(int i=0; i < NUM_PPP_LINKS; i++)
		{
		if(ppp_if[i].auth[0] != NULL)
			{
			strcat((char*)msg,szOption);
			strcat((char*)msg,(char*)ppp_if[i].auth);
			strcat((char*)msg,szUnOption);
			}
		}

#endif

	if(strlen((const char*)msg) == 0)
		sprintf(msg,szhtml_NoOption);

	strcat(pHTML,(const char*)msg);
	break;

	case LIST_INTERFACES:
	// List interfaces
//	strcat(pHTML,szhtmllistsize2);
	strcat(pHTML,szInterfaceList);
#ifndef SEB
	wan_if_2ascii(packet_buf);
#endif
	strcat(pHTML,(const char*)packet_buf);
	strcat(pHTML,szUnTextArea);
	break;

#ifndef SEB
	case LIST_ROUTINGTABLE:
	// List routing table
//	strcat(pHTML,szhtmllistsize1);
	rtable_2ascii(packet_buf);
	strcat(pHTML,(const char*)packet_buf);
	strcat(pHTML,szUnTextArea);
	break;
#endif
	}

strcat(pHTML,szTableRowEnd);
HTML_len = strlen(pHTML);
return true;
}

//--------------------------------------------------------------------------
int Chttp::CreateHTML_button(int name,const char* szTitle,int column)
// Adds a button to the page, name of button is BUTTONn. n=name parameter
// Column is the table column. 0 = not in a table
{
if(szTitle == NULL)
	szTitle = HttpTable[name].button;			// Array of text for title page buttons

int temp=column;	
if(column--)
	{
	strcat(pHTML,szRow);						// Row 1 of table
	while(column--)
		strcat(pHTML,szColumnEmpty);			// Insert empty columns
	
	strcat(pHTML,szColumn);
	}
	
HTML_len = strlen(pHTML);
sprintf((unsigned char*)pHTML+HTML_len,szHTTPButton,szTitle,szBUTTON,name);
if(temp)
	strcat(pHTML,szRowEndCr);

HTML_len = strlen(pHTML);

return true;
}

//--------------------------------------------------------------------------
int Chttp::CreateHTML_form(unsigned char* value,const char* szDescription,int select)
// Adds a user entry field to the HTML page
// Creates a line in the format below and appends it to the web page:
{
HTML_len = strlen(pHTML);

switch(select)
	{
	case TYPE_BOOL:
	// Checkbox
	sprintf((unsigned char*)pHTML+HTML_len,szhtml_table_bool,szDescription,form_num++,value);
	break;
	
	case TYPE_STRING:
	// Text field
	sprintf((unsigned char*)pHTML+HTML_len,szhtml_table_string,szDescription,form_num++,value);
	break;

	case TYPE_PASSWORD:
	// Password field
	sprintf((unsigned char*)pHTML+HTML_len,szhtml_table_passwd,szDescription,form_num++,value);
	}

HTML_len = strlen(pHTML);
return 0;
}

//--------------------------------------------------------------------------
void Chttp::CreatePage(int pagenumber)
// Generate a page of HTML
{
serialise_destination = SERIALISE_WEBPAGE;
CreateHTML_header(NULL,NULL);
bool WANT_CANCEL_BUTTON = true;
bool WANT_OK_BUTTON = true;

form_num = 1;
// Page title
if(pagenumber == PAGE_INDEX)
	{
	sprintf(msg,(const char*)HttpTable[pagenumber].title,(char*)szSoftwareVersion2);
	strcat(pHTML,(const char*)msg);
	}
else
	strcat(pHTML,(const char*)HttpTable[pagenumber].title);
strcat(pHTML,szTable);

HTML_len = strlen(pHTML);

// Send HTML header to remote
transport->output((unsigned char*)pHTML,HTML_len, ACK,0,HW_LAN);
HTML_len = 0;
*pHTML = NULL;

// Add user entry forms in a table
switch(pagenumber)
	{
	case PAGE_INDEX:
	CreateHTML_button(PAGE_LAN,0,true);
#ifndef SEB
	CreateHTML_button(PAGE_PPP_WAN,0,true);
	CreateHTML_button(PAGE_ROUTING,0,true);
	CreateHTML_button(PAGE_RAS,0,true);
#endif
	CreateHTML_button(PAGE_SERIAL,0,true);
#ifdef DHCP_SERVER
	CreateHTML_button(PAGE_DHCP,0,true);
#endif
#ifdef SMTP
	CreateHTML_button(PAGE_SENDMAIL,0,true);
#endif
	CreateHTML_button(PAGE_SET_PASSWD,0,true);
	CreateHTML_button(PAGE_CALLBAR,0,true);	
	CreateHTML_button(PAGE_LOADSAVECONFIG,0,true);

	WANT_CANCEL_BUTTON = false;
	WANT_OK_BUTTON = false;
	break;

	case PAGE_ENTER_PASSWD:
	serialise(NULL,szUsername,TYPE_STRING);
	serialise(NULL,szPassword,TYPE_PASSWORD);
	break;	

	case PAGE_CALLBAR:
	serialise(lan[UNSAVED].from_ip_addr1,szPermitFromIP,TYPE_IP);
	serialise(lan[UNSAVED].to_ip_addr1,szPermitToIP,TYPE_IP);
	serialise(lan[UNSAVED].from_ip_addr2,szPermitFromIP,TYPE_IP);
	serialise(lan[UNSAVED].to_ip_addr2,szPermitToIP,TYPE_IP);
	break;
	
#ifdef DHCP_SERVER
	case PAGE_DHCP:
	serialise(address_pool.dhcp_enabled,szDHCPenable,TYPE_BOOL);
	serialise(address_pool.first_ip_addr,szDHCPfirstIP,TYPE_IP);
	serialise(address_pool.last_ip_addr,szDHCPlastIP,TYPE_IP);
	serialise(IP_LEASE_EXPIRE_TIME,szDHCPleasetime,TYPE_INT);
	break;
#endif

#ifdef SMTP
	case PAGE_SENDMAIL:
	serialise((int32)"seb@digitaslsp.co.uk","Mail from",TYPE_STRING);
	serialise(NULL,"Mail to",TYPE_STRING);
	serialise(NULL,"Message",TYPE_STRING);
	break;
#endif

	case PAGE_LOADSAVECONFIG:
//	serialise(NULL,szUsername,TYPE_STRING);
//	serialise(NULL,szPassword,TYPE_PASSWORD);
	CreateHTML_list(szStartupProfile,LIST_DROPLIST_PROFILES,(int16)profile);
	CreateHTML_button((10*PAGE_LOADSAVECONFIG)+BUTTON_SAVE,szSaveSelectedProfile,3);
	CreateHTML_button((10*PAGE_LOADSAVECONFIG)+BUTTON_STARTUP,szMakeStartupProfile,3);
	CreateHTML_button((10*PAGE_LOADSAVECONFIG)+BUTTON_DELETE,szDeleteThisProfile,3);
	serialise((int32)szProfileName,szSaveToNewProfile,TYPE_STRING);		
	CreateHTML_button((10*PAGE_LOADSAVECONFIG)+BUTTON_SAVE2,szSaveToNewProfile2,3);
	if(dirty)
		{
		// LAN settings were changed
		strcat((char*)pHTML+HTML_len,(char*)szLANSettingChanged);
		HTML_len = strlen(pHTML);
//		serialise((int32)szLANSettingChanged,szNull,TYPE_STRING);		
		CreateHTML_button((10*PAGE_LOADSAVECONFIG)+BUTTON_SAVE3,szSaveLanSettings,3);
		CreateHTML_button((10*PAGE_LOADSAVECONFIG)+BUTTON_DISCARD,szDiscardLanSettings,3);
		}
	WANT_OK_BUTTON = false;
	break;
	
	case PAGE_PPP_WAN:
	serialise(NULL,szIFname,TYPE_STRING);
	serialise(NULL,szPassword,TYPE_PASSWORD);
	serialise(NULL,szDemand,TYPE_BOOL);
	serialise(NULL,szDialNumber,TYPE_STRING);
//	serialise(ppp_if[pagenumber].maxinactivitytime/100,szIdleDisconnectSeconds);	// Inactive time before link will be reset
//	serialise(ppp_if[pagenumber].useChap,szEncryptPassword,TYPE_BOOL);
	CreateHTML_button((10*PAGE_PPP_WAN)+BUTTON_INSERT,szAdd);
	CreateHTML_button((10*PAGE_PPP_WAN)+BUTTON_DELETE,szDelete);
	CreateHTML_list(szNo,LIST_INTERFACES);
	break;

	case PAGE_SERIAL:
	if(modem_config.autobaud_flag == 1)
		// Autobaud entry is selected
		CreateHTML_list(szBaudrate,LIST_DROPLIST_BAUD,99);
	else
		// Current (fixed) baudrate is selected
		CreateHTML_list(szBaudrate,LIST_DROPLIST_BAUD,modem_config.baudrate_flag);

	CreateHTML_list(szFlowControl,LIST_DROPLIST_FLOW,modem_config.flowcontrol_flag);
	CreateHTML_list(szDataFormat,LIST_DROPLIST_FORMAT,modem_config.parity_flag);
	serialise(modem_config.com_enable_rfc2217,szHTMLrfc2217,TYPE_BOOL);
	CreateHTML_list(szATDroplist,LIST_DROPLIST_SHELL,modem_config.shell);
	serialise((int32)&modem_config.at_cmd[0],(char*)szATDefaults,TYPE_STRING);

	// Send HTML data to remote
	HTML_len = strlen(pHTML);
	transport->output((unsigned char*)pHTML,HTML_len, ACK,0,HW_LAN);
	HTML_len = 0;
	*pHTML = NULL;

	serialise(modem_config.tcp_idletimer/1000,szTCPidletimer,TYPE_INT);
	serialise(modem_config.remote_port,szTCPremotePort,TYPE_INT);
	serialise(modem_config.local_port,szTCPLocalPort,TYPE_INT);
	break;
	
	case PAGE_LAN:
	serialise((int)lan[UNSAVED].hostname,szRouterName,TYPE_STRING);
	serialise(lan[UNSAVED].ip_addr,szip_addr,TYPE_IP);
	serialise(lan[UNSAVED].netmask,sznetmask,TYPE_IP);
	serialise(lan[UNSAVED].subnet,szsubnet,TYPE_IP);
	serialise(lan[UNSAVED].gateway,szgateway,TYPE_IP);
	serialise(lan[UNSAVED].tcp_idletimer/1000,szTCPidletimer2,TYPE_INT);
	CreateHTML_list("LED mode",LIST_DROPLIST_LEDS,lan[UNSAVED].led_mode);
#ifdef SWITCH_FIX
	serialise(lan[UNSAVED].idle_reset,szLanIdleReBootAsk,TYPE_BOOL);
#endif	
#ifdef TFTP
	serialise(lan[UNSAVED].permitTFTP,szPermitTFTP,TYPE_BOOL);
#endif
	break;

	case PAGE_SET_PASSWD:
	serialise((int)lan[UNSAVED].username,szUsername,TYPE_PASSWORD);
	serialise((int)lan[UNSAVED].password,szPassword,TYPE_PASSWORD);
	serialise((int)lan[UNSAVED].password,szConfirmPassword,TYPE_PASSWORD);
	break;

#ifndef SEB
	case PAGE_ROUTING:
	serialise(NULL,szip_addr,TYPE_IP);
	serialise(NULL,sznetmask,TYPE_IP);
	serialise(NULL,szgateway,TYPE_IP);

	CreateHTML_list(szInterface,LIST_DROPLIST_LAN);
	strcat(pHTML + HTML_len,szP);
	HTML_len += strlen(szP);
	CreateHTML_button((10*PAGE_ROUTING)+BUTTON_INSERT,szInsert);
	CreateHTML_button((10*PAGE_ROUTING)+BUTTON_DELETE,szDelete);
	strcat(pHTML + HTML_len,szUnP);
	HTML_len += strlen(szUnP);
	CreateHTML_list(szNo,LIST_ROUTINGTABLE);
	break;

	case PAGE_RAS:
	serialise(ras.enableras,szAllowRAS,TYPE_BOOL);							// Allow Ras dial-in client
	serialise(ras.PermitLANAccess,szPermitLANAccess,TYPE_BOOL);	// Enable Ras dial-in client to access LAN
	// RAS address pool
//	serialise(ip_pool[0].ip_addr,szHTTPfirstip,TYPE_IP);
//	serialise(2,(unsigned char*)"Number of addresses");
	break;
#endif

	}	// endswitch

// End the table
strcat(pHTML,szUnTable);

// Add OK and Cancel buttons.
if(WANT_CANCEL_BUTTON)
	CreateHTML_button((10*pagenumber)+BUTTON_CANCEL,szCancel);
	
if(WANT_OK_BUTTON)
	CreateHTML_button((10*pagenumber)+BUTTON_OK,szOKButton);

if(dirty)
	strcat(pHTML,szConfigChanged);

// End form and HTML
CreateHTML_ender(NULL,NULL);
return;
}

//--------------------------------------------------------------------------
int Chttp::restore_settings(unsigned char* CurrentLine,int pagenumber)
// Extract parameters from a page of HTML
// returns : 1 = no change
//  		0 = settings have changed
{
unsigned char tmpstr1[MAXLEN1];
int32 temp32;
int16 temp16;
int pos;

switch(pagenumber)
	{

#ifndef SEB
int i;	
	case PAGE_PPP_WAN:
	// Retrieve the new interface name
	restore_param(CurrentLine,TYPE_STRING,(int16*)tmpstr1);

	// Check the interface name is unique
	for(i=0; i < NUM_PPP_LINKS; i++)
		{
		if(0 == strcmp((char*)tmpstr1,(char*)ppp_if[i].auth))
			// An identical interface exists.
			return 1;
		}
	// Find a spare location in the PPP interface array
	// Nb don't use location 0 - reserved for LAN
	for(int i=1; i < NUM_PPP_LINKS; i++)
		{
		if(ppp_if[i].auth[0] == NULL)
			{
			// Save the interface details
			strcpy((char*)ppp_if[i].auth,(char*)tmpstr1);
			restore_param(CurrentLine,TYPE_STRING,(int16*)&ppp_if[i].password);
			restore_param(CurrentLine,TYPE_BOOL,(int16*)&ppp_if[i].demand);
			restore_param(CurrentLine,TYPE_STRING,(int16*)&ppp_if[i].dialnumber);

			//restore_param(CurrentLine,TYPE_INT,0,&temp32);
			//	ppp_if[i].maxinactivitytime = temp32 * 100;
			//restore_param(CurrentLine,TYPE_BOOL,(int16*)&hardware_if[i].useChap);
			return 0;
			}
		}
 
	break;

	case PAGE_ROUTING:
	int32 temp;
	restore_param(CurrentLine,TYPE_IP,0,&ip_addr);
	restore_param(CurrentLine,TYPE_IP,0,&ip_mask);
	restore_param(CurrentLine,TYPE_IP,0,&gateway);

	interface_unit=0;
	ParseReqText(CurrentLine,(unsigned char*)"D4=",TYPE_STRING,(int32*)tmpstr1);
	// Find the index number of the interface
	for(int i=0; i < NUM_PPP_LINKS; i++)
		{
		if(0 == strcmp((char*)tmpstr1,(char*)ppp_if[i].auth))
			{
			interface_unit = i;
			break;
			}
		}

	ParseReqText(CurrentLine,(unsigned char*)"D5=",TYPE_IP,&temp);
	break;

	case PAGE_RAS:
	restore_param(CurrentLine,TYPE_BOOL,(int16*)&ras.enableras);
	restore_param(CurrentLine,TYPE_BOOL,(int16*)&ras.PermitLANAccess);
	i=restore_param(CurrentLine,TYPE_IP,0,&temp32);

	if(temp32 != 0)
		{
		// Initialise RAS address pool using new base IP addr
		for(int i=0; i < NUM_PPP_LINKS; i++)
			ip_pool[i].ip_addr = temp32 + i;
		}
	break;
#endif

	case PAGE_CALLBAR:
	restore_param(CurrentLine,TYPE_IP,0,&lan[UNSAVED].from_ip_addr1);
	restore_param(CurrentLine,TYPE_IP,0,&lan[UNSAVED].to_ip_addr1);
	restore_param(CurrentLine,TYPE_IP,0,&lan[UNSAVED].from_ip_addr2);
	restore_param(CurrentLine,TYPE_IP,0,&lan[UNSAVED].to_ip_addr2);
	dirty = true;
	break;

#ifdef DHCP_SERVER
	case PAGE_DHCP:
	restore_param(CurrentLine,TYPE_BOOL,(int16*)&address_pool.dhcp_enabled);	
	restore_param(CurrentLine,TYPE_IP,0,&address_pool.first_ip_addr);
	restore_param(CurrentLine,TYPE_IP,0,&address_pool.last_ip_addr);
	restore_param(CurrentLine,TYPE_STRING,0,&IP_LEASE_EXPIRE_TIME);
	break;
#endif

	case PAGE_SERIAL:
	restore_param(CurrentLine,TYPE_INT,0,&temp32);
	if(temp32 == 99L)
		// Auto-baudrate
		modem_config.autobaud_flag = 1;
	else
		{// Fixed baudrate
		modem_config.autobaud_flag = 0;
		modem_config.baudrate_flag = (int)temp32;
		}
	
	restore_param(CurrentLine,TYPE_INT,0,&temp32);
	modem_config.flowcontrol_flag = (int)temp32;
	
	restore_param(CurrentLine,TYPE_INT,0,&temp32);
	modem_config.parity_flag = (int)temp32;

	restore_param(CurrentLine,TYPE_BOOL,&temp16);
	modem_config.com_enable_rfc2217 = (bool)temp16;			
	
	restore_param(CurrentLine,TYPE_INT,0,&temp32);
	if(	modem_config.shell != (int)temp32)
		{
		// Shell has been changed
		modem_config.shell = (int)temp32;
		if(modem_config.shell == PROFILE_AT)
			// Set AT decoder defaults for new shell
			ATFactoryReset(HW_DTE0,modem_config.shell);
		}

	restore_param(CurrentLine,TYPE_STRING,(int16*)modem_config.at_cmd);

	// Send default AT commands to shell AT decoder now
	if((strlen(modem_config.at_cmd)) && (shell != NULL))
		{
		if(modem_config.shell == PROFILE_AT)
			{
			// If user has entered "AT" or "at", remove it
			pos=0;
			if( (( modem_config.at_cmd[0] == 'a') || ( modem_config.at_cmd[0] == 'A')) &&
				(( modem_config.at_cmd[1] == 't') || ( modem_config.at_cmd[1] == 'T')) )
				pos=2;

			// parse() destroys the input string...
			strcpy((char*)ipstr,&modem_config.at_cmd[pos]);
	 		((CTCPModem*)shell)->parse(ipstr);
		 	}
		}

	restore_param(CurrentLine,TYPE_INT,0,&temp32);
	if(temp32 >= 20)
		// 20sec lower limit !
		modem_config.tcp_idletimer = temp32*1000;	
	
	restore_param(CurrentLine,TYPE_INT,0,&temp32);
	modem_config.remote_port = temp32;
	restore_param(CurrentLine,TYPE_INT,0,&temp32);
	modem_config.local_port = temp32;

	UpdateUart(HW_DTE0);
	break;
	
	case PAGE_LAN:
	restore_param(CurrentLine,TYPE_STRING,(int16*)&lan[UNSAVED].hostname);
	restore_param(CurrentLine,TYPE_IP,0,&lan[UNSAVED].ip_addr);
	restore_param(CurrentLine,TYPE_IP,0,&lan[UNSAVED].netmask);
	restore_param(CurrentLine,TYPE_IP,0,&lan[UNSAVED].subnet);
	restore_param(CurrentLine,TYPE_IP,0,&lan[UNSAVED].gateway);
	restore_param(CurrentLine,TYPE_INT,0,&temp32);
	if(temp32 > 30)
		// 30sec lower limit !
		lan[UNSAVED].tcp_idletimer = temp32*1000;	

	restore_param(CurrentLine,TYPE_INT,0,&temp32);
	if(temp32 <= 2)
		{
		// Update LEDS now
		lan[UNSAVED].led_mode = temp32;
		if(temp32 == LEDS_ON)
			plan->configure_leds(LEDS_ON);
		else
			// LEDS_OFF or LEDS_ON_AT_STARTUP
			plan->configure_leds(LEDS_OFF);
		}

	lan[UNSAVED].subnet = lan[UNSAVED].ip_addr & lan[UNSAVED].netmask;
#ifdef SWITCH_FIX
	restore_param(CurrentLine,TYPE_BOOL,&temp16);
	lan[UNSAVED].idle_reset = (bool)temp16;
#endif
#ifdef TFTP
	restore_param(CurrentLine,TYPE_BOOL,&temp16);
	lan[UNSAVED].permitTFTP = (bool)temp16;
#endif
	break;

	case PAGE_SET_PASSWD:
	// New password and id entered
	traceout.suppress = true;
	restore_param(CurrentLine,TYPE_STRING,(int16*)tmpstr1);
	restore_param(CurrentLine,TYPE_STRING,(int16*)macstr);
	restore_param(CurrentLine,TYPE_STRING,(int16*)baudstr);
	traceout.suppress = false;

	if(! strcomp(macstr,baudstr))
		// Passwords don't match
		return 0;
		
//	sprintf(msg,"username=>%s<= password =>%s<= entered",tmpstr1,macstr);
//	OutputDebugString(LOG_INFO,LOG_HTML,(char*)msg);
	
	if( ((strcomp(lan[ACTIVE].username,tmpstr1)) &&
		(strcomp(lan[ACTIVE].password,baudstr)) ))
		// Same as saved settings, don't set flag to save
		break;
		
	// New password is accceptable
	strcpy((char*)lan[UNSAVED].username,(const char*)tmpstr1);
	strcpy((char*)lan[UNSAVED].password,(const char*)baudstr);
	dirty=true;
	break;

	case PAGE_LOADSAVECONFIG:
	restore_param(CurrentLine,TYPE_STRING,(int16*)ipstr);		// Profile
	restore_param(CurrentLine,TYPE_STRING,(int16*)macstr);		// New profile name
	break;
	
	case PAGE_ENTER_PASSWD:
	// Verify password and userid against stored values
	traceout.suppress = true;
	restore_param(CurrentLine,TYPE_STRING,(int16*)tmpstr1);
	restore_param(CurrentLine,TYPE_STRING,(int16*)macstr);
	traceout.suppress = false;

	if(0 != strcmp((char*)lan[ACTIVE].username,(char*)tmpstr1))
		return 0;
	if(!strcomp(lan[ACTIVE].password,macstr))
		return 0;
	// Fall thru to return no change
	}//endcase

return 1;	// No change
}	

//-------------------------------------------------------------------
bool Chttp::strcomp(unsigned char* s1,unsigned char* s2)
// If the strings are not identical, returns false
{
int len1, len2,i;
len1 = strlen((char*)s1);
len2 = strlen((char*)s2);
if( len1 != len2 )
	{
	return false;
	}

for(i=0; i< len1;i++)
	{
	if( s1[i] != s2[i] )
		return false;
	}
	
return true;
}

//--------------------------------------------------------------------------
int Chttp::restore_param(unsigned char* CurrentLine,int mode,int16* param16,int32* param32)
// Parse the HTML string for a parameter.
// & is parameter end delimiter
// Tn= is parameter start delimiter
// Returns 0 if successful, else -1 
{
int retval = 0;
//*param = 0L;							// WARNING - CAUSES A CRASH !!Default fail value
unsigned char *pCh;

sprintf(sForm_Num,szHTTPField,form_num++);	// Generate field start string "Tn="
	
unsigned char* cPos = (unsigned char*)strstr((char*)CurrentLine,(char*)sForm_Num);
if(cPos == NULL )
	{
	// Checkboxes don't report a value if unchecked !
	if(mode == TYPE_BOOL)
		{
		*param16 = 0;
		sprintf(msg,szHTTPFormErrBool,sForm_Num);
		}
	else
		sprintf(msg,szHTTPFormErrString,sForm_Num);
		
	OutputDebugString(LOG_INFO,LOG_HTML,(const char*)msg);
	return(-1);
	}
pCh = cPos + ( strlen((const char*)sForm_Num));		// pCh Points at start of data field

// End of data field is a '&' character
cPos = (unsigned char*)strchr((char*)pCh,'&');
int fieldlen = (int) ( cPos-pCh );							// The length of the field we are looking for
if(cPos == NULL)
	{
	// End of field char not found. This happens if it is the last field in the form
	// Calculate field length using string null termination char as field end instead
	cPos = (unsigned char*)strchr((char*)pCh,'\0');
	fieldlen = (int) ( cPos-pCh );							// The length of the field we are looking for
	}

if(fieldlen > MAXLEN1)
	{
	// Field too long.
	sprintf(msg,szHTTPFieldLength,sForm_Num,fieldlen);
	OutputDebugString(LOG_INFO,LOG_HTML,(const char*)msg);
	return -1;
	}

// Make a copy of the data field for parsing..
memcpy(packet_buf,pCh,fieldlen);
pCh = packet_buf;
*(pCh + fieldlen) = NULL;

char endptr;
char *pendptr=&endptr;

switch(mode)
	{
	case TYPE_INT:				// Listboxes return an int value
	if(*pCh != 0 )
		{
		*param32 = strtoul((const char*)packet_buf, &pendptr, 10);
		if(*param32 == 0L)
			{
			sprintf(msg,szHTTPFormErrINT,sForm_Num,packet_buf);
			retval = -1;		// strtoul failed
			}
		}
	sprintf(msg,szHTTPFormINT,sForm_Num,*param32);
	break;

	case TYPE_IP:
	// Returns an int32
	*param32 = ascii_2ip(pCh);
	if(*param32 == 0xFFFFFFFFL)
		{
		sprintf(msg,szHTTPFormErrIP,sForm_Num,packet_buf);
		retval = -1;
		}

	sprintf(msg,szHTTPFormIP,sForm_Num,ip_2ascii(*param32));
	break;

	case TYPE_STRING:
	// Nb Browsers send '+' instead of ' ' and '%HH' for some chars too. sprintg() reformats the string
	sprintg((char*)pCh);
	strcpy((char*)param16,(const char*)packet_buf);
	sprintf(msg,szHTTPFormString,sForm_Num,param16);
	break;
	
	case TYPE_BOOL:
	// Checkbox id exists, so it must be ticked
	*param16 = 1;
	sprintf(msg,szHTTPFormBool,sForm_Num);
	break;
	
	default:
	sprintf(msg,szHTTPFormParamErr,sForm_Num);	
	retval = -1;
	}

OutputDebugString(LOG_INFO,LOG_HTML,(const char*)msg);
return retval;
}

//--------------------------------------------------------------------------
int Chttp::ParseReqText(unsigned char* CurrentLine,unsigned char* find_text,int mode,int32* param)
// Return the value following find_text
// Entry CurrentLine must be NULL terminated
// -1 = fail, 0=pass
{
char *ch;
char templine[50];			// Temp storage
memset(templine,0,50);

char* cPos = strstr((char*)CurrentLine,(char*)find_text);
if(cPos == NULL )
	return(-1);

ch = cPos + strlen((const char*)find_text);		// ch Points at start of data field

// End of string data field is a '&' character
if(mode != TYPE_INT)
	{	
	cPos = strchr((char*)ch,'&');
	if((cPos == NULL) || ((ch - cPos ) > 50))
		return (-1);
	}
else
	{	
	cPos = strchr((char*)ch,'=');
	if((cPos == NULL) || ((ch - cPos ) > 50))
		return (-1);
	}
// cPos now points at end of data field + 1

// Make a copy of the data field for parsing..
memcpy(templine,ch,min(50,cPos-ch));
templine[cPos-ch]=NULL;
ch = templine;
*param = 0;			// Default fail value
char endptr;
char *pendptr=&endptr;

switch(mode)
	{
	case TYPE_INT:
	if(*ch != '0' )
		{
		*param = strtoul((const char*)templine, &pendptr, 10);
		if(*param == 0)
			break;		// strtoul failed

		return (0);
		}
	break;

	case TYPE_IP:
	*param = ascii_2ip((unsigned char*)ch);
	if(*param != 0xFFFFFFFFL)
		return (0);
	break;

	case TYPE_STRING:
	strcpy((char*)param,(const char*)ch);
	return (0);
	}

sprintf(msg,szInvalidForm);
OutputDebugString(LOG_INFO,LOG_HTML,(const char*)msg);
return (-1);
}

int Chttp::ParseReq(unsigned char* CurrentLine,unsigned int len)
{
int keylen;

int m_nMethod = METHOD_UNSUPPORTED;
// Parse current line for a method (command)from the method table
for(int i =0; i < MethodTableLen; i++)
	{
	keylen = (unsigned int)strlen((const char*)MethodTable[i].key);
	if(len >= keylen)
		if(0== strncmp((char*)CurrentLine, MethodTable[i].key,keylen ))
			{
			// Trim the method out of the command line
			m_nMethod = MethodTable[i].id;
			TrimLeft(CurrentLine,keylen);
			len = (unsigned int)strlen((const char*)CurrentLine);
			break;
			}
	}

return m_nMethod;
}

//--------------------------------------------------------------------------
unsigned char* Chttp::TrimLeft(unsigned char* CurrentLine,int keylen)
// Remove "keylen" chars from left of line
// Then remove whitespace characters from the left of the string.
// MUST BE NULL TERMINATED !!
{
unsigned int len = (unsigned int)strlen((const char*)CurrentLine);
char ch;
int whitespace_chars = 0;
int pos=0;

// Trim out the leftmost "KEYLEN" chars first
keylen = (keylen > len)? len : keylen;
if(keylen > 0)
	{
	len=strlen((const char*)CurrentLine) - keylen;
	memmove(CurrentLine,&CurrentLine[keylen],len);
	}
	
// Count leading whitespace chars
while(pos < len)
	{
	ch = CurrentLine[pos++];
	if((ch == ' '))
		whitespace_chars++;
	else pos=len;
	};

// Trim out leading whitespace
if(whitespace_chars > 0)
	{
	len=strlen((const char*)CurrentLine) - whitespace_chars;
	memmove(CurrentLine,&CurrentLine[whitespace_chars],len);
	}
	
return CurrentLine;
}
