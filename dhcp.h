// dhcp.h

// DHCP state machine states
#define DHCP_INIT		1
#define DHCP_SELECTING	2
#define DHCP_REQUESTING	3
#define DHCP_BOUND		4
#define DHCP_RENEWING	5
#define DHCP_REBINDING	6
#define DHCP_REBOOTING	7
#define DHCP_REBOOT		8

// DHCP frame types
#define DHCP_DISCOVER	1
#define DHCP_OFFER		2
#define DHCP_REQUEST	3
#define DHCP_DECLINE	4
#define DHCP_ACK		5
#define DHCP_NAK		6
#define DHCP_RELEASE	7

// DHCP_ADDRESS_LEASE->status constants
#define AVAIL			0
#define OFFERED			1
#define TIMEDOUT		2	// Keep as long as possible incase original client returns
#define LEASED			3
#define DECLINED		4	// Quarantined ip. Client detected duplicate. Inform administrator

#define UDP_DHCP_LISTEN	67
#define UDP_DHCP_CLIENT	68

#define DHCP_OPTIONSFIELD		236
#define DHCP_OPTIONSLEN			128
#define DHCP_MAGIC_COOKIE_LEN	4
#define DHCP_CHADDR_FIELD		28

class Cdhcp : public CClient
{
private:
int32 dest_addr;
int32 pool_get_ip_address(int32 ip_addr,int16* mac_addr,int newstatus);
bool pool_release_ip_address(int32 ip_addr,int16* mac_addr,int newstatus=AVAIL);
int32 T1_renewal;
int32 T2_rebinding;
int32 T3_duration;

public:
void init(int,CProtocol_L3*,bool);
int receive(unsigned char **rxdata,unsigned int *len,unsigned int window=TCP_MAX_WINDOW);
void addresspooltimeout(int32 currenttime);
void OnMessage(int16 messg,int32 lParam,int32 wParam,int16 zParam);
int16 client_state;
};
