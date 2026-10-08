// Clan.cpp
// Implements LAN read / write/ status operations for RTL8019 chip

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "lan.h"
#include "arp.h"
#include "utils.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

Clan::Clan(){}
Clan::~Clan(){}

//***********************Equates for NIC Registers (NE2000 compatible)***
#define COMMAND				0 //0xD000
// Page 0 Write regs
#define PAGESTART			COMMAND + 01
#define PAGESTOP			COMMAND + 02
#define BOUNDARY			COMMAND + 03
#define TRANSMITPAGE		COMMAND + 04
#define TRANSMITBYTECOUNT0	COMMAND + 05
#define TRANSMITBYTECOUNT1	COMMAND + 06
#define INTERRUPTSTATUS		COMMAND + 07
#define REMOTESTARTADDRESS0	COMMAND + 0x8
#define REMOTESTARTADDRESS1	COMMAND + 0x9
#define REMOTEBYTECOUNT0	COMMAND + 0xa
#define REMOTEBYTECOUNT1	COMMAND + 0xb
#define RECEIVECONFIGURATION	COMMAND + 0xc
#define TRANSMITCONFIGURATION	COMMAND + 0xd
#define DATACONFIGURATION	COMMAND + 0xe
#define INTERRUPTMASK		COMMAND + 0xf

// Page 0 Read regs
#define TRANSMITSTATUS		COMMAND + 04
#define NCR					COMMAND + 05
#define CRDMA0				COMMAND + 0x8
#define CRDMA1				COMMAND + 0x9
#define L8019ID0			COMMAND + 0x0a		// Always 'P'
#define L8019ID1			COMMAND + 0x0b		// Always 'p'
#define RECEIVESTATUS		COMMAND + 0xc
#define FAE_TALLY			COMMAND + 0xd
#define CRC_TALLY			COMMAND + 0xe
#define MISS_PKT_TALLY		COMMAND + 0xf

// All pages remote DMA port
#define IOPORT				COMMAND + 0x10

// Page 1
#define PAR0				COMMAND + 0x1
#define CURRENT				COMMAND + 0x7
#define MAR0				COMMAND + 0x8

// Page 3
#define L9346CR				COMMAND + 0x1
#define CONFIG0				COMMAND + 0x3
#define CONFIG1				COMMAND + 0x4
#define CONFIG2				COMMAND + 0x5
#define CONFIG3				COMMAND + 0x6
#define TEST				COMMAND + 0x7

//-----------------------------------------------------------------
int Clan::init()
{
//***********************************************************************
//DriverInitialize
// Initializes the 8019 NIC for a typical network system.
// Receive Buffer Ring 4 2600h to 4000h
// Transmit Buffer 4 2000h to 2600h
//
//***********************************************************************
int n=0;
int i=0;
int rcr = '\x04'; // NO ERRORED FRAMES. D4 = Promiscuous
int tcr = '\xe0'; // value for trans. config. reg
int dcr = '\xD8'; // D0=0-> Byte. D0=1->word.				// value for data config. reg. Word transfers
int imr = '\x00'; // value for intr. mask reg

idletimer_ms=getTickCount();
framesdiscarded=0;
framessent=0;
framesreceived=0;
frameoverruns=0;
ring_overflows=0;

// 8019 BUFFER values.
TRANSMITBUFFER		=0x59;		// 5900h (Nb 60 causes corruption of Rx Frame. Memory shadow ? )
RECEIVEPAGESTART	=0x40;		// 4000h The lowest value that works
RECEIVEPAGEEND		=0x58;		// 5800h
RECEIVEPAGELENGTH	=RECEIVEPAGEEND - RECEIVEPAGESTART;

delay_ms(20);
hw->host_init();											// Init the DSP i/o bus

// A delay...
// Without it, SEB can crash, and the MAC addr regs were not written properly !
delay_ms(300);

/*
// Check the LAN chip
int a,b,c;
hw->host_byte_read(&a,REMOTEBYTECOUNT0);
hw->host_byte_read(&b,REMOTEBYTECOUNT1);
hw->host_byte_read(&c,COMMAND);
serialout((char*)"LAN Chip registers:\r\n",HW_DTE0);
sprintf(msg,"REMOTEBYTECOUNT0=0x%X (Expect 0x50)\r\nREMOTEBYTECOUNT1=0x%X (Expect 0x70)\r\nCOMMAND=0x%X (Expect 0x21)\r\n",a,b,c);
serialout((char*)msg,HW_DTE0);
*/

int data = 0x21;											// Page 0
hw->host_byte_write(&data,COMMAND);
hw->host_byte_write(&dcr,DATACONFIGURATION);
data =0;
hw->host_byte_write(&data,REMOTEBYTECOUNT0);
hw->host_byte_write(&data,REMOTEBYTECOUNT1);
hw->host_byte_write(&rcr,RECEIVECONFIGURATION);
data =0x02;
hw->host_byte_write(&data,TRANSMITCONFIGURATION);			// Loopback mode

data =TRANSMITBUFFER;
hw->host_byte_write(&data,TRANSMITPAGE);

// Allocate on chip RAM for receive circular buffer frame
// Nb PAGESTART, CURRENT and BOUNDARY must be initialised to the same value
hw->host_byte_write(&RECEIVEPAGESTART,PAGESTART);			// Static value. Circ buffer begin
hw->host_byte_write(&RECEIVEPAGEEND,PAGESTOP);				// Static value. Circ buffer end
hw->host_byte_write(&RECEIVEPAGESTART,BOUNDARY);			// Read pointer.. DMA RAM addr ->IOPORT. Auto. updated

// Init PAR, MAR,CURR regs in page 1
data =0x61;
hw->host_byte_write(&data,COMMAND);							// Page 1
hw->host_byte_write(&RECEIVEPAGESTART,CURRENT);				// CURRENT pointer must be same as PAGESTART. Set after reset only.  Frames into circ buffer

n=0;
// Set mac addr
do
	{
	data = mac_addr[n++];	
	hw->host_byte_write(&data,PAR0 + i++);
	data = data>>8;
	hw->host_byte_write(&data,PAR0 + i++);
	}while(n < 3);

/*
//TODO set multicast addr - what is it ?
data = 0xff;
for(i=0;i<8;i++)
	hw->host_byte_write(&data,MAR0 + i);
*/

data =0x22;
hw->host_byte_write(&data,COMMAND);					// Start mode

data =0xff;
hw->host_byte_write(&data,INTERRUPTSTATUS);			// Clear interrupt flags
hw->host_byte_write(&imr,INTERRUPTMASK);			// Don't want IRQs
hw->host_byte_write(&tcr,TRANSMITCONFIGURATION);

configure_leds(lan[ACTIVE].led_mode);

frame_in_queue = false;

return 1;
}

//-----------------------------------------------------------------
void Clan::configure_leds(int config)
// Set up the LED functions.
// LEDS_ON or LEDS_OFF are the only configurations available
// ( else leds will be on )
{
int read;
int data = 0xC1;						// Page 3
hw->host_byte_write(&data,COMMAND);
// Get current values in the register
// and set/reset low power mode bit
hw->host_byte_read(&read,CONFIG0);
read = read & ~0x04;
if(config == LEDS_OFF)
	read = read | 0x04;

data = 0xC0;							// Config register write enable
hw->host_byte_write(&data,L9346CR);

// D3 = "Low power mode"
// 8019 just turns off its LED outputs !
hw->host_byte_write(&read,CONFIG3);

data = 0;								// Config register write disable
hw->host_byte_write(&data,L9346CR);
}

//-----------------------------------------------------------------
bool Clan::link_test(unsigned char* str,bool advice)
// Enable/disable link check.
// Text status copied into strng parameter
{
int data;
bool bRes = true;

data = 0xC1;										// Page 3
hw->host_byte_write(&data,COMMAND);
//	data = 0xC0;										// Config register write enable
//	hw->host_byte_write(&data,L9346CR);

hw->host_byte_read(&data,CONFIG0);
if(data & 0x04)
	{
	strcpy((char*)str,szLANDisconnected);
	if(advice)
		// Advise user to connect cable and reboot
		strcat((char*)str,szEthNotConnected2);
	bRes = false;
	}
else
	strcpy((char*)str,szLANConnected);

//	sprintf(msg,"CONFIG0=0x%2X (expect 00xxxN00b N=1: link fail N=0 link OK)\r\n",data);
//	serialout((char*)msg,HW_DTE0);

data = 0;											// Config register write disable
hw->host_byte_write(&data,L9346CR);

return bRes;
}

//-----------------------------------------------------------------
bool Clan::output_lan(unsigned char* outp_buf, int len,int protocol,int16 *src_mac, int16 *dest_mac, int32 dest_ip_addr)
{
// ***********************************************************************
// DriverSend 8019
// Either transmits a packet passed to it or queues up the
// packet if the transmitter is busy (COMMAND register 4 26h).
// ***********************************************************************
// Returns false if ethernet link not connected	

int data;
if( len & 0x01 )								// Make even length
	len = len + 1;

if(outp_buf != packet_buf + LAN_HDRLEN)
	{
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)szLANMovingMemory);
	memcpy(&packet_buf[14],outp_buf,len);		// Add frame to Ethernet header
	}

//sprintf(msg,"Output_lan target mac=%u",dest_mac);
//OutputDebugString(LOG_DEBUG,LOG_IP,(const char*)msg);

if(dest_mac == NULL)
	{
	// MAC address not supplied....
	dest_mac = arp->isincache(dest_ip_addr,NULL);
	if(((dest_mac[0] == 0xFFFF)&&
		(dest_mac[1] == 0xFFFF)&&
		(dest_mac[2] == 0xFFFF))||
		(dest_mac == NULL))
		{
		// If destination not in cache, send an ARP request
		arp->output_arp(dest_ip_addr,NULL);

		// Packet is discarded
		framesdiscarded++;
		return true;
		}
/*
	if(dest_mac == NULL)
		{
		// If destination not in cache, send an ARP request
		arp->output_arp(dest_ip_addr,NULL);

		// Packet is discarded
		framesdiscarded++;
		return true;
		}
*/
	}

unsigned char* p = packet_buf;
PUTMAC(dest_mac,p);
PUTMAC(src_mac,p);

packet_buf[12] = (char)(protocol>>8);		// Ethertype
packet_buf[13] = (char)protocol & 0xff;		// Ethertype

len = len + LAN_HDRLEN;
len = len < 60?  60 : len;					// Pad Ethernet frames to 60 bytes

// Report any collision
data = 0x22;
hw->host_byte_write(&data,COMMAND);			// Page 0
hw->host_byte_read(&data,TRANSMITSTATUS);
if( data == 0x0E)
	{// Link is not connected
	framesdiscarded++;
	return false;
	}
/*
if(data & 0x04)
	// Collision detected
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)"LAN collision");
*/
do
	{
	hw->host_byte_read(&data,COMMAND);
	}while(data == 0x26);

/*
// Transfer packet to NIC buffer RAM
sprintf(msg,"LAN chip sending %u bytes:",len);
OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
hw->uartOutputEmpty(HW_DTE0);
hexdump(packet_buf,len,HW_DTE0);
hw->uartOutputEmpty(HW_DTE0);
*/

PCtoNIC((char*)packet_buf,len);

data =0x21;
hw->host_byte_write(&data,COMMAND);				// Page 0

data =TRANSMITBUFFER;							// Set location of transmit bytes
hw->host_byte_write(&data,TRANSMITPAGE);
data =len;
hw->host_byte_write(&data,TRANSMITBYTECOUNT0);	// Set numberof bytes to transmit
data =len>>8;
hw->host_byte_write(&data,TRANSMITBYTECOUNT1);
data =0x26;
hw->host_byte_write(&data,COMMAND);				// Issue transmit command

// bump up stats
framessent++;
return true;
}


//-----------------------------------------------------------------
void Clan::input_lan(char *input_buf,int *len)
{
int b,c,pages_to_read;
*len = 0;
int data = 0x22;
hw->host_byte_write(&data,COMMAND);			// Page 0

hw->host_byte_read(&c,INTERRUPTSTATUS);

if (c & 0x10)
	{
	OutputDebugString(LOG_DEBUG,LOG_SYSTEM,szLANRingOverflow);
	init();
	ring_overflow_recovery();
	if(ring_overflows==1)
		// reinit and ring_overflow_recovery() failed. Reboot
		watchDogTimerDisable=1;
	
	ring_overflows=1;	
	frameoverruns++;
	return;
	}
else
	ring_overflows=0;

hw->host_byte_read(&b,BOUNDARY);			// First packet not yet read by host. DMA aborts if it reaches this address

data = 0x62;
hw->host_byte_write(&data,COMMAND);			// Page 1 of NIC
hw->host_byte_read(&c,CURRENT);				// First buffer used to store a packet
data = 0x22;
hw->host_byte_write(&data,COMMAND);			// Page 0 of NIC

b = b & 0xff;
c = c & 0xff;
// Note pages_to_read is in 256-byte pages.
pages_to_read = c-b;

if(pages_to_read < 0)
	{
	// Input buffer ring has wrapped (normal for a circular buffer)
	pages_to_read = RECEIVEPAGEEND - b;					// Data at top of buffer
	pages_to_read += c-RECEIVEPAGESTART;				// Data at bottom of buffer;
/*
	sprintf(msg,"Ring wrap CURRENT=0X%X BOUNDARY=0X%X Pages=%u",c,b,pages_to_read);
	OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);
*/
	}

if(pages_to_read == 0)
	return;

// Note len is in 256-byte pages.
*len = NICtoPC(b,(char*)packet_buf,pages_to_read);

/*
sprintf(msg,"Read %u bytes from page 0X%X",*len,b);
OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);
*/
	
hw->host_byte_read(&b,BOUNDARY);
data = 0x62;
hw->host_byte_write(&data,COMMAND);			// Page 1 of NIC
hw->host_byte_read(&c,CURRENT);
data = 0x22;
hw->host_byte_write(&data,COMMAND);			// Page 0 of NIC

// Update BOUNDARY to point at next frame if a frame was read
// after a remote DMA.
if(*len > 4)
	// NICtoPC read a frame, update BOUNDARY (read) pointer.
	hw->host_byte_write((int*)&packet_buf[1],BOUNDARY);
else
	{// NICtoPC failed to read a frame
//	OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)"NICtoPC failed to read a frame");
	hw->host_byte_write(&c,BOUNDARY);
	}

// bump up stats
framesreceived++;

// Time of last received frame is now.
idletimer_ms=getTickCount();
}

#pragma CODE_SECTION("sectionFastCode");
//-----------------------------------------------------------------
void Clan::PCtoNIC(char *outp_buf,unsigned int bytecount)
{
// ***********************************************************************
// PCtoNIC 8019 chip
//
// This routine will transfer a packet from the PC's RAM
// to the local RAM on the NIC card.
// ***********************************************************************
int n,data;

data = 0x22;
hw->host_byte_write(&data,COMMAND);				// Select NIC Page 0

/*
// Dummy read to clear the PRQ bit. Datasheet p13
// Read the addr before the transmitbuffer which is unused
data = 2;
hw->host_byte_write(&data,REMOTEBYTECOUNT0);
data = bytecount>>8;
hw->host_byte_write(&data,REMOTEBYTECOUNT1);

loop_dma:
data = 0xff;
hw->host_byte_write(&data,REMOTESTARTADDRESS0);
hw->host_byte_write(&TRANSMITBUFFER-1,REMOTESTARTADDRESS1);
data = 0x0a;
hw->host_byte_write(&data,COMMAND);				// Dummy "remote read" command. 

hw->host_byte_read(&data,REMOTESTARTADDRESS0);
if(data == 0xff)
	{
	delay_ms(1);
	goto loop_dma;
	}
*/

data = bytecount;
hw->host_byte_write(&data,REMOTEBYTECOUNT0);
data = bytecount>>8;
hw->host_byte_write(&data,REMOTEBYTECOUNT1);

data = 0;
hw->host_byte_write(&data,REMOTESTARTADDRESS0);	// Always 0
hw->host_byte_write(&TRANSMITBUFFER,REMOTESTARTADDRESS1);

data = 0x12;
hw->host_byte_write(&data,COMMAND);				// "remote write" command. Initiates NIC internal DMA

char *p = outp_buf;

for(n=0; n< bytecount; n++)
	hw->host_byte_write((int*)p++,IOPORT);

CheckDMA:
hw->host_byte_read(&data,INTERRUPTSTATUS);

if(!(data & 0x40))									// DMA done ?
	goto CheckDMA;

data = 0Xfc;//0x40;
hw->host_byte_write(&data,INTERRUPTSTATUS);		// Clear DMA interrupt bit in ISR
}

#pragma CODE_SECTION("sectionFastCode");
//-----------------------------------------------------------------
int Clan::NICtoPC(int16 buf_page,char *inp_buf,int len)
{
// ***********************************************************************
// NICtoPC 8019 chip
//
// This routine will transfer a packet from the RAM
// on the NIC card to the RAM in the DSP.
//
// len			256-byte pages to read. 16-bit var
// buf_page		read start page addr.
// Returns number of bytes read. Includes LAN chip 4 byte header
// ***********************************************************************
int data;
if(len == 0) 
	return 0;

int bytesread=0;
hw->host_byte_write(&len,REMOTEBYTECOUNT1);					// HI byte of Number of bytes to read
len = 0;
hw->host_byte_write(&len,REMOTEBYTECOUNT0);					// LO byte of Number of bytes to read

data = 0;
hw->host_byte_write(&data,REMOTESTARTADDRESS0);				// LO byte of read start addr. Always 0
hw->host_byte_write((int*)&buf_page,REMOTESTARTADDRESS1);	// HI byte of read start addr

data = 0x0a;
hw->host_byte_write(&data,COMMAND);							// Remote "DMA" read command

// Extract info from the 4 byte frame header generated by the LAN chip..
// Format : 
// | Rcv status(8) | Next frame ptr(8) | Length lo(8) | Length hi(8) |
while(bytesread < 4)
	hw->host_byte_read((int*)&inp_buf[bytesread++],IOPORT);

if(!(inp_buf[0] & 0x01))
	{
	// No errors bit is not set. Discard frame
//	sprintf(msg,"**LAN discard status=0x%X next=%u len hi=%u len=%u buf_page=0X%X",inp_buf[0],inp_buf[1],inp_buf[3],inp_buf[2],buf_page);
//	OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);

	// Abort DMA
	data = 0x22;
	hw->host_byte_write(&data,COMMAND);					// Abort DMA, page 0

	// Clear all interrupt(s)
	data = 0xff;//(int)inp_buf[0];
	hw->host_byte_write(&data,INTERRUPTSTATUS);

	sprintf(msg,szLANdiscard,inp_buf[0]);
	OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);
		
	return 0;											// 0 bytes read
	}
	
// Extract the frame length from the header.
len = ( inp_buf[2] + (inp_buf[3] << 8));
len = min(len,LAN_MRU);									// Don't attempt to read more bytes than are in a frame
len += 4;												// Add 4 bytes for LAN chip header

/*
sprintf(msg,"LAN chip receiving %u bytes on page 0x%X",len,buf_page);
OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
uartOutputEmpty();
*/

// Loop Reading a single frame
for(;bytesread < len;bytesread++)
	{
	hw->host_byte_read((int*)&inp_buf[bytesread],IOPORT);

	hw->host_byte_read(&data,INTERRUPTSTATUS);
	if(( data & 0x40 ) > 0)
		{
		// DMA hit the boundary register, or count was reached.(This is okay)
//		sprintf(msg,"DMA IRQ at n=%u",n);
//		OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);
//		uartOutputEmpty();
		break;
		}
	};

// Clear interrupt(s)
// data = data & 0x40;
data = data & 0xfc;
hw->host_byte_write(&data,INTERRUPTSTATUS);

// Abort DMA. If more frames in buffer, collect on next pass
data = 0x22;
hw->host_byte_write(&data,COMMAND);						// Abort DMA, page 0

return bytesread;
}

//-----------------------------------------------------------------
void Clan::ring_overflow_recovery()
{
int txp_bit;
int data;
bool resend = true;
int tcr = '\xe0'; 									// value for trans. config. reg

hw->host_byte_read(&txp_bit,COMMAND);				// Save the TXP bit of COMMAND reg
txp_bit = txp_bit & 0x04;

data = 0x21;
hw->host_byte_write(&data,COMMAND);					// STOP command. Page 0

// Wait 1.6ms.
delay_ms(2);

data =0;
hw->host_byte_write(&data,REMOTEBYTECOUNT0);		// Clear remote byte count regs
hw->host_byte_write(&data,REMOTEBYTECOUNT1);

if(txp_bit)
	{
	hw->host_byte_read(&data,INTERRUPTSTATUS);
	if((data& 0x04 ) || ( data &0x02 ))				// If PTX or TXE bit set...
		resend = false;
	}

data =0x02;
hw->host_byte_write(&data,TRANSMITCONFIGURATION);	// Tx Loopback mode

data =0x22;
hw->host_byte_read(&data,COMMAND);					// START command

while(data)
	input_lan((char*)packet_buf,&data);

data =0xff;
hw->host_byte_write(&data,INTERRUPTSTATUS);			// Clear interrupt flags

hw->host_byte_write(&tcr,TRANSMITCONFIGURATION);	// Tx Loopback off

if(resend)
	{
	data = 0x26;									// Issue resend command
	hw->host_byte_write(&data,COMMAND);
	}
}


