// auto_ip.h

class Cauto_ip : public CClient
{
private:

public:
void init(int,CProtocol_L3*,bool);
int receive(unsigned char **rxdata,unsigned int *len,unsigned int flags);
};
