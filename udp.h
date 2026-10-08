// udp.h
#define UDP_HEADERLEN		8
#define UDP_PSEUDOHEADERLEN	12

#define UDP_TFTP			69
#define UDP_DHCP_CLIENT		68		// DHCP client port
#define UDP_DHCP_LISTEN		67		// DHCP server listener port

//#define UDP_DATA	200			// Position of UDP client data in packet_buf. TODO optimise to avoid memcpy in udp::output

/*
class CudpClient: public CClient
{
public:
int16 port;
int16 protocol;
int16 block;
};
*/

class Cudp : public CProtocol_L3
{
public:
void init(int);
unsigned int input(void *datagram,int len,int32 ip_src_addr,int32 ip_dest_addr,int unit);
void output(unsigned char* udp_data, int udp_len, char, int32,int unit,bool retransmission=false);
CClient* CreateClient(int16 portnumber);
void close();
void output_dhcp(int frametype,int32 server_ip_addr=0xffffffff,int32 our_ip_addr=0,char* Param_Req_List=NULL);		// DHCP client frame send proc
void OnMessage(int16 messg, int32 lParam,int32 wParam,int16);
};

