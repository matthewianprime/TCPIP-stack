// icmp.h

// ICMP type field values
#define ICMP_ECHO_REPLY			0
#define ICMP_DEST_UNREACHABLE	3
#define ICMP_ECHO_REQUEST		8
#define ICMP_ROUTER_ADVERT		9
#define ICMP_ROUTERSELECTION	10
#define ICMP_TTL				11
#define ICMP_PARAM_PROBLEM		12

// ICMP type 3 code field values
#define HOST_UNREACHABLE		0	// LAN or RAS host unavail
#define NET_UNREACHABLE			1	// Can't route to network
#define PROTOCOL_UNREACHABLE	2	// Non-ip protocol, or a version we don't understand.
#define DEST_HOST_UNKNOWN		7	// Can't route to host.

#define ICMP_HDRLEN				8

class Cicmp : public CProtocol_L3
{
public:
// Base class overrides
void init(int);
unsigned int input(void *datagram,int len,int32 ip_src_addr,int32 ip_dest_addr,int unit);
void output(unsigned char* data, int type, char code, int32 ip_addr,int request_id,bool retransmission=false);
void OnMessage(int16 messg,int32 lParam,int32 wParam,int16 zParam=0);

private:
unsigned int icmp_seq_num;
int32 ip_id;
};

