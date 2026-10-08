// ip.h
class CipV4 : public CProtocol
{
public:
void init(int);
void input(int,unsigned char *p,int);
int CreateTransport(char protocol,int,int client_port=0,CClient* ptrClient=NULL,int32 their_ip=0);
int select_interface(int32 dest_addr);

private:
int route(unsigned char *in_buf, int16 len, int);
int demand_if; // Interface number for demand dial
int16 ip_id;
};

