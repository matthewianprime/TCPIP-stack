// http.h

// HTML buttons
enum HTML_BUTTONS{
	BUTTON_CANCEL	=1,
	BUTTON_OK,
	BUTTON_INSERT,
	BUTTON_DELETE,
	BUTTON_SAVE,
	BUTTON_SAVE2,
	BUTTON_SAVE3,
	BUTTON_DISCARD,
	BUTTON_LOAD,
	BUTTON_STARTUP
	};

enum DATA_TYPE
	{
	TYPE_BOOL		= 1,		// Checkbox return
	TYPE_INT,					// Droplist integer return
	TYPE_IP,					// IP entered as text
	TYPE_STRING,				// String entered as text
	TYPE_PASSWORD				// String entered as text, display stars not chars entered
	};

enum LIST_STYLES
	{
	LIST_DROPLIST_LAN	=1,
	LIST_DROPLIST_BAUD,
	LIST_DROPLIST_FLOW,
	LIST_DROPLIST_FORMAT,
	LIST_DROPLIST_SHELL,
	LIST_DROPLIST_PROFILES,
	LIST_ROUTINGTABLE,
	LIST_INTERFACES,
	LIST_DROPLIST_LEDS
	};
	
//#define MAXLEN_DESCRIPTION	30		// Description text in web forms page
#define DELTA_XPOS			38			// X-distabce between boxes on webpage

class Chttp : public CClient
{
public:

void init(int,CProtocol_L3*,bool);
int receive(unsigned char **data,unsigned int *len,unsigned int window);
int serial_receive(unsigned char **data,unsigned int *len,unsigned int window);

enum PAGE_ID
	{
	PAGE_UNSUPPORTED = 0,
	PAGE_LAN,
	PAGE_SERIAL,
	PAGE_PPP_WAN,
	PAGE_ROUTING,
#ifndef SEB
	PAGE_RAS,
#endif
#ifdef SMTP
	PAGE_SENDMAIL,
#endif
	PAGE_LOADSAVECONFIG,
	PAGE_SET_PASSWD,
	PAGE_INDEX,
	PAGE_CALLBAR,
	PAGE_ENTER_PASSWD,
	PAGE_DHCP
	}m_nPage;
	
struct _tagHttpTable
	{
	enum PAGE_ID	page;
	const char		*button;
	const char		*title;
	};
	
int MethodTableLen;
enum METHOD_TYPE
	{
	METHOD_UNSUPPORTED = 0,
	METHOD_GET,
	METHOD_POST,
	METHOD_HEAD,
	METHOD_PUT,
	METHOD_DELETE
	}m_nMethod;

struct _tagMethodTable
	{
	enum METHOD_TYPE	id;
	const char			*key;
	};

private:
bool echo;			// Terminal echo
int ParseReq(unsigned char* CurrentLine,unsigned int len);
unsigned char* TrimLeft(unsigned char* CurrentLine,int len);
void AddHTML_list_option(const char* title,int value,int selected=-1);
int ParseReqText(unsigned char* data,unsigned char*,int,int32*);
int CreateHTML_ender(unsigned char* fn,unsigned int window);
int CreateHTML_form(unsigned char* data=NULL,const char* description=NULL,int select = 0);
int CreateHTML_header(unsigned char* fn,unsigned int window);
int CreateHTML_button(int name,const char* szTitle=NULL,int paragraph=0);
int CreateHTML_list(const char* title,int select,int16 selected=0xffff);

int restore_settings(unsigned char* CurrentLine,int page);
int restore_param(unsigned char* CurrentLine,int mode,int16* param16,int32 *param32=NULL);
void serialise(int32 data,const char* description,int type=TYPE_INT);
void CreatePage(int);
void flashwrite(int data);

bool strcomp(unsigned char* s1,unsigned char* s2);

int32 ip_addr;
int32 ip_mask;
int32 gateway;
int interface_unit;

char *pHTML;					// Points to the web page we're constructing
int form_num;
int HTML_len;					// Length of web page
unsigned char sForm_Num[5];		// Form number text, "Tn"
int auth_ok;
int m_nLastMethod;
};


