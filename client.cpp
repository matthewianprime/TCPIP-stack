// protocols.cpp
// Base class definitions used by LCP, PAP, CHAP etc
// Uset to define protocol entry points
// Abstraction of the protent structure used in BSD
#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "utils.h"

CClient::CClient(){}
CClient::~CClient(){}
void CClient::init(int,CProtocol_L3* pTransport,bool bAutoDelete){}
// Serial data in
int CClient::serial_receive(unsigned char **data,unsigned int *len,unsigned int window){return 0;}
// LAN data in
int CClient::receive(unsigned char **data,unsigned int *len,unsigned int window){return 0;}
// Transport (TCP/UDP) calls this func when a state change happens
int CClient::OnTransport(int,int){return 1;}
// Report available transmission buffer space
int16 CClient::GetTransmitBuffer(){return (int16)UART_BUFLEN;}
// Report available transmission buffer space
int16 CClient::GetReceiveBuffer(){return (int16)UART_BUFLEN;}
