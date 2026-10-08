// Carp.h

class Carp : public CProtocol_L3
{
private:

public:
void init(int unit);
bool input_arp(unsigned char*,int len,int16 ip_id,int32 ip_src_addr,int unit);
int16* isincache(int32 ip_addr,int16* mac_addr,int* cache_location = NULL,bool update_time=true);
void addtocache(int32 ip_addr,int16 *mac_addr,int type=ARP_DYNAMIC,bool checkexist=true);
void output_arp(int32 ip_addr,int16 *target_mac_addr = NULL,int operation = 1,int32 our_ip = 0,int16 *our_mac_addr=NULL);
void arpcachetimeout();
void OnMessage(int16,int32,int32,int16);
};

