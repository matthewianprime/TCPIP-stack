// telnet.h

class CTelnet : public CClient
{
public:
void init(int,CProtocol_L3*,bool);
int receive(unsigned char **rxdata,unsigned int *plen,unsigned int window);
int serial_receive(unsigned char **data,unsigned int *len,unsigned int window);
int OnTransport(int message,int unused);
int16 GetTransmitBuffer();

private:
int IF_NUM;							// The serial port we are using
bool echo;							// Command echo
bool trace;							// Initiated as a trace port flag
};




