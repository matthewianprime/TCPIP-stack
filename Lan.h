// lan.h
// Header for LAN interface functions
//#define LED_COL		0
//#define LED_LINK	0x10
//#define LED_RXTX	0
//define LED_CRS				0x20
// Values tie in with HTTP drop list
#define LEDS_OFF			0x00
#define LEDS_ON				0x01
#define LEDS_ON_AT_STARTUP	0x02

class Clan
{
public:
Clan();
~Clan();

int init();
void input_lan(char *input_buf,int *len);
bool output_lan(unsigned char* outp_buf, int len,int protocol,int16 *src_mac, int16 *dest_mac, int32 dest_ip_addr);
bool link_test(unsigned char* str,bool advice=false);
void configure_leds(int config);
int16 framesreceived;
int16 framessent;
int16 framesdiscarded;
int16 frameoverruns;
int16 ring_overflows;
int32 idletimer_ms;

private:
int NICtoPC(int16 buf_page,char *inp_buf,int len);
void PCtoNIC(char *outp_buf,unsigned int bytecount);
void ring_overflow_recovery();
int frame_in_queue;

//8019 BUFFER values.
int TRANSMITBUFFER;			//
int RECEIVEPAGESTART;		// The lowest value that works 8019
int RECEIVEPAGEEND;			//
int RECEIVEPAGELENGTH;
};

