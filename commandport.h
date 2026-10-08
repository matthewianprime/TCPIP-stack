// Commandport.h
	
#define COMMANDLINE_BUFLEN	40
#define PARAM_NUMERIC 100

#define PASSWORD_AND_USERNAME_REQUIRED	0
#define USERNAME_ACCEPTED				1
#define PASSWORD_AND_USERNAME_ACCEPTED	2

class CCommandPort : public CClient
{
public:
~CCommandPort();
void init(int,CProtocol_L3*,bool);
int receive(unsigned char**,unsigned int *len,unsigned int window);
int serial_receive(unsigned char **data,unsigned int *len,unsigned int window=1460);
int OnTransport(int,int);						// Overrides CClient function
int16 GetTransmitBuffer();
void OnMessage(int16 messg,int32 lParam,int32 wParam,int16 zParam=0);

int use_scratchpad;
int ParamTableLen;
int MethodTableLen;

private:
int echo;
bool DoCommandLine(unsigned char* CurrentLine,unsigned int len);
int ParseMethod(unsigned char* CurrentLine,unsigned int len);
int ParseParam(unsigned char* CurrentLine,unsigned int len);
int getparam(unsigned char* CurrentLine,unsigned char* ip_addr);
int prompt;
unsigned char sPrompt[MAXLEN1];
char m_nNumber;									// Store for number param on command line
bool pinging;
unsigned char inbuf[COMMANDLINE_BUFLEN+1];		// Command parsing buffer for input TCP data
unsigned int inbuf_in;							// Command line buffer in pointer
};
