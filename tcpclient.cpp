// TCPClient.cpp
// Base class for TCP applications eg Telnet
#include "object.h"
#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "string_flash.h"

int CTcpClient::send(unsigned char *data,unsigned int *len){return 0;}

// Receive an asynchronous reply to a request
//void CTcpClient::async_reply(int reply_protocol,unsigned int seq_num){}

void CClient::init(int,CProtocol_L3* pTransport,bool bAutoDelete){}
int CClient::serial_receive(unsigned char *data,unsigned int *len,unsigned int window){return 0;}
int CClient::receive(unsigned char **data,unsigned int *len,unsigned int window){return 0;}
int CClient::OnTransport(int,int){return ACKNOWLEDGE;}
int CClient::GetBuffer(){return 0;}



