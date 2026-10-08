// auto_ip.cpp
// UDP based ip address assignment protocol, proprietary.
// Works in conjunction with "DSP discover" windows software.

// In response to text "DSP DISCOVER" on UDP port 5050, responds with IP settings on port 5051
// If "DSP SETPARAMS" received on UDP port 5050, adopts new ip config.

#include "router.h"
#include "pppd.h"
#include "ffs.h"
#include "utils.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

#include "protocols.h"
#include "tcp.h"
#include "udp.h"
#include "auto_ip.h"
#include "arp.h"

//--------------------------------------------------------------------------
void Cauto_ip::init(int,CProtocol_L3* pUDP,bool bAutoDelete)
// One time init, the DHCP instance is static
{
name = szAuto_ip;
outbuf = &packet_buf[TCP_DATA_OFFSET];
transport = pUDP;	// TODO this should be ip_protocols[1] I think
autodelete = bAutoDelete;				// Delete instance when quit flag is set
quit = false;
}

//--------------------------------------------------------------------------
int Cauto_ip::receive(unsigned char **rxdata,unsigned int *len,unsigned int flags)
// Check the message contains text "DSP DISCOVER"
// If it does, respond with basic configuration settings
{
char *data = (char*)*rxdata;
char *p = (char*)data;
int rc = NORESPONSE;
int msglen;
int32 ip,mask,gate;
int16 *pMac;

//sprintf(msg,"AIP Rx %u",*len);
//OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(char*)msg);
//printformatstring(msg,MSG_BUFLEN,p,*len,true,false);
//OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,msg,true,false);
data = (char*)&packet_buf[UDP_DATA_OFFSET];

if(0 == strcmp(p,szDSP_DISCOVER))
	{
	// Respond to the request for our settings
//	OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,"AIP Rx DISCOVER");
	p = data;
	strcpy(p,szSEB);
	INCPTR(strlen(szSEB)+1,p);
	PUTLONG(lan[ACTIVE].ip_addr,p);
	PUTLONG(lan[ACTIVE].netmask,p);
	PUTLONG(lan[ACTIVE].gateway,p)
	PUTMAC(mac_addr,p);
	strcpy(p,(char*)lan[ACTIVE].hostname);
	INCPTR(strlen((char*)lan[ACTIVE].hostname)+1,p);
	*len = (int)(p-data);

	*rxdata = &packet_buf[UDP_DATA_OFFSET];
	transport->port_remote = 5051;
	// There is no cache entry for the DSP Discover host, broadcast the response
	transport->dest_addr = IP_ADDR_BROADCAST;
	rc = ACKNOWLEDGE;
	}
else if(0 == strcmp(p,szDSP_SETPARAMS))
	{
	// Accept new parameters
	msglen = strlen(p);	// Parse over SETPARAMS
	INCPTR(msglen+1,p);
	
	msglen = strlen(p);	// Parse over MAC
	pMac = ascii_2mac((unsigned char*)p);
	if(!( (pMac[0] == mac_addr[0] ) &&
		(pMac[1] == mac_addr[1] ) &&
		(pMac[2] == mac_addr[2] ) ))
			// This SETPARAMS frame is not for us
			return rc;

	INCPTR(msglen+1,p);
			
	GETLONG(ip,p);
	GETLONG(mask,p);
	GETLONG(gate,p);
	
	sprintf(msg,"AIP Rx SETPARAMS\r\nIP addr\t%0a\r\nNetmask\t%0a\r\nGateway\t%0a\r\nName\t%s\r\n",ip,mask,gate,p);
	OutputDebugString(LOG_DEBUG,LOG_TCPMODEM,(char*)msg);

	if( ip != 0)
		// Assigning a fixed ip address
		// This setting ensures it is saved to
		// flash as opposed to the 0.0.0.0 DHCP setting
		lan[UNSAVED].dhcp_server = 0;

	lan[UNSAVED].ip_addr = ip;
	lan[UNSAVED].netmask = mask;
	lan[UNSAVED].gateway = gate;
	lan[UNSAVED].subnet = lan[UNSAVED].netmask & lan[UNSAVED].ip_addr;
	if(strlen(p) < 20)
		strcpy((char*)lan[UNSAVED].hostname,p);
	// Save LAN settings to flash if they have changed
	ffs->ffsReset();
	ffs->del((unsigned char*)szLAN,szFILETYPE_SYSTEM,true);
	if(ffs->open(MODE_CREATE,szLAN,szFILETYPE_SYSTEM))
		{
		ffs->save_to_flash(SAVE_RESTORE_LAN);
		ffs->close();
		dirty = false;
		auto_ip_assigned_address = 1;
		}

	p = data;
	*len = 0;
	}

return rc;
}
