//Utils.cpp

#include "router.h"
#include "pppd.h"
#include "tcp.h"
#include "tcpmodem.h"
#include "utils.h"
#include "ffs.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

//-------------------------------------------------------------------
void OutputDebugString(int16 level,int16 module,const char* OutputString,bool endline)
// Send string to stdout or serial port buffer
{
if((level > traceout.level) || (!(module & traceout.module)))
	return;
if(traceout.suppress)
	return;	
	
int len = strlen((const char*)OutputString);
if(len == 0)
	return;
	
// Which serial port or object owns the trace output ?
unsigned int owner = hw->uart[HW_TRACE].owner;
if(owner == OWNER_NONE)
	return;

if(( owner >= OWNER_SW_OBJECT) && (owner <= OWNER_SHELL))
	// The owner is a CClient object.
	// Copy the data to the trace uart buffer.
	// Do not copy data to owner object as packet_buf will be corrupted (eg by the lan fame)
	// CRouter::get_input_uarts() will copy the trace uart data to the software object
	owner = HW_TRACE;

// Check there is space in the trace buffer for this message
if(TRACE_BUFLEN < (hw->uartOutputQueue(owner) + len + 2))
	{
	if(traceout.overrun == false)
		{
		traceout.overrun = true;							// Flag to avoid repeating this message
		// Trace buffer has overflowed ! Warn user.
		sprintf(msg,szTraceBufferOverflow);
		// Copy warning text to trace uart.
		for(int i=0 ; i<len ; i++)
			hw->uartOutputChar(owner,msg[i]);
		}
	return;
	}

traceout.overrun=false;

/*
// Debug. Trace directly out of DTE0.
// Use if a bug causes SEB to crash before (useful?) trace info is output
serialout(OutputString,HW_DTE0);
if(endline)
	serialout("\r\n",HW_DTE0);
return;
*/

// Copy trace data to owners uart buffer.
for(int i=0 ; i<len ; i++)
	hw->uartOutputChar(owner,OutputString[i]);

if(endline)
	{
	hw->uartOutputChar(owner,'\r');
	hw->uartOutputChar(owner,'\n');
	}
}

#pragma CODE_SECTION("sectionFastCode");
//-------------------------------------------------------------------
int serialout(const char *OutputString,int HW_IF,unsigned int len)
// Send string to serial port
// Return USED buffer space
{
if(len == 0xffff)
	len = strlen(OutputString);

for(int i=0 ; i<len ; i++)
	hw->uartOutputChar(HW_IF,(unsigned int)OutputString[i]);

return 	hw->uartOutputQueue(HW_IF);
}

//--------------------------------------------------------------------------
void UartAutobaudEnable(int HW_IF,bool enable,bool change_config)
// Preferred method of controlling autobaud setting.
// Sets config.character_time_uS as well as baudrate and autobaud
{
if(enable)
	{
	// Enable autobaud
	if(change_config)
		modem_config.autobaud_flag = 1;
	hw->uart[HW_IF].uart_autobaud_active = 1;
	hw->uart[HW_IF].baudOversampleRate = modem_config.baudrate_flag;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	}
else
	{
	// Disable autobaud
	if(change_config)
		modem_config.autobaud_flag = 0;
	hw->uart[HW_IF].uart_autobaud_active = 0;
	}
}

//-------------------------------------------------------------------
void UpdateUart(int HW_IF, int OnLine)
// Update serial interface from config structure
{
// If Parity is not 8N1, or if we are online switch autobaud off
if((modem_config.autobaud_flag == 0) || (modem_config.parity_flag > 0) || OnLine)
	{
	// Autobaud off
	hw->uart[HW_IF].uart_autobaud_active = 0;
	// Don't alter the baudrate when online
	if(!OnLine)
		hw->uart[HW_IF].baudOversampleRate = modem_config.baudrate_flag;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	}
else
	// Autobaud on
	hw->uart[HW_IF].uart_autobaud_active = 1;
	
hw->uart[HW_IF].uart_parity_rx = modem_config.parity_flag;
hw->uart[HW_IF].uart_parity_tx = modem_config.parity_flag;

// If flowcontrol is RTSCTS_ONLINE then update UART based on on/offline condition
if((!(bool)OnLine) && (modem_config.flowcontrol_flag == uartFlowControl_RTSCTS_ONLINE))
	hw->uart[HW_IF].uartFlowControl = uartFlowControl_OFF;
else
	hw->uart[HW_IF].uartFlowControl = modem_config.flowcontrol_flag;

//if(OnLine)
	// Reset xoff flow control condition
//	hw->uart[HW_IF].rxXon=1;
	
if(modem_config.dcdflag == 0)
	// DCD is forced on
	hw->uart[HW_IF].flag_dcd = HI;
else
	hw->uart[HW_IF].flag_dcd = LO;
}

//-------------------------------------------------------------------
int16 freemem()
// Return the amount of free memory
{
char* heap = new(char[100]);		// Alloc a chunk so it sits on the top of the heap. A single char could be fitted in free space lower down.
if(!heap)
	return 0;
	
delete[] heap;
int16 freemem = /*__SYSMEM_SIZE*/ 0x4000 - (int16)heap;
return freemem;
}

//-------------------------------------------------------------------
extern unsigned int lan_delay_ms;
void delay_ms(int time)
// Returns after specified delay in ms
{
lan_delay_ms=0;
while(time >= lan_delay_ms)
	continue;
}

//--------------------------------------------------------------------------
int32 getTickCount()
// Returns 32-bit  value containing time in ms.
{
#ifdef LO_POWER
return (int32) GBL_count_1ms << 1;
#else
// Set time to 5m30s before pre-1.95 Arp cache bug starts: ADD 65205000
return (int32)GBL_count_1ms;
#endif
}

#ifndef SEB
//-------------------------------------------------------------------
int GetHardwareInterface(int OWNER_IF_NUM,CClient *pOwner,int HW_CLASS)
// HW_CLASS - type of interface requested, DTE/TA/X21/LAN
// OWNER_IF_NUM - specifies who is asking - either:
// 	if (OWNER_IF_NUM < NUM_UARTS),owner is hardware too (eg DTE requesting TA)
//	if (OWNER_IF_NUM >=NUM_UARTS),owner is software object (see OWNER_ definitions for object class)
// pOwner - "this" pointer to the software owner

// *****??Restriction?? - if both are HW interfaces, the owner must be a DTE port*******
// This is because DTE ports are IRQ driven.
{
int IF_NUM = 0;

if (HW_CLASS < NUM_UARTS)
	{
	// Open a specific DTE port

/*
	if(hardware_if[IF_NUM].owner != OWNER_NONE)	
		{
		// Requested interface is in use
		OutputDebugString(LOG_DEBUG,LOG_DEBUG,"Cannot open port",false);
		return HW_NONE;
		}
*/
	OutputDebugString(LOG_DEBUG,LOG_DEBUG,(const char*)owner_2ascii(OWNER_IF_NUM),false);

	sprintf(msg,szHWselected,HW_2ascii(IF_NUM));
	OutputDebugString(LOG_DEBUG,LOG_DEBUG,(const char*)msg);
			
	// Interface free ! Mark up who owns it.
	hw->uart[IF_NUM].owner = OWNER_IF_NUM;
	hw->uart[IF_NUM].pClient = (taguart::CClient*)pOwner;
	// Flush serial buffers
	hw->uartOutputFlush(IF_NUM);
	hw->uartInputFlush(IF_NUM);

	return IF_NUM;
	}

// Open a port of the specified class
// eg. HW_CLASS_DTE
for(; IF_NUM < NUM_UARTS; IF_NUM++ )
		{
		// Find a free interface of the right class ( DTE/TA/X21/LAN )
		if(( hw->uart[IF_NUM].owner == OWNER_NONE ) &&
			( hw->uart[IF_NUM].HW_CLASS == HW_CLASS ))
			{
			// Found a suitable interface ! Mark up who owns it.
			hw->uart[IF_NUM].owner = OWNER_IF_NUM;
			hw->uart[IF_NUM].pClient = (taguart::CClient*)pOwner;
			// Flush serial buffers
			hw->uart[IF_NUM].uart_buffer_rx_out = hw->uart[IF_NUM].uart_buffer_rx_in;
			hw->uart[IF_NUM].uart_buffer_tx_out = hw->uart[IF_NUM].uart_buffer_tx_in;		

			// If the owner is a hardware interface too, mark it as owned
			// by the interface it is selecting.
			if(OWNER_IF_NUM < NUM_UARTS)
				{
				// Owner is a hardware interface
				hw->uart[OWNER_IF_NUM].owner = IF_NUM;
				hw->uart[OWNER_IF_NUM].pClient = NULL;
				// Flush serial buffers
				hw->uart[IF_NUM].uart_buffer_rx_out = hw->uart[IF_NUM].uart_buffer_rx_in;
				hw->uart[IF_NUM].uart_buffer_tx_out = hw->uart[IF_NUM].uart_buffer_tx_in;
				}

			OutputDebugString(LOG_DEBUG,LOG_DEBUG,(const char*)owner_2ascii(OWNER_IF_NUM),false);

			sprintf(msg,szHWselected,HW_2ascii(IF_NUM));
			OutputDebugString(LOG_DEBUG,LOG_DEBUG,(const char*)msg);

			return IF_NUM;
			}
		}

sprintf(msg,szHWunavailable,owner_2ascii(OWNER_IF_NUM));
OutputDebugString(LOG_DEBUG,LOG_DEBUG,(const char*)msg);

return HW_NONE;
}


bool IsHardwareInterfaceAvailable(int HW_CLASS)
// HW_CLASS - type of interface requested, DTE/TA/X21/LAN
{
int IF_NUM = 0;

for(; IF_NUM < NUM_UARTS; IF_NUM++ )
		{
		// Find a free interface of the right class ( DTE/TA/X21/LAN )
		if(( hw->uart[IF_NUM].owner == OWNER_NONE ) &&
			( hw->uart[IF_NUM].HW_CLASS == HW_CLASS ))
			{
			sprintf(msg,szHWavailable,HW_2ascii(IF_NUM));
			OutputDebugString(LOG_DEBUG,LOG_DEBUG,(const char*)msg);
			return IF_NUM;
			}
		}

OutputDebugString(LOG_DEBUG,LOG_DEBUG,szHWunavailable);
return HW_NONE;
}

//-------------------------------------------------------------------
int GetHardwareOwner(CClient *pTcpClient)
// Called by an object to discover what hardware port it is connected to.
// Returns HW_NONE if the object is not connected to a DTE or ISDN prt
{
int IF_NUM= 0;

for(; IF_NUM < NUM_UARTS; IF_NUM++ )
		{
		if(hw->uart[IF_NUM].owner != OWNER_NONE )
			{
			// This interface is owned
			if (pTcpClient == (CClient*)hw->uart[IF_NUM].pClient)
				{
//				sprintf(msg,"\r\nClient has owner %s\r\n",HW_2ascii(IF_NUM),IF_NUM);
//				OutputDebugString(LOG_DEBUG,LOG_DEBUG,(const char*)msg);
				return IF_NUM;
				}
			}
		}
		
//sprintf(msg,"\r\nNo hardware connected to owner object\r\n",ipstr);
//OutputDebugString(LOG_DEBUG,LOG_DEBUG,(const char*)msg);
return HW_NONE;
}

//-------------------------------------------------------------------
int ReleaseHardwareInterface(int IF_NUM,int owner)
// Mark up a hardware interface as available.
// If it is connected/owned by another hardware interface, then
// mark that up as free too.
{
if(IF_NUM >= NUM_UARTS)
	// Error not a hardware interface !
	return OWNER_NONE;

if(owner != hw->uart[IF_NUM].owner)
	{
//	owner_2ascii(msg,owner);
	if(owner != HW_ALL)
		{
//		sprintf(msg,"Wrong owner %s attempted release IF_NUM=%d\r\n",,IF_NUM);
//		OutputDebugString(LOG_DEBUG,LOG_DEBUG,(const char*)msg);
		return false;
		}
	}

sprintf(msg,szHWrelease,HW_2ascii(IF_NUM));
strcat((char*)msg,(const char*)owner_2ascii(owner));
OutputDebugString(LOG_DEBUG,LOG_DEBUG,(const char*)msg);

int OWNER_IF_NUM = hw->uart[IF_NUM].owner;

hw->uart[IF_NUM].owner = OWNER_NONE;
hw->uart[IF_NUM].pClient = NULL;

if(OWNER_IF_NUM < NUM_UARTS)
	{
	// Owner is a hardware interface
	hw->uart[OWNER_IF_NUM].owner = OWNER_NONE;
	hw->uart[OWNER_IF_NUM].pClient = NULL;
	}

return true;
}
#endif // ifndef SEB

