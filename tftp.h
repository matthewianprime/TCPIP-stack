// tftp.h rfc783

// tftp opcodes
#define TFTP_RRQ					1
#define TFTP_WRQ					2
#define TFTP_DATA					3
#define TFTP_ACK					4
#define TFTP_ERROR					5

//tftp error codes
#define ERR_UNDEFINED				0
#define ERR_FILE_NOT_FOUND			1
#define ERR_ACCESS_VIOLATION		2
#define ERR_DISK_FULL				3
#define ERR_ILLEGAL_OPERATION		4
#define ERR_UNKNOWN_ID				5
#define ERR_FILE_EXISTS				6
#define ERR_NOSUCH_USER				7

#define SOFTWARE_UPLOAD				1
#define CONFIG_UPLOAD				2

// TFTP transfer modes
#define MODE_NETASCII				0
#define MODE_OCTET					1
#define MODE_MAIL					2

#define TFTP_MAX_BLOCKSIZE			512


class Ctftp : public CClient
{
public:
int16 port;
int16 protocol;
int16 blocknumber;
void init(int,CProtocol_L3*,bool);
int receive(unsigned char **udp_data,unsigned int *len,unsigned int window=1460);
int OnTransport(int message,int unused);

private:
unsigned char pFilename[20];
unsigned char pMode[20];
bool bSendingFile;
int mode;						// octet,netascii,mail etc
bool bSoftwareUpdate;
bool bBootSoftwareUpdate;
};


