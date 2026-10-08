// startup.h

#define INIT_LAN	0	// Initialise LAN only
#define INIT_ALL	1	// Initialise all

class CInit : public CObj
{
public:
int init(int mode);
bool ArpFor(int32 dest_ip,int32 waittime);
};

