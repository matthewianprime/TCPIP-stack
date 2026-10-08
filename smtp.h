// smtp.h

#define MAXMAILLEN		30	// Max length of an email address

// SMTP state definitions
enum SMTP_STATE
	{
	SMTP_RESET = 0,
	SMTP_LISTEN,			// Server only
	SMTP_WAIT_USERNAME,		// Server only
	SMTP_WAIT_RCPT,			// Server only
	SMTP_WAIT_DATA,			// Server only
	SMTP_RX_DATA,			// Server only. Receiving mail data
	SMTP_TX_DATA,			// Server only. Receiving mail data
	SMTP_WAIT_ESTABLISH,	// Client. Waiting for transport to open
	SMTP_INIT_SENDMAIL,		// Client
	SMTP_SEND_HELO,			// Client. Sent HELO, awaiting HELO
	SMTP_SENT_HELO,			// Client. Sent HELO, awaiting HELO
	SMTP_SENT_MAILFROM,		// Client. Sent MAILFROM, awaiting 250 OK
	SMTP_SENT_MAILTO,		// Client. Sent MAILTO, awaiting 250 OK
	SMTP_SENT_RCPT_TO,		// Client. Sent RCPT To, awaiting 250 OK
	SMTP_SENT_DATA,			// Client. Sent DATA, awaiting 354
	SMTP_SENDING_MAIL,		// Client. Sending mail
	SMTP_SENT_MAIL,			// Client. Sent mail, awaiting 250 OK
	SMTP_SENT_QUIT,			// Client
	SMTP_WAIT_QUIT,			// Server only.
	SMTP_IDLE
	};


class Csmtp : public CClient
{
public:
void init(int,CProtocol_L3*,bool);
int receive(unsigned char **rxdata,unsigned int *plen,unsigned int window);
int serial_receive(unsigned char **data,unsigned int *len,unsigned int window);
int OnTransport(int message,int unused);
int16 GetTransmitBuffer();
int SendMail(char* dest_email_addr,char* mail_subject,char *mail_body);
int Init_SendMail(char* dest_email_addr,char* mail_subject,char *mail_body);
void OnMessage(int16 msg,int32 lParam,int32 wParam,int16 zParam=0);

enum METHOD_TYPE
	{
	METHOD_UNSUPPORTED = 0,
	METHOD_HELO,
	METHOD_MAILFROM,
	METHOD_RCPTTO,
	METHOD_MSGID,
	METHOD_DATA,
	METHOD_QUIT,
	METHOD_RESET,
	METHOD_NOOP,
	METHOD_MSGSTARTEND,
	METHOD_220,
	METHOD_250,
	METHOD_251,
	METHOD_450,
	METHOD_550,
	METHOD_451,
	METHOD_551,
	METHOD_552,
	METHOD_553,
	METHOD_354,
	METHOD_554
	}m_nMethod;

struct _tagMethodTable
	{
	enum METHOD_TYPE	id;
	const char			*key;
	};

int smtp_state;
private:
METHOD_TYPE ParseInput(unsigned char* CurrentLine,unsigned int len);
int MethodTableLen;

char mailfrom[MAXMAILLEN];
int32 SMTP_ip_addr;						// Outgoing SMTP server IP address
};




